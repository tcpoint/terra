#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "zflanger.h"

struct AppState
{
    ZFlanger            flanger;
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

        float processed = state->flanger.process(sample);

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
    const char *wavPath   = nullptr;
    float       delay     = 0.5f;   // 0-1, maps to .1-7ms
    float       feedback  = 0.3f;   // 0-1
    float       lfoManual = 0.0f;   // -1 to 1
    float       lfoFreq   = 0.3f;   // 0-10 Hz
    float       lfoDepth  = 0.5f;   // 0-1
    int         waveform  = 0;      // ZLFO::WV_TRIANGLE etc, 0-3

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
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--lfo-manual") == 0 && i + 1 < argc)
        {
            lfoManual = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--lfo-freq") == 0 && i + 1 < argc)
        {
            lfoFreq = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--lfo-depth") == 0 && i + 1 < argc)
        {
            lfoDepth = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--waveform") == 0 && i + 1 < argc)
        {
            waveform = atoi(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--delay <0-1>] [--feedback <0-1>] "
                    "[--lfo-manual <-1-1>] [--lfo-freq <0-10>] [--lfo-depth <0-1>] "
                    "[--waveform <0-3>]\n",
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

    state.flanger.init(static_cast<float>(sampleRate));
    state.flanger.setDelay(delay);
    state.flanger.setFeedback(feedback);
    state.flanger.setLFOManual(lfoManual);
    state.flanger.setLFOFreq(lfoFreq);
    state.flanger.setLFODepth(lfoDepth);
    state.flanger.setWaveform(static_cast<uint8_t>(waveform));

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
        printf("Playing '%s' through zero flanger (delay=%.2f feedback=%.2f "
               "lfo-manual=%.2f lfo-freq=%.2fHz lfo-depth=%.2f waveform=%d)...\n",
               wavPath, delay, feedback, lfoManual, lfoFreq, lfoDepth, waveform);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (delay=%.2f feedback=%.2f lfo-manual=%.2f lfo-freq=%.2fHz "
               "lfo-depth=%.2f waveform=%d). Press Ctrl+C to stop.\n",
               delay, feedback, lfoManual, lfoFreq, lfoDepth, waveform);
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
