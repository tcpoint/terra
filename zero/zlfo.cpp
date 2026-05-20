#include "daisysp.h"
#include "dsp.h"
#include "zlfo.h"
#include <math.h>

using namespace daisysp;

// CrossFade cfade;
float LFOEngine::Process()
{
    float out, t;
    switch(waveform_)
    {
        case WAVE_SIN: out = sinf(phase_ * TWOPI_F); break;
        case WAVE_TRI:
            t   = -1.0f + (2.0f * phase_);
            out = 2.0f * (fabsf(t) - 0.5f);
            break;
        case WAVE_SAW: out = -1.0f * (((phase_ * 2.0f)) - 1.0f); break;
        case WAVE_RAMP: out = ((phase_ * 2.0f)) - 1.0f; break;
        default: out = 0.0f; break;
    }
    phase_ += phase_inc_;
    if(phase_ > 1.0f)
    {
        phase_ = 1.0f - (phase_ - 1.0f);
        phase_inc_ = -phase_inc_;
        eoc_ = true;
    }
    else if(phase_ < -1.0f)
    {
        phase_ = -1.0f - (phase_ + 1.0f);
        phase_inc_ = -phase_inc_;
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
    // set default values for everything else
    setFrequency(.3);
    setDepth(0.7);
    setManual(0.0);
    setWaveform(WV_TRIANGLE);
}

float ZLFO::process()
{
    float phase = osc.Process();
    return phase + this->offset;
}

void ZLFO::setDepth(float depth)
{
    this->depth = fclamp(depth, 0.0, 0.93);
    osc.SetAmp(this->depth);
    this->offset = (1.0f - this->depth) * this->manual;
}


/** Set lfo frequency.
    \param freq Frequency in Hz
*/
void ZLFO::setFrequency(float freq)
{
    osc.SetFreq(freq);
}

void ZLFO::setManual(float manual)
{
    this->manual = manual;
    this->offset = (1.0f - this->depth) * manual;
}

void ZLFO::setWaveform(uint8_t wf)
{
    if(wf == waveform) {
        return;
    }
    waveform = wf;
    if(wf >= WV_LAST) {
        wf = WV_TRIANGLE;
    }
    switch(wf) {
    case WV_TRIANGLE:
        wf = LFOEngine::WAVE_TRI;
        break;
    case WV_SINE:
        wf = LFOEngine::WAVE_SIN;
        break;
    case WV_SAW:
        wf = LFOEngine::WAVE_SAW;
        break;
    case WV_RAMP:
        wf = LFOEngine::WAVE_RAMP;
        break;
    }
    osc.SetWaveform(wf);
}

