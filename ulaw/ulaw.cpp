#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

Parameter bitsParam;

Led led1;

bool  passThruOn;
int   crushbits = 7;
float mu_val    = 255.0f;

static float ulaw_encode(float sample, float mu)
{
    float sign = (sample >= 0.0f) ? 1.0f : -1.0f;
    return sign * log1pf(mu * fabsf(sample)) / log1pf(mu);
}

static float ulaw_decode(float sample, float mu)
{
    float sign = (sample >= 0.0f) ? 1.0f : -1.0f;
    return sign * expm1f(fabsf(sample) * log1pf(mu)) / mu;
}

static float quantize(float sample, int levels)
{
    return floorf(sample * levels) / levels;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float sample = in[0][i];
        if(passThruOn)
        {
            out[0][i] = out[1][i] = sample;
        }
        else
        {
            float compressed = ulaw_encode(sample, mu_val);
            float quantized  = quantize(compressed, 1 << crushbits);
            float processed  = ulaw_decode(quantized, mu_val);
            out[0][i] = out[1][i] = processed;
        }
    }
}

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        mu_val = bitsParam.Process();
        break;
    case 4:
        petal.ProcessDigitalControls();
        {
            bool sw1 = petal.switches[Terrarium::SWITCH_1].Pressed();
            bool sw2 = petal.switches[Terrarium::SWITCH_2].Pressed();
            bool sw3 = petal.switches[Terrarium::SWITCH_3].Pressed();
            crushbits = (sw3 ? 4 : 0) + (sw2 ? 2 : 0) + (sw1 ? 1 : 0) + 7;
        }
        break;
    case 6:
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
        }
        break;
    }
    processCnt++;
}

void InitControls(float samplerate)
{
    bitsParam.Init(petal.knob[Terrarium::KNOB_1], 1.0f, 255.0f, Parameter::LOGARITHMIC);
    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);

    InitControls(samplerate);

    passThruOn = false;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    mu_val = bitsParam.Process();
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
