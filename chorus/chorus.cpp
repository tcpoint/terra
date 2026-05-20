#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
Chorus     ch;

bool  effectOn;
float wet;

float deltarget, del;
float lfotarget, lfo;

Parameter delayParam;
Parameter speedParam;
Parameter depthParam;
Parameter mixParam;
Parameter feedbackParam;

Led led1;

void ProcessControls()
{
    static uint32_t processCnt = 0;

    switch(processCnt % 8)
    {
    case 0:
        petal.ProcessAnalogControls();
        {
            float k = speedParam.Process();
            ch.SetLfoFreq(k * k * 20.f);
        }
        lfotarget = depthParam.Process();
        break;
    case 2:
        deltarget = delayParam.Process();
        break;
    case 4:
        ch.SetFeedback(feedbackParam.Process());
        wet = mixParam.Process();
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
        ch.SetDelay(del);

        fonepole(lfo, lfotarget, .0001f);
        ch.SetLfoDepth(lfo);

        if(effectOn)
        {
            ch.Process(in[0][i]);
            out[0][i] = out[1][i] = ((ch.GetLeft() + ch.GetRight()) * wet) / 2 + in[0][i] * (1.f - wet);
        }
        else
        {
            out[0][i] = out[1][i] = in[0][i];
        }
    }
}

void InitControls()
{
    delayParam.Init(petal.knob[Terrarium::KNOB_1],    0.0f, 1.0f, Parameter::LINEAR);
    speedParam.Init(petal.knob[Terrarium::KNOB_2],    0.0f, 1.0f, Parameter::LINEAR);
    depthParam.Init(petal.knob[Terrarium::KNOB_3],    0.0f, 1.0f, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_4],      0.0f, 1.0f, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    InitControls();
    ch.Init(sample_rate);

    effectOn = false;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    {
        float k = speedParam.Process();
        ch.SetLfoFreq(k * k * 20.f);
    }
    deltarget = del = delayParam.Process();
    ch.SetDelay(del);
    lfotarget = lfo = depthParam.Process();
    ch.SetLfoDepth(lfo);
    wet = mixParam.Process();
    ch.SetFeedback(feedbackParam.Process());
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(5);
        ProcessControls();
        led1.Update();
    }
}
