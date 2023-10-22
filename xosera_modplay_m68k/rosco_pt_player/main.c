#include "pt_mod.h"
#include <stdio.h>

int main(PtMod * mod)
{

    printf("Song name        : %s\n", mod->song_name);
    printf("Signature        : %c%c%c%c\n", mod->signature[0], mod->signature[1], mod->signature[2], mod->signature[3]);

    for (int i = 0; i < 31; i++)
    {
        printf("       Sample %02d : %-22s [L: %5d, V: %03d, FT: %03d]\n",
               i + 1,
               mod->samples[i].sample_name,
               BE2(mod->samples[i].sample_length) * 2,
               mod->samples[i].volume,
               mod->samples[i].finetune);
    }

    uint8_t pattern_count = PtPatternCount(mod);
    printf("Pattern count    : %d\n", pattern_count);
    printf("Song length      : %d\n", mod->song_length);
    printf("Song layout      : [");
    for (int i = 0; i < mod->song_length; i++)
    {
        printf("%02d, ", mod->positions[i]);
        if (i > 0 && i % 20 == 0)
        {
            printf("\n                    ");
        }
    }
    printf("]\n\n");

    PtPattern * patterns = (PtPattern *)(mod + 1);
    PtPattern * pattern  = &patterns[0];

    printf("Display pattern 0:\n");

    for (int i = 0; i < 64; i++)
    {
        printf(
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

    return 0;
}
