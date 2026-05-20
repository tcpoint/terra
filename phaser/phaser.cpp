#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
Phaser     phaser;

bool  effectOn;
float wet;

float freqtarget, freq;
float lfotarget, lfo;
int   numstages;

Parameter dryWetParam;
Parameter lfoFreqParam;
Parameter lfoDepthParam;
Parameter freqParam;
Parameter feedbackParam;

Led led1;

void InitControls()
{
    dryWetParam.Init(petal.knob[Terrarium::KNOB_1], 0.0, 1.0, Parameter::LINEAR);
    lfoFreqParam.Init(petal.knob[Terrarium::KNOB_4], 0.0, 1.0, Parameter::LINEAR);
    lfoDepthParam.Init(petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);
    freqParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        wet = dryWetParam.Process();
        phaser.SetFeedback(feedbackParam.Process());
        break;
    case 2:
        {
            bool sw1 = petal.switches[Terrarium::SWITCH_1].Pressed();
            bool sw2 = petal.switches[Terrarium::SWITCH_2].Pressed();
            bool sw3 = petal.switches[Terrarium::SWITCH_3].Pressed();
            numstages = (sw1 ? 4 : 0) + (sw2 ? 2 : 0) + (sw3 ? 1 : 0) + 1;
            phaser.SetPoles(numstages);
        }
        break;
    case 4:
        {
            float k = lfoFreqParam.Process();
            phaser.SetLfoFreq(k * k * 20.f);
            lfotarget = lfoDepthParam.Process();
        }
        break;
    case 6:
        petal.ProcessDigitalControls();
        freqtarget = freqParam.Process();
        freqtarget = freqtarget * freqtarget * 7000.f;
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
        fonepole(freq, freqtarget, .0001f);
        phaser.SetFreq(freq);

        fonepole(lfo, lfotarget, .0001f);
        phaser.SetLfoDepth(lfo);

        out[0][i] = out[1][i] = in[0][i];
        if(effectOn)
        {
            float phasorOut = phaser.Process(in[0][i]) / numstages;
            out[0][i] = out[1][i] = phasorOut * wet + in[0][i] * (1.f - wet);
        }
    }
}

int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    InitControls();

    phaser.Init(sample_rate);

    effectOn      = false;
    freqtarget    = freq = 0.f;
    lfotarget     = lfo  = 0.f;
    numstages     = 4;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    wet = dryWetParam.Process();
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(1);
        ProcessControls();
        led1.Update();
    }
}
