#include "daisy_petal.h"
#include "daisysp.h" 
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

// Declare a local daisy_petal for hardware access
DaisyPetal hw;

dsy_gpio led1;

Parameter vtime, vfreq, vsend;
bool bypass;
ReverbSc verb;

// This runs at a fixed rate, to prepare audio samples
void callback(float *in, float *out, size_t size)
{
    float dryl, dryr, wetl, wetr, sendl, sendr;
    hw.DebounceControls();
    verb.SetFeedback(vtime.Process());
    verb.SetLpFreq(vfreq.Process());
    vsend.Process(); // Process Send to use later
    if (hw.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        bypass = !bypass;
    for (size_t i = 0; i < size; i+=2)
    {
        dryl = in[i];
        dryr = in[i+1];
        sendl = dryl * vsend.Value();
        sendr = dryr * vsend.Value();
        verb.Process(sendl, sendr, &wetl, &wetr);
        if (bypass)
        {
            out[i] = in[i]; // left
            out[i+1] = in[i+1]; // right
        }
        else
        {
            out[i] = dryl + wetl;
            out[i+1] = dryr + wetr;
        }
    }
}

int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    vtime.Init(hw.knob[Terrarium::KNOB_1], 0.6f, 0.999f, Parameter::LOGARITHMIC);
    vfreq.Init(hw.knob[Terrarium::KNOB_2], 500.0f, 20000.0f, Parameter::LOGARITHMIC);
    vsend.Init(hw.knob[Terrarium::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);
    verb.Init(samplerate);

    hw.StartAdc();
    hw.StartAudio(callback);

    led1.pin = hw.seed.GetPin(22);
    led1.mode = DSY_GPIO_MODE_OUTPUT_PP;
    led1.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&led1);

    bypass = false;

    while(1) 
    {
        // Do Stuff InfInitely Here
        dsy_system_delay(10);
        dsy_gpio_write(&led1, bypass ? 0 : 1);
    }
}
