#include "pt_mod.h"
#include <stdio.h>

#ifdef ROSCO_M68K
#include "dprintf.h"
#else
#define dprintf(...) printf(__VA_ARGS__)
#include "outback.h"
#include "syndicate.h"
#include "xenon2.h"
#endif

#define MODDATA ___syndicate_mod

int print_mod(PtMod * mod)
{
    dprintf("Song name        : %s\n", mod->song_name);
    dprintf(
        "Signature        : %c%c%c%c\n", mod->signature[0], mod->signature[1], mod->signature[2], mod->signature[3]);

    for (int i = 0; i < 31; i++)
    {
        dprintf("       Sample %02d : %-22s [L: %5d, V: %03d, FT: %03d]\n",
                i + 1,
                mod->samples[i].sample_name,
                BE2(mod->samples[i].sample_length) * 2,
                mod->samples[i].volume,
                mod->samples[i].finetune);
    }

    uint8_t pattern_count = PtPatternCount(mod);
    dprintf("Pattern count    : %d\n", pattern_count);
    dprintf("Song length      : %d\n", mod->song_length);
    dprintf("Song layout      : [");
    for (int i = 0; i < mod->song_length; i++)
    {
        dprintf("%02d, ", mod->positions[i]);
        if (i > 0 && i % 20 == 0)
        {
            dprintf("\n                    ");
        }
    }
    dprintf("]\n\n");

#ifdef PRINT_PATTERN_0
    PtPattern * patterns = (PtPattern *)(mod + 1);
    PtPattern * pattern  = &patterns[0];

    dprintf("Display pattern 0:\n");

    for (int i = 0; i < 64; i++)
    {
        dprintf(
            "#%03d: C:1 S:%03d N:%3s P:%03d E:%03x    C:2 S:%03d N:%3s P:%03d E:%03x    C:3 S:%03d N:%3s P:%03d E:%03x "
            "   C:4 S:%03d N:%3s P:%03d E:%03x\n",
            i,
            PtSampleNumber(NOTE(pattern->rows[i].channel_notes[0])),
            PtNoteName(PtNotePeriod(NOTE(pattern->rows[i].channel_notes[0]))),
            PtNotePeriod(NOTE(pattern->rows[i].channel_notes[0])),
            PtEffect(NOTE(pattern->rows[i].channel_notes[0])),
            PtSampleNumber(NOTE(pattern->rows[i].channel_notes[1])),
            PtNoteName(PtNotePeriod(NOTE(pattern->rows[i].channel_notes[1]))),
            PtNotePeriod(NOTE(pattern->rows[i].channel_notes[1])),
            PtEffect(NOTE(pattern->rows[i].channel_notes[1])),
            PtSampleNumber(NOTE(pattern->rows[i].channel_notes[2])),
            PtNoteName(PtNotePeriod(NOTE(pattern->rows[i].channel_notes[2]))),
            PtNotePeriod(NOTE(pattern->rows[i].channel_notes[2])),
            PtEffect(NOTE(pattern->rows[i].channel_notes[2])),
            PtSampleNumber(NOTE(pattern->rows[i].channel_notes[3])),
            PtNoteName(PtNotePeriod(NOTE(pattern->rows[i].channel_notes[3]))),
            PtNotePeriod(NOTE(pattern->rows[i].channel_notes[3])),
            PtEffect(NOTE(pattern->rows[i].channel_notes[3])));
    }
#endif

    return 0;
}

#ifndef ROSCO_M68K
int main(void)
{
    PtMod * mod = (PtMod *)MODDATA;
    print_mod(mod);

#ifdef DUMP_SAMPLES
    PtMemorySample samples[31];
    char           buf[20];

    PtPopulateMemorySamples(mod, samples);

    for (int i = 0; i < 31; i++)
    {
        snprintf(buf, 20, "%d.raw", i + 1);

        FILE * out = fopen(buf, "w");
        if (out)
        {
            fwrite(samples[i].data, 2, samples[i].length, out);
            fclose(out);
        }
        else
        {
            printf("Failed to open '%s' for output", buf);
        }
    }

#endif
}
#endif
