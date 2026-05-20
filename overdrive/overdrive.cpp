#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"
#include "common.h"
#include "biquad.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

#define OVERSAMPLE_FACTOR 8
#define MAX_GAIN  40
#define MAX_VOLUME 10

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

Upsample     upsample;
TubeOverdrive overdrive;
Downsample   downsample;
Tonestack    tonestack;

Parameter bassParam;
Parameter midsParam;
Parameter trebleParam;
Parameter volumeParam;
Parameter gainParam;

float bass, mids, treble, volume, gain;
bool  effectOn;

Led led1;

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8)
    {
    case 0:
        petal.ProcessAnalogControls();
        bass = bassParam.Process();
        tonestack.setBass(bass);
        tonestack.reset();
        break;
    case 2:
        mids = midsParam.Process();
        tonestack.setMids(mids);
        tonestack.reset();
        break;
    case 4:
        treble = trebleParam.Process();
        tonestack.setTreble(treble);
        tonestack.reset();
        volume = volumeParam.Process();
        break;
    case 6:
        petal.ProcessDigitalControls();
        gain      = gainParam.Process();
        effectOn  = petal.switches[Terrarium::SWITCH_1].Pressed();
        led1.Set(effectOn ? 1.0f : 0.0f);
        break;
    }
    processCnt++;
}

void AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        if(!effectOn)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        else
        {
            float sample  = in[0][i] * gain * MAX_GAIN;
            float *samples = upsample.Process(sample);
            for(int j = 0; j < OVERSAMPLE_FACTOR; j++)
                samples[j] = overdrive.Process(samples[j]);
            sample = downsample.Process(samples);
            sample = tonestack.Process(sample);
            sample = volume * MAX_VOLUME * sample;
            out[0][i] = out[1][i] = sample;
        }
    }
}

void Init(float samplerate)
{
    bassParam.Init(petal.knob[Terrarium::KNOB_1],   0.0f, 1.0f, Parameter::LOGARITHMIC);
    midsParam.Init(petal.knob[Terrarium::KNOB_2],   0.0f, 1.0f, Parameter::LINEAR);
    trebleParam.Init(petal.knob[Terrarium::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);
    volumeParam.Init(petal.knob[Terrarium::KNOB_4], 0.0f, 1.0f, Parameter::LOGARITHMIC);
    gainParam.Init(petal.knob[Terrarium::KNOB_5],   0.0f, 1.0f, Parameter::LINEAR);

    upsample.Init(samplerate);
    overdrive.Init(false);
    downsample.Init(samplerate);
    tonestack.Init();

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
    led1.Set(0.0f);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    Init(samplerate);

    effectOn = false;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    bass   = bassParam.Process();   tonestack.setBass(bass);
    mids   = midsParam.Process();   tonestack.setMids(mids);
    treble = trebleParam.Process(); tonestack.setTreble(treble);
    tonestack.reset();
    volume = volumeParam.Process();
    gain   = gainParam.Process();

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
