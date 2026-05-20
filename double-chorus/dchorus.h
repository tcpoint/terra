#pragma once
#ifndef DCHORUS_H
#define DCHORUS_H
#ifdef __cplusplus

#include "daisysp.h"
#include "biquad.h"
#include <stdint.h>

/** @file chorus.h */

namespace daisysp
{
/**  
    @brief Single Chorus engine. Used in Chorus.
    @author Ben Sergentanis
*/
class DChorusEngine
{
  public:
    DChorusEngine() {}
    ~DChorusEngine() {}

    /** Initialize the module
        \param sample_rate Audio engine sample rate.
    */
    void Init(float sample_rate, float phase);

    /** Get the next sample
        \param in Sample to process
    */
    float Process(float in);

    /** How much to modulate the delay by.
        \param depth Works 0-1.
    */
    void SetLfoDepth(float depth);

    /** Set lfo frequency.
        \param freq Frequency in Hz
    */
    void SetLfoFreq(float freq);

    /** Set the internal delay rate. 
        \param delay Tuned for 0-1. Maps to .1 to 50 ms.
    */
    void SetDelay(float delay);

    /** Set the delay time in ms.
        \param ms Delay time in ms.
    */
    void SetDelayMs(float ms);

    /** Set the feedback amount.
        \param feedback Amount from 0-1.
    */
    void SetFeedback(float feedback);

  private:
    float                    sample_rate_;
    static constexpr int32_t kDelayLength
        = 2400; // 50 ms at 48kHz = .05 * 48000

    //triangle lfos
    Oscillator lfo_;
    float lfo_amp_;

    float feedback_;

    float delay_;

    DelayLine<float, kDelayLength> del_;

    float ProcessLfo();
    LowpassBiquad filt;
};

/**  
    Based on https://www.izotope.com/en/learn/understanding-chorus-flangers-and-phasers-in-audio-production.html \n
    and https://www.researchgate.net/publication/236629475_Implementing_Professional_Audio_Effects_with_DSPs \n
*/
#define NUM_CHORUSES 4
class DChorus
{
  public:
    DChorus() {}
    ~DChorus() {}

    /** Initialize the module
        \param sample_rate Audio engine sample rate
    */
    void Init(float sample_rate);

    /** Get the net floating point sample.
        \param in Sample to process
    */
    float Process(float in);


    /** Set lfo depths.
        \param depth Both channels lfo depth. Works 0-1.
    */
    void SetLfoDepth(float depth);

    /** Set both lfo frequencies.
        \param depth Both channel lfo freqs in Hz.
    */
    void SetLfoFreq(float freq);

    /** Set channels delay amounts.
        \param delay Both channel delay amount. Works 0-1.
    */
    void SetDelay(float delay);


    /** Set channels delay in ms.
        \param ms Both channel delay amounts in ms.
    */
    void SetDelayMs(float ms);

    /** Set channels feedback.
        \param feedback Both channel feedback. Works 0-1.
    */
    void SetFeedback(float feedback);

  private:
    DChorusEngine engines_[NUM_CHORUSES];
};
} //namespace daisysp
#endif
#endif