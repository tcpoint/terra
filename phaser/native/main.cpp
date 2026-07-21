#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "Effects/phaser.h"

using namespace daisysp;

struct AppState
{
    Phaser              phaser;
    float                mix       = 0.5f;
    int                  numstages = 4;
    std::vector<float>   wavSamples;
    size_t               wavPos  = 0;
    bool                 wavMode = false;
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

        float phasorOut = state->phaser.Process(sample) / state->numstages;
        float mixed     = phasorOut * state->mix + sample * (1.0f - state->mix);

        out[i * 2 + 0] = mixed;
        out[i * 2 + 1] = mixed;
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

    sf_count_t tailPadding = static_cast<sf_count_t>(info.samplerate) * 2;
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
    const char *wavPath   = nullptr;
    float       mix       = 0.5f;
    float       freq      = 0.3f;  // 0-1, mapped to freq*freq*7000Hz
    float       feedback  = 0.3f;
    float       lfoFreq   = 0.3f;  // 0-1, mapped to freq*freq*20Hz
    float       lfoDepth  = 0.5f;
    int         stages    = 4;     // 1-8

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
        {
            mix = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--freq") == 0 && i + 1 < argc)
        {
            freq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--lfo-freq") == 0 && i + 1 < argc)
        {
            lfoFreq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--lfo-depth") == 0 && i + 1 < argc)
        {
            lfoDepth = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--stages") == 0 && i + 1 < argc)
        {
            stages = atoi(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--mix <0-1>] [--freq <0-1>] "
                    "[--feedback <0-1>] [--lfo-freq <0-1>] [--lfo-depth <0-1>] "
                    "[--stages <1-8>]\n",
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

    state.mix       = mix;
    state.numstages = stages;
    state.phaser.Init(static_cast<float>(sampleRate));
    state.phaser.SetFreq(freq * freq * 7000.0f);
    state.phaser.SetFeedback(feedback);
    state.phaser.SetLfoFreq(lfoFreq * lfoFreq * 20.0f);
    state.phaser.SetLfoDepth(lfoDepth);
    state.phaser.SetPoles(stages);

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
        printf("Playing '%s' through phaser (mix=%.2f freq=%.2f feedback=%.2f "
               "lfo-freq=%.2f lfo-depth=%.2f stages=%d)...\n",
               wavPath, mix, freq, feedback, lfoFreq, lfoDepth, stages);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (mix=%.2f freq=%.2f feedback=%.2f lfo-freq=%.2f "
               "lfo-depth=%.2f stages=%d). Press Ctrl+C to stop.\n",
               mix, freq, feedback, lfoFreq, lfoDepth, stages);
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
