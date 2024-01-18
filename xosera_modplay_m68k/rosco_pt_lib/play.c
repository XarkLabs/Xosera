#include "dprintf.h"
#include "pt_mod.h"
#include <basicio.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <xosera_m68k_api.h>
#include <xosera_m68k_defs.h>

#define SILENCE_VADDR 0xffff

#define BUFFER_A 0xa000
#define BUFFER_B 0xa400

// #define DEBUG

typedef struct
{
    PtMemorySample * current_sample;
    uint16_t         next_chunk_start;
    uint16_t         next_buffer_start;
    uint16_t         buffer_size;
    uint16_t         buffer_a_addr;
    uint16_t         buffer_b_addr;
} Channel;

/* Returns true if this we loaded the last chunk */
static bool load_sample_chunk(PtMemorySample * sample,
                              uint16_t         addr,
                              uint16_t         chunk_start,
                              uint16_t         chunk_len,
                              uint16_t *       outActualSize)
{
    xv_prep();

    xm_setw(WR_XADDR, addr);

    uint16_t chunk_end = chunk_start + chunk_len;
    if (chunk_end > sample->length)
    {
        chunk_end = sample->length;
    }

#ifdef DEBUG
    dprintf("LOAD: %d-%d in buffer at 0x%04x\n", chunk_start, chunk_end, addr);
#endif

    *outActualSize = chunk_end - chunk_start;

    for (uint16_t i = chunk_start; i < chunk_end; i++)
    {
        xm_setw(XDATA, sample->data[i]);
    }

    return chunk_end == sample->length;
}

/* Returns true if this we loaded the last chunk; Mutates state in argument and Xosera */
static bool load_next_chunk(Channel * channel, uint16_t * outActualSize)
{
    bool result = load_sample_chunk(channel->current_sample,
                                    channel->next_buffer_start,
                                    channel->next_chunk_start,
                                    channel->buffer_size,
                                    outActualSize);

    channel->next_buffer_start =
        channel->next_buffer_start == channel->buffer_b_addr ? channel->buffer_a_addr : channel->buffer_b_addr;

    channel->next_chunk_start = channel->next_chunk_start + channel->buffer_size;

    return result;
}

static void init_channel(Channel *        channel,
                         PtMemorySample * sample,
                         uint16_t         buffer_a,
                         uint16_t         buffer_b,
                         uint16_t         buffer_size)
{
    channel->current_sample    = sample;
    channel->next_chunk_start  = 0;
    channel->next_buffer_start = buffer_a;
    channel->buffer_a_addr     = buffer_a;
    channel->buffer_b_addr     = buffer_b;
    channel->buffer_size       = buffer_size;
}

static void restart_channel(Channel * channel)
{
#ifdef debug
    dprintf("Restart channel\n");
#endif
    channel->next_chunk_start = 0;
}

static void xosera_channel_ready(Channel * channel)
{
    xv_prep();

    uint16_t actualSize;
    bool     last = load_next_chunk(channel, &actualSize);

    xreg_setw(AUD0_LENGTH, actualSize | AUD_LENGTH_TILEMEM_B);
    xreg_setw(AUD0_START,
              channel->next_buffer_start == channel->buffer_b_addr ? channel->buffer_a_addr : channel->buffer_b_addr);

    if (actualSize != channel->buffer_size)
    {
        dprintf("Got non-buffer size chunk [%d words]\n", actualSize);
    }
    else
    {
        dprintf("Got full chunk\n");
    }

    if (last)
    {
        restart_channel(channel);
    }
}

int xosera_play(PtMod * mod, int number, uint16_t rate)
{
    xv_prep();

    while (checkchar())
    {
        readchar();
    }

    PtMemorySample samples[31];
    PtPopulateMemorySamples(mod, samples);

    dprintf("Samples populated in memory; Will play #%d\n", number);
    dprintf("Sample length is %d words\n", samples[number].length);

    dprintf("First 5 words: 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
            samples[number].data[0],
            samples[number].data[1],
            samples[number].data[2],
            samples[number].data[3],
            samples[number].data[4]);

    uint32_t clk_hz = xosera_sample_hz();
    uint16_t period = (clk_hz + rate - 1) / rate;        // rate is samples per second
    dprintf("Period is %d\n", period);

    Channel channel;
    init_channel(&channel, &samples[number], BUFFER_A, BUFFER_B, 0x400);

    xreg_setw(AUD_CTRL, AUD_CTRL_AUD_EN_F);
    xreg_setw(AUD0_PERIOD, period);
    xreg_setw(AUD0_VOL, MAKE_AUD_VOL(AUD_VOL_FULL / 2, AUD_VOL_FULL / 2));

    xosera_channel_ready(&channel);

    int num_readys = 0;
    int max_loops  = 0;
    int min_loops  = INT_MAX;
    int this_loops = 0;

    dprintf("Playing annoying loop; hit a key when it all becomes too much\n");

    while (!checkchar())
    {
        if ((xreg_getw(AUD_CTRL) & 0x0100) == 0)
        {
            xosera_channel_ready(&channel);

            if (this_loops < min_loops && this_loops > 0)
            {
                min_loops = this_loops;
            }

            if (this_loops > max_loops)
            {
                max_loops = this_loops;
            }

            num_readys++;
            this_loops = 0;
        }
        else
        {
            this_loops++;
        }
    }

    xreg_setw(AUD_CTRL, 0x0000);

    dprintf("Loaded buffers %d times. Had between %d and %d loops of free time between loading\n",
            num_readys,
            min_loops,
            max_loops);
    return 1;
}
