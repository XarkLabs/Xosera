#include "xosera_mod_play.h"
#include "pt_mod.h"
#include <stdbool.h>
#include <stdint.h>
#include <xosera_m68k_api.h>
#include <xosera_m68k_defs.h>

#include "xosera_freq.h"        // Include this after NTSC is defined (or not)

#if LOG
#ifdef ROSCO_M68K
#include "dprintf.h"
#define pointer_t uint32_t
#else
#include <stdio.h>
#define dprintf(...) printf(__VA_ARGS__)
#define pointer_t    uint64_t
#endif
#endif

typedef struct
{
    uint8_t  active;
    uint8_t  command;
    uint8_t  paramx;
    uint8_t  paramy;
    uint16_t data0;
    uint32_t data1;
} Effect;

typedef struct
{
    PtMemorySample * current_sample;
    uint16_t         next_chunk_start;
    uint16_t         next_buffer_start;
    uint16_t         period;
    uint16_t         buffer_size;
    uint16_t         buffer_a_addr;
    uint16_t         buffer_b_addr;

    // registers
    uint8_t xosera_channel;

    // effects
    uint8_t current_volume;
    Effect  current_effect;
} Channel;

#if LOG
typedef enum
{
    EVENT_LOAD_SAMPLE,
    EVENT_VOLUME_CHANGE,
    EVENT_VOLUME_SLIDE,
    EVENT_SONG_SPEED,
    EVENT_DELAY_SAMPLE,
    EVENT_CUT_SAMPLE,
    EVENT_DELAY_COMMAND_TRIGGERED,
    EVENT_LOOP_TRIGGERED,
    EVENT_VOLUME_SLIDE_STEP,
    EVENT_DELAY_PATTERN,
} LogEvent;

typedef struct
{
    LogEvent event;
    uint8_t  pattern;
    uint8_t  patternPos;
    uint8_t  tick;
    union
    {
        struct
        {
            uint16_t  channel;
            uint16_t  addr;
            uint16_t  chunk_start;
            uint16_t  chunk_end;
            uint16_t  loaded_size;
            pointer_t sample_addr;
        } load_sample;
        struct
        {
            uint16_t channel;
            uint16_t volume;
        } volume_change;
        struct
        {
            uint16_t channel;
            uint16_t previous_volume;
            uint8_t  change_param;
            uint8_t  paramx;
            uint8_t  paramy;
        } volume_slide;
        struct
        {
            uint16_t channel;
            uint16_t previous_speed;
            uint8_t  change_param;
            uint16_t new_speed;
        } song_speed_change;
        struct
        {
            uint16_t channel;
            uint16_t delay_ticks;
        } delay_cut_sample;
        struct
        {
            uint16_t channel;
        } delay_triggered;
        struct
        {
            uint16_t channel;
            uint16_t loop_start;
            uint16_t loop_length;
        } loop_triggered;
        struct
        {
            uint16_t channel;
            uint16_t previous_volume;
            uint8_t  paramx;
            uint8_t  paramy;
        } volume_slide_step;
        struct
        {
            uint16_t channel;
            uint16_t delay_divisions;
        } delay_pattern;
    };
} __attribute__((packed)) LogLine;
#endif

// We need to have the bare minimum stuff zeroed in
// here, or effects processing gets a bit bonkers...
static PtSample silenceSample = {
    .volume        = 0,
    .repeat_point  = 0,
    .repeat_length = 0,
};

static PtMemorySample silence = {
    .sample = &silenceSample,
};

static volatile uint16_t StepFrames;
static int volatile timerCounter;
static volatile int patternPos = -1;
static volatile int position   = 0;
static volatile int pattern    = 0;

static Channel channel0;
static Channel channel1;
static Channel channel2;
static Channel channel3;

static PtMod *        mod;
static PtMemorySample samples[31];
static PtPattern *    patterns;

#if LOG
#if LOG_PERSIST
static uint16_t * logIdx = (uint16_t *)0xbfffe;
static LogLine *  dbgLog = (LogLine *)0xc0000;
#else
static LogLine    actualLog[1000];
static uint16_t   actualLogIdx = 0;
static uint16_t * logIdx       = &actualLogIdx;
static LogLine *  dbgLog       = actualLog;
#endif

static void sllog(uint16_t  channel,
                  uint16_t  addr,
                  uint16_t  chunk_start,
                  uint16_t  chunk_end,
                  uint16_t  loaded_size,
                  pointer_t sample_addr)
{
    dbgLog[*logIdx].event                   = EVENT_LOAD_SAMPLE;
    dbgLog[*logIdx].pattern                 = pattern;
    dbgLog[*logIdx].patternPos              = patternPos;
    dbgLog[*logIdx].tick                    = timerCounter;
    dbgLog[*logIdx].load_sample.channel     = channel;
    dbgLog[*logIdx].load_sample.addr        = addr;
    dbgLog[*logIdx].load_sample.chunk_start = chunk_start;
    dbgLog[*logIdx].load_sample.chunk_end   = chunk_end;
    dbgLog[*logIdx].load_sample.sample_addr = sample_addr;
    dbgLog[*logIdx].load_sample.loaded_size = loaded_size;

    if (++(*logIdx) == 1000)
    {
        *logIdx = 0;
    }
}

static void vclog(uint16_t channel, uint16_t volume)
{
    dbgLog[*logIdx].event                 = EVENT_VOLUME_CHANGE;
    dbgLog[*logIdx].pattern               = pattern;
    dbgLog[*logIdx].patternPos            = patternPos;
    dbgLog[*logIdx].tick                  = timerCounter;
    dbgLog[*logIdx].volume_change.channel = channel;
    dbgLog[*logIdx].volume_change.volume  = volume;

    if (++(*logIdx) == 1000)
    {
        *logIdx = 0;
    }
}

static void vslog(uint16_t channel, uint16_t previous_volume, uint16_t change_param, uint8_t paramx, uint8_t paramy)
{
    dbgLog[*logIdx].event                        = EVENT_VOLUME_SLIDE;
    dbgLog[*logIdx].pattern                      = pattern;
    dbgLog[*logIdx].patternPos                   = patternPos;
    dbgLog[*logIdx].tick                         = timerCounter;
    dbgLog[*logIdx].volume_slide.channel         = channel;
    dbgLog[*logIdx].volume_slide.previous_volume = previous_volume;
    dbgLog[*logIdx].volume_slide.change_param    = change_param;
    dbgLog[*logIdx].volume_slide.paramx          = paramx;
    dbgLog[*logIdx].volume_slide.paramy          = paramy;

    if (++(*logIdx) == 1000)
    {
        *logIdx = 0;
    }
}

static void vsslog(uint16_t channel, uint16_t previous_volume, uint8_t paramx, uint8_t paramy)
{
    dbgLog[*logIdx].event                             = EVENT_VOLUME_SLIDE_STEP;
    dbgLog[*logIdx].pattern                           = pattern;
    dbgLog[*logIdx].patternPos                        = patternPos;
    dbgLog[*logIdx].tick                              = timerCounter;
    dbgLog[*logIdx].volume_slide_step.channel         = channel;
    dbgLog[*logIdx].volume_slide_step.previous_volume = previous_volume;
    dbgLog[*logIdx].volume_slide_step.paramx          = paramx;
    dbgLog[*logIdx].volume_slide_step.paramy          = paramy;

    if (++(*logIdx) == 1000)
    {
        *logIdx = 0;
    }
}

static void sslog(uint16_t channel, uint16_t previous_speed, uint16_t change_param, uint16_t new_speed)
{
    dbgLog[*logIdx].event                            = EVENT_SONG_SPEED;
    dbgLog[*logIdx].pattern                          = pattern;
    dbgLog[*logIdx].patternPos                       = patternPos;
    dbgLog[*logIdx].tick                             = timerCounter;
    dbgLog[*logIdx].song_speed_change.channel        = channel;
    dbgLog[*logIdx].song_speed_change.previous_speed = previous_speed;
    dbgLog[*logIdx].song_speed_change.change_param   = change_param;
    dbgLog[*logIdx].song_speed_change.new_speed      = new_speed;

    if (++(*logIdx) == 1000)
    {
        *logIdx = 0;
    }
}

static void dslog(uint16_t channel, uint8_t delayTicks)
{
    dbgLog[*logIdx].event                        = EVENT_DELAY_SAMPLE;
    dbgLog[*logIdx].pattern                      = pattern;
    dbgLog[*logIdx].patternPos                   = patternPos;
    dbgLog[*logIdx].tick                         = timerCounter;
    dbgLog[*logIdx].delay_cut_sample.channel     = channel;
    dbgLog[*logIdx].delay_cut_sample.delay_ticks = delayTicks;

    if (++(*logIdx) == 1000)
    {
        logIdx = 0;
    }
}

static void cslog(uint16_t channel, uint8_t delayTicks)
{
    dbgLog[*logIdx].event                        = EVENT_CUT_SAMPLE;
    dbgLog[*logIdx].pattern                      = pattern;
    dbgLog[*logIdx].patternPos                   = patternPos;
    dbgLog[*logIdx].tick                         = timerCounter;
    dbgLog[*logIdx].delay_cut_sample.channel     = channel;
    dbgLog[*logIdx].delay_cut_sample.delay_ticks = delayTicks;

    if (++(*logIdx) == 1000)
    {
        logIdx = 0;
    }
}

static void dtlog(uint16_t channel)
{
    dbgLog[*logIdx].event                   = EVENT_DELAY_COMMAND_TRIGGERED;
    dbgLog[*logIdx].pattern                 = pattern;
    dbgLog[*logIdx].patternPos              = patternPos;
    dbgLog[*logIdx].tick                    = timerCounter;
    dbgLog[*logIdx].delay_triggered.channel = channel;

    if (++(*logIdx) == 1000)
    {
        logIdx = 0;
    }
}

static void ltlog(uint16_t channel, uint16_t loop_start, uint16_t loop_length)
{
    dbgLog[*logIdx].event                      = EVENT_LOOP_TRIGGERED;
    dbgLog[*logIdx].pattern                    = pattern;
    dbgLog[*logIdx].patternPos                 = patternPos;
    dbgLog[*logIdx].tick                       = timerCounter;
    dbgLog[*logIdx].loop_triggered.channel     = channel;
    dbgLog[*logIdx].loop_triggered.loop_start  = loop_start;
    dbgLog[*logIdx].loop_triggered.loop_length = loop_length;

    if (++(*logIdx) == 1000)
    {
        logIdx = 0;
    }
}

static void dplog(uint16_t channel, uint8_t delay_divisions)
{
    dbgLog[*logIdx].event                         = EVENT_DELAY_PATTERN;
    dbgLog[*logIdx].pattern                       = pattern;
    dbgLog[*logIdx].patternPos                    = patternPos;
    dbgLog[*logIdx].tick                          = timerCounter;
    dbgLog[*logIdx].delay_pattern.channel         = channel;
    dbgLog[*logIdx].delay_pattern.delay_divisions = delay_divisions;

    if (++(*logIdx) == 1000)
    {
        logIdx = 0;
    }
}

#if LOG_PERSIST
void ptmodClearLog()
{
    *logIdx = 0;
}

void ptmodPrintLastlog()
{
    if (*logIdx > 2)
    {
        if (dbgLog[0].event >= EVENT_LOAD_SAMPLE && dbgLog[0].event <= EVENT_LOOP_TRIGGERED)
        {
            if (dbgLog[1].event >= EVENT_LOAD_SAMPLE && dbgLog[2].event <= EVENT_LOOP_TRIGGERED)
            {
                if (dbgLog[2].event >= EVENT_LOAD_SAMPLE && dbgLog[2].event <= EVENT_LOOP_TRIGGERED)
                {
                    dprintf("Found possible lastrun log of %d entries - printing\n", *logIdx);
                    ptmodPrintlog();
                }
            }
        }
    }

    *logIdx = 0;
}
#endif

void ptmodPrintlog()
{
    for (int i = 0; i < *logIdx; i++)
    {
        if (dbgLog[i].event == EVENT_LOAD_SAMPLE)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: Loaded sample 0x%08lx chunk [%d-%d:%d] to addr 0x%04x\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].load_sample.channel,
                    dbgLog[i].load_sample.sample_addr,
                    dbgLog[i].load_sample.chunk_start,
                    dbgLog[i].load_sample.chunk_end,
                    dbgLog[i].load_sample.loaded_size,
                    dbgLog[i].load_sample.addr);
        }
        else if (dbgLog[i].event == EVENT_VOLUME_SLIDE)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: volume slide from %d (param: 0x%02x [x = 0x%02x; y = 0x%02x])\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].volume_slide.channel,
                    dbgLog[i].volume_slide.previous_volume,
                    dbgLog[i].volume_slide.change_param,
                    dbgLog[i].volume_slide.paramx,
                    dbgLog[i].volume_slide.paramy);
        }
        else if (dbgLog[i].event == EVENT_VOLUME_SLIDE_STEP)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: volume slide step from %d (paramx: 0x%02x : paramy: 0x%02x)\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].volume_slide_step.channel,
                    dbgLog[i].volume_slide_step.previous_volume,
                    dbgLog[i].volume_slide_step.paramx,
                    dbgLog[i].volume_slide_step.paramy);
        }
        else if (dbgLog[i].event == EVENT_SONG_SPEED)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: Song speed change from %d to %d (param: 0x%02x)\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].song_speed_change.channel,
                    dbgLog[i].song_speed_change.previous_speed,
                    dbgLog[i].song_speed_change.new_speed,
                    dbgLog[i].song_speed_change.change_param);
        }
        else if (dbgLog[i].event == EVENT_VOLUME_CHANGE)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: volume changed to %d\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].volume_change.channel,
                    dbgLog[i].volume_change.volume);
        }
        else if (dbgLog[i].event == EVENT_DELAY_SAMPLE)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: delayed sample by %d ticks\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].delay_cut_sample.channel,
                    dbgLog[i].delay_cut_sample.delay_ticks);
        }
        else if (dbgLog[i].event == EVENT_CUT_SAMPLE)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: cut sample after %d ticks\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].delay_cut_sample.channel,
                    dbgLog[i].delay_cut_sample.delay_ticks);
        }
        else if (dbgLog[i].event == EVENT_DELAY_COMMAND_TRIGGERED)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: delay expired; command triggered\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].delay_triggered.channel);
        }
        else if (dbgLog[i].event == EVENT_LOOP_TRIGGERED)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: loop triggered (start: 0x%04x ; len: 0x%04x)\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].loop_triggered.channel,
                    dbgLog[i].loop_triggered.loop_start,
                    dbgLog[i].loop_triggered.loop_length);
        }
        else if (dbgLog[i].event == EVENT_DELAY_PATTERN)
        {
            dprintf("[%02x:%02x::0x%02x]: Channel %d: delay pattern by %d division(s)\n",
                    dbgLog[i].pattern,
                    dbgLog[i].patternPos,
                    dbgLog[i].tick,
                    dbgLog[i].delay_pattern.channel,
                    dbgLog[i].delay_pattern.delay_divisions);
        }
    }

    *logIdx = 0;
}
#else
#define sllog(...)
#define vclog(...)
#define vslog(...)
#define vsslog(...)
#define sslog(...)
#define cslog(...)
#define dslog(...)
#define dtlog(...)
#define ltlog(...)
#define dplog(...)
#define printlog()
#endif

#if ASM_SAMPLE_LOAD
#define load_next_chunk load_next_chunk_asm
extern bool load_next_chunk_asm(Channel * channel, uint16_t * outActual);
#else
/* Returns true if we loaded the last chunk */
static inline uint16_t load_sample_chunk(uint8_t          xosera_channel,
                                         PtMemorySample * sample,
                                         uint16_t         addr,
                                         uint16_t         chunk_start,
                                         uint16_t         chunk_len)
{
    xv_prep();

    uint16_t sample_len = sample->length;

    if (chunk_start > sample_len)
    {
        return 0;
    }

    uint16_t chunk_end = chunk_start + chunk_len;

    if (chunk_end > sample_len)
    {
        chunk_end = sample_len;
    }

    uint16_t result = chunk_end - chunk_start;
    if (result == 0)
    {
        return 0;
    }

    xm_setw(WR_XADDR, addr);

    sllog(xosera_channel, addr, chunk_start, chunk_end, result, (pointer_t)sample);

    for (uint16_t i = chunk_start; i < chunk_end; i++)
    {
        if (i != 0)
        {        // skip first word
            xm_setw(XDATA, sample->data[i]);
        }
    }

#if !LOG
    // Hack to silence GCC warning...
    (void)(xosera_channel);
#endif

    return result;
}

static inline uint16_t load_next_chunk(Channel * channel)
{
    if (channel->next_chunk_start >= channel->current_sample->length)
    {
        if (channel->current_sample->sample->repeat_length > 1)
        {
            ltlog(channel->xosera_channel,
                  channel->current_sample->sample->repeat_point,
                  channel->current_sample->sample->repeat_length);
            if (channel->current_sample->sample->repeat_point == 0)
            {
                channel->next_chunk_start = 1;        // skip first word!
            }
            else
            {
                channel->next_chunk_start = channel->current_sample->sample->repeat_point;
            }
        }
        else
        {
            return 0;
        }
    }

    uint16_t result = load_sample_chunk(channel->xosera_channel,
                                        channel->current_sample,
                                        channel->next_buffer_start,
                                        channel->next_chunk_start,
                                        channel->buffer_size);

    channel->next_buffer_start =
        channel->next_buffer_start == channel->buffer_b_addr ? channel->buffer_a_addr : channel->buffer_b_addr;

    channel->next_chunk_start = channel->next_chunk_start + result;

    return result;
}
#endif        // ASM_SAMPLE_LOAD

static void start_channel_sample(Channel * channel, PtMemorySample * sample, uint16_t period)
{
    channel->current_sample = sample;
    channel->current_volume = sample->sample->volume;

    if (period)
    {
        // Support retrigger / effect change with continued note
        channel->period = period;
    }

    channel->next_chunk_start = 1;        // First word is loop info, skip it!
}

static void init_channel(Channel *        channel,
                         PtMemorySample * sample,
                         uint16_t         period,
                         uint16_t         buffer_a,
                         uint16_t         buffer_b,
                         uint16_t         buffer_size,
                         uint8_t          xosera_channel)
{
    channel->next_buffer_start     = buffer_a;
    channel->buffer_a_addr         = buffer_a;
    channel->buffer_b_addr         = buffer_b;
    channel->buffer_size           = buffer_size;
    channel->xosera_channel        = xosera_channel;
    channel->current_effect.active = false;
    channel->current_sample        = sample;
    channel->current_volume        = 0x40;
    channel->period                = period;
    channel->next_chunk_start      = 1;
}

static inline uint16_t makeStereoVolume(uint8_t volume)
{
    return volume << 9 | volume << 1;
}

static inline void xosera_set_start_length(uint8_t xosera_channel, uint16_t start, uint16_t len)
{
    xv_prep();

    switch (xosera_channel)
    {
        case 0:
            xreg_setw(AUD0_LENGTH, len);
            xreg_setw(AUD0_START, start);
            break;
        case 1:
            xreg_setw(AUD1_LENGTH, len);
            xreg_setw(AUD1_START, start);
            break;
        case 2:
            xreg_setw(AUD2_LENGTH, len);
            xreg_setw(AUD2_START, start);
            break;
        case 3:
            xreg_setw(AUD3_LENGTH, len);
            xreg_setw(AUD3_START, start);
            break;
    }
}

static inline void xosera_set_period_start_length(uint8_t xosera_channel, uint16_t period, uint16_t start, uint16_t len)
{
    xv_prep();

    switch (xosera_channel)
    {
        case 0:
            xreg_setw(AUD0_PERIOD, period);
            xreg_setw(AUD0_LENGTH, len);
            xreg_setw(AUD0_START, start);
            break;
        case 1:
            xreg_setw(AUD1_PERIOD, period);
            xreg_setw(AUD1_LENGTH, len);
            xreg_setw(AUD1_START, start);
            break;
        case 2:
            xreg_setw(AUD2_PERIOD, period);
            xreg_setw(AUD2_LENGTH, len);
            xreg_setw(AUD2_START, start);
            break;
        case 3:
            xreg_setw(AUD3_PERIOD, period);
            xreg_setw(AUD3_LENGTH, len);
            xreg_setw(AUD3_START, start);
            break;
    }
}

static inline void xosera_set_period_vol_start_length(uint8_t  xosera_channel,
                                                      uint16_t period,
                                                      uint16_t vol,
                                                      uint16_t start,
                                                      uint16_t len)
{
    xv_prep();

    switch (xosera_channel)
    {
        case 0:
            xreg_setw(AUD0_VOL, vol);
            xreg_setw(AUD0_PERIOD, period);
            xreg_setw(AUD0_LENGTH, len);
            xreg_setw(AUD0_START, start);
            break;
        case 1:
            xreg_setw(AUD1_VOL, vol);
            xreg_setw(AUD1_PERIOD, period);
            xreg_setw(AUD1_LENGTH, len);
            xreg_setw(AUD1_START, start);
            break;
        case 2:
            xreg_setw(AUD2_VOL, vol);
            xreg_setw(AUD2_PERIOD, period);
            xreg_setw(AUD2_LENGTH, len);
            xreg_setw(AUD2_START, start);
            break;
        case 3:
            xreg_setw(AUD3_VOL, vol);
            xreg_setw(AUD3_PERIOD, period);
            xreg_setw(AUD3_LENGTH, len);
            xreg_setw(AUD3_START, start);
            break;
    }
}

static inline void xosera_set_vol(uint8_t xosera_channel, uint16_t vol)
{
    xv_prep();

    switch (xosera_channel)
    {
        case 0:
            xreg_setw(AUD0_VOL, vol);
            break;
        case 1:
            xreg_setw(AUD1_VOL, vol);
            break;
        case 2:
            xreg_setw(AUD2_VOL, vol);
            break;
        case 3:
            xreg_setw(AUD3_VOL, vol);
            break;
    }
}

static inline void set_channel_volume(Channel * channel, uint16_t volume)
{
    if (volume > 0x40)
    {
        volume = 0x40;
    }

    channel->current_volume = volume;
    xosera_set_vol(channel->xosera_channel, makeStereoVolume(volume));
}

static inline void start_silence(Channel * channel)
{
    xosera_set_period_vol_start_length(channel->xosera_channel, 50000, 0x0, BUFFER_SILENCE, BUFFER_MEM);
    channel->current_volume = 0;
}

static inline void xosera_channel_ready(Channel * channel)
{
    if (channel->current_sample == &silence)
    {
        start_silence(channel);
#if !SILENCE
    }
    else
    {
        uint16_t actualLoaded = load_next_chunk(channel);

        if (actualLoaded == 0)
        {
            start_silence(channel);
        }
        else
        {
            xosera_set_period_start_length(
                channel->xosera_channel,
                channel->period,
                channel->next_buffer_start == channel->buffer_b_addr ? channel->buffer_a_addr : channel->buffer_b_addr,
                (actualLoaded - 1) | BUFFER_MEM);
        }
#else
        // just a hack to stop GCC warning...
        force = !force;
#endif
    }
}

static void xosera_trigger_channel(Channel * channel)
{
    if (channel->current_sample == &silence)
    {
        start_silence(channel);
#if !SILENCE
    }
    else
    {
        uint16_t actualLoaded = load_next_chunk(channel);

        if (actualLoaded == 0)
        {
            start_silence(channel);
        }
        else
        {
            xosera_set_period_vol_start_length(
                channel->xosera_channel,
                channel->period | 0x8000,
                makeStereoVolume(channel->current_volume),
                channel->next_buffer_start == channel->buffer_b_addr ? channel->buffer_a_addr : channel->buffer_b_addr,
                (actualLoaded - 1) | BUFFER_MEM);
        }
#else
        // just a hack to stop GCC warning...
        force = !force;
#endif
    }
}

/*
 * Handle immediate effects.
 *
 * Note: Delay sample isn't handled here as it needs us to conditionally
 * trigger, so that's handled directly in the step function.
 */
static inline void handleEffect(uint16_t effect, Channel * channel)
{
    if ((effect & 0xF00) == 0xB00)
    {
        // Position jump (at end of division) - just set position and pattern, it will get picked up
        // automagically at the beginning of the next division...
        position   = effect & 0x00FF;
        pattern    = mod->positions[position];
        patternPos = -1;
    }
    else if ((effect & 0xF00) == 0xD00)
    {
        // Pattern break (at end of division) - just set position, pattern and patternPos, it will
        // get picked up automagically at the beginning of the next division...

        if (++position >= mod->song_length)
        {
            position = 0;
        }

        pattern = mod->positions[position];

        // -1 as We increment this immediately on starting the division!
        patternPos = (((effect & 0x00F0) >> 4) * 10) + (effect & 0x000F) - 1;
    }
    else if ((effect & 0xF00) == 0xC00)
    {
        // Volume
        uint16_t volume = effect & 0x00FF;

        vclog(channel->xosera_channel, volume);
        set_channel_volume(channel, volume);
    }
    else if ((effect & 0xF00) == 0xA00)
    {
        // Volume slide - temporal command
        Effect * current_effect = &channel->current_effect;
        current_effect->command = 0xA;
        current_effect->paramx  = (effect & 0xF0) >> 4;
        current_effect->paramy  = (effect & 0x0F);
        current_effect->active  = true;

        vslog(channel->xosera_channel,
              channel->current_volume,
              (effect & 0xFF),
              current_effect->paramx,
              current_effect->paramy);
    }
    else if ((effect & 0xFF0) == 0xEA0)
    {
        // Fine "slide" up (not actually a slide, so non-temporal)
        uint16_t volume = channel->current_volume + (effect & 0x000F);

        if (volume > 0x40)
        {
            volume = 0x40;
        }

        vclog(channel->xosera_channel, volume);
        set_channel_volume(channel, volume);
    }
    else if ((effect & 0xFF0) == 0xEB0)
    {
        // Fine "slide" down (not actually a slide, so non-temporal)
        int16_t volume = channel->current_volume - (effect & 0x000F);

        if (volume < 0)
        {
            volume = 0;
        }

        vclog(channel->xosera_channel, volume);
        set_channel_volume(channel, volume);
    }
    else if ((effect & 0xFF0) == 0xEC0)
    {
        // Cut sample - temporal command
        Effect * current_effect = &channel->current_effect;
        current_effect->command = 0xE;
        current_effect->paramx  = 0xC;
        current_effect->paramy  = (effect & 0x0F);
        current_effect->active  = true;

        cslog(channel->xosera_channel, (effect & 0x0F));
    }
    else if ((effect & 0xFF0) == 0xEE0)
    {
        // Pattern delay - temporal command
        uint8_t  delay_divisions;
        Effect * current_effect = &channel->current_effect;
        current_effect->command = 0xE;
        current_effect->paramx  = 0xE;
        current_effect->paramy = delay_divisions = (effect & 0x0F);
        current_effect->active                   = true;
        current_effect->data0                    = delay_divisions * StepFrames;

        dplog(channel->xosera_channel, (effect & 0x0F));
    }
    else if ((effect & 0xF00) == 0xF00)
    {
        // Speed - TODO Only Ticks/Div currently, also needs BPM!
        sslog(channel->xosera_channel, StepFrames, effect & 0x00FF, effect & 0x00FF);
        StepFrames = effect & 0x00FF;
    }
}

static inline void handleTemporalEffects(Channel * channel)
{
    Effect * effect = &channel->current_effect;

    if (channel->current_effect.active)
    {
        switch (effect->command)
        {
            case 0xA:;
                // Volume slide
                int16_t volume = channel->current_volume;
                vsslog(channel->xosera_channel, volume, effect->paramx, effect->paramy);
                if (effect->paramx)
                {
                    // slide up
                    volume = volume + effect->paramx;
                    if (volume >= 0x40)
                    {
                        volume         = 0x40;
                        effect->active = false;
                    }
                }
                else if (effect->paramy)
                {
                    // slide down
                    volume = volume - effect->paramy;

                    if (volume <= 0)
                    {
                        volume         = 0;
                        effect->active = false;
                    }
                }

                set_channel_volume(channel, volume);
                break;

            case 0xE:
                // Extended command
                switch (effect->paramx)
                {
                    case 0xC:
                        // Cut sample
                        if (effect->paramy-- == 0)
                        {
                            // Expired - cut sample volume
                            set_channel_volume(channel, 0);
                            dtlog(channel->xosera_channel);
                            effect->active = false;
                        }

                        break;

                    case 0xD:
                        // Delay sample
                        if (effect->paramy-- == 0)
                        {
                            // Expired - play sample
                            uint32_t note         = channel->current_effect.data1;
                            uint8_t  sampleNumber = PtSampleNumber(note);

                            start_channel_sample(channel, &samples[sampleNumber - 1], XOSERA_FREQ[PtNotePeriod(note)]);
                            xosera_trigger_channel(channel);
                            dtlog(channel->xosera_channel);
                            effect->active = false;
                        }

                        break;

                    case 0xE:
                        // Delay pattern
                        if (--effect->data0 == 0)
                        {
                            // Expired - Deactivate effect
                            effect->active = false;
                        }

                        // Just add one to timer to stretch this division...
                        timerCounter += 1;

                        break;
                }

                break;
        }
    }
}

/* Called by interrupt handler (on TIMER_INTR) */
void ptmodTimeStep()
{
    if (--timerCounter == 0)
    {
        patternPos++;

        if (patternPos == 64)
        {
            if (++position >= mod->song_length)
            {
                position = 0;
            }

            pattern    = mod->positions[position];
            patternPos = 0;
        }

        PtPatternRow row = patterns[pattern].rows[patternPos];

        for (int mod_channel = 0; mod_channel < 4; mod_channel++)
        {
            uint32_t note         = row.channel_notes[mod_channel];
            uint8_t  sampleNumber = PtSampleNumber(note);

            Channel * channel = mod_channel == 0   ? &channel0
                                : mod_channel == 1 ? &channel1
                                : mod_channel == 2 ? &channel2
                                                   : &channel3;

            // TODO cancelling effect every division - right thing to do?
            channel->current_effect.active = false;

            uint16_t effect = PtEffect(note);

            if (sampleNumber > 0)
            {
                uint16_t command = (effect & 0x0F00);
                uint16_t paramx  = (effect & 0x00F0);

                if (command == 0x0E00 && paramx == 0x00D0)
                {
                    // has a delayed sample command - deal with that
                    uint16_t delayTicks = (effect & 0x000F);

                    channel->current_effect.active  = true;
                    channel->current_effect.command = 0xE;
                    channel->current_effect.paramx  = 0xD;
                    channel->current_effect.paramy  = delayTicks;
                    channel->current_effect.data1   = note;

                    dslog(channel->xosera_channel, delayTicks);
                }
                else
                {
                    start_channel_sample(channel, &samples[sampleNumber - 1], XOSERA_FREQ[PtNotePeriod(note)]);

                    // TODO if period was zero, do we need to retrigger - check how this is used by mods in the wild...
                    xosera_trigger_channel(channel);
                }
            }

            // Handle these *after* loading so they can take effect immediately if a sample is also played on this
            // division...
            handleEffect(effect, channel);
        }

        // Set up timer for next step
        timerCounter = StepFrames;
    }

    // Handle these *after* the division is processed, so we can start any temporal
    // effects on the same tick as the division itself...
    handleTemporalEffects(&channel0);
    handleTemporalEffects(&channel1);
    handleTemporalEffects(&channel2);
    handleTemporalEffects(&channel3);
}

/* Called by interrupt handler (on AUDIO_INTR) */
uint8_t ptmodServiceSamples(uint8_t channel_mask)
{
    uint8_t serviced = 0;

    if (channel_mask & 0x08)
    {
        xosera_channel_ready(&channel3);
        serviced |= 0x08;
    }
    if (channel_mask & 0x04)
    {
        xosera_channel_ready(&channel2);
        serviced |= 0x04;
    }
    if (channel_mask & 0x02)
    {
        xosera_channel_ready(&channel1);
        serviced |= 0x02;
    }
    if (channel_mask & 0x01)
    {
        xosera_channel_ready(&channel0);
        serviced |= 0x01;
    }

    return serviced;
}

bool ptmodPlay(PtMod * the_mod, VoidVoidCb cb_install_intr)
{
    mod = the_mod;
    PtFixLoops(mod);
    PtPopulateMemorySamples(mod, samples);
    patterns = PtPatternData(mod);

#if LOG
    for (int i = 0; i < 31; i++)
    {
        dprintf("Sample %d - %.22s [0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
                i,
                mod->samples[i].sample_name,
                (samples[i].data[0] & 0xF0) >> 4,
                samples[i].data[0] & 0x0F,
                (samples[i].data[1] & 0xF0) >> 4,
                samples[i].data[1] & 0x0F,
                (samples[i].data[2] & 0xF0) >> 4);
    }
#endif

    // Prime the pump
    pattern      = mod->positions[0];
    StepFrames   = 6;
    timerCounter = 1; /* immediate start */
    patternPos   = -1;
    position     = 0;

    init_channel(&channel0, &silence, 50000, BUFFER_A0, BUFFER_B0, BUFFER_LEN, 0);
    init_channel(&channel1, &silence, 50000, BUFFER_A1, BUFFER_B1, BUFFER_LEN, 1);
    init_channel(&channel2, &silence, 50000, BUFFER_A2, BUFFER_B2, BUFFER_LEN, 2);
    init_channel(&channel3, &silence, 50000, BUFFER_A3, BUFFER_B3, BUFFER_LEN, 3);
    cb_install_intr();

    return 1;
}
