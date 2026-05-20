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
    static int processCnt = 0;

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
        break;
    case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            effectOn = !effectOn;
            led1.Set(effectOn ? 1.0f : 0.0f);
        }
        wet = mixParam.Process();
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

        out[0][i] = out[1][i] = in[0][i];

        if(effectOn)
        {
            ch.Process(in[0][i]);
            out[0][i] = out[1][i] = ((ch.GetLeft() + ch.GetRight()) * wet) / 2 + in[0][i] * (1.f - wet);
        }
    }
}

void InitControls()
{
    delayParam.Init(petal.knob[Terrarium::KNOB_1], 0.0, 1.0, Parameter::LINEAR);
    speedParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    depthParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_4], 0.0, 1.0, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    InitControls();
    ch.Init(sample_rate);

    effectOn  = false;
    deltarget = del = 0.f;
    lfotarget = lfo = 0.f;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    wet = mixParam.Process();
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(5);
        ProcessControls();
        led1.Update();
    }
}
