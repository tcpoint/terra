#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"
#include "common.h"
#include "biquad.h"
#include "dchorus.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
DChorus     ch;

bool  effectOn;
float wet;

float deltarget, del;
float lfotarget, lfo;
float makeup;

Parameter delayParam;
Parameter speedParam;
Parameter depthParam;
Parameter mixParam;
Parameter feedbackParam;
Parameter makeupParam;

CrossFade cfade;
LowpassBiquad filt;

Led led1;


void processControls()
{
    static int processCnt = 0;
    switch(processCnt++ % 8) {
    case 0:
        {
            float k = speedParam.Process();
            ch.SetLfoFreq(k * k * 20.f);
        }
        wet = mixParam.Process();
        cfade.SetPos(wet);
        break;
    case 2:
        lfotarget = depthParam.Process();
        deltarget = delayParam.Process();
        break;
    case 4:
        ch.SetFeedback(feedbackParam.Process());
        makeup = makeupParam.Process() * 2;
        break;
    case 6:
        petal.switches[Terrarium::FOOTSWITCH_1].Debounce();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            effectOn = !effectOn;
            led1.Set(effectOn ? 1.0f : 0.0f);
        }
        led1.Update();
    }
}

static void AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        fonepole(del, deltarget, .0001f); //smooth at audio rate
        ch.SetDelay(del);

        fonepole(lfo, lfotarget, .0001f); //smooth at audio rate
        ch.SetLfoDepth(lfo);

        float sample = in[0][i];

        if(effectOn)
        {
            float chorus_sigs = ch.Process(sample);
            // change to crossfade
            // out[0][i] = out[1][i] = (chorus_sigs * wet) + in[0][i] * (1.f - wet);
            float mixed = cfade.Process(sample, chorus_sigs);
            // antialias filter here????
            float makeup_sample = mixed * makeup;
            out[0][i] = out[1][i] = filt.Process(makeup_sample);
        } else {
            out[0][i] = out[1][i] = sample;
        }
    }
}

void InitControls(float sample_rate)
{
    delayParam.Init(petal.knob[Terrarium::KNOB_1], 0.0, 1.0, Parameter::LINEAR);
    speedParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    depthParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_4], 0.0, 1.0, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);
    makeupParam.Init(petal.knob[Terrarium::KNOB_6], 0, 1, Parameter::LINEAR);

    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);
    filt.Init(10000.0, 0.7071, 1.0, sample_rate);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}


int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);////////Adjust the blocksize 
    InitControls(sample_rate);
    ch.Init(sample_rate);

    effectOn  = false;
    wet       = .9f;
    deltarget = del = 0.f;
    lfotarget = lfo = 0.f;

    petal.StartAdc();
    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(5);
        processControls();
    }
}
