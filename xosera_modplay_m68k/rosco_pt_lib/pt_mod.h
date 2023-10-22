#ifndef __PT_MOD_H
#define __PT_MOD_H

#include <stdint.h>

typedef struct
{
    char     sample_name[22];
    uint16_t sample_length;
    uint8_t  reserved : 4;
    int8_t   finetune : 4;
    uint8_t  volume;
    uint16_t repeat_point;
    uint16_t repeat_length;
} __attribute__((packed)) PtSample;

typedef struct
{
    char     song_name[20];
    PtSample samples[31];
    uint8_t  song_length;
    uint8_t  always_127;
    uint8_t  positions[128];
    char     signature[4];
} __attribute__((packed)) PtMod;

typedef struct
{
    uint32_t channel_notes[4];
} __attribute__((packed)) PtPatternRow;

typedef struct
{
    PtPatternRow rows[64];
} __attribute__((packed)) PtPattern;

typedef struct
{
    uint16_t * data;
    uint16_t   length;
    PtSample * sample;
} PtMemorySample;

/**
 * Number of patterns in the mod
 */
uint8_t PtPatternCount(PtMod * mod);

/**
 * Pattern array
 */
PtPattern * PtPatternData(PtMod * mod);

/**
 * Start of sample data in the mod
 */
uint16_t * PtSampleData(PtMod * mod);

/**
 * Populate array of memory samples (must have >= 31 entries!)
 */
void PtPopulateMemorySamples(PtMod * mod, PtMemorySample * array);

/**
 * Fix up inconsistent sample loop data for a single PtSample
 */
void PtFixLoop(PtSample * sample);

/**
 * Fix up inconsistent sample loop data for all PtSamples
 */
void PtFixLoops(PtMod * mod);

/**
 * Human-readable note name (or `---` or `???`).
 */
const char * PtNoteName(uint16_t period);

#ifndef __PT_MOD_NO_MACROS__
#ifdef __LITTLE_ENDIAN__
#define BE2(n) ((((n) >> 8) & 0x00FF) | (((n) << 8) & 0xFF00))
#define BE4(n)                                                                                                         \
    ((((n) >> 24) & 0x000000FF) | (((n) >> 8) & 0x0000FF00) | (((n) << 8) & 0x00FF0000) | (((n) << 24) & 0xFF000000))
#else
#define BE2(n) ((n))
#define BE4(n) ((n))
#endif

#define NOTE BE4

#define PtSampleNumber(note) ((uint8_t)((note & 0xF0000000) >> 24 | (note & 0x0000F000) >> 12))
#define PtNotePeriod(note)   ((uint16_t)((note & 0x0FFF0000) >> 16))
#define PtEffect(note)       ((uint16_t)(note & 0x00000FFF))

// Convert Amiga period into Xosera period. If FP is too slow we might need to precompute these
// instead, but this will do for now.
//
// Calculated based on ~3.5 (actually, half NTSC miggy clock) Amiga ticks per period
//
// NTSC: 25 / (7.09 / 2)  (Xosera freq / half NTSC Amiga clock)
#define PtXoseraPeriodNTSC(migPrd) ((((migPrd)) * 6.983240223463687))
// PAL: 25 / (7.16 / 2)   (Xosera freq / half PAL Amiga clock)
#define PtXoseraPeriodPAL(migPrd) ((((migPrd)) * 7.052186177715092))

#ifdef NTSC
// #define PtXoseraPeriod(migPrd)  (( ((migPrd)) * 6.983240223463687 ))
#define PtXoseraPeriod PtXoseraPeriodNTSC
#else
// #define PtXoseraPeriod(migPrd)  (( ((migPrd)) * 7.052186177715092 ))
#define PtXoseraPeriod PtXoseraPeriodPAL
#endif

#endif

#endif        //__PT_MOD_H
