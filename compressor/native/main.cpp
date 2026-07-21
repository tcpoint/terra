#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "Dynamics/compressor.h"

using namespace daisysp;

struct AppState
{
    Compressor          comp;
    bool                autogain = false;
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

        float key      = fabsf(sample);
        float compressed = state->comp.Process(sample, key);

        out[i * 2 + 0] = compressed;
        out[i * 2 + 1] = compressed;
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
    const char *wavPath   = nullptr;
    float       attack    = 0.01f;  // 0.01 - 1.0
    float       release   = 0.1f;   // 0.01 - 1.0
    float       ratio     = 4.0f;   // 1 - 40
    float       threshold = -24.0f; // -80 - 0 dB
    float       makeup    = 1.0f;   // 1 - 40, ignored if --autogain
    bool        autogain  = false;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--attack") == 0 && i + 1 < argc)
        {
            attack = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--release") == 0 && i + 1 < argc)
        {
            release = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--ratio") == 0 && i + 1 < argc)
        {
            ratio = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--threshold") == 0 && i + 1 < argc)
        {
            threshold = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--makeup") == 0 && i + 1 < argc)
        {
            makeup = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--autogain") == 0 && i + 1 < argc)
        {
            autogain = atoi(argv[++i]) != 0;
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--attack <0.01-1>] [--release <0.01-1>] "
                    "[--ratio <1-40>] [--threshold <-80-0>] [--makeup <1-40>] "
                    "[--autogain <0|1>]\n",
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

    state.comp.Init(static_cast<float>(sampleRate));
    state.comp.SetAttack(attack);
    state.comp.SetRelease(release);
    state.comp.SetRatio(ratio);
    state.comp.SetThreshold(threshold);
    if(autogain)
    {
        state.comp.AutoMakeup(true);
    }
    else
    {
        state.comp.SetMakeup(makeup);
    }

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
        printf("Playing '%s' through compressor (attack=%.3f release=%.3f ratio=%.1f "
               "threshold=%.1f makeup=%.1f autogain=%d)...\n",
               wavPath, attack, release, ratio, threshold, makeup, autogain);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (attack=%.3f release=%.3f ratio=%.1f threshold=%.1f "
               "makeup=%.1f autogain=%d). Press Ctrl+C to stop.\n",
               attack, release, ratio, threshold, makeup, autogain);
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
