#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#include <string>

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
Compressor comp;

Parameter attack;
Parameter release;
Parameter ratio;
Parameter threshold;
Parameter makeup;


dsy_gpio led1;

// int   drywet;
bool  bypass;
bool autogain = false;
bool last_autogain = false;

void ProcessControls()
{
    petal.UpdateAnalogControls();
    petal.DebounceControls();

    //knobs
    float currentAttack = attack.Process();
    if(currentAttack != comp.GetAttack()) {
        comp.SetAttack(currentAttack);
    }
    float currentRelease = attack.Process();
    if(currentRelease != comp.GetRelease()) {
        comp.SetRelease(currentAttack);
    }
    float currentRatio = ratio.Process();
    if(currentRatio != comp.GetRatio()) {
        comp.SetRatio(currentRatio);
    }
    float currentThreshold = threshold.Process();
    if(currentThreshold != comp.GetThreshold())
    {
        comp.SetThreshold(currentThreshold);
    }

    // autogain switch
    autogain = petal.switches[Terrarium::SWITCH_1].Pressed();
    if(autogain)
    {
        if(autogain != last_autogain)
        {
            last_autogain = true;
            comp.AutoMakeup(true);
        }
    }
    else
    {
        if(last_autogain)
        {
            comp.AutoMakeup(false);
        }
        last_autogain = false;
        float currentMakeup = makeup.Process();
        if(currentMakeup != comp.GetMakeup())
        {
            comp.SetMakeup(currentMakeup);
        }

    }

    //footswitch
    if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
    {
        bypass = !bypass;
    }
}

void Init(float samplerate)
{
    comp.Init(samplerate);

    attack.Init(petal.knob[Terrarium::KNOB_1], 0.01f, 1.0f, Parameter::EXPONENTIAL);
    release.Init(petal.knob[Terrarium::KNOB_2], 0.01f, 1.0f, Parameter::EXPONENTIAL);
    ratio.Init(petal.knob[Terrarium::KNOB_3], 1.0f, 40.0f, Parameter::LINEAR);
    threshold.Init(petal.knob[Terrarium::KNOB_4], -80.0f, 0.0f, Parameter::LINEAR);
    makeup.Init(petal.knob[Terrarium::KNOB_5], 1.0f, 40.0f, Parameter::LINEAR);  // log?
}

static void AudioCallback(float *in, float *out, size_t size)
{
    ProcessControls();

    for(size_t i = 0; i < size; i+=2)
    {
        if(bypass)
        {
            out[i] = in[i];
            out[i + 1] = in[i + 1];
        }
        else
        {
            out[i] = comp.Process(in[i]);
            out[i + 1] = comp.Process(in[i + 1]);            
        }
    }
}

int main(void)
{
    float samplerate;
    petal.Init(); // Initialize hardware (daisy seed, and petal)
    samplerate = petal.AudioSampleRate();
    Init(samplerate);

    bypass = true;

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    led1.pin = petal.seed.GetPin(22);
    led1.mode = DSY_GPIO_MODE_OUTPUT_PP;
    led1.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&led1);

    while(1)
    {
        dsy_gpio_write(&led1, bypass ? 0.0f : 1.0f);
        dsy_system_delay(6);
    }
}

