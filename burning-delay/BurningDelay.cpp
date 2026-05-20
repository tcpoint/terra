#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#include <string>

#define MAX_DELAY static_cast<size_t>(48000 * 0.3f)

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMem;
float wetdry = 0.5f;
OnePole tone;

struct Delay
{
    DelayLine<float, MAX_DELAY>  *delay;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;

    float Process(float in)
    {
        fonepole(currentDelay, delayTarget, .0002f);
        delay->SetDelay(currentDelay);
        float read = delay->Read();
        float degraded = tone.Process(read);
        delay->Write(in + feedback * degraded);
        return degraded;
    }
};

Delay     delay;
Parameter delayParams;
Parameter feedbackParam;
Parameter mixParam;
CrossFade cfade;

Led led1;

// int   drywet;
bool  passThruOn;

void ProcessControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    ProcessControls();

    for(size_t i = 0; i < size; i++)
    {
        float sample = in[0][i];

        if(passThruOn)
        {
            out[0][i] = out[1][i] = sample;
            continue;
        }

        float delay_sample = delay.Process(sample);
        cfade.SetPos(wetdry);
        float mix = cfade.Process(sample, delay_sample);
        out[0][i] = out[1][i] = mix;
    }
}

void InitControls(float samplerate) {
    delayParams.Init(petal.knob[Terrarium::KNOB_1],
                   samplerate * .05,
                   MAX_DELAY,
                   Parameter::LINEAR);

    feedbackParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    // Initialize & set params for CrossFade object
    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void InitDelay(float samplerate)
{
    delayMem.Init();
    delay.delay = &delayMem;
    delay.currentDelay = delay.delayTarget = samplerate * 0.5f;
    delay.feedback = 0.5f;
}

int main(void)
{
    float samplerate;
    petal.Init(); // Initialize hardware (daisy seed, and petal)
    samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);////////Adjust the blocksize

    InitControls(samplerate);
    InitDelay(samplerate);
    tone.Init();
    tone.SetFrequency(7000.0f / samplerate);

    passThruOn = false;

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    led1.Set(1.0f);

    while(1)
    {
        System::Delay(1);
    }
}

uint32_t processCnt = 0;
void ProcessControls()
{
    switch(processCnt++ % 8) {
    case 0:
        petal.ProcessAnalogControls();        
        delay.delayTarget = delayParams.Process();
        break;
    case 2:
        petal.ProcessAnalogControls();
        delay.feedback = feedbackParam.Process();
        break;
    case 4:
        petal.ProcessAnalogControls();        
        wetdry = mixParam.Process();
        break;
    case 6:
        petal.ProcessDigitalControls();        
        //footswitch
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
        }
        led1.Update();
        break;
    }
}
