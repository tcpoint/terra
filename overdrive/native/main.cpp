#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <atomic>
#include <vector>

#include <portaudio.h>
#include <sndfile.h>

#include "biquad.h"

#define OVERSAMPLE_FACTOR 8
#define MAX_GAIN 40
#define MAX_VOLUME 10

// Copied as-is from ../overdrive.cpp (no hardware dependency, self-contained DSP).
class Tonestack
{
public:
    void Init()
    {
        s2 = s3 = 0.f;
        t = m = l = 0.5f;
        reset();
    }
    float Process(float s)
    {
        float denom = a0 * a1 * s + a2 * s2 + a3 * s3;
        float h = (fabsf(denom) < 1e-15f) ? 0.f
                : (b1 * s + b2 * s2 + b3 * s3) / denom;
        s3 = s2;
        s2 = s;
        return h;
    }
    void setTreble(float treble) { t = treble; }
    void setMids(float mids)     { m = mids; }
    void setBass(float bass)     { l = bass; }
    void reset()
    {
        b1 = t * c1 * r1 + m * c3 * r3 + l * (c1 * r2 + c2 * r2) + c1 * r3 + c2 * r3;
        b2 = t * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4) - m * m * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
            m * (c1 * c3 * r1 * r3 + c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
            l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4) +
            l * m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3) +
            c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r3 * r4;
        b3 = l * m * (c1 * c2 * c3 * r1 * r2 * r3 + c1 * c2 * c3 * r2 * r3 * r4) -
            m * m * (c1 * c2 * c3 * r1 * r3 * r3 + c1 * c2 * c3 * r3 * r3 * r4) +
            m * (c1 * c2 * c3 * r1 * r3 * r3 + c1 * c2 * c3 * r3 * r3 * r4) +
            t * c1 * c2 * c3 * r1 * r3 * r4 - t * m * c1 * c2 * c3 * r1 * r3 * r4 +
            t * l * c1 * c2 * c3 * r1 * r2 * r4;
        a0 = 1;
        a1 = c1 * r1 + c1 * r3 + c2 * r3 + c2 * r4 + c3 * r4 +
            m * c3 * r3 + l * (c1 * r2 + c2 * r2);
        a2 = m * (c1 * c3 * r1 * r3 - c2 * c3 * r3 * r4 + c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
            l * m * (c1 * c3 * r2 * r3 + c2 * c3 * r3 * r3) -
            m * m * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
            l * (c1 * c2 * r2 * r4 + c1 * c2 * r1 * r2 + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4) +
            c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4 + c1 * c2 * r3 * r4 +
            c1 * c2 * r1 * r3 + c1 * c3 * r3 * r4 + c2 * c3 * r3 * r4;
        a3 = l * m * (c1 * c2 * c3 * r1 * r2 * r3 + c1 * c2 * c3 * r2 * r3 * r4) -
            m * m * (c1 * c2 * c3 * r1 * r3 * r3 + c1 * c2 * c3 * r3 * r3 * r4) +
            m * (c1 * c2 * c3 * r3 * r3 * r4 + c1 * c2 * c3 * r1 * r3 * r3 - c1 * c2 * c3 * r1 * r3 * r4) -
            l * c1 * c2 * c3 * r1 * r2 * r4 + c1 * c2 * c3 * r1 * r3 * r4;
    }
private:
    float a0, a1, a2, a3;
    float b1, b2, b3;
    float t, l, m;
    float s2, s3;

    // tonestack 59 Bassman
    static float c1;  // 250pf
    static float c2;  // 20nf
    static float c3;  // 20nf
    static float r1;  // 250K
    static float r2;  // 1M
    static float r3;  // 25K
    static float r4;  // 56K
};

float Tonestack::c1 = 0.000000000250f;
float Tonestack::c2 = 0.000000020f;
float Tonestack::c3 = 0.000000020f;
float Tonestack::r1 = 250000.0f;
float Tonestack::r2 = 1000000.0f;
float Tonestack::r3 = 25000.0f;
float Tonestack::r4 = 56000.0f;

class Upsample
{
public:
    void Init(float samplerate)
    {
        filter.Init(20000.0, 0.7071, 1.0, samplerate * OVERSAMPLE_FACTOR);
    }
    float* Process(float in)
    {
        buffer[0] = filter.Process(in);
        for(int i = 1; i < OVERSAMPLE_FACTOR; i++)
            buffer[i] = filter.Process(0.f);
        return buffer;
    }
private:
    LowpassBiquad filter;
    float buffer[OVERSAMPLE_FACTOR];
};

class Downsample
{
public:
    void Init(float samplerate)
    {
        filter.Init(20000.0, 0.7071, 1.0, samplerate * OVERSAMPLE_FACTOR);
    }
    float Process(float* input)
    {
        float out = 0.f;
        for(int i = 0; i < OVERSAMPLE_FACTOR; i++)
            out = filter.Process(input[i]);
        return out;
    }
private:
    LowpassBiquad filter;
};

class TubeOverdrive
{
public:
    void Init(bool invert)
    {
        this->invert = invert;
    }
    float Process(float in)
    {
        float result;
        float in2 = in * in;
        float in3 = in * in2;
        if(in < -0.98338f)
        {
            result = -0.32623f;
        } else if(in <= -0.50698f) {
            result =
                (0.078101f * in3) +
                (0.23041f  * in2) +
                (0.29710f  * in) -
                0.18261f;
        } else if(in <= -0.20759f) {
            result =
                (0.42263f * in3) +
                (0.75441f * in2) +
                (0.56276f * in) -
                0.13772f;
        } else if(in <= -0.00212f) {
            result =
                (-0.78522f * in3) +
                (0.10672f  * in2) +
                (0.62768f  * in) -
                0.13322f;
        } else if(in <= 0.20041f) {
            result =
                (-0.78522f * in3) +
                (0.10563f  * in2) +
                (0.62766f  * in) -
                0.13081f;
        } else if(in <= 0.50062f) {
            result =
                (-0.10856f * in3) +
                (0.12369f  * in2) +
                (0.59147f  * in) -
                0.13081f;
        } else if(in <= 0.89961f) {
            result =
                (0.32873f  * in3) +
                (-0.88720f * in2) +
                (0.16548f  * in) -
                0.30825f;
        } else {
            result = 0.70220f;
        }
        return (invert ? -1.f : 1.f) * result;
    }
private:
    bool invert;
};

struct AppState
{
    Upsample            upsample;
    TubeOverdrive       overdrive;
    Downsample          downsample;
    Tonestack           tonestack;
    float               gain   = 0.5f;
    float               volume = 0.5f;
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

        sample = sample * state->gain * MAX_GAIN;
        float *samples = state->upsample.Process(sample);
        for(int j = 0; j < OVERSAMPLE_FACTOR; j++)
            samples[j] = state->overdrive.Process(samples[j]);
        sample = state->downsample.Process(samples);
        sample = state->tonestack.Process(sample);
        sample = state->volume * MAX_VOLUME * sample;

        out[i * 2 + 0] = sample;
        out[i * 2 + 1] = sample;
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
    const char *wavPath = nullptr;
    float       bass    = 0.5f;
    float       mids    = 0.5f;
    float       treble  = 0.5f;
    float       volume  = 0.5f;
    float       gain    = 0.5f;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
        {
            wavPath = argv[++i];
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
        else if(strcmp(argv[i], "--volume") == 0 && i + 1 < argc)
        {
            volume = atof(argv[++i]);
        }
        else if(strcmp(argv[i], "--gain") == 0 && i + 1 < argc)
        {
            gain = atof(argv[++i]);
        }
        else
        {
            fprintf(stderr,
                    "Usage: %s [--wav <path>] [--bass <0-1>] [--mids <0-1>] "
                    "[--treble <0-1>] [--volume <0-1>] [--gain <0-1>]\n",
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

    state.gain   = gain;
    state.volume = volume;
    state.upsample.Init(static_cast<float>(sampleRate));
    state.overdrive.Init(false);
    state.downsample.Init(static_cast<float>(sampleRate));
    state.tonestack.Init();
    state.tonestack.setBass(bass);
    state.tonestack.setMids(mids);
    state.tonestack.setTreble(treble);
    state.tonestack.reset();

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
        printf("Playing '%s' through overdrive (bass=%.2f mids=%.2f treble=%.2f "
               "volume=%.2f gain=%.2f)...\n",
               wavPath, bass, mids, treble, volume, gain);
        while(Pa_IsStreamActive(stream) == 1)
        {
            Pa_Sleep(50);
        }
    }
    else
    {
        signal(SIGINT, HandleSigint);
        printf("Live mode (bass=%.2f mids=%.2f treble=%.2f volume=%.2f "
               "gain=%.2f). Press Ctrl+C to stop.\n",
               bass, mids, treble, volume, gain);
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
