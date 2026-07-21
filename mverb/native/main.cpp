#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "MVerb.h"

constexpr unsigned long kBlockSize = 256;

struct AppState
{
    MVerb<float>        mverb;
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

    float inL[kBlockSize], inR[kBlockSize], outL[kBlockSize], outR[kBlockSize];

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
        inL[i] = inR[i] = sample;
    }

    const float *inputs[2]  = {inL, inR};
    float       *outputs[2] = {outL, outR};
    state->mverb.process(inputs, outputs, static_cast<int>(framesPerBuffer));

    for(unsigned long i = 0; i < framesPerBuffer; i++)
    {
        out[i * 2 + 0] = outL[i];
        out[i * 2 + 1] = outR[i];
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

    // Reverb tails are long: pad several seconds of trailing silence.
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
    const char *wavPath       = nullptr;
    float       dampingFreq   = 0.5f;
    float       bandwidthFreq = 0.8f;
    float       decay         = 0.6f;
    float       preDelay      = 0.1f;
    float       size          = 0.7f;
    float       mix           = 0.4f;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--damping") == 0 && i + 1 < argc)
        {
            dampingFreq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--bandwidth") == 0 && i + 1 < argc)
        {
            bandwidthFreq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--decay") == 0 && i + 1 < argc)
        {
            decay = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--predelay") == 0 && i + 1 < argc)
        {
            preDelay = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            size = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
        {
            mix = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--damping <0-1>] [--bandwidth <0-1>] "
                    "[--decay <0-1>] [--predelay <0-1>] [--size <0-1>] [--mix <0-1>]\n",
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

    state.mverb.setSampleRate(static_cast<float>(sampleRate));
    state.mverb.setParameter(MVerb<float>::DENSITY, 0.5f);
    state.mverb.setParameter(MVerb<float>::GAIN, 1.0f);
    state.mverb.setParameter(MVerb<float>::EARLYMIX, 0.75f);
    state.mverb.setParameter(MVerb<float>::DAMPINGFREQ, dampingFreq);
    state.mverb.setParameter(MVerb<float>::BANDWIDTHFREQ, bandwidthFreq);
    state.mverb.setParameter(MVerb<float>::DECAY, decay);
    state.mverb.setParameter(MVerb<float>::PREDELAY, preDelay);
    state.mverb.setParameter(MVerb<float>::SIZE, size);
    state.mverb.setParameter(MVerb<float>::MIX, mix);

    PaError err = Pa_Initialize();
    if(err != paNoError)
    {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    PaStream *stream        = nullptr;
    int       inputChannels = state.wavMode ? 0 : 1;
    err = Pa_OpenDefaultStream(&stream, inputChannels, 2, paFloat32, sampleRate,
                                kBlockSize, AudioCallback, &state);
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
        printf("Playing '%s' through mverb (damping=%.2f bandwidth=%.2f decay=%.2f "
               "predelay=%.2f size=%.2f mix=%.2f)...\n",
               wavPath, dampingFreq, bandwidthFreq, decay, preDelay, size, mix);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (damping=%.2f bandwidth=%.2f decay=%.2f predelay=%.2f "
               "size=%.2f mix=%.2f). Press Ctrl+C to stop.\n",
               dampingFreq, bandwidthFreq, decay, preDelay, size, mix);
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
