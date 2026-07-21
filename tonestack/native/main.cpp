#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "ToneStack.h"

using namespace DSP;

// Copied from ../tonestack.cpp (self-contained data, no hardware dependency).
TSParameters ToneStack::presets[] = {
    #define k *1e3
    #define M *1e6
    #define nF *1e-9
    #define pF *1e-12
    {250 k, 1 M, 25 k, 56 k, 250 pF, 20 nF, 20 nF},        /* 59 Bassman 5F6-A */
    {250 k, 250 k, 4.8 k, 100 k, 250 pF, 100 nF, 47 nF},   /* 64 Princeton AA1164 */
    {250 k, 1 M, 25 k, 47 k, 600 pF, 20 nF, 20 nF},         /* Mesa Dual Rect. 'Orange' */
    {1 M, 1 M, 10 k, 100 k, 50 pF, 22 nF, 22 nF},           /* Vox "top boost" */
    {220 k, 1 M, 22 k, 33 k, 470 pF, 22 nF, 22 nF},         /* 59/81 JCM-800 Lead 100 2203 */
    {250 k, 250 k, 10 k, 100 k, 120 pF, 100 nF, 47 nF},     /* 69 Twin Reverb AA270 */
    {500 k, 1 M, 25 k, 47 k, 150 pF, 22 nF, 22 nF},         /* Hughes & Kettner Tube 20 */
    {250 k, 250 k, 10 k, 100 k, 150 pF, 82 nF, 47 nF},      /* Roland Jazz Chorus */
    {250 k, 1 M, 50 k, 33 k, 100 pF, 22 nF, 22 nF},         /* Pignose G40V */
    #undef k
    #undef M
    #undef nF
    #undef pF
};

struct AppState
{
    ToneStack           tonestack;
    std::vector<float>  wavSamples;
    size_t              wavPos  = 0;
    bool                wavMode = false;
};

static std::atomic<bool> g_keepRunning{true};

static void HandleSigint(int) { g_keepRunning = false; }

static int AudioCallback(const void                    *inputBuffer,
                          void                          *outputBuffer,
                          unsigned long                  framesPerBuffer,
                          const PaStreamCallbackTimeInfo *,
                          PaStreamCallbackFlags,
                          void *userData)
{
    AppState    *state = static_cast<AppState *>(userData);
    float       *out   = static_cast<float *>(outputBuffer);
    const float *in    = static_cast<const float *>(inputBuffer);

    bool done = false;
    for(unsigned long i = 0; i < framesPerBuffer; i++)
    {
        float sample = 0.0f;
        if(state->wavMode)
        {
            if(state->wavPos < state->wavSamples.size())
            {
                sample = state->wavSamples[state->wavPos++];
            }
            else
            {
                done = true;
            }
        }
        else
        {
            sample = in[i];
        }

        float processed = state->tonestack.process(sample);

        out[i * 2 + 0] = processed;
        out[i * 2 + 1] = processed;
    }

    return done ? paComplete : paContinue;
}

static bool LoadWav(const char *path, std::vector<float> &samplesOut, int &sampleRateOut)
{
    SF_INFO info;
    memset(&info, 0, sizeof(info));
    SNDFILE *file = sf_open(path, SFM_READ, &info);
    if(!file)
    {
        fprintf(stderr, "Failed to open WAV file '%s': %s\n", path, sf_strerror(nullptr));
        return false;
    }

    std::vector<float> interleaved(static_cast<size_t>(info.frames) * info.channels);
    sf_count_t read = sf_readf_float(file, interleaved.data(), info.frames);
    sf_close(file);

    sf_count_t tailPadding = static_cast<sf_count_t>(info.samplerate) * 1;
    samplesOut.assign(static_cast<size_t>(read + tailPadding), 0.0f);
    for(sf_count_t i = 0; i < read; i++)
    {
        float sum = 0.0f;
        for(int c = 0; c < info.channels; c++)
        {
            sum += interleaved[i * info.channels + c];
        }
        samplesOut[i] = sum / info.channels;
    }

    sampleRateOut = info.samplerate;
    return true;
}

int main(int argc, char *argv[])
{
    const char *wavPath = nullptr;
    int         model   = 4;  // JCM-800
    float       bass    = 0.5f;
    float       mids    = 0.5f;
    float       treble  = 0.5f;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--model") == 0 && i + 1 < argc)
        {
            model = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--bass") == 0 && i + 1 < argc)
        {
            bass = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--mids") == 0 && i + 1 < argc)
        {
            mids = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--treble") == 0 && i + 1 < argc)
        {
            treble = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--model <0-8>] [--bass <0-1>] "
                    "[--mids <0-1>] [--treble <0-1>]\n",
                    argv[0]);
            return 1;
        }
    }

    AppState state;
    int      sampleRate = 48000;

    if(wavPath != nullptr)
    {
        if(!LoadWav(wavPath, state.wavSamples, sampleRate))
        {
            return 1;
        }
        state.wavMode = true;
    }

    state.tonestack.init(static_cast<float>(sampleRate));
    state.tonestack.setmodel(model);
    state.tonestack.updatecoefs(bass, mids, treble);

    PaError err = Pa_Initialize();
    if(err != paNoError)
    {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    PaStream *stream        = nullptr;
    int       inputChannels = state.wavMode ? 0 : 1;
    err = Pa_OpenDefaultStream(&stream, inputChannels, 2, paFloat32, sampleRate,
                                256, AudioCallback, &state);
    if(err != paNoError)
    {
        fprintf(stderr, "Failed to open audio stream: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if(err != paNoError)
    {
        fprintf(stderr, "Failed to start audio stream: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 1;
    }

    if(state.wavMode)
    {
        printf("Playing '%s' through tonestack (model=%d bass=%.2f mids=%.2f "
               "treble=%.2f)...\n",
               wavPath, model, bass, mids, treble);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (model=%d bass=%.2f mids=%.2f treble=%.2f). "
               "Press Ctrl+C to stop.\n",
               model, bass, mids, treble);
        while(g_keepRunning)
        {
            Pa_Sleep(100);
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
