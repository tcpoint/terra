/*
Copyright (c) 2020 Electrosmith, Corp

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.
*/

#pragma once
#ifndef DSY_ZFLANGER_H
#define DSY_ZFLANGER_H
#ifdef __cplusplus

#include <stdint.h>
#include "Utility/delayline.h"
#include "zlfo.h"

/** @file zflanger.h */

/** @brief Flanging Audio Effect 
 *
 * Generates a modulating phase shifted copy of a signal, and recombines
 * with the original to create a 'flanging' sound effect.
 */
class ZFlanger
{
  public:
    ZFlanger() {};
    /** Initialize the modules
        \param sample_rate Audio engine sample rate.
    */
    void init(float sample_rate);

    /** Get the next sample
        \param in Sample to process
    */
    float process(float in);

    /** How much of the signal to feedback into the delay line.
        \param feedback Works 0-1.
    */
    void setFeedback(float feedback);

    /** How much to modulate the delay by.
        \param depth Works 0-1.
    */
    void setLFODepth(float depth);

    /** Set lfo frequency.
        \param freq Frequency in Hz
    */
    void setLFOFreq(float freq);

    void setLFOManual(float manual);

    void setWaveform(const uint8_t wf);

    /** Set the internal delay rate. 
        \param delay Tuned for 0-1. Maps to .1 to 7 ms.
    */
    void setDelay(float delay);

    /** Set the delay time in ms.
        \param ms Delay time in ms, .1 to 7 ms.
    */
    void setDelayMs(float ms);

    // void SetMix(float mix);
  private:
    ZLFO lfo;
    float                    sample_rate;
    static constexpr size_t kDelayLength = 672; // 14 ms at 48kHz = .014 * 48000
    static constexpr size_t kDelayLength2 = 336; // 7 ms at 48kHz = .007 * 48000

    float feedback;

    float delay;
    float delay2;
    float mix;

    daisysp::DelayLine<float, kDelayLength+1> del;
    daisysp::DelayLine<float, kDelayLength2> del2;
};

#endif
#endif
