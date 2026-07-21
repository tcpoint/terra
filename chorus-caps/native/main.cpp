#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

// dsp/Delay.h uses the BSD-style `uint` typedef, which the original hardware
// build gets transitively from newlib; glibc doesn't provide it by default.
typedef unsigned int uint;

#include "common.h"
#include "Chorus.h"

using namespace DSP;

struct AppState
{
    HP1<sample_t> hp;
    Delay         delay;
    struct
    {
        Sine sine;
    } lfo;
    float               fs       = 48000.0f;
    float               time     = 15.0f;
    float               width    = 3.0f;
    float               blend    = 0.7f;
    float               ff       = 0.5f;
    float               fb       = 0.3f;
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

    float t = state->time;
    float w = state->width;
    if(w >= t - 3)
        w = t - 3;

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

        sample_t x = sample;
        sample_t y = x;
        x = state->hp.process(x);
        y -= state->fb * state->delay.get_linear(t);
        state->delay.put(y);
        y += state->blend * x
             + state->ff * state->delay.get_cubic(t + w * state->lfo.sine.get());

        out[i * 2 + 0] = y;
        out[i * 2 + 1] = y;
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
    const char *wavPath      = nullptr;
    float       timeMs       = 15.0f;  // 2.5-40 ms
    float       widthMs      = 3.0f;   // 0.5-10 ms
    float       rate         = 0.3f;   // 0.02-5 Hz
    float       blend        = 0.7f;   // 0-1
    float       feedforward  = 0.5f;   // 0-1
    float       feedback     = 0.3f;   // 0-1

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--time") == 0 && i + 1 < argc)
        {
            timeMs = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--width") == 0 && i + 1 < argc)
        {
            widthMs = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
        {
            rate = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--blend") == 0 && i + 1 < argc)
        {
            blend = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedforward") == 0 && i + 1 < argc)
        {
            feedforward = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--time <2.5-40ms>] [--width <0.5-10ms>] "
                    "[--rate <0.02-5hz>] [--blend <0-1>] [--feedforward <0-1>] "
                    "[--feedback <0-1>]\n",
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

    state.fs = static_cast<float>(sampleRate);
    float ms = state.fs * .001f;
    state.time  = timeMs * ms;
    state.width = widthMs * ms;
    state.blend = blend;
    state.ff    = feedforward;
    state.fb    = feedback;

    state.delay.init(static_cast<uint>(.050 * state.fs));
    state.delay.reset();
    state.hp.reset();
    state.hp.set_f(250.0f / state.fs);
    state.lfo.sine.set_f(rate, state.fs, 0);

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
        printf("Playing '%s' through chorus-caps (time=%.1fms width=%.1fms "
               "rate=%.2fHz blend=%.2f feedforward=%.2f feedback=%.2f)...\n",
               wavPath, timeMs, widthMs, rate, blend, feedforward, feedback);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (time=%.1fms width=%.1fms rate=%.2fHz blend=%.2f "
               "feedforward=%.2f feedback=%.2f). Press Ctrl+C to stop.\n",
               timeMs, widthMs, rate, blend, feedforward, feedback);
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
