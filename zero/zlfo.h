#pragma once

#include <stdint.h>
#include "dsp.h"

/** Naive waveform oscillator for use as an LFO. */
class LFOEngine
{
  public:
    LFOEngine() {}
    ~LFOEngine() {}

    enum
    {
        WAVE_SIN,
        WAVE_TRI,
        WAVE_SAW,
        WAVE_RAMP,
        WAVE_LAST,
    };

    void Init(float sample_rate)
    {
        sr_        = sample_rate;
        sr_recip_  = 1.0f / sample_rate;
        freq_      = 100.0f;
        amp_       = 0.5f;
        pw_        = 0.5f;
        phase_     = 0.0f;
        phase_inc_ = CalcPhaseInc(freq_);
        waveform_  = WAVE_SIN;
        eoc_       = true;
        eor_       = true;
    }

    inline void SetFreq(const float f)
    {
        freq_      = f;
        phase_inc_ = CalcPhaseInc(f);
    }

    inline void SetAmp(const float a)  { amp_ = a; }
    inline void SetWaveform(const uint8_t wf)
    {
        waveform_ = wf < WAVE_LAST ? wf : WAVE_SIN;
    }

    inline bool IsEOR()     { return eor_; }
    inline bool IsEOC()     { return eoc_; }
    inline bool IsRising()  { return phase_ < 0.5f; }
    inline bool IsFalling() { return phase_ >= 0.5f; }

    float Process();

    void PhaseAdd(float _phase) { phase_ += _phase; }
    void Reset(float _phase = 0.0f) { phase_ = _phase; }

  private:
    float   CalcPhaseInc(float f);
    uint8_t waveform_;
    float   amp_, freq_, pw_;
    float   sr_, sr_recip_, phase_, phase_inc_;
    float   last_out_, last_freq_;
    bool    eor_, eoc_;
};

class ZLFO
{
public:
    ZLFO() {};
    void  init(float sample_rate);
    float process();
    void  setDepth(float depth);
    void  setFrequency(float freq);
    void  setManual(float manual);
    void  setWaveform(uint8_t wf);

    enum
    {
        WV_TRIANGLE,
        WV_SINE,
        WV_SAW,
        WV_RAMP,
        WV_LAST,
    };

private:
    LFOEngine osc;
    float     depth;
    float     manual;
    float     offset;
    uint8_t   waveform;
};
