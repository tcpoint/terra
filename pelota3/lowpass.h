#pragma once

#include <stdint.h>

namespace daisysp
{
class Lowpass
{
  public:
    Lowpass() {}
    ~Lowpass() {}
    void  Init(float sample_rate);
    float Process(float in);
    inline void SetQ(float q)       { q_ = q; Reset(); }
    inline void SetFreq(float freq) { freq_ = freq; Reset(); }

  private:
    float sample_rate_, freq_, q_, b0_, b1_, b2_, a1_, a2_,
        xnm1_, xnm2_, ynm1_, ynm2_;
    void Reset();
};
} // namespace daisysp
