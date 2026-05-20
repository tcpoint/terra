#include "daisysp.h"
#include "dsp.h"
#include "zlfo.h"
#include <math.h>

using namespace daisysp;

float LFOEngine::Process()
{
    float out, t;
    switch(waveform_)
    {
        case WAVE_SIN:  out = sinf(phase_ * TWOPI_F); break;
        case WAVE_TRI:
            t   = -1.0f + (2.0f * phase_);
            out = 2.0f * (fabsf(t) - 0.5f);
            break;
        case WAVE_SAW:  out = -1.0f * (phase_ * 2.0f - 1.0f); break;
        case WAVE_RAMP: out = phase_ * 2.0f - 1.0f; break;
        default:        out = 0.0f; break;
    }
    phase_ += phase_inc_;
    if(phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
        eoc_ = true;
    }
    else
    {
        eoc_ = false;
    }
    eor_ = (phase_ - phase_inc_ < 0.5f && phase_ >= 0.5f);
    return out * amp_;
}

float LFOEngine::CalcPhaseInc(float f)
{
    return f * sr_recip_;
}

void ZLFO::init(float sample_rate)
{
    osc.Init(sample_rate);
    setFrequency(0.3f);
    setDepth(0.7f);
    setManual(0.0f);
    setWaveform(WV_TRIANGLE);
}

float ZLFO::process()
{
    return osc.Process() + offset;
}

void ZLFO::setDepth(float depth)
{
    this->depth = fclamp(depth, 0.0f, 0.93f);
    osc.SetAmp(this->depth);
    offset = (1.0f - this->depth) * manual;
}

void ZLFO::setFrequency(float freq)
{
    osc.SetFreq(freq);
}

void ZLFO::setManual(float manual)
{
    this->manual = manual;
    offset = (1.0f - depth) * manual;
}

void ZLFO::setWaveform(uint8_t wf)
{
    if(wf >= WV_LAST)
        wf = WV_TRIANGLE;
    if(wf == waveform)
        return;
    waveform = wf;
    switch(wf) {
    case WV_TRIANGLE: osc.SetWaveform(LFOEngine::WAVE_TRI);  break;
    case WV_SINE:     osc.SetWaveform(LFOEngine::WAVE_SIN);  break;
    case WV_SAW:      osc.SetWaveform(LFOEngine::WAVE_SAW);  break;
    case WV_RAMP:     osc.SetWaveform(LFOEngine::WAVE_RAMP); break;
    }
}
