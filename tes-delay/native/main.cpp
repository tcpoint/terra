#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "Utility/delayline.h"
#include "Utility/dsp.h"
#include "Dynamics/crossfade.h"
#include "Filters/tone.h"
#include "Dynamics/balance.h"

using namespace daisysp;

constexpr size_t MAX_DELAY   = static_cast<size_t>(48000 * 1.f);
constexpr float  FIXED_FREQ  = 8500.0f;

struct Delay
{
    DelayLine<float, MAX_DELAY> line;
    Tone                        flt_8_5k;
    Tone                        flt;
    Balance                     bal;
    float                       currentDelay = 0.0f;
    float                       delayTarget  = 0.0f;
    float                       feedback     = 0.0f;

    float Process(float in)
    {
        fonepole(currentDelay, delayTarget, .0002f);
        line.SetDelay(currentDelay);
        float read            = line.Read();
        float filtered_sample = flt_8_5k.Process(in);
        filtered_sample       = flt.Process(filtered_sample);
        filtered_sample       = bal.Process(filtered_sample, in);
        line.Write(((feedback * read) + filtered_sample) * 0.5f);
        return read;
    }
};

struct AppState
{
    Delay              delay;
    CrossFade          cfade;
    std::vector<float> wavSamples;
    size_t             wavPos   = 0;
    bool               wavMode  = false;
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
    AppState   *state = static_cast<AppState *>(userData);
    float      *out   = static_cast<float *>(outputBuffer);
    const float *in   = static_cast<const float *>(inputBuffer);

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

        float delayed = state->delay.Process(sample);
        float mixed   = state->cfade.Process(sample, delayed);

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
    float       delayMs   = 300.0f;
    float       feedback  = 0.3f;
    float       mix       = 0.5f;
    float       toneFreq  = 1000.0f;  // one of 1000/2000/4000/8000 on hardware; any Hz here

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--delay") == 0 && i + 1 < argc)
        {
            delayMs = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
        {
            mix = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--tone-freq") == 0 && i + 1 < argc)
        {
            toneFreq = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--delay <ms>] [--feedback <0-1>] "
                    "[--mix <0-1>] [--tone-freq <hz>]\n",
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

    state.delay.line.Init();
    state.delay.flt_8_5k.Init(static_cast<float>(sampleRate));
    state.delay.flt_8_5k.SetFreq(FIXED_FREQ);
    state.delay.flt.Init(static_cast<float>(sampleRate));
    state.delay.flt.SetFreq(toneFreq);
    state.delay.bal.Init(static_cast<float>(sampleRate));
    state.delay.delayTarget = state.delay.currentDelay = delayMs * 0.001f * sampleRate;
    state.delay.feedback                                = feedback;
    state.cfade.Init();
    state.cfade.SetCurve(CROSSFADE_CPOW);
    state.cfade.SetPos(mix);

    PaError err = Pa_Initialize();
    if(err != paNoError)
    {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    PaStream *stream         = nullptr;
    int       inputChannels  = state.wavMode ? 0 : 1;
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
        printf("Playing '%s' through tes-delay (delay=%.0fms feedback=%.2f mix=%.2f "
               "tone-freq=%.0fHz)...\n",
               wavPath, delayMs, feedback, mix, toneFreq);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (delay=%.0fms feedback=%.2f mix=%.2f tone-freq=%.0fHz). "
               "Press Ctrl+C to stop.\n",
               delayMs, feedback, mix, toneFreq);
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
