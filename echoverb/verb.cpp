#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

#include "verb.h"

void Verb::AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    float dryl, dryr, wetl, wetr, sendl, sendr;
    for(size_t i = 0; i < size; i++)
    {
        dryl = in[0][i];
        dryr = in[1][i];
        sendl = dryl * vsend.Value();
        sendr = dryr * vsend.Value();
        verb.Process(sendl, sendr, &wetl, &wetr);
        if(bypass)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        else
        {
            out[0][i] = dryl + wetl;
            out[1][i] = dryr + wetr;
        }
    }
}

void Verb::Init(DaisyPetal * petal, float samplerate)
{
    this->petal = petal;
    this->samplerate = samplerate;

    vtime.Init(petal->knob[Terrarium::KNOB_4], 0.6f,    0.999f,   Parameter::LOGARITHMIC);
    vfreq.Init(petal->knob[Terrarium::KNOB_5], 500.0f,  20000.0f, Parameter::LOGARITHMIC);
    vsend.Init(petal->knob[Terrarium::KNOB_6], 0.0f,    1.0f,     Parameter::LINEAR);
    verb.Init(samplerate);
    bypass = true;
    led2.Init(petal->seed.GetPin(Terrarium::LED_2), false);
    led2.Set(0.0f);
}

void Verb::ProcessControls()
{
    switch(processCnt % 4) {
    case 0:
        petal->ProcessAnalogControls();
        verb.SetFeedback(vtime.Process());
        led2.Update();
        break;
    case 1:
        verb.SetLpFreq(vfreq.Process());
        break;
    case 2:
        vsend.Process();
        break;
    case 3:
        petal->ProcessDigitalControls();
        if(petal->switches[Terrarium::FOOTSWITCH_2].RisingEdge()) {
            bypass = !bypass;
            led2.Set(bypass ? 0.0f : 1.0f);
        }
        break;
    }
    processCnt++;
}
