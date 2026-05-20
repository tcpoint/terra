#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

Flanger    flanger;
DaisyPetal petal;

bool  effectOn = false;
float wet;
float deltarget, del;
float lfotarget, lfoDepth;

Parameter delayParam;
Parameter feedbackParam;
Parameter lfoFreqParam;
Parameter lfoDepthParam;
Parameter mixParam;

Led led1;

void InitControls()
{
    delayParam.Init(petal.knob[Terrarium::KNOB_1],    0.0f,  1.0f,  Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_2], 0.0f,  1.0f,  Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_3],      0.0f,  1.0f,  Parameter::LINEAR);
    lfoFreqParam.Init(petal.knob[Terrarium::KNOB_4],  0.0f,  10.0f, Parameter::LOGARITHMIC);
    lfoDepthParam.Init(petal.knob[Terrarium::KNOB_5], 0.0f,  1.0f,  Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        deltarget = delayParam.Process();
        flanger.SetFeedback(feedbackParam.Process());
        break;
    case 2:
        wet = mixParam.Process();
        flanger.SetLfoFreq(lfoFreqParam.Process());
        break;
    case 4:
        lfotarget = lfoDepthParam.Process();
        break;
    case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            effectOn = !effectOn;
            led1.Set(effectOn ? 1.0f : 0.0f);
        }
        break;
    }
    processCnt++;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        fonepole(del, deltarget, .0001f);
        flanger.SetDelay(del);
        fonepole(lfoDepth, lfotarget, .0001f);
        flanger.SetLfoDepth(lfoDepth);

        if(effectOn)
        {
            float sig = flanger.Process(in[0][i]);
            out[0][i] = out[1][i] = sig * wet + in[0][i] * (1.f - wet);
        }
        else
        {
            out[0][i] = out[1][i] = in[0][i];
        }
    }
}

int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    InitControls();

    flanger.Init(sample_rate);

    petal.StartAdc();
    petal.ProcessAnalogControls();
    deltarget = del = delayParam.Process();
    flanger.SetDelay(del);
    flanger.SetFeedback(feedbackParam.Process());
    wet = mixParam.Process();
    flanger.SetLfoFreq(lfoFreqParam.Process());
    lfotarget = lfoDepth = lfoDepthParam.Process();
    flanger.SetLfoDepth(lfoDepth);
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(1);
        ProcessControls();
        led1.Update();
    }
}
