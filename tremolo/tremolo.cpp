#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;
Tremolo    treml, tremr;

bool effectOn;
int  waveform;

float freq;
float depth;
Parameter freqParam;
Parameter depthParam;

Led led1;

void ProcessControls()
{
    static int processCnt = 0;

    switch(processCnt)
    {
    case 0:
        petal.ProcessAnalogControls();
        freq = freqParam.Process() * 20.0f;
        treml.SetFreq(freq);
        tremr.SetFreq(freq);
        break;
    case 2:
        depth = depthParam.Process();
        treml.SetDepth(depth);
        tremr.SetDepth(depth);
        break;
    case 4:
        petal.ProcessDigitalControls();
        {
            bool sw1 = petal.switches[Terrarium::SWITCH_1].Pressed();
            bool sw2 = petal.switches[Terrarium::SWITCH_2].Pressed();
            bool sw3 = petal.switches[Terrarium::SWITCH_3].Pressed();
            waveform = (sw1 ? 4 : 0) + (sw2 ? 2 : 0) + (sw3 ? 1 : 0);
            treml.SetWaveform(waveform);
            tremr.SetWaveform(waveform);
        }
        break;
    case 6:
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
        {
            effectOn = !effectOn;
            led1.Set(effectOn ? 1.0f : 0.0f);
        }
        break;
    }
    processCnt = (processCnt + 1) % 8;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = effectOn ? treml.Process(in[0][i]) : in[0][i];
        out[1][i] = effectOn ? tremr.Process(in[0][i]) : in[0][i];
    }
}

void InitControls()
{
    freqParam.Init(petal.knob[Terrarium::KNOB_1], 0.0, 1.0, Parameter::LINEAR);
    depthParam.Init(petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

int main(void)
{
    petal.Init();
    float sample_rate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    treml.Init(sample_rate);
    tremr.Init(sample_rate);
    InitControls();

    effectOn = false;
    waveform = 0;
    treml.SetWaveform(waveform);
    tremr.SetWaveform(waveform);

    petal.StartAdc();
    petal.ProcessAnalogControls();
    freq = freqParam.Process() * 20.0f;
    treml.SetFreq(freq);
    tremr.SetFreq(freq);
    depth = depthParam.Process();
    treml.SetDepth(depth);
    tremr.SetDepth(depth);
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        petal.DelayMs(5);
        ProcessControls();
        led1.Update();
    }
}
