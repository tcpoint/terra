#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"
#include "common.h"
#include "Chorus.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;
using namespace DSP;

DaisyPetal petal;

HP1<sample_t> hp;
float time, width, rate;

struct { Sine sine; } lfo;

Delay delay;
float fs, over_fs;
float blend;
float ff;
float fb;

Parameter timeParam;      // t (ms)
Parameter widthParam;     // width (ms)
Parameter rateParam;      // rate (Hz)
Parameter blendParam;     // blend
Parameter feedforwardParam; // feedforward
Parameter feedbackParam;  // feedback

bool bypass = true;

void setrate(float r)
{
    if (r == rate) return;
    rate = r;
    lfo.sine.set_f(rate, fs, lfo.sine.get_phase());
}

void ProcessControls()
{
    static int processCnt = 0;
    switch(processCnt % 8)
    {
    case 0:
        petal.ProcessAnalogControls();
        {
            float ms = fs * .001;
            time  = timeParam.Process() * ms;
            width = widthParam.Process() * ms;
        }
        break;
    case 2:
        setrate(rateParam.Process());
        blend = blendParam.Process();
        break;
    case 4:
        ff = feedforwardParam.Process();
        fb = feedbackParam.Process();
        break;
    case 6:
        petal.ProcessDigitalControls();
        if(petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge())
            bypass = !bypass;
        break;
    }
    processCnt++;
}

Led led1;

void AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    float t = time;
    float w = width;
    if(w >= t - 3) w = t - 3;

    led1.Set(bypass ? 0.0f : 1.0f);

    for(size_t i = 0; i < size; i++)
    {
        if(bypass)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        else
        {
            sample_t x = in[0][i];
            sample_t y = x;
            x = hp.process(x);
            y -= fb * delay.get_linear(t);
            delay.put(y);
            y += blend * x + ff * delay.get_cubic(t + w * lfo.sine.get());
            out[0][i] = out[1][i] = y;
        }
    }
}

void Init(float samplerate)
{
    fs       = samplerate;
    over_fs  = 1.f / fs;

    time  = 0;
    width = 0;
    rate  = 0;

    delay.init((int)(.050 * fs));
    delay.reset();
    hp.reset();
    hp.set_f(250 * over_fs);
    lfo.sine.set_f(0.02f, fs, 0);

    timeParam.Init(petal.knob[Terrarium::KNOB_1], 2.5f,  40.0f, Parameter::LOGARITHMIC);
    widthParam.Init(petal.knob[Terrarium::KNOB_2], 0.5f,  10.0f, Parameter::LINEAR);
    rateParam.Init(petal.knob[Terrarium::KNOB_3],  0.02f,  5.0f, Parameter::LINEAR);
    blendParam.Init(petal.knob[Terrarium::KNOB_4], 0.0f,   1.0f, Parameter::LINEAR);
    feedforwardParam.Init(petal.knob[Terrarium::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    feedbackParam.Init(petal.knob[Terrarium::KNOB_6],    0.0f, 1.0f, Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
    led1.Set(0.0f);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    Init(samplerate);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
