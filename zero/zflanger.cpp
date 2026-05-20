#include "dsp.h"
#include "daisysp.h"
#include "zflanger.h"
#include <math.h>

using namespace daisysp;

void ZFlanger::init(float sample_rate)
{
    this->sample_rate = sample_rate;
    lfo.init(sample_rate);

    setFeedback(0.2f);

    del.Init();
    del2.Init();
    del2.SetDelay(kDelayLength2);
    setDelay(0.75f);
}

float ZFlanger::process(float in)
{
    float lfo_sig  = lfo.process();
    float curr_delay = kDelayLength2 + (lfo_sig * (delay / 2.0f));
    del.SetDelay((size_t)(curr_delay < 1.0f ? 1.0f : curr_delay));

    float out        = del.Read();
    float delayed_in = del2.Read();
    del.Write(delayed_in + out * feedback);
    del2.Write(in);

    return (delayed_in + out) * 0.5f;
}

void ZFlanger::setFeedback(float feedback)
{
    feedback       = fclamp(feedback, 0.0f, 1.0f);
    this->feedback = feedback * 0.97f;
}

void ZFlanger::setDelay(float delay)
{
    setDelayMs(.1f + delay * 6.9f);
}

void ZFlanger::setDelayMs(float ms)
{
    ms    = ms < 0.1f ? 0.1f : ms;
    delay = ms * 0.001f * sample_rate;
}

void ZFlanger::setLFOFreq(float freq)
{
    lfo.setFrequency(freq);
}

void ZFlanger::setLFOManual(float manual)
{
    lfo.setManual(manual);
}

void ZFlanger::setLFODepth(float depth)
{
    lfo.setDepth(depth);
}

void ZFlanger::setWaveform(uint8_t waveform)
{
    lfo.setWaveform(waveform);
}
