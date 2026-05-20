#include "biquad.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI (3.141592654)
#endif

MyBiquad::MyBiquad()
{
    prev_input_1 = 0.0;
    prev_input_2 = 0.0;
    prev_output_1 = 0.0;
    prev_output_2 = 0.0;
}

void MyBiquad::Init(
    float frequency,
    float Q,
    float dbGain,
    float sample_rate)
{


// Calculate helper variables for
// generating 'a' and 'b' coefficients
//////////////////////////////////////
    float A = pow(10, dbGain / 40); //convert to db
    float omega = 2 * M_PI * frequency / sample_rate;
    float sn = sin(omega);
    float cs = cos(omega);
    float alpha = sn / (2*Q);
    float beta = sqrt(A + A);
// Load 'a' and 'b' coefficients
// into tmp biquad
/////////////////////////////////
    load_coefficients(A, omega,
                        sn, cs,
                        alpha, beta);
//Scale coeffs to a0
////////////////////
    // tmp->a1 = (tmp->a1) / (tmp->a0);
    // tmp->a2 = (tmp->a2) / (tmp->a0);
    // tmp->b0 = (tmp->b0) / (tmp->a0);
    // tmp->b1 = (tmp->b1) / (tmp->a0);
    // tmp->b2 = (tmp->b2) / (tmp->a0);
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

// Load rest of data
/////////////////////////////////
}

float MyBiquad::Process(float input)
{
    float output =  (b0 * input) +
                    (b1 * prev_input_1) +
                    (b2 * prev_input_2) -
                    (a1 * prev_output_1) -
                    (a2 * prev_output_2);
    prev_input_2 = prev_input_1;
    prev_input_1 = input;
    prev_output_2 = prev_output_1;
    prev_output_1 = output;
    //update last samples...
    return output;
}

void LowpassBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = (1.0 - cs) /2.0;
    b1 = 1.0 - cs;
    b2 = (1.0 - cs) /2.0;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cs;
    a2 = 1.0 - alpha;
}

void HighpassBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = (1 + cs) /2.0;
    b1 = -(1 + cs);
    b2 = (1 + cs) /2.0;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
}

void BandpassBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = alpha;
    b1 = 0;
    b2 = -alpha;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
} 


void NotchBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = 1;
    b1 = -2 * cs;
    b2 = 1;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
}

void PeakBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = 1 + (alpha * A);
    b1 = -2 * cs;
    b2 = 1 - (alpha * A);
    a0 = 1 + (alpha /A);
    a1 = -2 * cs;
    a2 = 1 - (alpha /A);
}        

void LowShelfBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
    b1 = 2 * A * ((A - 1) - (A + 1) * cs);
    b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
    a0 = (A + 1) + (A - 1) * cs + beta * sn;
    a1 = -2 * ((A - 1) + (A + 1) * cs);
    a2 = (A + 1) + (A - 1) * cs - beta * sn;
}        

void HighShelfBiquad::load_coefficients(float A, float omega,
                        float sn, float cs,
                        float alpha, float beta)
{
    b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
    b1 = -2 * A * ((A - 1) + (A + 1) * cs);
    b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
    a0 = (A + 1) - (A - 1) * cs + beta * sn;
    a1 = 2 * ((A - 1) - (A + 1) * cs);
    a2 = (A + 1) - (A - 1) * cs - beta * sn;
}
