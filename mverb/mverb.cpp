#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#include "MVerb.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

#include "common.h"

DaisyPetal petal;

MVerb<float> DSY_SDRAM_BSS mverb;

Parameter dampingFreqParam;
Parameter bandwidthFreqParam;
Parameter decayParam;
Parameter preDelayParam;
Parameter sizeParam;
Parameter mixParam;

float dampingFreq;
float bandwidthFreq;
float decay;
float preDelay;
float size;
float mix;

Led led1;
Led led2;

bool passThruOn;

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        dampingFreq = dampingFreqParam.Process();
        mverb.setParameter(MVerb<float>::DAMPINGFREQ, dampingFreq);
        bandwidthFreq = bandwidthFreqParam.Process();
        mverb.setParameter(MVerb<float>::BANDWIDTHFREQ, bandwidthFreq);
        break;
    case 2:
        decay = decayParam.Process();
        mverb.setParameter(MVerb<float>::DECAY, decay);
        preDelay = preDelayParam.Process();
        mverb.setParameter(MVerb<float>::PREDELAY, preDelay);
        break;
    case 4:
        size = sizeParam.Process();
        mverb.setParameter(MVerb<float>::SIZE, size);
        mix = mixParam.Process();
        mverb.setParameter(MVerb<float>::MIX, mix);
        break;
    case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
        }
        break;
    }
    processCnt++;
}

void AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    if(passThruOn)
    {
        for(size_t i = 0; i < size; i++)
            out[0][i] = out[1][i] = in[0][i];
    }
    else
    {
        mverb.process(in, out, (int)size);
    }
}

void InitControls(float samplerate)
{
    dampingFreqParam.Init(petal.knob[Terrarium::KNOB_1],  0.0f, 1.0f, Parameter::LINEAR);
    bandwidthFreqParam.Init(petal.knob[Terrarium::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    decayParam.Init(petal.knob[Terrarium::KNOB_3],        0.0f, 1.0f, Parameter::LINEAR);
    preDelayParam.Init(petal.knob[Terrarium::KNOB_4],     0.0f, 1.0f, Parameter::LINEAR);
    sizeParam.Init(petal.knob[Terrarium::KNOB_5],         0.0f, 1.0f, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_6],          0.0f, 1.0f, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
    led2.Init(petal.seed.GetPin(Terrarium::LED_2), false);
}

void InitReverb(float samplerate)
{
    mverb.setSampleRate(samplerate);
    mverb.setParameter(MVerb<float>::DENSITY,   0.5f);
    mverb.setParameter(MVerb<float>::GAIN,       1.0f);
    mverb.setParameter(MVerb<float>::EARLYMIX,   0.75f);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);

    InitControls(samplerate);
    InitReverb(samplerate);

    passThruOn = true;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    dampingFreq = dampingFreqParam.Process();
    mverb.setParameter(MVerb<float>::DAMPINGFREQ, dampingFreq);
    bandwidthFreq = bandwidthFreqParam.Process();
    mverb.setParameter(MVerb<float>::BANDWIDTHFREQ, bandwidthFreq);
    decay = decayParam.Process();
    mverb.setParameter(MVerb<float>::DECAY, decay);
    preDelay = preDelayParam.Process();
    mverb.setParameter(MVerb<float>::PREDELAY, preDelay);
    size = sizeParam.Process();
    mverb.setParameter(MVerb<float>::SIZE, size);
    mix = mixParam.Process();
    mverb.setParameter(MVerb<float>::MIX, mix);

    led1.Set(0.0f);
    led2.Set(0.0f);
    led1.Update();
    led2.Update();

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
        led2.Update();
    }
}
