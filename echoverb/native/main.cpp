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
#include "Effects/reverbsc.h"
#include "biquad.h"

using namespace daisysp;

constexpr size_t MAX_DELAY = static_cast<size_t>(48000 * 1.f);

struct BasicDelay
{
    DelayLine<float, MAX_DELAY> line;
    LowpassBiquad               filt1, filt2;
    CrossFade                   cfade;
    float                       currentDelay = 0.0f;
    float                       delayTarget  = 0.0f;
    float                       feedback     = 0.0f;
    float                       mix          = 0.5f;
    bool                        filterOn     = false;

    void Init(float samplerate)
    {
        line.Init();
        cfade.Init();
        cfade.SetCurve(CROSSFADE_CPOW);
        filt1.Init(2000.0, 0.7071, 1.0, samplerate);
        filt2.Init(2000.0, 0.7071, 1.0, samplerate);
    }

    float Process(float sample)
    {
        cfade.SetPos(mix);
        float processedSample = sample;
        if(filterOn)
        {
            processedSample = filt1.Process(processedSample);
            processedSample = filt2.Process(processedSample);
        }
        fonepole(currentDelay, delayTarget, .0002f);
        line.SetDelay(currentDelay);
        float read = line.Read();
        line.Write(((feedback * read) + processedSample) * 0.5f);
        return cfade.Process(sample, read);
    }
};

struct Verb
{
    ReverbSc verb;
    float    send = 0.4f;

    void Init(float samplerate) { verb.Init(samplerate); }

    void Process(float inL, float inR, float &outL, float &outR)
    {
        float sendl = inL * send;
        float sendr = inR * send;
        float wetl, wetr;
        verb.Process(sendl, sendr, &wetl, &wetr);
        outL = inL + wetl;
        outR = inR + wetr;
    }
};

struct AppState
{
    BasicDelay          delay;
    Verb                verb;
    bool                delayFirst = true;
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

        float l, r;
        if(state->delayFirst)
        {
            float delayed = state->delay.Process(sample);
            state->verb.Process(delayed, delayed, l, r);
        }
        else
        {
            float verbed_l, verbed_r;
            state->verb.Process(sample, sample, verbed_l, verbed_r);
            l = state->delay.Process(verbed_l);
            r = state->delay.Process(verbed_r);
        }

        out[i * 2 + 0] = l;
        out[i * 2 + 1] = r;
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

    sf_count_t tailPadding = static_cast<sf_count_t>(info.samplerate) * 4;
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
    float       delayMs  = 300.0f;
    float       feedback = 0.3f;
    float       mix      = 0.5f;
    bool        filterOn = false;
    float       verbTime = 0.8f;
    float       verbFreq = 8000.0f;
    float       send     = 0.4f;
    bool        delayFirst = true;

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
        else if(strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
        {
            filterOn = atoi(argv[++i]) != 0;
        }
        else if(strcmp(argv[i], "--time") == 0 && i + 1 < argc)
        {
            verbTime = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--verb-freq") == 0 && i + 1 < argc)
        {
            verbFreq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--send") == 0 && i + 1 < argc)
        {
            send = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--order") == 0 && i + 1 < argc)
        {
            delayFirst = strcmp(argv[++i], "verb-first") != 0;
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--delay <ms>] [--feedback <0-1>] "
                    "[--mix <0-1>] [--filter <0|1>] [--time <0.6-0.999>] "
                    "[--verb-freq <500-20000>] [--send <0-1>] "
                    "[--order delay-first|verb-first]\n",
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

    state.delayFirst = delayFirst;

    state.delay.Init(static_cast<float>(sampleRate));
    state.delay.delayTarget = state.delay.currentDelay = delayMs * 0.001f * sampleRate;
    state.delay.feedback                                = feedback;
    state.delay.mix                                     = mix;
    state.delay.filterOn                                = filterOn;

    state.verb.Init(static_cast<float>(sampleRate));
    state.verb.verb.SetFeedback(verbTime);
    state.verb.verb.SetLpFreq(verbFreq);
    state.verb.send = send;

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
        printf("Playing '%s' through echoverb (delay=%.0fms feedback=%.2f mix=%.2f "
               "filter=%d time=%.3f verb-freq=%.0f send=%.2f order=%s)...\n",
               wavPath, delayMs, feedback, mix, filterOn, verbTime, verbFreq, send,
               delayFirst ? "delay-first" : "verb-first");
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (delay=%.0fms feedback=%.2f mix=%.2f filter=%d time=%.3f "
               "verb-freq=%.0f send=%.2f order=%s). Press Ctrl+C to stop.\n",
               delayMs, feedback, mix, filterOn, verbTime, verbFreq, send,
               delayFirst ? "delay-first" : "verb-first");
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
