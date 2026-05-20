#pragma once

#include <stdint.h>
#include "Utility/delayline.h"
#include "zlfo.h"

/** Flanging effect: modulating phase-shifted copy mixed with the original. */
class ZFlanger
{
  public:
    ZFlanger() {};
    void  init(float sample_rate);
    float process(float in);

    /** How much of the signal to feedback into the delay line. Works 0-1. */
    void setFeedback(float feedback);

    /** How much to modulate the delay by. Works 0-1. */
    void setLFODepth(float depth);

    /** Set LFO frequency in Hz. */
    void setLFOFreq(float freq);

    void setLFOManual(float manual);
    void setWaveform(const uint8_t wf);

    /** Set delay from 0-1, maps to 0.1–7 ms. */
    void setDelay(float delay);

  private:
    void setDelayMs(float ms);

    ZLFO lfo;
    float sample_rate;

    // del2 is a fixed pre-delay (kDelayLength2 samples); del is the
    // modulated line. Mixing pre-delayed and modulated reads produces flanging.
    static constexpr size_t kDelayLength  = 672; // 14 ms at 48kHz
    static constexpr size_t kDelayLength2 = 336; //  7 ms at 48kHz

    float feedback;
    float delay;
    float delay2;
    float mix;

    daisysp::DelayLine<float, kDelayLength + 1> del;
    daisysp::DelayLine<float, kDelayLength2>    del2;
};
