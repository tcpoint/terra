#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#define MAX_DELAY static_cast<size_t>(48000)

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMem;
float wetdry = 0.5f;

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
        delay->Write(((feedback * read) + in) * 0.5f);
        return read;
    }
};

Delay     delay;
Parameter delayParams;
Parameter feedbackParam;
Parameter mixParam;
CrossFade cfade;

Led led1;

bool     passThruOn;
uint32_t processCnt = 0;

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
        float mix = cfade.Process(sample, delay_sample);
        out[0][i] = out[1][i] = mix;
    }
}

void InitControls(float samplerate)
{
    delayParams.Init(petal.knob[Terrarium::KNOB_1],
                   samplerate * .05,
                   MAX_DELAY,
                   Parameter::LINEAR);

    feedbackParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);
    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);
    cfade.SetPos(wetdry);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void InitDelay(float samplerate)
{
    delayMem.Init();
    delay.delay = &delayMem;
    delay.currentDelay = delay.delayTarget = samplerate * 0.5f;
}

int main(void)
{
    float samplerate;
    petal.Init();
    samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);

    InitControls(samplerate);
    InitDelay(samplerate);

    passThruOn = false;
    led1.Set(1.0f);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(1);
    }
}

void ProcessControls()
{
    petal.ProcessAnalogControls();
    switch(processCnt++ % 4) {
    case 0:
        delay.delayTarget = delayParams.Process();
        break;
    case 1:
        delay.feedback = feedbackParam.Process();
        break;
    case 2:
        wetdry = mixParam.Process();
        cfade.SetPos(wetdry);
        break;
    case 3:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
        }
        led1.Update();
        break;
    }
}
