#ifndef VERB_H
#define VERB_H

#include "common.h"

class Verb {
public:
    Verb() :
        processCnt(0)
    {}
    void AudioCallback(InputBuffer in, OutputBuffer out, size_t size);
    void Init(DaisyPetal * petal, float samplerate);
    void ProcessControls();
private:
    DaisyPetal * petal;
    Led led2;

    Parameter vtime;
    Parameter vfreq;
    Parameter vsend;
    bool bypass;
    ReverbSc verb;
    float samplerate;
    int processCnt;
};

#endif // VERB_H
