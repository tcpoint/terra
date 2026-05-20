#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

#include "common.h"
#include "basicdelay.h"
#include "verb.h"

DaisyPetal petal;
BasicDelay delay;
Verb verb;

bool delay_first = true;

void AudioCallback(InputBuffer in, OutputBuffer out, size_t size)
{
    if(delay_first) {
        delay.AudioCallback(in, out, size);
        verb.AudioCallback(out, out, size);
    } else {
        verb.AudioCallback(in, out, size);
        delay.AudioCallback(out, out, size);
    }
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);

    delay.Init(&petal, samplerate);
    verb.Init(&petal, samplerate);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(1);
        delay.ProcessControls();
        verb.ProcessControls();
        delay_first = petal.switches[Terrarium::SWITCH_1].Pressed();
    }
}
