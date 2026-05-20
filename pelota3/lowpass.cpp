#include "lowpass.h"
#include <math.h>

using namespace daisysp;

void Lowpass::Reset()
{
    float theta = 2.0f * M_PI * freq_ / sample_rate_;
    float K     = tanf(theta / 2);
    float W     = K * K;
    float alpha = 1.0f + K / q_ + W;

    b0_ = W / alpha;
    b1_ = 2.0f * W / alpha;
    b2_ = b0_;
    a1_ = 2.0f * (W - 1.0f) / alpha;
    a2_ = (1.0f - K / q_ + W) / alpha;
}

void Lowpass::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    freq_ = 1000.0f;
    q_    = 0.7f;
    Reset();
    xnm1_ = xnm2_ = ynm1_ = ynm2_ = 0.0f;
}

float Lowpass::Process(float in)
{
    float yn = b0_ * in + b1_ * xnm1_ + b2_ * xnm2_ - a1_ * ynm1_ - a2_ * ynm2_;
    xnm2_ = xnm1_;
    xnm1_ = in;
    ynm2_ = ynm1_;
    ynm1_ = yn;
    return yn;
}
