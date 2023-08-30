# Xosera - Xark's Open Source Embedded Retro Adapter

Xosera is a Verilog design that implements an audio/video controller (aka Embedded Retro Adapter) providing Amiga-ish level video raphics digital audio capibilities.

The design was originally for iCE40UltraPlus5K FPGAs, but has been ported to other FPGAs (with enough internal memory, like larger ECP5).

It was designed primarily for the rosco_m68K series of 68K based retro computers, but adaptable for other systems (to interface it requires 6 register address signals, an 8-bit bidirectional bus and a couple of control signals). It provides 12-bit RGB color video with text and bitmap modes with dual layers and 4 voice 8-bit digital audio (similar-ish to 80s era 68K home computers).

This document is meant to provide the low-level reference register information to operate it (and should match the Verilog implementation). Please mention it if you spot any discrepency.

**Section Index:**  

- [Xosera - Xark's Open Source Embedded Retro Adapter](#xosera---xarks-open-source-embedded-retro-adapter)
  - [Xosera Reference Information](#xosera-reference-information)
    - [Xosera Main Registers (XM Registers) Summary](#xosera-main-registers-xm-registers-summary)
    - [Xosera Main Register Details (XM Registers)](#xosera-main-register-details-xm-registers)
      - [0x0 **`XM_SYS_CTRL`** (R/W+) - System Control](#0x0-xm_sys_ctrl-rw---system-control)
      - [0x1 **`XM_INT_CTRL`** (R/W+) - Interrupt Control](#0x1-xm_int_ctrl-rw---interrupt-control)
      - [0x2 **`XM_TIMER`** (R/W) - Timer Functions](#0x2-xm_timer-rw---timer-functions)
      - [0x3 **`XM_RD_XADDR`** (R/W+) - XR Read Address](#0x3-xm_rd_xaddr-rw---xr-read-address)
      - [0x4 **`XM_WR_XADDR`** (R/W) - XR Write Address](#0x4-xm_wr_xaddr-rw---xr-write-address)
      - [0x5 **`XM_XDATA`** (R+/W+) - XR Read/Write Data](#0x5-xm_xdata-rw---xr-readwrite-data)
      - [0x6 **`XM_RD_INCR`** (R/W) - Increment for VRAM Read Address](#0x6-xm_rd_incr-rw---increment-for-vram-read-address)
      - [0x7 **`XM_RD_ADDR`** (R/W+) - VRAM Read Address](#0x7-xm_rd_addr-rw---vram-read-address)
      - [0x8 **`XM_WR_INCR`** (R/W) - Increment for VRAM Write Address](#0x8-xm_wr_incr-rw---increment-for-vram-write-address)
      - [0x9 **`XM_WR_ADDR`** (R/W) - VRAM Write Address](#0x9-xm_wr_addr-rw---vram-write-address)
      - [0xA **`XM_DATA`** (R+/W+) - VRAM Read/Write Data](#0xa-xm_data-rw---vram-readwrite-data)
      - [0xB **`XM_DATA_2`** (R+/W+) - VRAM Read/Write Data (2nd)](#0xb-xm_data_2-rw---vram-readwrite-data-2nd)
      - [0xC **`PIXEL_X`** (-/W+) - X coordinate for pixel address/mask generation (also used to set `PIXEL_BASE`)](#0xc-pixel_x--w---x-coordinate-for-pixel-addressmask-generation-also-used-to-set-pixel_base)
      - [0xD **`PIXEL_Y`** (-/W+) - Y coordinate for pixel address/mask generation (also used to set `PIXEL_WIDTH`)](#0xd-pixel_y--w---y-coordinate-for-pixel-addressmask-generation-also-used-to-set-pixel_width)
      - [0xE **`XM_UART`** (R+/W+)](#0xe-xm_uart-rw)
      - [0xF **`XM_FEATURE`** (R/-) - Xosera feature bits](#0xf-xm_feature-r----xosera-feature-bits)
  - [Xosera Extended Register / Extended Memory Region Summary](#xosera-extended-register--extended-memory-region-summary)
    - [Xosera Extended Registers Details (XR Registers)](#xosera-extended-registers-details-xr-registers)
    - [Video Config and Copper XR Registers Summary](#video-config-and-copper-xr-registers-summary)
    - [Video Config and Copper XR Registers Details](#video-config-and-copper-xr-registers-details)
      - [0x00 **`XR_VID_CTRL`** (R/W) - Border Color / Playfield Color-Swap](#0x00-xr_vid_ctrl-rw---border-color--playfield-color-swap)
      - [0x01 **`XR_COPP_CTRL`** (R/W) - Copper Enable](#0x01-xr_copp_ctrl-rw---copper-enable)
      - [0x02 **`XR_AUD_CTRL`** (R/W) - Audio Control](#0x02-xr_aud_ctrl-rw---audio-control)
      - [0x03 **`XR_SCANLINE`** (R/W) - current video scan line/trigger Xosera host CPU video interrupt](#0x03-xr_scanline-rw---current-video-scan-linetrigger-xosera-host-cpu-video-interrupt)
      - [0x04 **`XR_VID_LEFT`** (R/W) - video display window left edge](#0x04-xr_vid_left-rw---video-display-window-left-edge)
      - [0x05 **`XR_VID_RIGHT`** (R/W) - video display window right edge](#0x05-xr_vid_right-rw---video-display-window-right-edge)
      - [0x06 **`XR_POINTER_H`** (-/W+) - pointer sprite H position](#0x06-xr_pointer_h--w---pointer-sprite-h-position)
      - [0x07 **`XR_POINTER_V`** (-/W+) - pointer sprite V position and colormap select](#0x07-xr_pointer_v--w---pointer-sprite-v-position-and-colormap-select)
      - [0x08 **`XR_UNUSED_08`** (--) - unused XR register](#0x08-xr_unused_08------unused-xr-register)
      - [0x09 **`XR_UNUSED_09`** (--) - unused XR register](#0x09-xr_unused_09------unused-xr-register)
      - [0x0A **`XR_UNUSED_0A`** (--) - unused XR register](#0x0a-xr_unused_0a------unused-xr-register)
      - [0x0B **`XR_UNUSED_0B`** (--) - unused XR register](#0x0b-xr_unused_0b------unused-xr-register)
      - [0x0C **`XR_UNUSED_0C`** (--) - unused XR register](#0x0c-xr_unused_0c------unused-xr-register)
      - [0x0D **`XR_UNUSED_0D`** (--) - unused XR register](#0x0d-xr_unused_0d------unused-xr-register)
      - [0x0E **`XR_UNUSED_0E`** (--) - unused XR register](#0x0e-xr_unused_0e------unused-xr-register)
      - [0x0F **`XR_UNUSED_0F`** (--) - unused XR register](#0x0f-xr_unused_0f------unused-xr-register)
    - [Playfield A \& B Control XR Registers Summary](#playfield-a--b-control-xr-registers-summary)
    - [Playfield A \& B Control XR Registers Details](#playfield-a--b-control-xr-registers-details)
      - [Bitmap Display Formats](#bitmap-display-formats)
      - [Tile Display Formats](#tile-display-formats)
      - [Final Color Index Selection](#final-color-index-selection)
      - [Color Look-up and Playfield Blending](#color-look-up-and-playfield-blending)
    - [2D Blitter Engine Operation](#2d-blitter-engine-operation)
      - [Logic Operation Applied to Blitter Operations](#logic-operation-applied-to-blitter-operations)
      - [Transparency Testing and Masking Applied to Blitter Operations](#transparency-testing-and-masking-applied-to-blitter-operations)
      - [Nibble Shifting and First/Last Word Edge Masking](#nibble-shifting-and-firstlast-word-edge-masking)
      - [Blitter Operation Status](#blitter-operation-status)
    - [2D Blitter Engine XR Registers Summary](#2d-blitter-engine-xr-registers-summary)
      - [2D Blitter Engine XR Registers Details](#2d-blitter-engine-xr-registers-details)
  - [Video Synchronized Co-Processor Details](#video-synchronized-co-processor-details)
    - [Programming the Co-processor](#programming-the-co-processor)
    - [Co-processor Instruction Set](#co-processor-instruction-set)
      - [Notes on the MOVE variants](#notes-on-the-move-variants)
    - [Co-processor Assembler](#co-processor-assembler)
    - [Default TILE, COLOR and COPPER Memory Contents](#default-tile-color-and-copper-memory-contents)

## Xosera Reference Information

Xosera uses an 8-bit parallel bus with 4 bits to select from one of 16 main 16-bit bus accessable registers (and a bit to select the even or odd register half, using 68000 big-endian convention of high byte at even addresses). Since Xosera uses an 8-bit bus, for low-level access on 68K systems multi-byte operations can utilize the `MOVEP.W` or `MOVEP.L` instructions that skip the unused half of the 16-bit data word (also known as a 8-bit "6800" style peripheral bus). When this document mentions, for example "writing a word", it means writing to the even and then odd bytes of a register (usually with `MOVE.P` but multiple `MOVE.B` instructions work also). For some registers there is a "special action" that happens on a 16-bit word when the odd (low byte) is either read or written (generally noted).

Xosera's 128KB of VRAM is organized as 64K x 16-bit words, so a full VRAM address is 16-bits (and an individual byte is not directly accessible, only 16-bit words, however write "nibble masking" is available).

In addition to the main registers and VRAM, there is an additional extended register / memory XR bus that provides access to control registers for system control, video configuration, drawing engines and display co-processor as well as additional memory regions for tile definitions, audio waveforms, color look-up and display coprocessor instructions.

___

### Xosera Main Registers (XM Registers) Summary

| Reg # | Reg Name          | Access | Description                                                                           |
|-------|-------------------|--------|---------------------------------------------------------------------------------------|
| 0x0   | **`XM_SYS_CTRL`** | R /W+  | Status flags, write to [15:8] inits `PIXEL_X/Y`, [3:0] VRAM write nibble mask         |
| 0x1   | **`XM_INT_CTRL`** | R /W+  | FPGA reconfigure, interrupt masking, interrupt status                                 |
| 0x2   | **`XM_TIMER`**    | R /W+  | Read 16-bit tenth millisecond timer (1/10,000 second), write 8-bit interval timer     |
| 0x3   | **`XM_RD_XADDR`** | R /W+  | XR register/address used for `XM_XDATA` read access                                   |
| 0x4   | **`XM_WR_XADDR`** | R /W   | XR register/address used for `XM_XDATA` write access                                  |
| 0x5   | **`XM_XDATA`**    | R+/W+  | Read from `XM_RD_XADDR` or write to `XM_WR_XADDR` (and increment address by 1)        |
| 0x6   | **`XM_RD_INCR`**  | R /W   | increment value for `XM_RD_ADDR` read from `XM_DATA`/`XM_DATA_2`                      |
| 0x7   | **`XM_RD_ADDR`**  | R /W+  | VRAM address for reading from VRAM when `XM_DATA`/`XM_DATA_2` is read                 |
| 0x8   | **`XM_WR_INCR`**  | R /W   | increment value for `XM_WR_ADDR` on write to `XM_DATA`/`XM_DATA_2`                    |
| 0x9   | **`XM_WR_ADDR`**  | R /W   | VRAM address for writing to VRAM when `XM_DATA`/`XM_DATA_2` is written                |
| 0xA   | **`XM_DATA`**     | R+/W+  | read/write VRAM word at `XM_RD_ADDR`/`XM_WR_ADDR` (and add `XM_RD_INCR`/`XM_WR_INCR`) |
| 0xB   | **`XM_DATA_2`**   | R+/W+  | 2nd `XM_DATA`(to allow for 32-bit read/write access)                                  |
| 0xC   | **`PIXEL_X`**     | - /W+  | X pixel sets `WR_ADDR` and nibble mask (also `PIXEL_BASE` for `XM_SYS_CTRL` write)    |
| 0xD   | **`PIXEL_Y`**     | - /W+  | Y pixel sets `WR_ADDR` and nibble mask (also `PIXEL_WIDTH` for `XM_SYS_CTRL` write)   |
| 0xE   | **`XM_UART`**     | R+/W+  | USB UART using FTDI chip in UPduino for additional 1 Mbps USB connection to PC *[1]*  |
| 0xF   | **`XM_FEATURE`**  | R /-   | Feature bits                                                                          |

(`R+` or `W+` indicates that reading or writing this register can have additional "side effects", respectively)
*[1]* USB UART is an optional debug convenience feature, since FTDI chip was present on UPduino.  It may not always be present.
___

### Xosera Main Register Details (XM Registers)

#### 0x0 **`XM_SYS_CTRL`** (R/W+) - System Control

<img src="./pics/wd_XM_SYS_CTRL.svg">

**Status bits for memory, blitter, hblank, vblank, `PIXEL_X/Y` setup and VRAM nibble write masking**

| Name          | Bits    | R/W  | Description                                                                     |
|---------------|---------|------|---------------------------------------------------------------------------------|
| `MEM_WAIT`    | `[15]`  | R/-  | memory read/write operation still in progress (for contended memory)            |
| `BLIT_FULL`   | `[14]`  | R/-  | blit queue full (can't safely write to blit registers when set)                 |
| `BLIT_BUSY`   | `[13]`  | R/-  | blit busy (blit operations not fully completed, but queue may be empty)         |
| `HBLANK`      | `[11]`  | R/-  | horizontal blank flag (i.e., current pixel is not visible, off left/right edge) |
| `VBLANK`      | `[10]`  | R/-  | vertical blank flag (i.e., current line is not visible, off top/bottom edge)    |
| `PIX_NO_MASK` | `[9]`   | R/W+ | `PIXEL_X/Y` won't set `WR_MASK` (low two bits of `PIXEL_X` ignored)             |
| `PIX_8B_MASK` | `[8]`   | R/W+ | `PIXEL_X/Y` 8-bit pixel mask for WR_MASK (on even 4-BPP coordinates)            |
| `WR_MASK`     | `[3:0]` | R/W  | `XM_DATA`/`XM_DATA_2` VRAM nibble write mask (see below)                        |

When bits `[15:8]` (even/upper byte) of `SYS_CTRL` is written, besides setting`PIXEL_X/Y` address generation options `PIX_NO_MASK` and `PIX_8B_MASK`, it also will initialize the internal registers `PIXEL_BASE` (base VRAM address) and `PIXEL_WIDTH` (width of line in words). `PIXEL_X` will be copied into `PIXEL_BASE` and `PIXEL_Y` into `PIXEL_WIDTH` (so generally they should be set before writing bits `[15:8]`).

> :mag: **`PIX_8B_MASK`** The X coordinate of `PIXEL_X` assumes 4-bit pixels, but if `PIX_8B_MASK` is enabled in `SYS_CTRL`, then mask will be set for 2 nibbles (8-bits) on even coordinates for easy use with 8-bit pixels.

> :mag: **`PIX_NO_MASK`** If `PIX_NO_MASK` is enabled in `SYS_CTRL`, then when writing to `PIXEL_X` or `PIXEL_Y` it will only update `WR_ADDR` and not the `SYS_CTRL` nibble write mask.  This is useful to allow writing the entire VRAM word.  This will make the lower two bits of `PIXEL_X` effectively ignored.

> :mag: **VRAM write mask:**  When a bit corresponding to a given nibble is zero, writes to that nibble are ignored and the original nibble is retained. For example, if the nibble mask is `0010` then only bits `[7:4]` in a word would be over-written writing to VRAM via `XM_DATA`/`XM_DATA_2` (other nibbles will be unmodified).  This can be useful to isolate pixels within a word of VRAM (without needing read, modify, write).  This can also be set automatically by `PIXEL_X/Y` (to isolate a specific 4-bit or 8-bit pixel via X, Y coordinate).

#### 0x1 **`XM_INT_CTRL`** (R/W+) - Interrupt Control

<img src="./pics/wd_XM_INT_CTRL.svg">  

**FPGA reconfigure, interrupt masking and interrupt status.**

| Name         | Bits   | R/W  | Description                                                             |
|--------------|--------|------|-------------------------------------------------------------------------|
| `RECONFIG`   | `[15]` | -/W+ | Reconfigure FPGA with `SYS_CTRL` `WR_MASK` `[1:0]` as new configuration |
| `BLIT_EN`    | `[14]` | R/W  | enable interrupt for blitter queue empty                                |
| `TIMER_EN`   | `[13]` | R/W  | enable interrupt for countdown timer                                    |
| `VIDEO_EN`   | `[12]` | R/W  | enable interrupt for video (vblank/COPPER)                              |
| `AUD3_EN`    | `[11]` | R/W  | enable interrupt for audio channel 3 ready                              |
| `AUD2_EN`    | `[10]` | R/W  | enable interrupt for audio channel 2 ready                              |
| `AUD1_EN`    | `[9]`  | R/W  | enable interrupt for audio channel 1 ready                              |
| `AUD0_EN`    | `[8]`  | R/W  | enable interrupt for audio channel 0 ready                              |
|              | `[7]`  | -/-  |                                                                         |
| `BLIT_INTR`  | `[6]`  | R/W  | interrupt pending for blitter queue empty                               |
| `TIMER_INTR` | `[5]`  | R/W  | interrupt pending for countdown timer                                   |
| `VIDEO_INTR` | `[4]`  | R/W  | interrupt pending for video (vblank/COPPER)                             |
| `AUD3_INTR`  | `[3]`  | R/W  | interrupt pending for audio channel 3 ready                             |
| `AUD2_INTR`  | `[2]`  | R/W  | interrupt pending for audio channel 2 ready                             |
| `AUD1_INTR`  | `[1]`  | R/W  | interrupt pending for audio channel 1 ready                             |
| `AUD0_INTR`  | `[0]`  | R/W  | interrupt pending for audio channel 0 ready                             |

> :mag: **Xosera Reconfig:** Writing a 1 to bit `[15]` will immediately reset and reconfigure the Xosera FPGA into one of four FPGA configurations stored in flash memory.  Normally, default config #0 is standard VGA 640x480 and config #1 is 848x480 wide-screen 16:9 (the other configurations are user defined and it will fall back to config #0 on missing or invalid configurations).  The new configuration is selected with bits `[1:0]` in `XM_SYS_CTRL` (low two VRAM mask bits) prior to setting the `RECONFIG` bit. Reconfiguration can take up to 100ms and during this period Xosera will be unresponsive (and no display will be generated, so monitor will blank).  All register settings, VRAM, TILEMEM and other memory on Xosera will be reset and restored to defaults.

#### 0x2 **`XM_TIMER`** (R/W) - Timer Functions

<img src="./pics/wd_XM_TIMER.svg">

**Tenth millisecond timer / 8-bit countdown timer.**  

**Read 16-bit timer, increments every 1/10<sup>th</sup> of a millisecond (10,000 Hz)**  
Can be used for fairly accurate timing. Internal fractional value is maintined (so as accurate as FPGA clock).  Can be used for elapsed time up to ~6.5 seconds (or unlimited, if the cumulative elapsed time is updated at least as often as timer wrap value).

> :mag: **`TIMER` atomic read:** To ensure an atomic 16-bit value, when the high byte of `TIMER` is read, the low byte is saved into an internal register and returned when `TIMER` low byte is read. Because of this, reading the full 16-bit `TIMER` register is recommended (or first even byte, then odd byte, or odd byte value may not be updated).

**Write to set 8-bit countdown timer interval**  
When written, the lower 8 bits sets a write-only 8-bit countdown timer interval.  Timer is decremented every 1/10<sup>th</sup>  millisecond and when timer reaches zero an Xosera timer interrupt (`TIMER_INTR`) will be generated and the count will be reset.  This register setting has no effect on the `TIMER` read (only uses same 1/10<sup>th</sup> ms time-base).

> :mag: **8-bit countdown timer** interval can only be written (as a read will return the free running `TIMER` value).  It is only useful to generate an interrupt (or polling bit in `XM_INT_CTRL`)

#### 0x3 **`XM_RD_XADDR`** (R/W+) - XR Read Address

<img src="./pics/wd_XM_RD_XADDR.svg">

 **XR Register / Memory Read Address**

**Extended register or memory address for data *read* via `XM_XDATA`**  
Specifies the XR register or XR region address to be read or written via `XM_XDATA`.  As soon as this register is written, a 16-bit read operation will take place on the register or memory region specified (and the data wil be made available from `XM_XDATA`).

#### 0x4 **`XM_WR_XADDR`** (R/W) - XR Write Address

<img src="./pics/wd_XM_WR_XADDR.svg">

**XR Register / Memory Write Address for data *written* to `XM_XDATA`**  
Specifies the XR register or XR region address to be accessed via `XM_XDATA`.  
The register ordering with `XM_WR_XADDR` followed by `XM_XDATA` allows a 32-bit write to set both the XR register/address `rrrr`  and the immediate word value `XXXX`, similar to:  

```text
MOVE.L #$rrrrXXXX,D0
MOVEP.L D0,XR_WR_XADDR(A1)
```

#### 0x5 **`XM_XDATA`** (R+/W+) - XR Read/Write Data

<img src="./pics/wd_XM_XDATA.svg">

**XR Register / Memory Read Data or Write Data**  
Read XR register/memory value from `XM_RD_XADDR` or write value to `XM_WR_XADDR` and increment `XM_RD_XADDR`/`XM_WR_XADDR`, respectively.  
When `XM_XDATA` is read, data pre-read from XR address `XM_RD_XADDR` is returned and then `XM_RD_XADDR` is incremented by one and pre-reading the next word begins.  
When `XM_XDATA` is written, value is written to XR address `XM_WR_XADDR` and then `XM_WR_XADDR` is incremented by one.

#### 0x6 **`XM_RD_INCR`** (R/W) - Increment for VRAM Read Address

<img src="./pics/wd_XM_RD_INCR.svg">

**Increment value for `XM_RD_ADDR` when `XM_DATA`/`XM_DATA_2` is read from**  
Twos-complement value added to `XM_RD_ADDR` address when `XM_DATA` or `XM_DATA_2` is read from.  
Allows quickly reading Xosera VRAM from `XM_DATA`/`XM_DATA_2` when using a fixed increment.

> :mag: **32-bit access:** Xosera treats 32-bit long access (e.g. `MOVEP.L` from `XM_DATA`) as two 16-bit accesses, so the increment would be the same as with 16-bit word access (but it will be applied twice, once for `XM_DATA` and once for `XM_DATA_2`).

#### 0x7 **`XM_RD_ADDR`** (R/W+) - VRAM Read Address

<img src="./pics/wd_XM_RD_ADDR.svg">

**VRAM *read* address for `XM_DATA`/`XM_DATA_2`**  
VRAM address that will be read when `XM_DATA` or `XM_DATA_2` register is read from.
When `XM_RD_ADDR` is written (or when auto incremented) the corresponding word in VRAM is pre-read and made
available at `XM_DATA`/`XM_DATA_2`.

#### 0x8 **`XM_WR_INCR`** (R/W) - Increment for VRAM Write Address

<img src="./pics/wd_XM_WR_INCR.svg">

**Increment value for `XM_WR_ADDR` when `XM_DATA`/`XM_DATA_2` is written.**  
Twos-complement value added to `XM_WR_ADDR` when `XM_DATA` or `XM_DATA_2` is written to.  
Allows quickly writing to Xosera VRAM via `XM_DATA`/`XM_DATA_2` when using a fixed increment.

> :mag: **32-bit access:** Xosera treats 32-bit long access (e.g. `MOVEP.L` to `XM_DATA`) as two 16-bit accesses, so the increment would be the same as with 16-bit word access (but it will be applied twice, once for `XM_DATA` and once for `XM_DATA_2`).

#### 0x9 **`XM_WR_ADDR`** (R/W) - VRAM Write Address

<img src="./pics/wd_XM_WR_ADDR.svg">

 **VRAM *write* address for `XM_DATA`/`XM_DATA_2`**  
Specifies VRAM address used when writing to VRAM via `XM_DATA`/`XM_DATA_2`. Writing a value here does not cause any VRAM access (which happens when data *written* to `XM_DATA` or `XM_DATA_2`).

#### 0xA **`XM_DATA`** (R+/W+) - VRAM Read/Write Data

<img src="./pics/wd_XM_DATA.svg">

**VRAM memory value *read* from `XM_RD_ADDR` or value to *write* to `XM_WR_ADDR`**  
When `XM_DATA` is read, data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the new VRAM address begins.  
When `XM_DATA` is written, the value is written to VRAM at `XM_WR_ADDR` and `XM_WR_INCR` is added to `XM_WR_ADDR`.

#### 0xB **`XM_DATA_2`** (R+/W+) - VRAM Read/Write Data (2nd)

<img src="./pics/wd_XM_DATA.svg">

**VRAM memory value *read* from `XM_RD_ADDR` or value to *write* to `XM_WR_ADDR`**  
When `XM_DATA_2` is read, data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the new VRAM address begins.  
When `XM_DATA_2` is written, the value is written to VRAM at `XM_WR_ADDR` and `XM_WR_INCR` is added to `XM_WR_ADDR`.
> :mag: **`XM_DATA_2`** This register is treated *identically* to `XM_DATA` and is intended to allow for 32-bit "long" `MOVEP.L` transfers to/from `XM_DATA` for additional transfer speed (back-to-back 16-bit data transfers with one instruction).

#### 0xC **`PIXEL_X`** (-/W+) - X coordinate for pixel address/mask generation (also used to set `PIXEL_BASE`)

<img src="./pics/wd_XM_PIXEL_X.svg">

**signed X coordinate for pixel addressing, write sets `WR_ADDR` and `SYS_CTRL` `WR_MASK` nibble mask to isolate pixel**

Used to allow using pixel coordinates to calculate VRAM write address and also isolate a specific 4-bit or 8-bit pixel to update. Upon write, sets:  
 `WR_ADDR = PIXEL_BASE + (PIXEL_Y * PIXEL_WIDTH) + (PIXEL_X >> 2)`  
 Also sets `SYS_CTRL` nibble write mask (unless `PIX_NO_MASK` was set in `SYS_CTRL`) to binary:  
 `1p00 >> PIXEL_X[1:0]` (the `p` can be `0` for 4-BPP or `1` for 8-BPP on even X coordinates).  
 The value in this register is also used to set `PIXEL_BASE` when `SYS_CTRL[15:8]` is written to (see `SYS_CTRL`).  Generally this needs to be done before using the pixel address generation feature.

> :mag: **`WR_ADDR` and `SYS_CTRL` write mask** These registers will generally be altered immediately whenever this register is altered, keep this in mind (especially it is easy to unintentionally alter the write mask, causing issues later with garbed writes to VRAM if it is not restored to `0xF`).  Also `SYS_CTRL` `PIX_NO_MASK` set will prevent the mask from being altered.

#### 0xD **`PIXEL_Y`** (-/W+) - Y coordinate for pixel address/mask generation (also used to set `PIXEL_WIDTH`)

<img src="./pics/wd_XM_PIXEL_Y.svg">

**signed Y coordinate for pixel addressing, write sets `WR_ADDR` and `SYS_CTRL` `WR_MASK` nibble mask to isolate pixel**

Used to allow using pixel coordinates to calculate VRAM write address and also isolate a specific 4-bit or 8-bit pixel to update. Upon write, sets:  
 `WR_ADDR = PIXEL_BASE + (PIXEL_Y * PIXEL_WIDTH) + (PIXEL_X >> 2)`  
 Also sets `SYS_CTRL` nibble write mask (unless `PIX_NO_MASK` was set in `SYS_CTRL`) to binary:  
 `1p00 >> PIXEL_X[1:0]` (the `p` can be `0` for 4-BPP or `1` for 8-BPP on even X coordinates).  
The value in this register is also used to set `PIXEL_WIDTH` when `SYS_CTRL[15:8]` is written to (see `SYS_CTRL`).  Generally this needs to be done before using the pixel address generation feature.

> :mag: **`WR_ADDR` and `SYS_CTRL` write mask** These registers will generally be altered immediately whenever this register is altered, keep this in mind (especially it is easy to unintentionally alter the write mask, causing issues later with garbed writes to VRAM if it is not restored to `0xF`).  If `SYS_CTRL` `PIX_NO_MASK` is set, the mask will not be altered.

#### 0xE **`XM_UART`** (R+/W+)

<img src="./pics/wd_XM_UART.svg">

**UART communication via USB using FTDI**
Basic UART allowing send/receive communication with host PC at 230,400 bps (aka 230.4 Kbaud) via FTDI USB, intended mainly to aid debugging.  Software polled, so this may limit effective incoming data rate.  Typically this register is read as individual bytes, reading the even status byte bits `[15:8]`then reading/writing the odd data byte bits `[7:0]` accordingly.  If `RXF` (receive buffer full) bit is set, then a data byte is waiting to be read from lower odd data byte (which will clear `RXF`). If `TXF` (transmit buffer full) is clear, then a byte can be transmitted by writing it to the lower odd data byte (which will set `TXF` until data has finished transmitting).  This UART can also be configured in the design as transmit only (to save resources).

> :mag: **USB UART** This register is an optional debug feature and may not always be present (in which case this register is ignored and will read as all zero).  THe `XM_FEATURE` has a `UART` bit which will be set if UART is present.

#### 0xF **`XM_FEATURE`** (R/-) - Xosera feature bits

<img src="./pics/wd_XM_FEATURE.svg">

**Xosera configured features (when reading)**
| Name      | Bits      | R/W | Description                                                     |
|-----------|-----------|-----|-----------------------------------------------------------------|
| `CONFIG`  | `[15:12]` | R/- | Current configuration number for Xosera FPGA (0-3 on iCE40UP5K) |
| `AUDCHAN` | `[11:8]`  | R/- | Number of audio output channels (normally 4)                    |
| `UART`    | `[7]`     | R/- | Debug UART is present                                           |
| `PF_B`    | `[6]`     | R/- | Playfield B enabled (2nd playfield to blend over playfield A)   |
| `BLIT`    | `[5]`     | R/- | 2D "blitter engine" enabled                                     |
| `COPP`    | `[4]`     | R/- | Screen synchronized co-processor enabled                        |
| `MONRES`  | `[3:0]`   | R/- | Monitor resolution (0=640x480 4:3, 1=848x480 16:9 on iCE40UP5K) |

> :mag: These can be used to (e.g.) quickly check for presence of a feature, or to detect the current monitor mode.  There is more detailed Xosera configuration information (including feature string, version code, build date and git hash) stored in the last 128 words of copper memory (`XR_COPPER_ADDR+0x580`) after initialization or reconfig.

___

## Xosera Extended Register / Extended Memory Region Summary

| XR Region Name    | XR Region Range | R/W | Description                                   |
|-------------------|-----------------|-----|-----------------------------------------------|
| XR video config   | 0x0000-0x000F   | R/W | config XR registers                           |
| XR playfield A    | 0x0010-0x0017   | R/W | playfield A XR registers                      |
| XR playfield B    | 0x0018-0x001F   | R/W | playfield B XR registers                      |
| XR audio control  | 0x0020-0x002F   | -/W | audio channel XR registers                    |
| XR blit engine    | 0x0040-0x004B   | -/W | 2D-blit engine XR registers                   |
| `XR_TILE_ADDR`    | 0x4000-0x53FF   | R/W | 5KW 16-bit tilemap/tile storage memory        |
| `XR_COLOR_ADDR`   | 0x8000-0x81FF   | R/W | 2 x 256W 16-bit color lookup memory (0xARGB)  |
| `XR_POINTER_ADDR` | 0x8200-0x82FF   | -/W | 256W 16-bit 32x32 4-BPP pointer sprite bitmap |
| `XR_COPPER_ADDR`  | 0xC000-0xC5FF   | R/W | 1.5KW 16-bit copper memory                    |

To access an XR register or XR memory address, write the XR register number or address to `XM_RD_XADDR` or `XM_WR_XADDR` then read or write (respectively) to `XM_XDATA`. Each word read or written to `XM_XDATA` will also automatically increment `XM_RD_XADDR` or `XM_WR_XADDR` (respectively) for contiguous reads or writes.  
While almost all XR memory regions can be read (except `POINTER`), when there is high memory contention (e.g., it is being used for video generation or other use), there is a `mem_wait` bit in `XM_SYS_CTRL` that will indicate when the last memory operation is still pending.  Usually this is not needed when writing, but can be needed reading (e.g., this is generally needed reading from COLOR memory, since it is used for every active pixel, even when the screen is blanked or inactive for the border color).
Also note that unlike the 16 main `XM` registers, the XR region should only be accessed as full 16-bit words (either reading or writing both bytes). The full 16-bits of the `XM_XDATA` value are pre-read when `XM_RD_XADDR` is written or incremented and a full 16-bit word is written when the odd (low-byte) of `XM_XDATA` is written (the even/upper byte is stored until the odd/lower byte).
___

### Xosera Extended Registers Details (XR Registers)

This XR registers are used to control of most Xosera operation other than CPU VRAM access and a few miscellaneous control functions (which accessed directly via the main registers).  
To write to these XR registers, write an XR address to `XM_WR_XADDR` then write data to `XM_XDATA`. Each write to `XM_XDATA` will increment `XM_WR_XADDR` address by one (so you can repeatedly write to `XR_XDATA` for contiguous registers or XR memory). Reading is similar, write the address to `XM_RD_XADDR` then read `XM_XDATA`. Each read of `XM_XDATA` will increment `XM_RD_XADDR` address by one (so you can repeatedly read `XR_XDATA` for contiguous registers or XR memory).

### Video Config and Copper XR Registers Summary

| Reg # | Reg Name           | R /W  | Description                                                               |
|-------|--------------------|-------|---------------------------------------------------------------------------|
| 0x00  | **`XR_VID_CTRL`**  | R /W  | Border color index and playfield color swap                               |
| 0x01  | **`XR_COPP_CTRL`** | R /W  | Display synchronized coprocessor control                                  |
| 0x02  | **`XR_AUD_CTRL`**  | R /W  | Audio channel and DMA processing control                                  |
| 0x03  | **`XR_SCANLINE`**  | R /W+ | Current display scanline / Trigger video interrupt                        |
| 0x04  | **`XR_VID_LEFT`**  | R /W  | Left edge start of active display window (normally 0)                     |
| 0x05  | **`XR_VID_RIGHT`** | R /W  | Right edge + 1 end of active display window (normally 640 or 848)         |
| 0x06  | **`XR_POINTER_H`** | - /W+ | pointer sprite H position (in native sceeen coordinates)                  |
| 0x07  | **`XR_POINTER_V`** | - /W+ | pointer sprite V position (in native sceeen coordinates) and color select |
| 0x08  | **`XR_UNUSED_08`** | - /-  |                                                                           |
| 0x09  | **`XR_UNUSED_09`** | - /-  |                                                                           |
| 0x0A  | **`XR_UNUSED_0A`** | - /-  |                                                                           |
| 0x0B  | **`XR_UNUSED_0B`** | - /-  |                                                                           |
| 0x0C  | **`XR_UNUSED_0C`** | - /-  |                                                                           |
| 0x0D  | **`XR_UNUSED_0D`** | - /-  |                                                                           |
| 0x0E  | **`XR_UNUSED_0E`** | - /-  |                                                                           |
| 0x0F  | **`XR_UNUSED_0F`** | - /-  |                                                                           |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)
___

### Video Config and Copper XR Registers Details

#### 0x00 **`XR_VID_CTRL`** (R/W) - Border Color / Playfield Color-Swap

<img src="./pics/wd_XR_VID_CTRL.svg">

**Border or blanked display color index and playfield A/B colormap swap**

| Name      | Bits    | R/W | Description                                                           |
|-----------|---------|-----|-----------------------------------------------------------------------|
| `SWAP_AB` | `[15]`  | R/W | Swap playfield colormaps (PF A uses colormap B, PF B uses colormap A) |
| `BORDCOL` | `[7:0]` | R/W | Color index for border or blanked pixels (for playfield A)            |

> :mag: **`SWAP_AB`** `SWAP_AB` effectively changes the layer order, so playfield A will be blended over playfield B.  

> :mag: **Playfield B border color** Playfield B always uses index 0 for its border or blanked pixels.  Index 0 is normally set to be transparent in colormap B (alpha of 0) to allow playfield A (or playfield A `BORDCOL`) to be visible under playfield B border or blanked pixels.

#### 0x01 **`XR_COPP_CTRL`** (R/W) - Copper Enable

<img src="./pics/wd_XR_COPP_CTRL.svg">

**Enable or disable copper program execution**
| Name      | Bits   | R/W | Description                                                            |
|-----------|--------|-----|------------------------------------------------------------------------|
| `COPP_EN` | `[15]` | R/W | Reset and enable copper at start of next frame (and subsequent frames) |

Setting `COPP_EN` allows control of the copper co-processor.  When disabled, copper execution stops immediately.  When enabled the copper execution state is reset, and at the beginning of the next frame (line 0, off the left edge) will start program execution at location `0x0000` (and the same at the start of each subsequent frame until disabled).  Unless care is taken when modifying running copper code, it is advised to disable the copper before modifying copper memory to avoid unexpected Xosera register or memory modifications (e.g., when uploading a new copper program, disable `COPP_EN` and re-enable it when the upload is complete and ready to run).

> :mag: **Copper:** At start of each frame, copper register `RA` will be reset to`0x0000`, the `B` flag set and copper PC set to XM address `0xC000` (first word of copper memory). See section below for details about copper operation.

#### 0x02 **`XR_AUD_CTRL`** (R/W) - Audio Control

<img src="./pics/wd_XR_AUD_CTRL.svg">

**Enable or disable audio channel processing**
| Name     | Bits  | R/W | Description                              |
|----------|-------|-----|------------------------------------------|
| `AUD_EN` | `[0]` | R/W | Enable audio DMA and channel procerssing |

#### 0x03 **`XR_SCANLINE`** (R/W) - current video scan line/trigger Xosera host CPU video interrupt

<img src="./pics/wd_XR_SCANLINE.svg">

Continuously updated with the scanline, lines 0-479 are visible (others are in vertical blank).  Each line starts with some non-visible pixels off the left edge of the display (160 pixels in 640x480 or 240 in 848x480).

A write to this register will trigger Xosera host CPU video interrupt (if unmasked and one is not already pending in `XM_INT_CTRL`).  This is mainly useful to allow COPPER to generate an arbitrary screen position synchronized CPU interrupt (in addition to the normal end of visible display v-blank interrupt).

#### 0x04 **`XR_VID_LEFT`** (R/W) - video display window left edge

<img src="./pics/wd_XR_VID_LEFT.svg">

Defines left-most native pixel of video display window, 0 to monitor native width-1 (normally 0 for full-screen).

#### 0x05 **`XR_VID_RIGHT`** (R/W) - video display window right edge  

<img src="./pics/wd_XR_VID_RIGHT.svg">

Defines right-most native pixel of video display window +1, 1 to monitor native width (normally `640` or `848` for 4:3 or 16:9
full-screen, respectively).

#### 0x06 **`XR_POINTER_H`** (-/W+) - pointer sprite H position

<img src="./pics/wd_XR_POINTER_H.svg">

Defines position of left-most pixel of 32x32 pointer sprite in native horizontal display pixel coordinates (including offscreen pixels, so value is offset from visible, see note below).  
The pointer image is a 32x32 pixel 4-bit per pixel bitmap (256 words, stored as normal) in write-only XR memory at `XR_POINTER_ADDR`. A pointer image nibble of `0` is transparent, others are combined with upper 4-bit colormap select set in `XR_POINTER_V` to form an 8-bit index overriding playfield A using colormap A (with normal playfield A blending).

> :mag: **POINTER_H notes** All 32 horizontal pixels of the pointer are fully visible at horizontal position `160-6` in `640x480` mode or `240-6` in `848x480` mode.  Values less than this will clip the pointer or hide it offscreen.  Similar for values on the right edge of the screen (add `640` or `848`).

#### 0x07 **`XR_POINTER_V`** (-/W+) - pointer sprite V position and colormap select

<img src="./pics/wd_XR_POINTER_V.svg">

Defines position of top-most line of 32x32 pointer sprite in native vertical display line coordinates.  
The pointer image is a 32x32 pixel 4-bit per pixel bitmap (256 words, stored as normal) in write-only XR memory at `XR_POINTER_ADDR`. A pointer image nibble of `0` is transparent, others are combined with upper 4-bit colormap select set in `XR_POINTER_V` to form an 8-bit index overriding playfield A using colormap A (with normal playfield A blending).

> :mag: **POINTER_V notes** All 32 vertical lines of of the pointer are fully visible at vertical position 0.  To clip the pointer off the top of the screen, V position needs to be `525-n` for `640x480` mode or `517-n` for `848x480` mode (where n is 1-32 lines to clip).  The pointer clips normally off the bottom of the screen (becoming fully hidden at 480).

#### 0x08 **`XR_UNUSED_08`** (--) - unused XR register

#### 0x09 **`XR_UNUSED_09`** (--) - unused XR register

#### 0x0A **`XR_UNUSED_0A`** (--) - unused XR register

#### 0x0B **`XR_UNUSED_0B`** (--) - unused XR register

#### 0x0C **`XR_UNUSED_0C`** (--) - unused XR register

#### 0x0D **`XR_UNUSED_0D`** (--) - unused XR register

#### 0x0E **`XR_UNUSED_0E`** (--) - unused XR register

#### 0x0F **`XR_UNUSED_0F`** (--) - unused XR register

___

### Playfield A & B Control XR Registers Summary

| Reg # | Name              | R/W | Description                                             |
|-------|-------------------|-----|---------------------------------------------------------|
| 0x10  | `XR_PA_GFX_CTRL`  | R/W | playfield A graphics control                            |
| 0x11  | `XR_PA_TILE_CTRL` | R/W | playfield A tile control                                |
| 0x12  | `XR_PA_DISP_ADDR` | R/W | playfield A display VRAM start address (start of frame) |
| 0x13  | `XR_PA_LINE_LEN`  | R/W | playfield A display line width in words                 |
| 0x14  | `XR_PA_HV_FSCALE` | R/W | playfield A horizontal and vertical fractional scaling  |
| 0x15  | `XR_PA_HV_SCROLL` | R/W | playfield A horizontal and vertical fine scroll         |
| 0x16  | `XR_PA_LINE_ADDR` | -/W | playfield A scanline start address (start of next line) |
| 0x17  | `XR_PA_UNUSED_17` | -/- |                                                         |
| 0x18  | `XR_PB_GFX_CTRL`  | R/W | playfield B graphics control                            |
| 0x19  | `XR_PB_TILE_CTRL` | R/W | playfield B tile control                                |
| 0x1A  | `XR_PB_DISP_ADDR` | R/W | playfield B display VRAM start address (start of frame) |
| 0x1B  | `XR_PB_LINE_LEN`  | R/W | playfield B display line width in words                 |
| 0x1C  | `XR_PB_HV_FSCALE` | R/W | playfield B horizontal and vertical fractional scaling  |
| 0x1D  | `XR_PB_HV_SCROLL` | R/W | playfield B horizontal and vertical fine scroll         |
| 0x1E  | `XR_PB_LINE_ADDR` | -/W | playfield B scanline start address (start of next line) |
| 0x1F  | `XR_PB_UNUSED_1F` | -/- |                                                         |
___

### Playfield A & B Control XR Registers Details

**0x10 `XR_PA_GFX_CTRL` (R/W)** - playfield A (base) graphics control  
**0x18 `XR_PB_GFX_CTRL` (R/W)** - playfield B (overlay) graphics control

<img src="./pics/wd_XR_Px_GFX_CTRL.svg">

**playfield A/B graphics control**
| Name        | Bits     | R/W | Description                                                                    |
|-------------|----------|-----|--------------------------------------------------------------------------------|
| `V_REPEAT`  | `[1:0]`  | R/W | Vertical pixel repeat count (0=1x, 1=2x, 2=3x, 3=4x)                           |
| `H_REPEAT`  | `[3:2]`  | R/W | Horizontal pixel repeat count (0=1x, 1=2x, 2=3x, 3=4x)                         |
| `BPP`       | `[5:4]`  | R/W | Bits-Per-Pixel for color indexing (0=1 BPP, 1=4 BPP, 2=8 BPP, 3=reserved)      |
| `BITMAP`    | `[6]`    | R/W | Bitmap or tiled (0=use `XR_Px_TILE_CTRL` for tiled options, 1=Bitmap)          |
| `BLANK`     | `[7]`    | R/W | Output blanked (to BORDCOL on playfield A, or 0 or playfield B                 |
| `COLORBASE` | `[15:8]` | R/W | Value XOR'd with color output index (for colors outside BPP and color effects) |

**0x11 `XR_PA_TILE_CTRL` (R/W)** - playfield A (base) tile control  
**0x19 `XR_PB_TILE_CTRL` (R/W)** - playfield B (overlay) tile control

<img src="./pics/wd_XR_Px_TILE_CTRL.svg">

**playfield A/B tile control**  
| Name           | Bits      | R/W | Description                                                                         |
|----------------|-----------|-----|-------------------------------------------------------------------------------------|
| `TILE_H`       | `[3:0]`   | R/W | Tile height-1 (0 to 15 for 1 to 16 high, stored as 8 or 16 high in tile definition) |
| `TILE_VRAM`    | `[8]`     | R/W | Tile glyph defintions in TILEMEM or VRAM (0=TILEMEM, 1=VRAM)                        |
| `DISP_TILEMEM` | `[9]`     | R/W | Tile display indices in VRAM or TILEMEM (0=VRAM, 1=TILEMEM)                         |
| `TILEBASE`     | `[15:10]` | R/W | Base address for start of tiles in TILEMEM or VRAM (aligned per tile size/BPP)      |

**0x12 `XR_PA_DISP_ADDR` (R/W)** - playfield A (base) display VRAM start address  
**0x1A `XR_PB_DISP_ADDR` (R/W)** - playfield B (overlay) display VRAM start address

<img src="./pics/wd_XR_Px_DISP_ADDR.svg">

**playfield A/B display start address**  
Address in VRAM for start of playfield display (either bitmap or tile indices/attributes map).

**0x13 `XR_PA_LINE_LEN` (R/W)** - playfield A (base) display line word length  
**0x1B `XR_PB_LINE_LEN` (R/W)** - playfield B (overlay) display line word length

<img src="./pics/wd_XR_Px_LINE_LEN.svg">

**playfield A/B display line word length**  
Word length added to line start address for each new line.  The first line will use `XR_Px_DISP_ADDR` and this value will be added at the end of each subsequent line.  It is not the length of of the displayed line (however it is typically at least as long or data will be shown multiple times).  Twos complement, so negative values are okay (for reverse scan line order in memory).

**0x14 `XR_PA_HV_SCROLL` (R/W)** - playfield A (base) horizontal and vertical fine scroll  
**0x1C `XR_PB_HV_SCROLL` (R/W)** - playfield B (overlay) horizontal and vertical fine scroll

<img src="./pics/wd_XR_Px_HV_SCROLL.svg">

**playfield A/B  horizontal and vertical fine scroll**  
| Name       | Bits     | R/W | Description                                |
|------------|----------|-----|--------------------------------------------|
| `H_SCROLL` | `[12:8]` | R/W | Horizontal fine pixel scroll (0-31 pixels) |
| `V_SCROLL` | `[5:0]`  | R/W | Vertical fine pixel scroll (0-63 pixels)   |

Horizontal fine scroll is typically constrained to the scaled width of 8 pixels (1 tile):
| `H_REPEAT` | scroll range |
|------------|--------------|
| 0 (1x)     | 0-7 pixels   |
| 1 (2x)     | 0-15 pixels  |
| 2 (3x)     | 0-23 pixels  |
| 3 (4x)     | 0-31 pixels  |

Vertical fine scroll is typically constrained to the scaled height of a pixel or tile (or one less than the line/tile height times
`V_REPEAT`).  Similar to `H_REPEAT` table above, but pixels scroll can be doubled with 8x16 tile size set.

**0x15 `XR_PA_LINE_ADDR` (WO)** - playfield A (base) display VRAM next line address  
**0x1D `XR_PB_LINE_ADDR` (WO)** - playfield B (overlay) display VRAM next line address

<img src="./pics/wd_XR_Px_LINE_ADDR.svg">

**playfield A/B display line address**  
Address in VRAM for start of the next scanline (bitmap or tile indices map). Normally this is updated internally, starting with `XR_Px_DISP_ADDR` and with `XR_Px_LINE_LEN` added at the end of each mode line (which can be every display line, or less depending on tile mode, `V_REPEAT` and `XR_Px_HV_FSCALE` vertical scaling).  This register can be used to change the internal address used for subsequent display lines (usually done via the COPPER). This register is write-only.

> :mag: **`XR_Px_LINE_ADDR`** will still have `XR_Px_LINE_LEN` added at the end of each display mode line (when not repeating the
> line), so you may need to subtract `XR_Px_LINE_LEN` words from the value written to `XR_Px_LINE_ADDR` to account for this.

**0x16 `XR_PA_HV_FSCALE` (R/W)** - playfield A (base) horizontal and vertical fractional scale  
**0x1E `XR_PB_HV_FSCALE` (R/W)** - playfield B (overlay) horizontal and vertical fractional scale

<img src="./pics/wd_XR_Px_HV_FSCALE.svg">

Will repeat the color of a pixel or scan-line every N+1<sup>th</sup> column or line.  This repeat scaling is applied in addition
to the integer pixel repeat (so a repeat value of 3x and fractional scale of 1 [repeat every line], would make 6x effective
scale).  

| Frac. Repeat | Horiz. 640 Scaled | Horiz. 848 Scaled | Vert. 480 Scaled |
|--------------|-------------------|-------------------|------------------|
| 0 (disable)  | 640 pixels        | 848 pixels        | 480 lines        |
| 1 (1 of 2)   | 320 pixels        | 424 pixels        | 240 lines        |
| 2 (1 of 3)   | 426.66 pixels     | 565.33 pixels     | 320 lines        |
| 3 (1 of 4)   | 480 pixels        | 636 pixels        | 360 lines        |
| 4 (1 of 5)   | 512 pixels        | 678.40 pixels     | 384 lines        |
| 5 (1 of 6)   | 533.33 pixels     | 706.66 pixels     | 400 lines        |
| 6 (1 of 7)   | 548.57 pixels     | 726.85 pixels     | 411.42 lines     |
| 7 (1 of 8)   | 560 pixels        | 742 pixels        | 420 lines        |

**0x17 `XR_PA_UNUSED_17` (-/-) - unused XR PA register 0x17**  
**0x1F `XR_PB_UNUSED_1F` (-/-) - unused XR PB register 0x1F**  
Unused XR playfield registers 0x17, 0x1F

#### Bitmap Display Formats

Bitmap display data starts at the VRAM address`XR_Px_DISP_ADDR`, and has no alignment restrictions.

In 1-BPP bitmap mode with 8 pixels per word, each one of 2 colors selected with 4-bit foreground and background color index. This mode operates in a similar manner as to 1-BPP tile "text" mode, but with a unique 8x1 pixel pattern in two colors specified per word.

<img src="./pics/wd_1-bpp_bitmap_word.svg">

In 4 BPP bitmap mode, there are 4 pixels per word, each one of 16 colors.

<img src="./pics/wd_4-bpp_bitmap_word.svg">

In 8 BPP bitmap mode, there are 2 pixels per word, each one of 256 colors.

<img src="./pics/wd_8-bpp_bitmap_word.svg">

#### Tile Display Formats

Tile indices and attribute map display data starts at `XR_Px_DISP_ADDR` in either VRAM or TILEMEM (TILEMEM selected with
`DISP_TILEMEM` bit in `XR_Px_TILE_CTRL`).  There are no alignement requirements for a tile

The tile bitmap definitions start at the aligned address specified in `TILEBASE` bits set in `XR_Px_TILE_CTRL` in TILEMEM or VRAM (VRAM selected with `TILE_VRAM` in `XR_Pßx_TILE_CTRL`).  The tileset address should be aligned based on the power of two greater than the size of the maximum glyph index used. When using all possible glyphs in a tileset, alignment would be as follows:

| BPP   | Size | Words    | Glyphs | Alignment       |
|-------|------|----------|--------|-----------------|
| 1-BPP | 8x8  | 4 words  | 256    | 0x0400 boundary |
| 1-BPP | 8x16 | 8 words  | 256    | 0x0800 boundary |
| 4-BPP | 8x8  | 16 words | 1024   | 0x4000 boundary |
| 8-BPP | 8x8  | 32 words | 1024   | 0x8000 boundary |

However, if using less tiles, you can relax alignment restrictions based on the maximum glyph index used.  For example if you are using 512 or less 8x8 4-BPP tiles, then you only need to be aligned on a 0x2000 word boundary (16 words per tile, times 512 tiles).

In 1-BPP tile mode for the tile display indices, there are 256 glyphs (and 4-bit foreground/background color in word).
<img src="./pics/wd_1-bpp_tile_word.svg">

In 1-BPP tile mode, two tile lines are stored in each word in the tile definition.  1-BPP can index 8x8 or 8x16 tiles, for 4 or 8
words per tile (a total of 2KiB or 4KiB with 256 glyphs).  
<img src="./pics/wd_1-bpp_tile_def.svg">

In 2 or 4 BPP tile mode for the tile display indices, there are 1024 glyphs, 4-bit color offset and horizontal and vertical tile
mirroring.  
<img src="./pics/wd_n-bpp_tile_word.svg">

In 2 or 4 BPP the tile definition is the same as the corresponding bitmap mode, using 64 contiguous pixels for each 8x8 tile.  
Each 4-BPP tile is 16 words and 8-bpp tile is 32 words (a total of 32KiB or 64KiB respectively, with 1024 glyphs).

#### Final Color Index Selection

In *all* of the above bitmap or tile modes for every native pixel (even border or blanked pixels), a color index is formed and used to look-up the final 16-bit ARGB color blended with the other playfield to form the color on the screen.  The final index for each playfield to look-up in its colormap is first exclusive-OR'd with the playfields respective `XR_Px_GFX_CTRL[15:8]` 8-bit "colorbase" value.  This allows utilization of the the entire colormap, even on formats that have less than 8-BPP and no "color index" offset attribute (and also useful as an additional color index modifier, even with an 8-BPP index).

E.g., if a playfield mode produced a color index of `0x07`, and the playfield `XR_Px_GFX_CTRL[15:8]` colorbase had a value `0x51`, a final index of `0x07` XOR `0x51` = `0x56` would be used for the colormap look-up.

#### Color Look-up and Playfield Blending

___

### 2D Blitter Engine Operation

The Xosera "blitter" is a simple VRAM data read/write "engine" that operates on 16-bit words in VRAM to copy, fill and do logic operations on arbitrary two-dimensional rectangular areas of Xosera VRAM (i.e., to draw, copy and manipulate "chunky" 4/8 bit pixel bitmap images). It supports a VRAM source and destination. The source can also be set to a constant. It repeats a basic word length operation for each "line" of a rectanglar area, and at the end of each line adds arbitrary "modulo" values to source and destination addresses (to position source and destination for the start of the next line). It also has a "nibble shifter" that can shift a line of word data 0 to 3 4-bit nibbles to allow for 4-bit/8-bit pixel level positioning and drawing. There are also first and last word edge transparency masks, to remove unwanted pixels from the words at the start and end of each line allowing for arbitrary pixel sized rectangular operations. There is a fixed logic equation with ANDC and XOR terms (a combination of which can set, clear, invert or pass through any source color bits). This also works in conjunction with "transparency" testing that can mask-out specified 4-bit or 8-bit pixel values when writing. This transparency masking allows specified nibbles in a word to remain unaltered when the word is overwritten (without read-modify-write VRAM access, and no speed penalty). The combination of these features allow for many useful graphical "pixel" operations to be performed in a single pass (copying, masking, shifting and logical operations).  To minimize idle time between blitter operations (during CPU setup), there is a one deep queue for operations (an interrupt can also be generated when the queue becomes empty).

___

#### Logic Operation Applied to Blitter Operations

&nbsp;&nbsp;&nbsp;`D = S & ~ANDC ^ XOR`  (destination word `D` = `S` source word AND'd with NOT of `ANDC` and XOR'd with `XOR`)

- `D` result word
  - written to VRAM (starting address set by `XR_BLIT_DST_D` and incrementing/decrementing)
- `S` primary source word, can be one of:
  - word read from VRAM (starting VRAM address set by `XR_BLIT_SRC_A` and incrementing/decrementing)
  - word constant (set by `XR_BLIT_SRC_S` when `S_CONST` set in `XR_BLIT_CTRL`)
- `ANC` constant AND-COMPLEMENT word
  - source word is AND'd with the NOT of this word
- `XOR` constant XOR source word
  - source word is XOR'd with the this word

Address values (`XR_BLIT_SRC_S`, `XR_BLIT_DST_D`) will be incremented each word and the `XR_BLIT_MOD_S` and `XR_BLIT_MOD_D` will be added at the end of each line (in a 2-D blit operation).

#### Transparency Testing and Masking Applied to Blitter Operations

&nbsp;&nbsp;&nbsp;4-bit mask `M = (S != {T})` where `T` is even/odd transparency nibbles or an 8-bit transparent value (for 8-bit pixel mode)

- `M` transparency result is 0 for transparent nibbles (ones that will not be altered in the destination word).
- `S` source word used in logical operation above
  - Either read from VRAM or a constant (when `S_CONST` set in `XR_BLIT_CTRL`)
- `T` transparency constant nibble pair/byte set in upper byte of `XR_BLIT_CTRL`

Transparency testing is enabled with `TRANSP` in `XR_BLIT_CTRL`.  When enabled a 4-bit nibble transparency mask will be set in two-bit pairs for 8-bit pixels when `TRANSP_8B` set in `XR_BLIT_CTRL` (both bits zero only when both nibbles of the transparency test are zero, otherwise both one). This allows any 4-bit or 8-bit pixel value in `S` to be considered transparent.

#### Nibble Shifting and First/Last Word Edge Masking

The `BLIT_SHIFT` register controls the nibble shifter that can shift 0 to 3 pixels to the right (or to the left in `DECR` mode). Pixels shifted out of a word, will be shifted into the next word. There are also first and last word 4-bit masks to mask unwanted pixels on the edges of a rectangle (when not word aligned, and pixels shifted off the end of the previous line). When drawing in normal increment mode, the first word mask will trim pixels on the left edge and the last word mask the right edge (or both at once for single word width).

#### Blitter Operation Status

The blitter provides two bits of status information that can be read from the XM register `XM_SYS_CTRL`.

- `BLIT_FULL` set when the blitter queue is full and it cannot accept new operations. The blitter supports one blit operation in progress and one queued in registers. Before writing to any blitter register, check this bit to know if the blitter is ready to have a new values stored into its registers (so it can keep busy, without idle gaps between operations).
- `BLIT_BUSY` set when blitter is busy with an operation (or has an operaion queued). Use this bit to check if blitter has finished with all operations (e.g., before using the result).

The biltter will also generate an Xosera interrupt with interrupt source #1 each time each blit operation is completed (which will be masked unless interrupt source #1 mask bit is set in `XM_SYS_CTRL`).

### 2D Blitter Engine XR Registers Summary

| Reg # | Name            | R/W  | Description                                                          |
|-------|-----------------|------|----------------------------------------------------------------------|
| 0x40  | `XR_BLIT_CTRL`  | -/W  | blitter control (transparency value, transp_8b, transp, S_const)     |
| 0x41  | `XR_BLIT_ANDC`  | -/W  | `ANDC` AND-COMPLEMENT constant value                                 |
| 0x42  | `XR_BLIT_XOR`   | -/W  | `XOR` XOR constant value                                             |
| 0x43  | `XR_BLIT_MOD_S` | -/W  | end of line modulo value added to `S` address                        |
| 0x44  | `XR_BLIT_SRC_S` | -/W  | `S` source read VRAM address (or constant value if `S_CONST`)        |
| 0x45  | `XR_BLIT_MOD_D` | -/W  | end of line modulo added to `D` destination address                  |
| 0x46  | `XR_BLIT_DST_D` | -/W  | `D` destination write VRAM address                                   |
| 0x47  | `XR_BLIT_SHIFT` | -/W  | first and last word nibble masks and nibble shift amount (0-3)       |
| 0x48  | `XR_BLIT_LINES` | -/W  | number of lines minus 1, (repeats blit word count after modulo calc) |
| 0x49  | `XR_BLIT_WORDS` | -/W+ | word count minus 1 per line (write queues blit operation)            |
| 0x4A  | `XR_BLIT_2A`    | -/-  | RESERVED                                                             |
| 0x4B  | `XR_BLIT_2B`    | -/-  | RESERVED                                                             |
| 0x4C  | `XR_BLIT_2C`    | -/-  | RESERVED                                                             |
| 0x4D  | `XR_BLIT_2D`    | -/-  | RESERVED                                                             |
| 0x4E  | `XR_BLIT_2E`    | -/-  | RESERVED                                                             |
| 0x4F  | `XR_BLIT_2F`    | -/-  | RESERVED                                                             |

**NOTE:** None of the blitter registers are readable (reads will return unpredictable values). However, registers will
not be altered between blitter operations so only registers that need new values need be written before writing
`XR_BLIT_WORDS` to start a blit operation. This necessitates careful consideration/coordination when using the blitter
from both interrupts and mainline code.

#### 2D Blitter Engine XR Registers Details

**0x20 `XR_BLIT_CTRL`** (-/W) - control bits (transparency control, S const)  

<img src="./pics/wd_XR_BLIT_CTRL.svg">

**blitter operation control**  
The basic logic operation applied to all operations: `D = A & B ^ C`
The operation has four options that can be independently specified in `XR_BLIT_CTRL`:

- `S_CONST` specifies `S` term is a constant (`XR_BLIT_SRC_S` used as constant instead of VRAM address to read)
  - NOTE: When `S` is a constant the value of `XR_BLIT_MOD_S` will still be added to it at the end of each line.

Additionally, a transparency testing can be to the source data: `4-bit mask M = (S != T)` (testing either per nibble
or byte, for 4-bit or 8-bit modes).  When a nibble/byte is masked, the existing pixel in VRAM will not be modified.

- `TRANSP` will enable transparency testing when set (when zero, no pixel values will be masked, but start and end of
   line masking will still apply)
- `TRANSP8` can be set so 8-bit transparency test (vs 4-bit).  Pixels are tested for transparency and masked only when
   the both nibbles in a byte match the transparency value
- `T` value is set in with the upper 8 bits of `XR_BLIT_CTRL` register and is the transparent value for even and odd
   4-bit pixels or a single 8-bit pixel value (when `TRANSP8` set).

**0x21 `XR_BLIT_ANDC`** (-/W) - source term ANDC value constant

<img src="./pics/wd_XR_BLIT_ANDC.svg">

**source constant term `ANDC` (AND-complement) value**  
Arbitrary value for used for `ANDC` AND-complement with source term `S`, in equation: `D = S & (~ANDC) ^ XOR`

**0x22 `XR_BLIT_XOR`** (-/W) - source term XOR value constant

<img src="./pics/wd_XR_BLIT_XOR.svg">

**source constant term `XOR` (exclusive OR) value**  
Arbitrary value for used for `XOR` exclusive-OR with source term `S`, in equation: `D = S & (~ANDC) ^ XOR`

**0x23 `XR_BLIT_MOD_S`** (-/W) - modulo added to `BLIT_SRC_S` address at end of line

<img src="./pics/wd_XR_BLIT_MOD_S.svg">

**modulo added to `BLIT_SRC_S` address at end of a line**  
Arbitrary twos complement value added to `S` address/constant at the end of each line. Typically zero when `BLIT_SRC_S`
image data is contiguous, or -1 when shift amount is non-zero.

**0x24 `XR_BLIT_SRC_S`** (-/W) - source `S` term (read from VRAM address or constant value)

<img src="./pics/wd_XR_BLIT_SRC_S.svg">

**source term `S` VRAM address (with term read from VRAM) or a constant value if `S_CONST` set**  
Address of source VRAM image data or arbitrary constant if `S_CONST` set in `XR_BLIT_CTRL`. This value will be
shifted by `XR_BLIT_SHIFT` nibble shift amount (when not a constant with `S_CONST` set).

**0x25 `XR_BLIT_MOD_D`** (-/W) - modulo added to `BLIT_DST_D` address at end of line

<img src="./pics/wd_XR_BLIT_MOD_D.svg">

**modulo added to `BLIT_DST_D` destination address at end of a line**  
Arbitrary twos complement value added to `D` destination address at the end of each line. Typically the
*destination_width*-*source_width* (in words) to adjust the destination pointer to the start of the next
rectangular image line.

**0x26 `XR_BLIT_DST_D` (-/W) - destination D VRAM write address

<img src="./pics/wd_XR_BLIT_DST_D.svg">

**destination D VRAM write address**  
Destination VRAM address.  Set to the first word of the destination address for the blit operation (or the last
word of the destination, if in `DECR` mode).

**0x27 `XR_BLIT_SHIFT`** (-/W) - first and last word nibble masks and nibble shift

<img src="./pics/wd_XR_BLIT_SHIFT.svg">

**first and last word nibble masks and nibble shift**  
The first word nibble mask will unconditionally mask out (make transparent) any zero nibbles on the first
word of each line (left edge). Similarly, the last word nibble mask will unconditionally mask out (make
transparent) any zero nibbles on the last word of each line (the right edge). The nibble shift specifies
a 0 to 3 nibble right shift on all words drawn. Nibbles shifted out the right of a word will be shifted
into the the next word (to provide a seamless pixel shift with word aligned writes). In "normal" use,
when the nibble shift is non-zero, the left and right nibble mask would typically contain the value `0xF0`
nibble shifted (e.g., `0xF0 >> nibble_shift`). The right edge mask hides "garbage" pixels wrapping from
left edge, and right edge mask hides excess pixels when image is not a exact word width. When shifting,
typically also you need to add an extra word to `BLIT_WORDS` and subtract one from `BLIT_MOD_S` (to avoid
skewing the image). When not shifting (nibble shift of zero) and your source image width is word aligned,
you typically want the complete last word so both left and right mask `0xFF` (no edge masking). If your
source image is not an exact multiple word width, you would typically want to trim the excess pixels from
the right edge (e.g., for a 7 nibble wide image, `0xFE`). For images 1 word wide, both left and right edge
masks will be AND'd together. Also this masking is AND'd with the normal transparency control (so if a
pixel is masked by either the left mask, right mask or is considered transparent it will not be modified).

**0x28 `XR_BLIT_LINES`** (-/W) - 15-bit number of lines heigh - 1 (1 to 32768)

<img src="./pics/wd_XR_BLIT_LINES.svg">

**15-bit number of lines high - 1 (or 0 to 32767 representing 1 to 32768)**  
Number of times to repeat blit operation minus one. Typically source image height with modulo values advancing addresses
for the next line of source and destination).  If `XR_BLIT_LINES` is zero (representing 1 line), then the blitter will effectively do a "1-D" linear operation on `XR_BLIT_WORDS` of data (useful to copy or fill contiguous VRAM words).

**0x29 `XR_BLIT_WORDS`** (-/W) - write queues operation, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times)

<img src="./pics/wd_XR_BLIT_WORDS.svg">

**write queues blit, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times))**  
Write to `XR_BLIT_WORDS` queues blit operation which either starts immediately (if blitter not busy), or will start when current operation completes. Before writing to any blitter registers, check `BLIT_FULL` bit in `XM_SYS_CTRL` to make sure the blitter is ready to accept a new operation.  The blitter supports one operation currently in progress and one operation queued up to start.  Once it has an operation queued  it reports `BLIT_FULL` (which means to wait before altering any BLIT registers). To check if all blit operation has completely finished (and no operations queued), you can check the `BLIT_BUSY` bit in `XM_SYS_CTRL` which will read true until the blit operation has come to a complete stop (e.g., when you want to use the result of a blitter operation and need to be sure it has completed).

___

## Video Synchronized Co-Processor Details

Xosera provides a dedicated video co-processor, synchronized to the display pixel clock called the "copper" (in homage to the similar unit in the Amiga). The copper can be programmed to perform video register manipulation in sync with the video beam. With careful programming this enables many advanced effects to be achieved, such as multi-resolution displays and simultaneous display of more colors than would otherwise be possible.

> :mag: **A note about the copper:** Originally Xosera used a copper designed and programmed by Ross Bamford (roscopeco).  It was a solid design, somewhat similar conceptually to the one in the Amiga.  It was especially impressive as it was pretty much his first Verilog design and it worked great.  The original copper design was in use for months, but eventually resource limitations in the FPGA forced me to make some hard choices in order to implement all the audio features planned.  To free up some FPGA resources, the current "copper slim" design was created (after squeezing everything else I could).  The new design is a bit "slimmer" in logic than the original, but the main feature is it can make more efficient use of copper memory, so it was reasonable to "steal" back some memory blocks needed to complete the audio design (and add a pointer sprite).  I am happy I was able to get the audio features implemented and fitting, but still a bit sad that I needed to replace Ross's fine copper design to do it.

Interaction with the copper happens via:

- **`XR_COPP_CTRL`** XR register (`0x01`) to enable/disable copper program execution. Exectution will always start at scanline 0 of a video frame (off the left edge, before visible pixels).
- **`XR_COPPER_ADDR`** 1.5K words of XR memory (`0xC000-0xC5FF`) for copper program and data storage.  The copper can write to other XR memory areas, but can only read from this area (for instructions or data).

In general, programming the copper comprises loading a copper program (aka 'copper list') into the `XR_COPPER_ADDR` area, and then enabling copper execution with the `XR_COPP_CTRL` register.

When the copper is enabled, at the beginning of each new frame the copper state is reset (but not copper memory) and and execution will begin at the start of copper memory (`XR_COPPER_ADDR` or offset 0), with `RA` set to 0 and the `B` flag set.  

You should make sure a valid copper program is present in copper memory *before* you enable it (although there is a default program present after reset).

> :mag: **Copper execution:** When enabled, copper execution won't begin until the start of the next video frame (in the horizontal blank off the left edge of scan line 0).  It will also be restarted the same way at the start of each subsequent video frame (with copper `PC`=`0x000`, `RA`=`0` and `B`=`1`).  Copper memory is unaltered between frames.

### Programming the Co-processor

As mentioned, copper programs reside in XR memory at `0xC000-0xC5FF` (`XR_COPPER_ADDR`). This memory segment is 1.5K 16-bit words in size and can contain an arbitrary mix of copper program instructions or data.  From the perspective of the copper program, this memory area is `0x000` to `0x5FF`, but the copper address space extends to `0xFFF` (with special addresses shown below).

As far as the copper is concerned, all coordinates are in absolute native pixel resolution, including offset (non-visible) pixels. The coordinates used by the copper are absolute per video mode (640 or 848 widescren) are are not affected by pixel doubling, scrolling or other video mode settings.

| Aspect | Video Mode | Full Res.  | H Visible   | V Visible |
|--------|------------|------------|-------------|-----------|
| 4:3    | 640 x 480  | 800 x 525  | 160 to 799  | 0 to 479  |
| 16:9   | 848 x 480  | 1088 x 517 | 240 to 1079 | 0 to 479  |
___

### Co-processor Instruction Set

| Copper Opcode Bits          | Assembly                     | Flag | Cyc | Description                              |
|-----------------------------|------------------------------|------|-----|------------------------------------------|
| `rr00` `oooo` `oooo` `oooo` | `SETI`   *xadr14*,`#`*val16* | `B`  | 4   | dest [xadr14] <= source #val16           |
| `iiii` `iiii` `iiii` `iiii` | + *im16* *value*             |      |     | *(2<sup>nd</sup> word of `SETI` opcode)* |
| `--01` `rccc` `cccc` `cccc` | `SETM`  *xadr16*,*cadr11*    | `B`  | 4   | dest [xadr16] <= source [cadr11]         |
| `rroo` `oooo` `oooo` `oooo` | +  *xadr16* *address*        |      |     | *(2<sup>nd</sup> word of `SETM` opcode)* |
| `--10` `0iii` `iiii` `iiii` | `HPOS`   #*im11*             |      | 4+  | wait until video HPOS >= im11            |
| `--10` `1iii` `iiii` `iiii` | `VPOS`   #*im11*             |      | 4+  | wait until video VPOS >= im11            |
| `--11` `0ccc` `cccc` `cccc` | `BRGE`   *cadd11*            |      | 4   | if (B==0) PC <= cadd11                   |
| `--11` `1ccc` `cccc` `cccc` | `BRLT`   *cadd11*            |      | 4   | if (B==1) PC <= cadd11                   |

**Legend:**

- *xadr14*   =   2-bit XR region + 12-bit offset (1st word of `SETI`, destination address)
- *im16*     =   16-bit immediate word (2nd word of `SETI`, source address)
- *cadr11*   =   11-bit copper address + register (1st word of `SETM`, source adress)
- *xadr16*   =   XR region + 14-bit offset (2nd word of `SETM`, destination address)
- *im11*     =   11-bit immediate value (used with `HPOS`, `VPOS` wait opcodes)
- *cadd11*   =   11-bit copper address/register (used with `BRGE`, `BRLT` branch opcodes)
- `B`        =   borrow flag set when `RA` < *val16* written (borrow after unsigned subtract)

> :mag: **copper addresses** *cadr11* bits`[15:14]` are ignored and copper memory is always used when reading, when writing *xadr14*/*xadr16* bits `[15:14]` can be set to `11` (`XR_COPPER_ADDR` region) to write to copper memory.

**Copper special addresses for memory mapped registers:**
| Pseudo reg       | Copper Addr | Operation                                 | Description                                 |
|------------------|-------------|-------------------------------------------|---------------------------------------------|
| `RA` (read)      | `0x0800`    | `RA`                                      | return current value in `RA` register       |
| `RA` (write)     | `0x0800`    | `RA` = *val16*, `B` = 0                   | set `RA` to *val16*, clear `B` flag         |
| `RA_SUB` (write) | `0x0801`    | `RA` = `RA` - *val16*, `B`=`RA` < *val16* | set `RA` to `RA` - *val16*, update `B` flag |
| `RA_CMP` (write) | `0x07FF`    | `B` = `RA` < *val16*                      | update B flag only (`RA` unaltered)         |

- `RA`  read 16-bit accumulator (`B` flag unaltered)
- `RA`  write 16-bit accumulator (`B` flag cleared)
- `RA_SUB` write performs subtract operation, `RA` set to `RA` minus value written and updating `B` borrow flag (set if `RA` was less than value written)
- `RA_CMP` write performs compare operation, setting `B` borrow flag if `RA` is less than value written (`RA` not altered)

**`WAIT` - [000o oYYY YYYY YYYY],[oXXX XXXX XXXX FFFF]**  

Wait for a given screen position to be reached (or exceeded).

Flags:

- [0] = Ignore vertical position
- [1] = Ignore horizontal position
- [2] = Reserved
- [3] = Reserved

If both horizontal and vertical ignore flags are set, this instruction will wait
indefinitely (until the end of the frame). This can be used as a special "wait for
end of frame" instruction.

**`SKIP` - [001o oYYY YYYY YYYY],[oXXX XXXX XXXX FFFF]**  

Skip the next instruction if a given screen position has been reached.

Flags:

- [0] = Ignore vertical position
- [1] = Ignore horizontal position
- [2] = Reserved
- [3] = Reserved

If both horizontal and vertical ignore flags are set, this instruction will **always**  
skip the next instruction. While not especially useful in its own right, this can come
in handy in conjunction with in-place code modification.

**`JMP` - [010o oAAA AAAA AAAA],[oooo oooo oooo oooo]**  

Jump to the given copper RAM address.

**`MOVER` - [011o FFFF AAAA AAAA],[DDDD DDDD DDDD DDDD]**  

Move 16-bit data to XR register specified by 8-bit address.

**`MOVEF` - [100A AAAA AAAA AAAA],[DDDD DDDD DDDD DDDD]**  

Move 16-bit data to `XR_TILE_ADDR` (or 'font') memory.

**`MOVEP` - [101o oooA AAAA AAAA],[DDDD DDDD DDDD DDDD]**  

Move 16-bit data to `XR_COLOR_ADDR` (or 'palette') memory.

**`MOVEC` - [110o oAAA AAAA AAAA],[DDDD DDDD DDDD DDDD]**  

Move 16-bit data to `XR_COPPER_ADDR` memory.

Key:

```text
  Y - Y position (11 bits)
  X - X position (11 bits)
  F - Flags
  R - Register
  A - Address
  D - Data
  o - Not used / don't care

```

___

#### Notes on the MOVE variants

With the available `MOVE` variants, the copper can directly MOVE to the following:

- Any Xosera XR register (including the copper control register)
- Xosera tile memory (or font memory).
- Xosera color memory
- Xosera copper memory

The copper **cannot** directly MOVE to the following, and this is by design:

- Xosera main registers
- Video RAM

It is also possible to change the copper program that runs on a frame-by-frame basis
(both from copper code and m68k program code) by modifying the copper control register. The copper
supports jumps within the same frame with the JMP instruction.

Because the copper can directly write to it's own memory segment, self-modifying code is supported
(by modifying copper memory from your copper code) and of course m68k program code can modify that
memory at will using the Xosera registers.

> **Note**: When modifying copper code from the m68k, and depending on the nature of your modifications,
you may need to sync with vblank to avoid display artifacts.
___

### Co-processor Assembler

Co-processor instructions will often be written programatically, or directly (in hex) within
a higher level machine language, for loading into the copper at runtime. The instruction format
has been specifically designed to make it easier to read and write code directly in hexadecimal.

However, you may also find it useful to write copper programs in a slightly higher-level language,
and translate these into machine instructions. For this purpose, a
[customasm](https://github.com/hlorenzi/customasm)-compatible assembler definition is provided
in the `copper/copper_asm` directory. See the `copper.casm` file along with the provided examples
for details of use.

A simple executable ruby script, `bin2c.rb` is also provided in that directory. This is a simple
utility that takes assembled copper binaries and outputs them as a C array (with associated size)
for direct embedding into C code.

Additionally, there are a bunch of handy C macros (in the Xosera m68k API headers) that facilitate
writing readable copper code directly in C source code. The included examples (in `copper` directory)
demonstrate the different ways of embedding copper code in C source.

### Default TILE, COLOR and COPPER Memory Contents

When Xosera is reconfigured (or on power up), the VRAM contents are undefined (garbage), but TILE
memory, COLOR memory and COPPER memory will be restored to default contents as follows:

| TILE address  | Description                                             |
|---------------|---------------------------------------------------------|
| 0x4000-0x47FF | 8x16 ST font (derived from Atari ST 8x16 font)          |
| 0x4800-0x4BFF | 8x8 ST font (derived from Atari ST 8x8 font             |
| 0x4C00-0x4FFF | 8x8 PC font (derived from IBM CGA 8x8)                  |
| 0x5000-0x53FF | 8x8 hexadecimal debug font (showing TILE number in hex) |

<!-- TODO: Add COLOR and COPPER info -->
