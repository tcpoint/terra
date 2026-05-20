#include "daisysp.h"
#include "daisy_petal.h"
#include "lowpass.h"
#include "terrarium.h"

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
Lowpass lpf_in;
Lowpass lpf_out1;
Tone    lpf_out2;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayMem;
float feedback;
float wetdry;

struct Delay
{
    DelayLine<float, MAX_DELAY> *delay;
    float                        currentDelay;
    float                        delayTarget;

    float Process(float in)
    {
        fonepole(currentDelay, delayTarget, .0002f);
        delay->SetDelay(currentDelay);
        float read           = delay->Read();
        float filtered_input = lpf_in.Process(in);
        delay->Write(((feedback * read) + filtered_input) * 0.5f);
        read = lpf_out1.Process(read);
        read = lpf_out2.Process(read);
        return read;
    }
};

Delay     delay;
Parameter delayParams;
Parameter feedbackParam;
Parameter mixParam;
CrossFade cfade;

Led  led1;
bool passThruOn;

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        delay.delayTarget = delayParams.Process();
        break;
    case 2:
        feedback = feedbackParam.Process();
        break;
    case 4:
        wetdry = mixParam.Process();
        cfade.SetPos(wetdry);
        break;
    case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            passThruOn = !passThruOn;
            led1.Set(passThruOn ? 0.0f : 1.0f);
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
        float sample       = in[0][i];
        float delay_sample = delay.Process(sample);

        if(passThruOn)
            out[0][i] = out[1][i] = sample;
        else
            out[0][i] = out[1][i] = cfade.Process(sample, delay_sample);
    }
}

void InitControls(float samplerate)
{
    delayParams.Init(petal.knob[Terrarium::KNOB_1],
                     samplerate * .05f,
                     MAX_DELAY,
                     Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    mixParam.Init(petal.knob[Terrarium::KNOB_3],      0.0f, 1.0f, Parameter::LINEAR);

    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void InitDelay(float samplerate)
{
    delayMem.Init();
    delay.delay = &delayMem;
    lpf_in.Init(samplerate);
    lpf_in.SetFreq(5400.0f);
    lpf_in.SetQ(0.76f);
    lpf_out1.Init(samplerate);
    lpf_out1.SetFreq(2200.0f);
    lpf_out1.SetQ(0.8f);
    lpf_out2.Init(samplerate);
    lpf_out2.SetFreq(3300.0f);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);

    InitControls(samplerate);
    InitDelay(samplerate);

    passThruOn = false;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    delay.delayTarget = delayParams.Process();
    feedback          = feedbackParam.Process();
    wetdry            = mixParam.Process();
    cfade.SetPos(wetdry);
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
