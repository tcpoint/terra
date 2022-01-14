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
void callback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size){
    float dryl, dryr, wetl, wetr, sendl, sendr;
    hw.ProcessAllControls();
    verb.SetFeedback(vtime.Process());
    verb.SetLpFreq(vfreq.Process());
    vsend.Process(); // Process Send to use later
    if (hw.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        bypass = !bypass;
    for (size_t i = 0; i < size; i++)
    {
        dryl = in[0][i];
        dryr = in[1][i];
        sendl = dryl * vsend.Value();
        sendr = dryr * vsend.Value();
        verb.Process(sendl, sendr, &wetl, &wetr);
        if (bypass)
        {
            out[0][i] = in[0][i]; // left
            out[1][i] = in[1][i]; // right
        }
        else
        {
            out[0][i] = dryl + wetl;
            out[1][i] = dryr + wetr;
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
        hw.DelayMs(10);
        dsy_gpio_write(&led1, bypass ? 0 : 1);
    }
}
