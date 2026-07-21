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

using namespace daisysp;

constexpr size_t MAX_DELAY = static_cast<size_t>(48000 * 1.f);

struct Delay
{
    DelayLine<float, MAX_DELAY> line;
    float                       currentDelay = 0.0f;
    float                       delayTarget  = 0.0f;

    float Process(float in, float fb)
    {
        fonepole(currentDelay, delayTarget, .0002f);
        line.SetDelay(currentDelay);
        float read = line.Read();
        line.Write((fb * read) + in);
        return read;
    }
};

struct AppState
{
    Delay               delays[3];
    float               feedback = 0.3f;
    float               mix      = 0.5f;
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

        float mixSum = 0.0f;
        for(int d = 0; d < 3; d++)
        {
            mixSum += state->delays[d].Process(sample, state->feedback);
        }
        float mixed = (state->mix * mixSum) / 3.0f + (1.0f - state->mix) * sample;

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
    const char *wavPath  = nullptr;
    float       delay1Ms = 250.0f;
    float       delay2Ms = 375.0f;
    float       delay3Ms = 500.0f;
    float       feedback = 0.3f;
    float       mix      = 0.5f;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
        }
        else if(strcmp(argv[i], "--delay1") == 0 && i + 1 < argc)
        {
            delay1Ms = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--delay2") == 0 && i + 1 < argc)
        {
            delay2Ms = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--delay3") == 0 && i + 1 < argc)
        {
            delay3Ms = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--feedback") == 0 && i + 1 < argc)
        {
            feedback = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
        {
            mix = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--delay1 <ms>] [--delay2 <ms>] "
                    "[--delay3 <ms>] [--feedback <0-1>] [--mix <0-1>]\n",
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

    float delayMs[3] = {delay1Ms, delay2Ms, delay3Ms};
    for(int d = 0; d < 3; d++)
    {
        state.delays[d].line.Init();
        state.delays[d].delayTarget = state.delays[d].currentDelay
            = delayMs[d] * 0.001f * sampleRate;
    }
    state.feedback = feedback;
    state.mix      = mix;

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
        printf("Playing '%s' through multidelay (delay1=%.0fms delay2=%.0fms "
               "delay3=%.0fms feedback=%.2f mix=%.2f)...\n",
               wavPath, delay1Ms, delay2Ms, delay3Ms, feedback, mix);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (delay1=%.0fms delay2=%.0fms delay3=%.0fms feedback=%.2f "
               "mix=%.2f). Press Ctrl+C to stop.\n",
               delay1Ms, delay2Ms, delay3Ms, feedback, mix);
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
