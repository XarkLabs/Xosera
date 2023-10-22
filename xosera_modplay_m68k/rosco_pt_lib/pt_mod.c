#include "pt_mod.h"
#include <stdio.h>

uint8_t PtPatternCount(PtMod * mod)
{
    uint8_t r = 0;

    for (int i = 0; i < 128; i++)
    {
        if (mod->positions[i] > r)
        {
            r = mod->positions[i];
        }
    }

    return r;
}

PtPattern * PtPatternData(PtMod * mod)
{
    return (PtPattern *)(mod + 1);
}

uint16_t * PtSampleData(PtMod * mod)
{
    return (uint16_t *)((uintptr_t)mod + sizeof(PtMod) + (PtPatternCount(mod) * 1024));
}

void PtPopulateMemorySamples(PtMod * mod, PtMemorySample * array)
{
    uint16_t * next_sample = PtSampleData(mod);

    for (int i = 0; i < 31; i++)
    {
        array[i].data   = next_sample;
        array[i].length = BE2(mod->samples[i].sample_length);
        array[i].sample = &mod->samples[i];

        next_sample += array[i].length;
    }
}

void PtFixLoop(PtSample * sample)
{
    if ((sample->repeat_point + sample->repeat_length) > sample->sample_length)
    {
        int delta = (sample->repeat_point + sample->repeat_length) - sample->sample_length;
        sample->repeat_point -= delta;

        if ((sample->repeat_point + sample->repeat_length) > sample->sample_length)
        {
            delta = (sample->repeat_point + sample->repeat_length) - sample->sample_length;
            sample->repeat_length -= delta;
        }
    }
}

void PtFixLoops(PtMod * mod)
{
    for (uint8_t i = 0; i < 31; i++)
    {
        PtFixLoop(&mod->samples[i]);
    }
}

const char * PtNoteName(uint16_t period)
{
    switch (period)
    {
        case 0:
            return "---";

        case 856:
            return "C-1";
        case 808:
            return "C#1";
        case 762:
            return "D-1";
        case 720:
            return "D#1";
        case 678:
            return "E-1";
        case 640:
            return "F-1";
        case 604:
            return "F#1";
        case 570:
            return "G-1";
        case 538:
            return "G#1";
        case 508:
            return "A-1";
        case 480:
            return "A#1";
        case 453:
            return "B-1";

        case 428:
            return "C-2";
        case 404:
            return "C#2";
        case 381:
            return "D-2";
        case 360:
            return "D#2";
        case 339:
            return "E-2";
        case 320:
            return "F-2";
        case 302:
            return "F#2";
        case 285:
            return "G-2";
        case 269:
            return "G#2";
        case 254:
            return "A-2";
        case 240:
            return "A#2";
        case 226:
            return "B-2";

        case 214:
            return "C-3";
        case 202:
            return "C#3";
        case 190:
            return "D-3";
        case 180:
            return "D#3";
        case 170:
            return "E-3";
        case 160:
            return "F-3";
        case 151:
            return "F#3";
        case 143:
            return "G-3";
        case 135:
            return "G#3";
        case 127:
            return "A-3";
        case 120:
            return "A#3";
        case 113:
            return "B-3";

        default:
            return "???";
    }
}
