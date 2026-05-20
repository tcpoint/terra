#include "dchorus.h"
#include <math.h>

using namespace daisysp;

//ChorusEngine stuff
void DChorusEngine::Init(float sample_rate, float phase)
{
    sample_rate_ = sample_rate;

    del_.Init();
    lfo_.Init(sample_rate);
    lfo_.SetAmp(1.0);
    lfo_.SetWaveform(Oscillator::WAVE_SIN);
    lfo_.Reset(phase);
    lfo_amp_  = 0.f;
    feedback_ = .2f;
    SetDelay(.75);

    SetLfoFreq(.3f);
    SetLfoDepth(.9f);
    filt.Init(10000.0, 0.7071, 1.0, sample_rate);
}

float DChorusEngine::Process(float in)
{
    float lfo_sig = ProcessLfo();
    del_.SetDelay(lfo_sig + delay_);

    float out = del_.Read();
    // float filteredMixedSample = filt.Process(in + out * feedback_);
    // ignore feedback for awhile
    float filteredMixedSample = filt.Process(in);
    del_.Write(filteredMixedSample);
    return out;
}

void DChorusEngine::SetLfoDepth(float depth)
{
    depth    = fclamp(depth, 0.f, .93f);
    lfo_amp_ = depth * delay_;
}

void DChorusEngine::SetLfoFreq(float freq)
{
    lfo_.SetFreq(freq);
}

void DChorusEngine::SetDelay(float delay)
{
    delay = (.1f + delay * 7.9f); //.1 to 8 ms
    SetDelayMs(delay);
}

void DChorusEngine::SetDelayMs(float ms)
{
    ms     = fmax(.1f, ms);
    delay_ = ms * .001f * sample_rate_; //ms to samples

    lfo_amp_ = fmin(lfo_amp_, delay_); //clip this if needed
}

void DChorusEngine::SetFeedback(float feedback)
{
    feedback_ = fclamp(feedback, 0.f, 1.f);
}

float DChorusEngine::ProcessLfo()
{
    float phase = lfo_.Process();
    return phase * lfo_amp_;
}

//Chorus Stuff
void DChorus::Init(float sample_rate)
{
    engines_[0].Init(sample_rate, 0);
    engines_[1].Init(sample_rate, PI_F * 0.5);
    engines_[2].Init(sample_rate, PI_F);
    engines_[3].Init(sample_rate, PI_F * 1.5);
}

float DChorus::Process(float in)
{
    float sig = 0.f;

    for(int i = 0; i < NUM_CHORUSES; i++)
    {
        sig += engines_[i].Process(in);
    }
    // add some kind of balance.
    return sig / NUM_CHORUSES;
}

void DChorus::SetLfoDepth(float depth)
{
    engines_[0].SetLfoDepth(depth);
    engines_[1].SetLfoDepth(depth);
    engines_[2].SetLfoDepth(depth);
    engines_[3].SetLfoDepth(depth);
}

void DChorus::SetLfoFreq(float freq)
{
    engines_[0].SetLfoFreq(freq);
    engines_[1].SetLfoFreq(freq);
    engines_[2].SetLfoFreq(freq);
    engines_[3].SetLfoFreq(freq);
}

void DChorus::SetDelay(float delay)
{
    engines_[0].SetDelay(delay);
    engines_[1].SetDelay(delay);
    engines_[2].SetDelay(delay);
    engines_[3].SetDelay(delay);
}

void DChorus::SetDelayMs(float ms)
{
    engines_[0].SetDelayMs(ms);
    engines_[1].SetDelayMs(ms);
    engines_[2].SetDelayMs(ms);
    engines_[3].SetDelayMs(ms);
}

void DChorus::SetFeedback(float feedback)
{
    engines_[0].SetFeedback(feedback);
    engines_[1].SetFeedback(feedback);
    engines_[2].SetFeedback(feedback);
    engines_[3].SetFeedback(feedback);
}
