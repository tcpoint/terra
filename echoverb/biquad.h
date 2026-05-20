// Bi-Quad Module

#ifndef BIQUAD_H
#define BIQUAD_H

class MyBiquad
{
public:
    MyBiquad();
    void Init(float frequency, float q, float dbGain, float sample_rate);
    float Process(float in);
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta) = 0;
    float a0;
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
    float prev_input_1;
    float prev_input_2;
    float prev_output_1;
    float prev_output_2;
    char* type;
};

class LowpassBiquad: public MyBiquad
{
public:
    LowpassBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};

class HighpassBiquad: public MyBiquad
{
public:
    HighpassBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};

class BandpassBiquad: public MyBiquad
{
public:
    BandpassBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};

class NotchBiquad : public MyBiquad
{
public:
    NotchBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};

class PeakBiquad: public MyBiquad
{
public:
    PeakBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};


class LowShelfBiquad: public MyBiquad
{
public:
    LowShelfBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};


class HighShelfBiquad: public MyBiquad
{
public:
    HighShelfBiquad() {}
protected:
    virtual void load_coefficients(float A, float omega, float sn, float cs, float alpha, float beta);
};

#endif
