/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
 * ------------------------------------------------------------
 * Copyright (c) 2021-2022 Xark
 * MIT License
 *
 * Xosera rosco_m68k C register definition header file
 * ------------------------------------------------------------
 */

// See: https://github.com/XarkLabs/Xosera/blob/master/REFERENCE.md

#if !defined(XOSERA_M68K_DEFS_H)
#define XOSERA_M68K_DEFS_H

#define XM_BASEADDR 0xf80060        // Xosera rosco_m68k 68010 register base address

// Xosera XR Memory Regions (size in 16-bit words)
#define XR_CONFIG_REGS  0x0000        // 0x0000-0x000F 16 config/ctrl registers
#define XR_PA_REGS      0x0010        // 0x0010-0x0017 8 playfield A video registers
#define XR_PB_REGS      0x0018        // 0x0018-0x001F 8 playfield B video registers
#define XR_AUDIO_REGS   0x0020        // 0x0020-0x002F 16 audio playback registers      // TODO: audio
#define XR_BLIT_REGS    0x0040        // 0x0040-0x004B 10 blitter registers
#define XR_TILE_ADDR    0x4000        // (R/W) 0x4000-0x53FF tile glyph/tile map memory
#define XR_TILE_SIZE    0x1400        //                     5120 x 16-bit tile glyph/tile map memory
#define XR_COLOR_ADDR   0x8000        // (R/W) 0x8000-0x81FF 2 x A & B color lookup memory
#define XR_COLOR_SIZE   0x0200        //                     2 x 256 x 16-bit words  (0xARGB)
#define XR_COLOR_A_ADDR 0x8000        // (R/W) 0x8000-0x80FF A 256 entry color lookup memory
#define XR_COLOR_A_SIZE 0x0100        //                     256 x 16-bit words (0xARGB)
#define XR_COLOR_B_ADDR 0x8100        // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
#define XR_COLOR_B_SIZE 0x0100        //                     256 x 16-bit words (0xARGB)
#define XR_POINTER_ADDR 0x8200        // (-/W) 0x8200-0x82FF 256 word 32x32 4-bpp pointer image
#define XR_POINTER_SIZE 0x0100        //                     256 x 16-bit words (4-bit pixels)
#define XR_COPPER_ADDR  0xC000        // (R/W) 0xC000-0xC5FF copper memory (16-bit words)
#define XR_COPPER_SIZE  0x0600        //                     1024+512 x 16-bit copper memory words

// Xosera version info put in COPPER memory after FPGA reconfigure
#define XV_INFO_BYTES 256        // 256 bytes total for "struct _xosera_info" (last 128 words in copper memory)
#define XV_INFO_WORDS 128        // 128 16-bit words (last 128 words in copper memory)
#define XV_INFO_ADDR  (XR_COPPER_ADDR + XR_COPPER_SIZE - XV_INFO_WORDS)

// Macros to make bit-fields easier (works similar to Verilog "+:" operator, e.g., word[RIGHTMOST_BIT +: BIT_WIDTH])
// encode value into bit-field for register
#define XB_(v, right_bit, bit_width) ((((uint16_t)(v)) & ((1 << (bit_width)) - 1)) << (right_bit))
// decode bit-field from register into value
#define XV_(v, right_bit, bit_width) ((((uint16_t)(v)) >> (right_bit)) & ((1 << (bit_width)) - 1))

// Xosera Main Registers (XM Registers, directly CPU accessable)
// NOTE: Main register numbers are multiplied by 4 for rosco_m68k, because of even byte 6800 8-bit addressing plus
// 16-bit registers
#define XM_SYS_CTRL 0x00        // (R /W+) [15:8] status bits, write setup PIXEL_X/Y & options, [7:0] write masking
#define XM_INT_CTRL 0x04        // (R /W+) FPGA config, interrupt status/control
#define XM_TIMER    0x08        // (R /W+) read 1/10th millisecond timer, write 8-bit interval timer count
#define XM_RD_XADDR 0x0C        // (R /W+) XR register/address for XM_XDATA read access
#define XM_WR_XADDR 0x10        // (R /W ) XR register/address for XM_XDATA write access
#define XM_XDATA    0x14        // (R /W+) read/write XR register/memory at XM_RD_XADDR/XM_WR_XADDR
#define XM_RD_INCR  0x18        // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
#define XM_RD_ADDR  0x1C        // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
#define XM_WR_INCR  0x20        // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
#define XM_WR_ADDR  0x24        // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
#define XM_DATA     0x28        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
#define XM_DATA_2   0x2C        // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
#define XM_PIXEL_X  0x30        // (- /W+) pixel X coordinate / setup pixel base address
#define XM_PIXEL_Y  0x34        // (- /W+) pixel Y coordinate / setup pixel line width
#define XM_UART     0x38        // (R+/W+) optional debug USB UART communication
#define XM_FEATURE  0x3C        // (R /W+) Xosera feature flags, write sets pixel base, width to X, Y and mask mode

// XR Extended Register / Region (accessed via XM_RD_XADDR/XM_WR_XADDR and XM_XDATA)
//  Video Config and Copper XR Registers
#define XR_VID_CTRL  0x00        // (R /W) display control and border color index
#define XR_COPP_CTRL 0x01        // (R /W) display synchronized coprocessor control
#define XR_AUD_CTRL  0x02        // (- /-) TODO: audio channel control
#define XR_SCANLINE  0x03        // (R /W) read scanline (incl. offscreen), write signal video interrupt
#define XR_VID_LEFT  0x04        // (R /W) left edge of active display window (typically 0)
#define XR_VID_RIGHT 0x05        // (R /W) right edge of active display window +1 (typically 640 or 848)
#define XR_POINTER_H 0x06        // (- /W) pointer sprite raw H position
#define XR_POINTER_V 0x07        // (- /W) pointer sprite raw V position / pointer color select
#define XR_UNUSED_08 0x08        // (- /-) unused XR 08
#define XR_UNUSED_09 0x09        // (- /-) unused XR 09
#define XR_UNUSED_0A 0x0A        // (- /-) unused XR 0A
#define XR_UNUSED_0B 0x0B        // (- /-) unused XR 0B
#define XR_UNUSED_0C 0x0C        // (- /-) unused XR 0C
#define XR_UNUSED_0D 0x0D        // (- /-) unused XR 0D
#define XR_UNUSED_0E 0x0E        // (- /-) unused XR 0E
#define XR_UNUSED_0F 0x0F        // (- /-) unused XR 0F
// Playfield A Control XR Registers
#define XR_PA_GFX_CTRL  0x10        // (R /W ) playfield A graphics control
#define XR_PA_TILE_CTRL 0x11        // (R /W ) playfield A tile control
#define XR_PA_DISP_ADDR 0x12        // (R /W ) playfield A display VRAM start address
#define XR_PA_LINE_LEN  0x13        // (R /W ) playfield A display line width in words
#define XR_PA_HV_FSCALE 0x14        // (R /W ) playfield A horizontal and vertical fractional scale
#define XR_PA_HV_SCROLL 0x15        // (R /W ) playfield A horizontal and vertical fine scroll
#define XR_PA_LINE_ADDR 0x16        // (- /W ) playfield A scanline start address (loaded at start of line)
#define XR_PA_UNUSED_17 0x17        // (- /- )
// Playfield B Control XR Registers
#define XR_PB_GFX_CTRL  0x18        // (R /W ) playfield B graphics control
#define XR_PB_TILE_CTRL 0x19        // (R /W ) playfield B tile control
#define XR_PB_DISP_ADDR 0x1A        // (R /W ) playfield B display VRAM start address
#define XR_PB_LINE_LEN  0x1B        // (R /W ) playfield B display line width in words
#define XR_PB_HV_FSCALE 0x1C        // (R /W ) playfield B horizontal and vertical fractional scale
#define XR_PB_HV_SCROLL 0x1D        // (R /W ) playfield B horizontal and vertical fine scroll
#define XR_PB_LINE_ADDR 0x1E        // (- /W ) playfield B scanline start address (loaded at start of line)
#define XR_PB_UNUSED_1F 0x1F        // (- /- )
// Audio Registers
#define XR_AUD0_VOL    0x20        // (- /W ) audio channel 0 8-bit L+R volume (0x80 = 100%)
#define XR_AUD0_PERIOD 0x21        // (- /W+) audio channel 0 15-bit period, bit [15] force restart
#define XR_AUD0_LENGTH 0x22        // (- /W ) audio channel 0 15-bit sample word length-1, bit [15] tile mem
#define XR_AUD0_START  0x23        // (- /W+) audio channel 0 sample start address (vram/tilemem), clear reload
#define XR_AUD1_VOL    0x24        // (- /W ) audio channel 1 8-bit L+R volume (0x80 = 100%)
#define XR_AUD1_PERIOD 0x25        // (- /W+) audio channel 1 15-bit period, bit [15] force restart
#define XR_AUD1_LENGTH 0x26        // (- /W ) audio channel 1 15-bit sample word length-1, bit [15] tile mem
#define XR_AUD1_START  0x27        // (- /W+) audio channel 1 sample start address (vram/tilemem), clear reload
#define XR_AUD2_VOL    0x28        // (- /W ) audio channel 2 8-bit L+R volume (0x80 = 100%)
#define XR_AUD2_PERIOD 0x29        // (- /W+) audio channel 2 15-bit period, bit [15] force restart
#define XR_AUD2_LENGTH 0x2A        // (- /W ) audio channel 2 15-bit sample word length-1, bit [15] tile mem
#define XR_AUD2_START  0x2B        // (- /W+) audio channel 2 sample start address (vram/tilemem), clear reload
#define XR_AUD3_VOL    0x2C        // (- /W ) audio channel 3 8-bit L+R volume (0x80 = 100%)
#define XR_AUD3_PERIOD 0x2D        // (- /W+) audio channel 3 15-bit period, bit [15] force restart
#define XR_AUD3_LENGTH 0x2E        // (- /W ) audio channel 3 15-bit sample word length-1, bit [15] tile mem
#define XR_AUD3_START  0x2F        // (- /W+) audio channel 3 sample start address (vram/tilemem), clear reload
// (Registers 0x30-0x3F reserved)
// Blitter Registers
#define XR_BLIT_CTRL  0x40        // (- /W ) blit control ([15:8]=transp value, [5]=8 bpp, [4]=transp on, [0]=S constant)
#define XR_BLIT_ANDC  0x41        // (- /W ) blit AND-COMPLEMENT constant value
#define XR_BLIT_XOR   0x42        // (- /W ) blit XOR constant value
#define XR_BLIT_MOD_S 0x43        // (- /W ) blit modulo added to S source after each line
#define XR_BLIT_SRC_S 0x44        // (- /W ) blit S source VRAM read address / constant value
#define XR_BLIT_MOD_D 0x45        // (- /W ) blit modulo added to D destination after each line
#define XR_BLIT_DST_D 0x46        // (- /W ) blit D destination VRAM write address
#define XR_BLIT_SHIFT 0x47        // (- /W ) blit first and last word nibble masks and nibble right shift (0-3)
#define XR_BLIT_LINES 0x48        // (- /W ) blit number of lines minus 1, (repeats blit word count after modulo calc)
#define XR_BLIT_WORDS 0x49        // (- /W+) blit word count minus 1 per line (write starts blit operation)
#define XR_UNUSED_4A  0x4A        // (- /- ) unused XR reg
#define XR_UNUSED_4B  0x4B        // (- /- ) unused XR reg
#define XR_UNUSED_4C  0x4C        // (- /- ) unused XR reg
#define XR_UNUSED_4D  0x4D        // (- /- ) unused XR reg
#define XR_UNUSED_4E  0x4E        // (- /- ) unused XR reg
#define XR_UNUSED_4F  0x4F        // (- /- ) unused XR reg

// Xosera register bits and constants

// SYS_CTRL bit numbers NOTE: These are bits in high byte of SYS_CTRL word (for access with fast address register
// indirect with no offset)
#define SYS_CTRL_MEM_WAIT_B    7        // (R /- )  memory read/write operation pending (with contended memory)
#define SYS_CTRL_BLIT_FULL_B   6        // (R /- )  blitter queue is full, do not write new operation to blitter registers
#define SYS_CTRL_BLIT_BUSY_B   5        // (R /- )  blitter is still busy performing an operation (not done)
#define SYS_CTRL_UNUSED_12_B   4        // (R /- )  unused (reads 0)
#define SYS_CTRL_HBLANK_B      3        // (R /- )  video signal is in horizontal blank period
#define SYS_CTRL_VBLANK_B      2        // (R /- )  video signal is in vertical blank period
#define SYS_CTRL_PIX_NO_MASK_B 1        // (R /W )  PIXEL_X/Y won't set WR_MASK (low two bits of PIXEL_X ignored)
#define SYS_CTRL_PIX_8B_MASK_B 0        // (R /W )  PIXEL_X/Y 8-bit pixel mask for WR_MASK
// SYS_CTRL bit flags
#define SYS_CTRL_MEM_WAIT_F    0x80        // (R /- )  memory read/write operation pending (with contended memory)
#define SYS_CTRL_BLIT_FULL_F   0x40        // (R /- )  blitter queue is full (do not write to blitter registers)
#define SYS_CTRL_BLIT_BUSY_F   0x20        // (R /- )  blitter is still busy performing an operation (not done)
#define SYS_CTRL_UNUSED_12_F   0x10        // (R /- )  unused (reads 0)
#define SYS_CTRL_HBLANK_F      0x08        // (R /- )  video signal is in horizontal blank period
#define SYS_CTRL_VBLANK_F      0x04        // (R /- )  video signal is in vertical blank period
#define SYS_CTRL_PIX_NO_MASK_F 0x02        // (R /W )  PIXEL_X/Y won't set WR_MASK (low two bits of PIXEL_X ignored)
#define SYS_CTRL_PIX_8B_MASK_F 0x01        // (R /W )  PIXEL_X/Y 8-bit pixel mask for WR_MASK
// INT_CTRL bit numbers within word
#define INT_CTRL_RECONFIG_B   15        // reconfigure FPGA to config # in bits [9:8] of INT_CTRL
#define INT_CTRL_BLIT_EN_B    14        // blitter ready interrupt mask
#define INT_CTRL_TIMER_EN_B   13        // timer match interrupt mask
#define INT_CTRL_VIDEO_EN_B   12        // v-blank or copper interrupt mask
#define INT_CTRL_AUD3_EN_B    11        // audio channel 3 ready (START addr was loaded)
#define INT_CTRL_AUD2_EN_B    10        // audio channel 2 ready (START addr was loaded)
#define INT_CTRL_AUD1_EN_B    9         // audio channel 1 ready (START addr was loaded)
#define INT_CTRL_AUD0_EN_B    8         // audio channel 0 ready (START addr was loaded)
#define INT_CTRL_UNUSED_7_B   7         // timer match read interrupt (status, write acknowledge)
#define INT_CTRL_BLIT_INTR_B  6         // blitter ready interrupt (read status, write acknowledge)
#define INT_CTRL_TIMER_INTR_B 5         // timer match read interrupt (status, write acknowledge)
#define INT_CTRL_VIDEO_INTR_B 4         // v-blank or copper interrupt (read status, write acknowledge)
#define INT_CTRL_AUD3_INTR_B  3         // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD2_INTR_B  2         // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD1_INTR_B  1         // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD0_INTR_B  0         // audio channel ready interrupt (read status, write acknowledge)
// INT_CTRL bit flag/mask
#define INT_CTRL_RECONFIG_F   0x8000        // reconfigure FPGA to config # in bits [9:8] of INT_CTRL
#define INT_CTRL_BLIT_EN_F    0x4000        // blitter ready interrupt enable
#define INT_CTRL_TIMER_EN_F   0x2000        // timer match interrupt enable
#define INT_CTRL_VIDEO_EN_F   0x1000        // v-blank or copper interrupt enable
#define INT_CTRL_AUD3_EN_F    0x0800        // audio channel 3 interrupt enable
#define INT_CTRL_AUD2_EN_F    0x0400        // audio channel 2 interrupt enable
#define INT_CTRL_AUD1_EN_F    0x0200        // audio channel 1 interrupt enable
#define INT_CTRL_AUD0_EN_F    0x0100        // audio channel 0 interrupt enable
#define INT_CTRL_AUD_ALL_EN_F 0x0F00        // all audio channel interrupts enable
#define INT_CTRL_EN_ALL_F     0x7F00        // enable all interrupts
#define INT_CTRL_UNUSED_7_F   0x0080        // timer match read interrupt (status, write acknowledge)
#define INT_CTRL_BLIT_INTR_F  0x0040        // blitter ready interrupt (read status, write acknowledge)
#define INT_CTRL_TIMER_INTR_F 0x0020        // timer match read interrupt (status, write acknowledge)
#define INT_CTRL_VIDEO_INTR_F 0x0010        // v-blank or copper interrupt (read status, write acknowledge)
#define INT_CTRL_AUD3_INTR_F  0x0008        // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD2_INTR_F  0x0004        // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD1_INTR_F  0x0002        // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD0_INTR_F  0x0001        // audio channel ready interrupt (read status, write acknowledge)
#define INT_CTRL_AUD_ALL_F    0x000F        // all audio channels status/acknowledge
#define INT_CTRL_CLEAR_ALL_F  0x007F        // clear all interrupts
// UART status bit number within even byte of XM_UART
#define UART_RXF_B 7           // UART receive buffer full (data waiting)
#define UART_TXF_B 6           // UART transmit buffer full (busy transmitting data)
#define UART_RXF_F 0x80        // UART receive buffer full (data waiting)
#define UART_TXF_F 0x40        // UART transmit buffer full (busy transmitting data)
// FEATURE bit numbers within word (for fields wider than 1 bit, XB_(xxx_B, xxx_W) macro can be used)
#define FEATURE_MONRES_B  0         // rightmost bit number for 4-bit monitor mode field
#define FEATURE_MONRES_W  4         // bit width for 4-bit monitor mode field
#define FEATURE_COPP_B    4         // bit number indicating presence of COPPER
#define FEATURE_BLIT_B    5         // bit number indicating presence of BLITTER
#define FEATURE_PF_B_B    6         // bit number indicating presence of playfield B (2nd playfield)
#define FEATURE_UART_B    7         // bit number indicating presence of debug UART
#define FEATURE_AUDCHAN_B 8         // rightmost bit number for 4-bit audio channels field
#define FEATURE_AUDCHAN_W 4         // bit width for 4-bit audio channels field
#define FEATURE_CONFIG_B  12        // rightmost bit number for 4-bit FPGA config field
#define FEATURE_CONFIG_W  4         // bit width for 4-bit FPGA config field
// FEATURE flag/mask
#define FEATURE_MONRES_F  0x000F        // bit-mask for 4-bit monitor mode field
#define FEATURE_COPP_F    0x0010        // bit flag indicating presence of COPPER
#define FEATURE_BLIT_F    0x0020        // bit flag indicating presence of BLITTER
#define FEATURE_PF_B_F    0x0040        // bit flag indicating presence of playfield B (2nd playfield)
#define FEATURE_UART_F    0x0080        // bit flag indicating presence of debug UART
#define FEATURE_AUDCHAN_F 0x0F00        // bit-mask for 4-bit audio channels field
#define FEATURE_CONFIG_F  0xF000        // bit-mask for 4-bit config field
// XR_VID_CTRL flag/mask
#define VID_CTRL_SWAP_AB_B 15            // flag to swap colormap used (pf A uses colormap B and vice versa)
#define VID_CTRL_SWAP_AB_W 1             // 1 bit
#define VID_CTRL_SWAP_AB_F 0x8000        // flag to swap colormap used (pf A uses colormap B and vice versa)
#define VID_CTRL_BORDCOL_B 0             // mask for pf A color index
#define VID_CTRL_BORDCOL_W 8             // 8 bit field
#define VID_CTRL_BORDCOL_F 0x00FF        // mask for pf A color index
// XR_COPP_CTRL constants
#define COPP_CTRL_COPP_EN_B 15            // bit number to enable/disable copper
#define COPP_CTRL_COPP_EN_W 1             // 1 bit
#define COPP_CTRL_COPP_EN_F 0x8000        // flag to enable/disable copper
// XR_AUD_CTRL constants
#define AUD_CTRL_AUD_EN_B 0             // bit number to enable/disable audio
#define AUD_CTRL_AUD_EN_W 1             // 1 bit
#define AUD_CTRL_AUD_EN_F 0x0001        // flag to enable/disable audio
// XR_VID_CTRL constants
#define VID_CTRL_SWAP_AB_B 15        // bit number to colormap used (pf A uses colormap B and vice versa)
#define VID_CTRL_BORDCOL_B 0         // rightmost bit number of pf A color index
#define VID_CTRL_BORDCOL_W 8         // bit width for pf A color index
// XR_Px_GFX_CTRL BPP mode constants
#define GFX_BPP_1 0        // 1-bpp (2 colors + selected via fore/back color attribute byte)
#define GFX_BPP_4 1        // 4-bpp (16 colors)
#define GFX_BPP_8 2        // 8-bpp (256 colors)
#define GFX_BPP_X 3        // (reserved)
// XR_Px_GFX_CTRL bit numbers within word)
#define GFX_CTRL_V_REPEAT_B  0
#define GFX_CTRL_V_REPEAT_W  2
#define GFX_CTRL_V_REPEAT_F  0x0003
#define GFX_CTRL_H_REPEAT_B  2
#define GFX_CTRL_H_REPEAT_W  2
#define GFX_CTRL_H_REPEAT_F  0x000C
#define GFX_CTRL_BPP_B       4
#define GFX_CTRL_BPP_W       2
#define GFX_CTRL_BPP_F       0x0030
#define GFX_CTRL_BITMAP_B    6
#define GFX_CTRL_BITMAP_W    1
#define GFX_CTRL_BITMAP_F    0x0040
#define GFX_CTRL_BLANK_B     7
#define GFX_CTRL_BLANK_W     1
#define GFX_CTRL_BLANK_F     0x0080
#define GFX_CTRL_COLORBASE_B 8
#define GFX_CTRL_COLORBASE_W 8
#define GFX_CTRL_COLORBASE_F 0xFF00
// XR_Px_TILE_CTRL bit numbers within word)
#define TILE_CTRL_TILE_H_B       0
#define TILE_CTRL_TILE_H_W       4
#define TILE_CTRL_TILE_H_F       0x000F
#define TILE_CTRL_TILE_VRAM_B    8
#define TILE_CTRL_TILE_VRAM_W    1
#define TILE_CTRL_TILE_VRAM_F    0x0100
#define TILE_CTRL_DISP_TILEMEM_B 9
#define TILE_CTRL_DISP_TILEMEM_W 1
#define TILE_CTRL_DISP_TILEMEM_F 0x0200
#define TILE_CTRL_TILEBASE_B     10
#define TILE_CTRL_TILEBASE_W     6
#define TILE_CTRL_TILEBASE_F     0xFC00
// XR_AUDx_PERIOD flag
#define AUD_PER_RESTART_B 15        // AUDx_PERIOD bit 15 is force channel restart
#define AUD_PER_RESTART_W 1
#define AUD_PER_RESTART_F 0x8000
// XR_AUDx_LENGTH flag
#define AUD_LEN_TILEMEM_B 15        // AUDx_LENGTH bit 15 is TILEMEM flag
#define AUD_LEN_TILEMEM_W 1
#define AUD_LEN_TILEMEM_F 0x8000
// XR_BLIT_CTRL
#define BLIT_CTRL_TRANSPVAL_B 8
#define BLIT_CTRL_TRANSPVAL_W 8
#define BLIT_CTRL_TRANSPVAL_G 0xFF00
#define BLIT_CTRL_8B_B        5
#define BLIT_CTRL_8B_W        1
#define BLIT_CTRL_8B_F        0x0020
#define BLIT_CTRL_TRANSP_B    4
#define BLIT_CTRL_TRANSP_W    1
#define BLIT_CTRL_TRANSP_F    0x0010
#define BLIT_CTRL_SCONST_B    0
#define BLIT_CTRL_SCONST_W    1
#define BLIT_CTRL_SCONST_F    0x0001
// XR_BLIT_SHIFT
#define BLIT_SHIFT_LMSK_B 12
#define BLIT_SHIFT_LMSK_W 4
#define BLIT_SHIFT_LMSK_G 0xF000
#define BLIT_SHIFT_RMSK_B 8
#define BLIT_SHIFT_RMSK_W 4
#define BLIT_SHIFT_RMSK_G 0x0F00
#define BLIT_SHIFT_CNT_B  0
#define BLIT_SHIFT_CNT_W  2
#define BLIT_SHIFT_CNT_F  0x0003

#define MAKE_GFX_CTRL(colbase, blank, bpp, bm, hx, vx)                                                                 \
    (XB_(colbase, 8, 8) | XB_(blank, 7, 1) | XB_(bm, 6, 1) | XB_(bpp, 4, 2) | XB_(hx, 2, 2) | XB_(vx, 0, 2))
#define MAKE_TILE_CTRL(tilebase, map_in_tile, glyph_in_vram, tileheight)                                               \
    (((tilebase)&0xFC00) | XB_(map_in_tile, 9, 1) | XB_(glyph_in_vram, 8, 1) | XB_(((tileheight)-1), 0, 4))
#define MAKE_HV_SCROLL(h_scrl, v_scrl) (XB_(h_scrl, 8, 8) | XB_(v_scrl, 0, 8))

#define MAKE_VID_CTRL(borcol, intmask) (XB_(borcol, 8, 0) | XB_(intmask, 0, 4))

// copper constants for HPOS/VPOS
#define COP_H_EOL      0x7FF        // copper HPOS value for wait end of line
#define COP_V_EOF      0x3FF        // copper VPOS value for wait end of frame
#define COP_V_WAITBLIT 0x7FF        // copper VPOS value for wait blit ready or end of frame
// copper special memory addresses
#define COP_RA     0xC800        // copper address for RA register
#define COP_RA_SUB 0xC801        // copper address for RA = RA - writeval
#define COP_RA_CMP 0xC7FF        // copper address for set B if RA < writeval
// copper instructions
#define COP_SETI(d_xadr14, i_val16)  ((uint16_t)0x0000 | ((uint16_t)(d_xadr14)&0xCFFF)), ((uint16_t)(i_val16))
#define COP_SETM(d_xadr16, s_cadr12) ((uint16_t)0xD000 | ((uint16_t)(s_cadr12)&0x0FFF)), ((uint16_t)(d_xadr16))
#define COP_HPOS(h_wait)             ((uint16_t)0x2000 | ((uint16_t)(h_wait)&0x07FF))
#define COP_VPOS(v_wait)             ((uint16_t)0x2800 | ((uint16_t)(v_wait)&0x07FF))
#define COP_BRGE(cadr11)             ((uint16_t)0x3000 | ((uint16_t)(cadr11)&0x07FF))
#define COP_BRLT(cadr11)             ((uint16_t)0x3800 | ((uint16_t)(cadr11)&0x07FF))
// copper pseudo instructions
#define COP_MOVE(i_val16, d_xadr14)  ((uint16_t)0x0000 | ((uint16_t)(d_xadr14)&0xCFFF)), ((uint16_t)(i_val16))
#define COP_MOVER(i_val16, d_xreg)   ((uint16_t)0x0000 | ((uint16_t)(XR_##d_xreg) & 0xCFFF)), ((uint16_t)(i_val16))
#define COP_MOVM(s_cadr12, d_xadr16) ((uint16_t)0xD000 | ((uint16_t)(s_cadr12)&0x0FFF)), ((uint16_t)(d_xadr16))
#define COP_LDI(i_val16)             ((uint16_t)0xC000 | ((uint16_t)(COP_RA)&0x0FFF)), ((uint16_t)(i_val16))
#define COP_LDM(s_cadr12)            ((uint16_t)0xD000 | ((uint16_t)(s_cadr12)&0x0FFF)), ((uint16_t)(COP_RA))
#define COP_STM(d_xadr16)            ((uint16_t)0xD000 | ((uint16_t)(COP_RA)&0x0FFF)), ((uint16_t)(d_xadr16))
#define COP_CLRB()                   ((uint16_t)0xD000 | ((uint16_t)(COP_RA)&0x0FFF)), ((uint16_t)(COP_RA))
#define COP_SUBI(i_val16)            ((uint16_t)0xC000 | ((uint16_t)(COP_RA_SUB)&0x0FFF)), ((uint16_t)(i_val16))
#define COP_ADDI(i_val16)            ((uint16_t)0xC000 | ((uint16_t)(COP_RA_SUB)&0x0FFF)), ((uint16_t)(-(int16_t)i_val16))
#define COP_SUBM(s_cadr12)           ((uint16_t)0xD000 | ((uint16_t)(s_cadr12)&0x0FFF)), ((uint16_t)(COP_RA_SUB))
#define COP_CMPI(i_val16)            ((uint16_t)0xC000 | ((uint16_t)(COP_RA_CMP)&0x0FFF)), ((uint16_t)(i_val16))
#define COP_CMPM(s_cadr12)           ((uint16_t)0xD000 | ((uint16_t)(s_cadr12)&0x0FFF)), ((uint16_t)(COP_RA_CMP))
#define COP_END()                    ((uint16_t)0x2800 | ((uint16_t)(COP_V_EOF)&0x07FF))

#endif        // XOSERA_M68K_DEFS_H
