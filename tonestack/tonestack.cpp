#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"
#include "common.h"
#include "ToneStack.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;
using namespace DSP;

DaisyPetal petal;

#define MAX_VOLUME 10
#define MODEL_JCM_800 4

ToneStack tonestack;
Parameter bassParam;
Parameter midsParam;
Parameter trebleParam;
float bass;
float mids;
float treble;
float old_bass   = -1.0f;
float old_mids   = -1.0f;
float old_treble = -1.0f;

bool effectOn;
Led led1;

TSParameters ToneStack::presets[] = {
    #define k *1e3
    #define M *1e6
    #define nF *1e-9
    #define pF *1e-12
    /* R1=treble R2=Bass R3=Mid, C1-3 related caps, R4=parallel resistor */
    {250 k, 1 M, 25 k, 56 k, 250 pF, 20 nF, 20 nF},        /* 59 Bassman 5F6-A */
    {250 k, 250 k, 4.8 k, 100 k, 250 pF, 100 nF, 47 nF},   /* 64 Princeton AA1164 */
    {250 k, 1 M, 25 k, 47 k, 600 pF, 20 nF, 20 nF},         /* Mesa Dual Rect. 'Orange' */
    {1 M, 1 M, 10 k, 100 k, 50 pF, 22 nF, 22 nF},           /* Vox "top boost" */
    {220 k, 1 M, 22 k, 33 k, 470 pF, 22 nF, 22 nF},         /* 59/81 JCM-800 Lead 100 2203 */
    {250 k, 250 k, 10 k, 100 k, 120 pF, 100 nF, 47 nF},     /* 69 Twin Reverb AA270 */
    {500 k, 1 M, 25 k, 47 k, 150 pF, 22 nF, 22 nF},         /* Hughes & Kettner Tube 20 */
    {250 k, 250 k, 10 k, 100 k, 150 pF, 82 nF, 47 nF},      /* Roland Jazz Chorus */
    {250 k, 1 M, 50 k, 33 k, 100 pF, 22 nF, 22 nF},         /* Pignose G40V */
    #if 0
    {250 k, 1 M, 25 k, 33 k, 500 pF, 22 nF, 22 nF},         /* 67 Major Lead 200 */
    {250 k, 1 M, 25 k, 56 k, 500 pF, 22 nF, 22 nF},         /* 81 2000 Lead */
    {250 k, 250 k, 25 k, 56 k, 250 pF, 47 nF, 47 nF},       /* undated M2199 30W solid state */
    #endif
    #undef k
    #undef M
    #undef nF
    #undef pF
};

void ProcessControls()
{
    static uint32_t processCnt = 0;
    switch(processCnt % 8)
    {
    case 0:
        petal.ProcessAnalogControls();
        bass = bassParam.Process();
        if(bass != old_bass) {
            old_bass = bass;
            tonestack.updatecoefs(bass, mids, treble);
        }
        break;
    case 2:
        mids = midsParam.Process();
        if(mids != old_mids) {
            old_mids = mids;
            tonestack.updatecoefs(bass, mids, treble);
        }
        break;
    case 4:
        treble = trebleParam.Process();
        if(treble != old_treble) {
            old_treble = treble;
            tonestack.updatecoefs(bass, mids, treble);
        }
        break;
    case 6:
        petal.ProcessDigitalControls();
        effectOn = petal.switches[Terrarium::SWITCH_1].Pressed();
        led1.Set(effectOn ? 1.0f : 0.0f);
        break;
    }
    processCnt++;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        if(!effectOn)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        else
        {
            out[0][i] = out[1][i] = tonestack.process(in[0][i]);
        }
    }
}

void Init(float samplerate)
{
    bassParam.Init(petal.knob[Terrarium::KNOB_1],   0.0f, 1.0f, Parameter::LOGARITHMIC);
    midsParam.Init(petal.knob[Terrarium::KNOB_2],   0.0f, 1.0f, Parameter::LINEAR);
    trebleParam.Init(petal.knob[Terrarium::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);

    tonestack.init(samplerate);
    tonestack.setmodel(MODEL_JCM_800);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
    led1.Set(0.0f);
}

int main(void)
{
    petal.Init();
    float samplerate = petal.AudioSampleRate();
    petal.SetAudioBlockSize(1);
    Init(samplerate);

    effectOn = false;

    petal.StartAdc();
    petal.ProcessAnalogControls();
    bass   = bassParam.Process();
    mids   = midsParam.Process();
    treble = trebleParam.Process();
    old_bass = bass; old_mids = mids; old_treble = treble;
    tonestack.updatecoefs(bass, mids, treble);

    petal.StartAudio(AudioCallback);
    while(1)
    {
        System::Delay(1);
        ProcessControls();
        led1.Update();
    }
}
