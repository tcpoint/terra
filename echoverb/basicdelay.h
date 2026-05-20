#ifndef BASICDELAY_H
#define BASICDELAY_H

#include "common.h"
#include "biquad.h"

#define MAX_DELAY static_cast<size_t>(48000 * 1.f)

struct Delay
{
    DelayLine<float, MAX_DELAY>  *delay;
    float                        currentDelay;
    float                        delayTarget;
    float feedback;

    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        delay->SetDelay(currentDelay);
        float read = delay->Read();
        // degrade feedback
        delay->Write(((feedback * read) + in) * 0.5);
        return read;
    }
    void setFeedback(float feedback)
    {
        this->feedback = feedback;
    }
};

class BasicDelay
{
public:
    BasicDelay() :
        processCnt(0),
        wetdry(0.5f),
        passThruOn(true)
    {
    }
    void AudioCallback(InputBuffer in, OutputBuffer out, size_t size);
    void Init(DaisyPetal * petal, float samplerate);
    void ProcessControls();
private:
    Delay     delay;
    Parameter delayParams;
    Parameter feedbackParam;
    Parameter mixParam;
    CrossFade cfade;
    LowpassBiquad filt1;
    LowpassBiquad filt2;
    // Tone filt3;
    bool filterOn;
    DaisyPetal * petal;
    float samplerate;

    Led led1;

    uint32_t processCnt;
    float wetdry;
    bool  passThruOn;
};

#endif // BASICDELAY_H
