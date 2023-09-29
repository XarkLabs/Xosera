# Xosera - Register Quick Reference

- [Xosera - Register Quick Reference](#xosera---register-quick-reference)
  - [Xosera Main Register Quick Refernce (XM Registers)](#xosera-main-register-quick-refernce-xm-registers)
    - [**0x0 `XM_SYS_CTRL`** (R/W+) - System Control](#0x0-xm_sys_ctrl-rw---system-control)
    - [**0x1 `XM_INT_CTRL`** (R/W+) - Interrupt Control](#0x1-xm_int_ctrl-rw---interrupt-control)
    - [**0x2 `XM_TIMER`** (R/W) - Timer Functions](#0x2-xm_timer-rw---timer-functions)
    - [**0x3 `XM_RD_XADDR`** (R/W+) - XR Read Address](#0x3-xm_rd_xaddr-rw---xr-read-address)
    - [**0x4 `XM_WR_XADDR`** (R/W) - XR Write Address](#0x4-xm_wr_xaddr-rw---xr-write-address)
    - [**0x5 `XM_XDATA`** (R+/W+) - XR Read/Write Data](#0x5-xm_xdata-rw---xr-readwrite-data)
    - [**0x6 `XM_RD_INCR`** (R/W) - Increment for VRAM Read Address](#0x6-xm_rd_incr-rw---increment-for-vram-read-address)
    - [**0x7 `XM_RD_ADDR`** (R/W+) - VRAM Read Address](#0x7-xm_rd_addr-rw---vram-read-address)
    - [**0x8 `XM_WR_INCR`** (R/W) - Increment for VRAM Write Address](#0x8-xm_wr_incr-rw---increment-for-vram-write-address)
    - [**0x9 `XM_WR_ADDR`** (R/W) - VRAM Write Address](#0x9-xm_wr_addr-rw---vram-write-address)
    - [**0xA `XM_DATA`** (R+/W+) - VRAM Read/Write Data](#0xa-xm_data-rw---vram-readwrite-data)
    - [**0xB `XM_DATA_2`** (R+/W+) - VRAM Read/Write Data (2nd)](#0xb-xm_data_2-rw---vram-readwrite-data-2nd)
    - [**0xC `PIXEL_X`** (-/W+) - X coordinate for pixel address/mask generation (also used to set `PIXEL_BASE`)](#0xc-pixel_x--w---x-coordinate-for-pixel-addressmask-generation-also-used-to-set-pixel_base)
    - [**0xD `PIXEL_Y`** (-/W+) - Y coordinate for pixel address/mask generation (also used to set `PIXEL_WIDTH`)](#0xd-pixel_y--w---y-coordinate-for-pixel-addressmask-generation-also-used-to-set-pixel_width)
    - [**0xE `XM_UART`** (R+/W+)](#0xe-xm_uart-rw)
    - [**0xF `XM_FEATURE`** (R/-) - Xosera feature bits](#0xf-xm_feature-r----xosera-feature-bits)
  - [Xosera Extended Register / Extended Memory Region Summary](#xosera-extended-register--extended-memory-region-summary)
  - [Xosera Extended Registers Quick Reference (XR Registers)](#xosera-extended-registers-quick-reference-xr-registers)
    - [**0x00 `XR_VID_CTRL`** (R/W) - Border Color / Playfield Color-Swap](#0x00-xr_vid_ctrl-rw---border-color--playfield-color-swap)
    - [**0x01 `XR_COPP_CTRL`** (R/W) - Copper Enable](#0x01-xr_copp_ctrl-rw---copper-enable)
    - [**0x02 `XR_AUD_CTRL`** (R/W) - Audio Control](#0x02-xr_aud_ctrl-rw---audio-control)
    - [**0x03 `XR_SCANLINE`** (R/W+) - current video scan line/trigger Xosera host CPU video interrupt](#0x03-xr_scanline-rw---current-video-scan-linetrigger-xosera-host-cpu-video-interrupt)
    - [**0x04 `XR_VID_LEFT`** (R/W) - video display window left edge](#0x04-xr_vid_left-rw---video-display-window-left-edge)
    - [**0x05 `XR_VID_RIGHT`** (R/W) - video display window right \*\*edge](#0x05-xr_vid_right-rw---video-display-window-right-edge)
    - [**0x06 `XR_POINTER_H`** (-/W+) - pointer sprite H position](#0x06-xr_pointer_h--w---pointer-sprite-h-position)
    - [**0x07 `XR_POINTER_V`** (-/W+) - pointer sprite V position and colormap select](#0x07-xr_pointer_v--w---pointer-sprite-v-position-and-colormap-select)
  - [Playfield A \& B Control XR Registers Quick Reference](#playfield-a--b-control-xr-registers-quick-reference)
    - [**0x10 `XR_PA_GFX_CTRL` (R/W)** - playfield A (base) graphics control](#0x10-xr_pa_gfx_ctrl-rw---playfield-a-base-graphics-control)
    - [**0x18 `XR_PB_GFX_CTRL` (R/W)** - playfield B (overlay) graphics control](#0x18-xr_pb_gfx_ctrl-rw---playfield-b-overlay-graphics-control)
    - [**0x11 `XR_PA_TILE_CTRL` (R/W)** - playfield A (base) tile control](#0x11-xr_pa_tile_ctrl-rw---playfield-a-base-tile-control)
    - [**0x19 `XR_PB_TILE_CTRL` (R/W)** - playfield B (overlay) tile control](#0x19-xr_pb_tile_ctrl-rw---playfield-b-overlay-tile-control)
    - [**0x12 `XR_PA_DISP_ADDR` (R/W)** - playfield A (base) display VRAM start address](#0x12-xr_pa_disp_addr-rw---playfield-a-base-display-vram-start-address)
    - [**0x1A `XR_PB_DISP_ADDR` (R/W)** - playfield B (overlay) display VRAM start address](#0x1a-xr_pb_disp_addr-rw---playfield-b-overlay-display-vram-start-address)
    - [**0x13 `XR_PA_LINE_LEN` (R/W)** - playfield A (base) display line word length](#0x13-xr_pa_line_len-rw---playfield-a-base-display-line-word-length)
    - [**0x1B `XR_PB_LINE_LEN` (R/W)** - playfield B (overlay) display line word length](#0x1b-xr_pb_line_len-rw---playfield-b-overlay-display-line-word-length)
    - [**0x14 `XR_PA_HV_FSCALE` (R/W)** - playfield A (base) horizontal and vertical fractional scale](#0x14-xr_pa_hv_fscale-rw---playfield-a-base-horizontal-and-vertical-fractional-scale)
    - [**0x1C `XR_PB_HV_FSCALE` (R/W)** - playfield B (overlay) horizontal and vertical fractional scale](#0x1c-xr_pb_hv_fscale-rw---playfield-b-overlay-horizontal-and-vertical-fractional-scale)
    - [**0x15 `XR_PA_H_SCROLL` (R/W)** - playfield A (base) horizontal fine scroll](#0x15-xr_pa_h_scroll-rw---playfield-a-base-horizontal-fine-scroll)
    - [**0x1D `XR_PB_H_SCROLL` (R/W)** - playfield B (overlay) horizontal fine scroll](#0x1d-xr_pb_h_scroll-rw---playfield-b-overlay-horizontal-fine-scroll)
    - [**0x16 `XR_PA_V_SCROLL` (R/W)** - playfield A (base) vertical repeat/tile scroll](#0x16-xr_pa_v_scroll-rw---playfield-a-base-vertical-repeattile-scroll)
    - [**0x1E `XR_PB_V_SCROLL` (R/W)** - playfield B (overlay) vertical repeat/tile scroll](#0x1e-xr_pb_v_scroll-rw---playfield-b-overlay-vertical-repeattile-scroll)
    - [**0x17 `XR_PA_LINE_ADDR` (-/W)** - playfield A (base) display VRAM next line address](#0x17-xr_pa_line_addr--w---playfield-a-base-display-vram-next-line-address)
    - [**0x1F `XR_PB_LINE_ADDR` (-/W)** - playfield B (overlay) display VRAM next line address](#0x1f-xr_pb_line_addr--w---playfield-b-overlay-display-vram-next-line-address)
  - [Bitmap Display Formats](#bitmap-display-formats)
  - [Tile Display Formats](#tile-display-formats)
    - [**1-BPP tilemap** - 8-bit 256 tile/glyph index with 4-bit background/forground color attributes per word](#1-bpp-tilemap---8-bit-256-tileglyph-index-with-4-bit-backgroundforground-color-attributes-per-word)
    - [**1-BPP tile definitions** two tile lines are stored in each word in the tile definition (8x8 or 8x16 tile size)](#1-bpp-tile-definitions-two-tile-lines-are-stored-in-each-word-in-the-tile-definition-8x8-or-8x16-tile-size)
    - [**4 BPP tilemap**  10-bit 1024 tile/glyph index, 4-bit color offset and horizontal and vertical tile mirror](#4-bpp-tilemap--10-bit-1024-tileglyph-index-4-bit-color-offset-and-horizontal-and-vertical-tile-mirror)
    - [**4-BPP tile definitions** 4 pixels per tile definition word (8x8 tile size)](#4-bpp-tile-definitions-4-pixels-per-tile-definition-word-8x8-tile-size)
    - [**8 BPP tilemap**  10-bit 1024 tile/glyph index, 4-bit color offset and horizontal and vertical tile mirror](#8-bpp-tilemap--10-bit-1024-tileglyph-index-4-bit-color-offset-and-horizontal-and-vertical-tile-mirror)
    - [**8-BPP tile definitions** 2 pixels per tile definition word (8x8 tile size)](#8-bpp-tile-definitions-2-pixels-per-tile-definition-word-8x8-tile-size)
  - [Audio Register Quick Reference # TODO: Missing](#audio-register-quick-reference--todo-missing)
  - [2D Blitter Engine Quick Reference](#2d-blitter-engine-quick-reference)
    - [**0x20 `XR_BLIT_CTRL`** (-/W) - control bits (transparency control, S const)](#0x20-xr_blit_ctrl--w---control-bits-transparency-control-s-const)
    - [**0x21 `XR_BLIT_ANDC`** (-/W) - source term ANDC value constant](#0x21-xr_blit_andc--w---source-term-andc-value-constant)
    - [**0x22 `XR_BLIT_XOR`** (-/W) - source term XOR value constant](#0x22-xr_blit_xor--w---source-term-xor-value-constant)
    - [**0x25 `XR_BLIT_MOD_D`** (-/W) - modulo added to `BLIT_DST_D` address at end of line](#0x25-xr_blit_mod_d--w---modulo-added-to-blit_dst_d-address-at-end-of-line)
    - [**0x26 `XR_BLIT_DST_D`** (-/W) - destination D VRAM write address](#0x26-xr_blit_dst_d--w---destination-d-vram-write-address)
    - [**0x27 `XR_BLIT_SHIFT`** (-/W) - first and last word nibble masks and nibble shift](#0x27-xr_blit_shift--w---first-and-last-word-nibble-masks-and-nibble-shift)
    - [**0x28 `XR_BLIT_LINES`** (-/W) - 15-bit number of lines heigh - 1 (1 to 32768)](#0x28-xr_blit_lines--w---15-bit-number-of-lines-heigh---1-1-to-32768)
    - [**0x29 `XR_BLIT_WORDS`** (-/W) - write queues operation, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times)](#0x29-xr_blit_words--w---write-queues-operation-word-width---1-1-to-65536-repeats-xr_blit_lines-times)
    - [Blitter Logic Operation](#blitter-logic-operation)
  - [Video Synchronized Co-Processor "Copper" Quick Reference](#video-synchronized-co-processor-copper-quick-reference)
    - [Copper Instruction Set](#copper-instruction-set)
    - [Copper Pseudo Instructions (when using CopAsm, or C macros)](#copper-pseudo-instructions-when-using-copasm-or-c-macros)
    - [Screen Coordinate Details](#screen-coordinate-details)

## Xosera Main Register Quick Refernce (XM Registers)

#### **0x0 `XM_SYS_CTRL`** (R/W+) - System Control

<img src="./pics/wd_XM_SYS_CTRL.svg">

#### **0x1 `XM_INT_CTRL`** (R/W+) - Interrupt Control

<img src="./pics/wd_XM_INT_CTRL.svg">  

#### **0x2 `XM_TIMER`** (R/W) - Timer Functions

<img src="./pics/wd_XM_TIMER.svg">

#### **0x3 `XM_RD_XADDR`** (R/W+) - XR Read Address

<img src="./pics/wd_XM_RD_XADDR.svg">

#### **0x4 `XM_WR_XADDR`** (R/W) - XR Write Address

<img src="./pics/wd_XM_WR_XADDR.svg">

#### **0x5 `XM_XDATA`** (R+/W+) - XR Read/Write Data

<img src="./pics/wd_XM_XDATA.svg">

#### **0x6 `XM_RD_INCR`** (R/W) - Increment for VRAM Read Address

<img src="./pics/wd_XM_RD_INCR.svg">

#### **0x7 `XM_RD_ADDR`** (R/W+) - VRAM Read Address

<img src="./pics/wd_XM_RD_ADDR.svg">

#### **0x8 `XM_WR_INCR`** (R/W) - Increment for VRAM Write Address

<img src="./pics/wd_XM_WR_INCR.svg">

#### **0x9 `XM_WR_ADDR`** (R/W) - VRAM Write Address

<img src="./pics/wd_XM_WR_ADDR.svg">

#### **0xA `XM_DATA`** (R+/W+) - VRAM Read/Write Data

<img src="./pics/wd_XM_DATA.svg">

#### **0xB `XM_DATA_2`** (R+/W+) - VRAM Read/Write Data (2<sup>nd</sup>)

<img src="./pics/wd_XM_DATA.svg">

#### **0xC `PIXEL_X`** (-/W+) - X coordinate for pixel address/mask generation (also used to set `PIXEL_BASE`)

<img src="./pics/wd_XM_PIXEL_X.svg">

#### **0xD `PIXEL_Y`** (-/W+) - Y coordinate for pixel address/mask generation (also used to set `PIXEL_WIDTH`)

<img src="./pics/wd_XM_PIXEL_Y.svg">

#### **0xE `XM_UART`** (R+/W+)

<img src="./pics/wd_XM_UART.svg">

#### **0xF `XM_FEATURE`** (R/-) - Xosera feature bits

<img src="./pics/wd_XM_FEATURE.svg">

___

## Xosera Extended Register / Extended Memory Region Summary

| XR Region Name    | XR Region Range | R/W | Description                                   |
|-------------------|-----------------|-----|-----------------------------------------------|
| XR video config   | `0x0000-0x000F` | R/W | config XR registers                           |
| XR playfield A    | `0x0010-0x0017` | R/W | playfield A XR registers                      |
| XR playfield B    | `0x0018-0x001F` | R/W | playfield B XR registers                      |
| XR audio control  | `0x0020-0x002F` | -/W | audio channel XR registers                    |
| XR blit engine    | `0x0040-0x004B` | -/W | 2D-blit engine XR registers                   |
| `XR_TILE_ADDR`    | `0x4000-0x53FF` | R/W | 5KW 16-bit tilemap/tile/audio storage memory  |
| `XR_COLOR_A_ADDR` | `0x8000-0x80FF` | R/W | 256W 16-bit color A lookup memory (0xARGB)    |
| `XR_COLOR_B_ADDR` | `0x8100-0x81FF` | R/W | 256W 16-bit color B lookup memory (0xARGB)    |
| `XR_POINTER_ADDR` | `0x8200-0x82FF` | -/W | 256W 16-bit 32x32 4-BPP pointer sprite bitmap |
| `XR_COPPER_ADDR`  | `0xC000-0xC5FF` | R/W | 1.5KW 16-bit copper memory                    |
___

## Xosera Extended Registers Quick Reference (XR Registers)

#### **0x00 `XR_VID_CTRL`** (R/W) - Border Color / Playfield Color-Swap

<img src="./pics/wd_XR_VID_CTRL.svg">

#### **0x01 `XR_COPP_CTRL`** (R/W) - Copper Enable

<img src="./pics/wd_XR_COPP_CTRL.svg">

#### **0x02 `XR_AUD_CTRL`** (R/W) - Audio Control

<img src="./pics/wd_XR_AUD_CTRL.svg">

#### **0x03 `XR_SCANLINE`** (R/W+) - current video scan line/trigger Xosera host CPU video interrupt

<img src="./pics/wd_XR_SCANLINE.svg">

#### **0x04 `XR_VID_LEFT`** (R/W) - video display window left edge

<img src="./pics/wd_XR_VID_LEFT.svg">

#### **0x05 `XR_VID_RIGHT`** (R/W) - video display window right **edge

<img src="./pics/wd_XR_VID_RIGHT.svg">

#### **0x06 `XR_POINTER_H`** (-/W+) - pointer sprite H position

<img src="./pics/wd_XR_POINTER_H.svg">

#### **0x07 `XR_POINTER_V`** (-/W+) - pointer sprite V position and colormap select

<img src="./pics/wd_XR_POINTER_V.svg">

___

## Playfield A & B Control XR Registers Quick Reference

#### **0x10 `XR_PA_GFX_CTRL` (R/W)** - playfield A (base) graphics control  

#### **0x18 `XR_PB_GFX_CTRL` (R/W)** - playfield B (overlay) graphics control

<img src="./pics/wd_XR_Px_GFX_CTRL.svg">

#### **0x11 `XR_PA_TILE_CTRL` (R/W)** - playfield A (base) tile control  

#### **0x19 `XR_PB_TILE_CTRL` (R/W)** - playfield B (overlay) tile control

<img src="./pics/wd_XR_Px_TILE_CTRL.svg">

#### **0x12 `XR_PA_DISP_ADDR` (R/W)** - playfield A (base) display VRAM start address  

#### **0x1A `XR_PB_DISP_ADDR` (R/W)** - playfield B (overlay) display VRAM start address

<img src="./pics/wd_XR_Px_DISP_ADDR.svg">

#### **0x13 `XR_PA_LINE_LEN` (R/W)** - playfield A (base) display line word length  

#### **0x1B `XR_PB_LINE_LEN` (R/W)** - playfield B (overlay) display line word length

<img src="./pics/wd_XR_Px_LINE_LEN.svg">

#### **0x14 `XR_PA_HV_FSCALE` (R/W)** - playfield A (base) horizontal and vertical fractional scale  

#### **0x1C `XR_PB_HV_FSCALE` (R/W)** - playfield B (overlay) horizontal and vertical fractional scale

<img src="./pics/wd_XR_Px_HV_FSCALE.svg">

#### **0x15 `XR_PA_H_SCROLL` (R/W)** - playfield A (base) horizontal fine scroll  

#### **0x1D `XR_PB_H_SCROLL` (R/W)** - playfield B (overlay) horizontal fine scroll

<img src="./pics/wd_XR_Px_H_SCROLL.svg">

#### **0x16 `XR_PA_V_SCROLL` (R/W)** - playfield A (base) vertical repeat/tile scroll  

#### **0x1E `XR_PB_V_SCROLL` (R/W)** - playfield B (overlay) vertical repeat/tile scroll

<img src="./pics/wd_XR_Px_V_SCROLL.svg">

#### **0x17 `XR_PA_LINE_ADDR` (-/W)** - playfield A (base) display VRAM next line address  

#### **0x1F `XR_PB_LINE_ADDR` (-/W)** - playfield B (overlay) display VRAM next line address

<img src="./pics/wd_XR_Px_LINE_ADDR.svg">

## Bitmap Display Formats

**1 BPP bitmap mode** - 4-bit background/forground color attributes and 8 pixels per word
<img src="./pics/wd_1-bpp_bitmap_word.svg">

**4 BPP bitmap mode** - 4 pixels per word, each one of 16 colors
<img src="./pics/wd_4-bpp_bitmap_word.svg">

**8 BPP bitmap mode** - 2 pixels per word, each one of 256 colors
<img src="./pics/wd_8-bpp_bitmap_word.svg">

## Tile Display Formats

#### **1-BPP tilemap** - 8-bit 256 tile/glyph index with 4-bit background/forground color attributes per word

<img src="./pics/wd_1-bpp_tile_word.svg">

#### **1-BPP tile definitions** two tile lines are stored in each word in the tile definition (8x8 or 8x16 tile size)

<img src="./pics/wd_1-bpp_tile_def.svg">

#### **4 BPP tilemap**  10-bit 1024 tile/glyph index, 4-bit color offset and horizontal and vertical tile mirror  

<img src="./pics/wd_n-bpp_tile_word.svg">

#### **4-BPP tile definitions** 4 pixels per tile definition word (8x8 tile size)

<img src="./pics/wd_4-bpp_bitmap_word.svg">

#### **8 BPP tilemap**  10-bit 1024 tile/glyph index, 4-bit color offset and horizontal and vertical tile mirror  

<img src="./pics/wd_n-bpp_tile_word.svg">

#### **8-BPP tile definitions** 2 pixels per tile definition word (8x8 tile size)

<img src="./pics/wd_8-bpp_bitmap_word.svg">

**Tile Defintion Alignment** in either TILE memory or VRAM
| BPP   | Size | Words/Tile | Max Tiles | Word Alignment  | Max Word Size |
|-------|------|------------|-----------|-----------------|---------------|
| 1-BPP | 8x8  | 4 words    | 256       | 0x0400 boundary | 0x0400 words  |
| 1-BPP | 8x16 | 8 words    | 256       | 0x0800 boundary | 0x0800 words  |
| 4-BPP | 8x8  | 16 words   | 1024      | 0x4000 boundary | 0x4000 words  |
| 8-BPP | 8x8  | 32 words   | 1024      | 0x8000 boundary | 0x8000 words  |

___

## Audio Register Quick Reference # TODO: Missing

___

## 2D Blitter Engine Quick Reference

#### **0x20 `XR_BLIT_CTRL`** (-/W) - control bits (transparency control, S const)  

<img src="./pics/wd_XR_BLIT_CTRL.svg">

#### **0x21 `XR_BLIT_ANDC`** (-/W) - source term ANDC value constant

<img src="./pics/wd_XR_BLIT_ANDC.svg">

#### **0x22 `XR_BLIT_XOR`** (-/W) - source term XOR value constant

<img src="./pics/wd_XR_BLIT_XOR.svg">

**0x23 `XR_BLIT_MOD_S`** (-/W) - modulo added to `BLIT_SRC_S` address at end of line

<img src="./pics/wd_XR_BLIT_MOD_S.svg">

**0x24 `XR_BLIT_SRC_S`** (-/W) - source `S` term (read from VRAM address or constant value)

<img src="./pics/wd_XR_BLIT_SRC_S.svg">

#### **0x25 `XR_BLIT_MOD_D`** (-/W) - modulo added to `BLIT_DST_D` address at end of line

<img src="./pics/wd_XR_BLIT_MOD_D.svg">

#### **0x26 `XR_BLIT_DST_D`** (-/W) - destination D VRAM write address

<img src="./pics/wd_XR_BLIT_DST_D.svg">

#### **0x27 `XR_BLIT_SHIFT`** (-/W) - first and last word nibble masks and nibble shift

<img src="./pics/wd_XR_BLIT_SHIFT.svg">

#### **0x28 `XR_BLIT_LINES`** (-/W) - 15-bit number of lines heigh - 1 (1 to 32768)

<img src="./pics/wd_XR_BLIT_LINES.svg">

#### **0x29 `XR_BLIT_WORDS`** (-/W) - write queues operation, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times)

<img src="./pics/wd_XR_BLIT_WORDS.svg">

### Blitter Logic Operation

&nbsp;&nbsp;&nbsp;`D = S & ~ANDC ^ XOR`  (destination word `D` &larr; `S` source word AND'd with NOT of `ANDC` and XOR'd with `XOR`)

- `D` result word
  - written to VRAM (starting address set by `XR_BLIT_DST_D` and incrementing/decrementing)
- `S` primary source word, can be one of:
  - word read from VRAM (starting VRAM address set by `XR_BLIT_SRC_A` and incrementing/decrementing)
  - word constant (set by `XR_BLIT_SRC_S` when `S_CONST` set in `XR_BLIT_CTRL`)
- `ANC` constant AND-COMPLEMENT word
  - source word is AND'd with the NOT of this word
- `XOR` constant XOR source word
  - source word is XOR'd with the this word

___

## Video Synchronized Co-Processor "Copper" Quick Reference

### Copper Instruction Set

| Copper Assembly             | Opcode Bits                 | B | # | ~  | Description                                         |
|-----------------------------|-----------------------------|---|---|----|-----------------------------------------------------|
| `SETI`   *xadr14*,`#`*im16* | `rr00` `oooo` `oooo` `oooo` | B | 2 | 4  | sets [xadr14] &larr; to #val16                      |
| + *im16* *value*            | `iiii` `iiii` `iiii` `iiii` | - | - | -  | *(im16, 2<sup>nd</sup> word of `SETI`)*             |
| `SETM`  *xadr16*,*cadr12*   | `--01` `rccc` `cccc` `cccc` | B | 2 | 4  | sets [xadr16] &larr; to [cadr12]                    |
| + *xadr16* *address*        | `rroo` `oooo` `oooo` `oooo` | - | - | -  | *(xadr16, 2<sup>nd</sup> word of `SETM`)*           |
| `HPOS`   `#`*im11*          | `--10` `0iii` `iiii` `iiii` |   | 1 | 4+ | wait until video H pos. >= *im11* or EOL            |
| `VPOS`   `#`*im11*          | `--10` `1bii` `iiii` `iiii` |   | 1 | 4+ | wait until video V pos. >= *im11[9:0]*,`b`=blitbusy |
| `BRGE`   *cadd11*           | `--11` `0ccc` `cccc` `cccc` |   | 1 | 4  | if (`B`==0) `PC` &larr; *cadd11*                    |
| `BRLT`   *cadd11*           | `--11` `1ccc` `cccc` `cccc` |   | 1 | 4  | if (`B`==1) `PC` &larr; *cadd11*                    |

| Legend   | Description                                                                                              |
|----------|----------------------------------------------------------------------------------------------------------|
| `B`      | borrow flag, set true when `RA` < *val16* written (borrow after unsigned subtract)                       |
| `#`      | number of 16-bit words needed for instruction (1 or 2)                                                   |
| `~`      | number of copper cycles, always 4 unless a wait (each cycle is the time for one native pixel to output)  |
| *xadr14* | 2-bit XR region + 12-bit offset (1<sup>st</sup> word of `SETI`, destination XR address)                  |
| *im16*   | 16-bit immediate word (2<sup>nd</sup> word of `SETI`, the source value)                                  |
| *cadr12* | 11-bit copper address or register with bit [11] (1<sup>st</sup> word of `SETM`, source copper adress)    |
| *xadr16* | 16-bit XR addreass (2<sup>nd</sup> word of `SETM`, destination XR address)                               |
| *im11*   | 11-bit value for `HPOS`, `VPOS` wait. With `VPOS` bit `[10]` indicates also stop waiting if blitter idle |
| *cadd11* | 11-bit copper program address for `BRGE`, `BRLT` branch opcodes                                          |

### Copper Pseudo Instructions (when using CopAsm, or C macros)

| Instruction                | Alias  | Words | Description                                                              |
|----------------------------|--------|-------|--------------------------------------------------------------------------|
| `MOVI` `#`*imm16*,*xadr14* | `SETI` | 2     | m68k order `SETI`, copy `#`*imm16* &rarr; *cadr12*                       |
| `MOVM` *cadr12*,*xadr16*   | `SETM` | 2     | m68k order`SETM`, copy *cadr12* &rarr; *xadr16*                          |
| `MOVE` `#`*imm16*,*xadr14* | `SETI` | 2     | m68k style `MOVE #` immediate copy `#`*imm16* &rarr; *xadr16*            |
| `MOVE` *cadr12*,*xadr16*   | `SETM` | 2     | m68k style `MOVE` memory copy *source* &rarr; *dest*                     |
| `LDI` `#`*imm16*           | `SETI` | 2     | Load `RA` register with value *imm16*, set `B`=`0`                       |
| `LDM` *cadr12*             | `SETM` | 2     | Load `RA` register with contents of memory *cadr12*, set `B`=`0`         |
| `STM` *xadr16*             | `SETM` | 2     | Store `RA` register contents into memory *xadr16*, set `B`=`0`           |
| `CLRB`                     | `SETM` | 2     | Store `RA` register into `RA`, set `B`=`0`                               |
| `SUBI` `#`*imm16*          | `SETI` | 2     | `RA` = `RA` - *imm16*, `B` flag updated                                  |
| `ADDI` `#`*imm16*          | `SETI` | 2     | `RA` = `RA` + *imm16*, `B` flag updated (for subtract of -*imm16*)       |
| `SUBM` *cadr12*            | `SETM` | 2     | `RA` = `RA` - contents of *cadr12*, `B` flag updated                     |
| `CMPI` `#`*imm16*          | `SETI` | 2     | test if `RA` < *imm16*, `B` flag updated (`RA` not altered)              |
| `CMPM` *cadr12*            | `SETM` | 2     | test it `RA` < contents of *cadr12*, `B` flag updated (`RA` not altered) |

**Copper addresses for memory mapped registers and operations:**
| Pseudo reg       | Copper Addr | Operation                                 | Description                                 |
|------------------|-------------|-------------------------------------------|---------------------------------------------|
| `RA` (read)      | `0x800`     | read value in `RA`, `B` unaltered         | return current value in `RA` register       |
| `RA` (write)     | `0x800`     | `RA` = *val16*, `B` =`0`                  | set `RA` to *val16*, clear `B` flag         |
| `RA_SUB` (write) | `0x801`     | `RA` = `RA` - *val16*, `B`=`RA` < *val16* | set `RA` to `RA` - *val16*, update `B` flag |
| `RA_CMP` (write) | `0x7FF`     | `B` = `RA` < *val16*                      | update B flag only (`RA` unaltered)         |

- `RA` read value in 16-bit accumulator, `B` flag unaltered
  - E.g., `SETM XR_ADDR,RA` to store `RA` contents into `XR_ADDR`
- `RA` write value from 16-bit accumulator,`B` flag cleared (since `RA` will be equal to value written)
  - E.g., `SETI RA,#123` to load `RA` with `123`
- `RA_SUB`write performs subtract operation, `RA` = `RA` minus the value written, `B` is set if `RA` is less than value written
  - E.g. `SETI RA_SUB,#1` to subtract `1` from `RA`
- `RA_CMP`write performs compare operation, setting `B` borrow flag if `RA` is less than the value written (`RA` not altered)
  - E.g., `SETI RA_CMP,#10` to set `B` if contents of `RA` is less than `10`

### Screen Coordinate Details

| Video Mode | Aspect | Full res.  | H off-left | H visible   | V visible | V off-bottom |
|------------|--------|------------|------------|-------------|-----------|--------------|
| 640 x 480  | 4:3    | 800 x 525  | 0 to 159   | 160 to 799  | 0 to 479  | 480 to 524   |
| 848 x 480  | 16:9   | 1088 x 517 | 0 to 239   | 240 to 1079 | 0 to 479  | 480 to 516   |
