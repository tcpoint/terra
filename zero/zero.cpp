#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"
#include "zflanger.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

// set controls
// depth, feedback, rate, delay, mix

ZFlanger flanger;
DaisyPetal petal;

bool  effectOn = false;
float deltarget, del;
float lfotarget, lfoDepth;

Parameter delayParam;
Parameter feedbackParam;
Parameter lfoManualParam;
Parameter lfoFreqParam;
Parameter lfoDepthParam;
// Parameter mixParam;

Led led1;

void InitControls(float samplerate) {
    delayParam.Init(petal.knob[Terrarium::KNOB_1], 0.0, 1.0, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    //mixParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    lfoManualParam.Init(petal.knob[Terrarium::KNOB_3], -1.0, 1.0, Parameter::LINEAR);
    lfoFreqParam.Init(petal.knob[Terrarium::KNOB_4], 0.0, 10.0, Parameter::LOGARITHMIC);
    lfoDepthParam.Init(petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void ProcessControls()
{
    static uint32_t processCnt = 0;
    uint8_t waveform = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        deltarget = delayParam.Process();
        flanger.setFeedback(feedbackParam.Process());
        break;
    case 2:
        flanger.setLFOManual(lfoManualParam.Process());
        flanger.setLFOFreq(lfoFreqParam.Process());
        break;
    case 4:
        lfotarget = lfoDepthParam.Process();
        break;
   case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge()) // ON/OFF footswitch
        {
            effectOn = !effectOn;
            led1.Set(effectOn ? 1.0f : 0.0f);
        }
        if(petal.switches[Terrarium::SWITCH_1].Pressed())
        {
            waveform += 2;
        }
        if(petal.switches[Terrarium::SWITCH_2].Pressed())
        {
            waveform +=1;
        }
        flanger.setWaveform(waveform);
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
        flanger.setDelay(del);
        fonepole(lfoDepth, lfotarget, .0001f);
        flanger.setLFODepth(lfoDepth);

        out[0][i] = out[1][i] = in[0][i];

        if(effectOn)
        {
            float sig = flanger.process(in[0][i]);
            out[0][i] = out[1][i] = sig;
        }
    }
}

int main(void)
{
    petal.Init();
    petal.SetAudioBlockSize(1);
    float sample_rate = petal.AudioSampleRate();

    InitControls(sample_rate);

    deltarget = del = 0.f;
    lfotarget = lfoDepth = 0.f;
    flanger.init(sample_rate);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(1);
        ProcessControls();
        led1.Update();
    }
}
