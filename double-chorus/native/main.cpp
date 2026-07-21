#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "biquad.h"
#include "Dynamics/crossfade.h"
#include "dchorus.h"

using namespace daisysp;

struct AppState
{
    DChorus             ch;
    CrossFade           cfade;
    LowpassBiquad       filt;
    float               mix    = 0.9f;
    float               makeup = 1.0f;
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

        float chorus_sig    = state->ch.Process(sample);
        float mixed          = state->cfade.Process(sample, chorus_sig);
        float makeup_sample  = mixed * state->makeup;
        float processed       = state->filt.Process(makeup_sample);

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
    const char *wavPath  = nullptr;
    float       delay    = 0.5f;  // 0-1
    float       speed    = 0.3f;  // 0-1, mapped to speed*speed*20Hz
    float       depth    = 0.5f;  // 0-1
    float       mix      = 0.9f;
    float       feedback = 0.2f;
    float       makeup   = 0.5f;  // 0-1, mapped to makeup*2

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--delay") == 0 && i + 1 < argc)
        {
            delay = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--speed") == 0 && i + 1 < argc)
        {
            speed = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--depth") == 0 && i + 1 < argc)
        {
            depth = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
        {
            mix = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--makeup") == 0 && i + 1 < argc)
        {
            makeup = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--delay <0-1>] [--speed <0-1>] "
                    "[--depth <0-1>] [--mix <0-1>] [--feedback <0-1>] "
                    "[--makeup <0-1>]\n",
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

    state.mix    = mix;
    state.makeup = makeup * 2.0f;
    state.ch.Init(static_cast<float>(sampleRate));
    state.ch.SetLfoFreq(speed * speed * 20.0f);
    state.ch.SetDelay(delay);
    state.ch.SetLfoDepth(depth);
    state.ch.SetFeedback(feedback);
    state.cfade.Init();
    state.cfade.SetCurve(CROSSFADE_CPOW);
    state.cfade.SetPos(mix);
    state.filt.Init(10000.0, 0.7071, 1.0, sampleRate);

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
        printf("Playing '%s' through double-chorus (delay=%.2f speed=%.2f "
               "depth=%.2f mix=%.2f feedback=%.2f makeup=%.2f)...\n",
               wavPath, delay, speed, depth, mix, feedback, makeup);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (delay=%.2f speed=%.2f depth=%.2f mix=%.2f "
               "feedback=%.2f makeup=%.2f). Press Ctrl+C to stop.\n",
               delay, speed, depth, mix, feedback, makeup);
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
