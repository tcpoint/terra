#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMems[3];
float feedback;
float wetdry = 0.5f;

struct Delay
{
    DelayLine<float, MAX_DELAY> *delay;
    float                        currentDelay;
    float                        delayTarget;

    float Process(float in, float fb)
    {
        fonepole(currentDelay, delayTarget, .0002f);
        delay->SetDelay(currentDelay);
        float read = delay->Read();
        delay->Write((fb * read) + in);
        return read;
    }
};

Delay     delays[3];
Parameter delayParams[3];
Parameter feedbackParam;
Parameter mixParam;

Led  led1;
bool passThruOn;

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

        float mix = 0;
        for(int d = 0; d < 3; d++)
        {
            mix += delays[d].Process(sample, feedback);
        }
        mix       = (wetdry * mix) / 3.0f + (1.0f - wetdry) * sample;
        out[0][i] = out[1][i] = mix;
    }
}

void InitDelays(float samplerate)
{
    for(int i = 0; i < 3; i++)
    {
        //Init delays
        delayMems[i].Init();
        delays[i].delay = &delayMems[i];
        //3 delay times
        delayParams[i].Init(petal.knob[Terrarium::KNOB_1 + i],
                       samplerate * .05,
                       MAX_DELAY,
                       Parameter::LOGARITHMIC);
    }
    feedbackParam.Init(petal.knob[Terrarium::KNOB_4], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);
    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

int main(void)
{
    float samplerate;
    petal.Init(); // Initialize hardware (daisy seed, and petal)
    samplerate = petal.AudioSampleRate();

    InitDelays(samplerate);

    passThruOn = false;
    led1.Set(1.0f);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while(1)
    {
        led1.Update();
        System::Delay(1);
    }
}

void ProcessControls()
{
    petal.ProcessAllControls();

    //knobs
    for(int i = 0; i < 3; i++)
    {
        delays[i].delayTarget = delayParams[i].Process();
    }

    feedback    = feedbackParam.Process();
    wetdry = mixParam.Process();

    //footswitch
    if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
    {
        passThruOn = !passThruOn;
        led1.Set(passThruOn ? 0.0f : 1.0f);
    }
}
