#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

#include "basicdelay.h"

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMem;

void BasicDelay::AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    cfade.SetPos(wetdry);
    for(size_t i = 0; i < size; i++)
    {
        float sample = in[0][i];
        float processedSample = sample;
        if(filterOn) {
            processedSample = filt1.Process(processedSample);
            processedSample = filt2.Process(processedSample);
        }
        float delay_sample = delay.Process(processedSample);

        if(passThruOn)
        {
            out[0][i] = out[1][i] = sample;
        }
        else
        {
            out[0][i] = out[1][i] = cfade.Process(sample, delay_sample);
        }
    }
}

void BasicDelay::Init(DaisyPetal * petal, float samplerate)
{
    this->petal = petal;
    this->samplerate = samplerate;
    delayMem.Init();
    delay.delay = &delayMem;

    delayParams.Init(petal->knob[Terrarium::KNOB_1],
                     samplerate * .05,
                     MAX_DELAY,
                     Parameter::LINEAR);
    feedbackParam.Init(petal->knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    mixParam.Init(petal->knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);

    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);

    led1.Init(petal->seed.GetPin(Terrarium::LED_1), false);
    led1.Set(0.0f);

    filt1.Init(2000.0, 0.7071, 1.0, samplerate);
    filt2.Init(2000.0, 0.7071, 1.0, samplerate);
    filterOn = false;
}

void BasicDelay::ProcessControls()
{
    switch(processCnt % 4) {
    case 0:
        petal->ProcessAnalogControls();
        delay.delayTarget = delayParams.Process();
        led1.Update();
        break;
    case 1:
        filterOn = petal->switches[Terrarium::SWITCH_2].Pressed();
        delay.setFeedback(feedbackParam.Process());
        break;
    case 2:
        wetdry = mixParam.Process();
        break;
    case 3:
        petal->ProcessDigitalControls();
        if(petal->switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
        }
        break;
    }
    processCnt++;
}
