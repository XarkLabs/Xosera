/**
 * @file xosera_mod_play.h
 * @author Ross Bamford (roscopeco AT gmail DOT com)
 * @brief ProTracker MOD player for Xosera
 * @version 0.1
 * @date 2022-06-30
 *
 * @copyright Copyright (c) 2022 Ross Bamford & Contributiors
 *
 */

#ifndef __XOSERA_MOD_PLAY_H__
#define __XOSERA_MOD_PLAY_H__

#include "pt_mod.h"
#include <stdbool.h>
#include <stdint.h>

/* Options you can set */
#ifndef DEFAULT_SPEED
#define DEFAULT_SPEED 6
#endif

/* Other settable options (all default to off) */
#ifndef NTSC
#define NTSC 0        // Set 1 to use NTSC timings (experimental!)
#endif
#ifndef SILENCE
#define SILENCE 0        // Set 1 to silence audio output
#endif
#ifndef LOG
#define LOG 0        // Define to enable logging (dumped at exit)
#endif

/* Xosera memory layout options */
#ifndef BUFFER_MEM
#define BUFFER_MEM 0x8000        // TILE memory for audio buffers
#endif
#ifndef BUFFER_LEN
#define BUFFER_LEN 0x0040        // 64W (128 8-bit samples)
#endif
#ifndef BUFFER_SILENCE
#define BUFFER_SILENCE XR_TILE_ADDR        // start of default font (=0x0000 for by default)
#endif
#ifndef BUFFER_A0
#define BUFFER_A0 XR_TILE_ADDR + 0x0800        // after default font
#endif
#ifndef BUFFER_B0
#define BUFFER_B0 BUFFER_A0 + BUFFER_LEN        // after first buffer
#endif
#ifndef BUFFER_A1
#define BUFFER_A1 BUFFER_B0 + BUFFER_LEN        // after default font
#endif
#ifndef BUFFER_B1
#define BUFFER_B1 BUFFER_A1 + BUFFER_LEN        // after first buffer
#endif
#ifndef BUFFER_A2
#define BUFFER_A2 BUFFER_B1 + BUFFER_LEN        // after default font
#endif
#ifndef BUFFER_B2
#define BUFFER_B2 BUFFER_A2 + BUFFER_LEN        // after first buffer
#endif
#ifndef BUFFER_A3
#define BUFFER_A3 BUFFER_B2 + BUFFER_LEN        // after default font
#endif
#ifndef BUFFER_B3
#define BUFFER_B3 BUFFER_A3 + BUFFER_LEN        // after first buffer
#endif

typedef void (*VoidVoidCb)(void);

/**
 * @brief Start playback of the given module.
 *
 * @param the_mod           The PtMod structure representing the song
 * @param cb_install_intr   The callback that will install the interrupt handler
 * @param cb_done           The callback that will be called at the end of the song
 *
 * @return bool             True on success, false otherwise
 */
bool ptmodPlay(PtMod * the_mod, VoidVoidCb cb_install_intr);

/**
 * @brief Called by interrupt handler (on TIMER_INTR)
 *
 */
void ptmodTimeStep(void);

/**
 * @brief Called by interrupt handler (on AUDIO_INTR)
 *
 * @param channel_mask
 */
uint8_t ptmodServiceSamples(uint8_t channel_mask);

#if LOG
void ptmodPrintlog(void);
#if LOG_PERSIST
void ptmodClearLog();
void ptmodPrintLastlog();
#else
#define ptmodClearLog(...)
#define ptmodPrintLastlog(...)
#endif
#else
#define ptmodPrintlog(...)
#define ptmodClearLog(...)
#define ptmodPrintLastlog(...)
#endif

#endif        //__XOSERA_MOD_PLAY_H__
