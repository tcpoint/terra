#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

#define MAX_DELAY     static_cast<size_t>(48000 * 1.f)
#define MAX_LFO_DELAY static_cast<size_t>(53400 * 1.f)

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

DaisyPetal petal;

DelayLine<float, MAX_LFO_DELAY> DSY_SDRAM_BSS delayMem;
float feedback;
float wetdry;
CrossFade cfade;
bool  passThruOn;

class Delay
{
public:
    float Process(float in)
    {
        float phase = lfo.Process();
        fonepole(currentDelay, delayTarget + (phase * depth), .0002f);
        delay->SetDelay(currentDelay);
        float read = delay->Read();
        delay->Write((in + read * feedback) / 2);
        return read;
    }

    void SetDelay(float d)  { delayTarget = d; }
    void SetFreq(float freq)
    {
        float chorus_freq = (20.f - 3.4f) * freq + 3.4f;
        lfo.SetFreq(chorus_freq);
    }
    void SetDepth(float _depth)
    {
        depth = samplerate * _depth * .040f;
    }
    void SetWaveform(uint8_t wf) { lfo.SetWaveform(wf); }
    void SetDelayMem(DelayLine<float, MAX_LFO_DELAY> *_delay) { delay = _delay; }

    void Init(float _samplerate)
    {
        samplerate = _samplerate;
        lfo.SetAmp(1.0f);
    }

private:
    DelayLine<float, MAX_LFO_DELAY> *delay;
    float currentDelay;
    float delayTarget;
    float depth;
    Oscillator lfo;
    float samplerate;
};

Delay     delay;
Parameter delayParams;
Parameter feedbackParam;
Parameter mixParam;
Parameter lfoFreqParam;
Parameter lfoDepthParam;

Led led1;

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8) {
    case 0:
        petal.ProcessAnalogControls();
        delay.SetDelay(delayParams.Process());
        break;
    case 2:
        feedback = feedbackParam.Process();
        wetdry   = mixParam.Process();
        break;
    case 4:
        delay.SetFreq(lfoFreqParam.Process());
        delay.SetDepth(lfoDepthParam.Process());
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
    cfade.SetPos(wetdry);
    for(size_t i = 0; i < size; i++)
    {
        float sample      = in[0][i];
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
    lfoFreqParam.Init(petal.knob[Terrarium::KNOB_4],  0.0f, 1.0f, Parameter::LOGARITHMIC);
    lfoDepthParam.Init(petal.knob[Terrarium::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);

    cfade.Init();
    cfade.SetCurve(CROSSFADE_CPOW);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
}

void InitDelay(float samplerate)
{
    delayMem.Init();
    delay.Init(samplerate);
    delay.SetDelayMem(&delayMem);
    delay.SetFreq(0.3f);
    delay.SetDepth(0.7f);
    delay.SetWaveform(Oscillator::WAVE_TRI);
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
    delay.SetDelay(delayParams.Process());
    feedback = feedbackParam.Process();
    wetdry   = mixParam.Process();
    delay.SetFreq(lfoFreqParam.Process());
    delay.SetDepth(lfoDepthParam.Process());
    led1.Set(0.0f);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
