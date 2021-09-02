# Xosera - Xark's Open Source Embedded Retro Adapter

Xosera is a Verilog design currently for iCE40UltraPlus5K FPGA that implements an "Embedded Retro Adapter"
designed primarily for the rosco_m68K series of retro computers (but adaptable to others).  It provides
color video text and graphics generation similar to late 80s 68K era home computers (along with other
capabilities).

This document is meant to provide the low-level reference information to operate it (and ideally
matches the actual Verilog implementation). Please mention it if you spot a discrepency.

**Section Index:**

- [Xosera - Xark's Open Source Embedded Retro Adapter](#xosera---xarks-open-source-embedded-retro-adapter)
  - [Xosera Reference Information](#xosera-reference-information)
    - [Xosera Main Register Summary (16-bit directly accessible)](#xosera-main-register-summary-16-bit-directly-accessible)
  - [Xosera Main Register Details](#xosera-main-register-details)
    - [Xosera Extended Register / Extended Memory Region Summary](#xosera-extended-register--extended-memory-region-summary)
    - [Xosera XR Registers](#xosera-xr-registers)
      - [Video Config and Copper XR Register Summary](#video-config-and-copper-xr-register-summary)
      - [Video Config and Copper XR Register Details](#video-config-and-copper-xr-register-details)
      - [Playfield A & B Control XR Register Summary](#playfield-a--b-control-xr-register-summary)
      - [Playfield A & B Control XR Register Details](#playfield-a--b-control-xr-register-details)
      - [2D Blitter Engine XR Register Summary](#2d-blitter-engine-xr-register-summary)
      - [Polygon / Line Draw Engine XR Register Summary](#polygon--line-draw-engine-xr-register-summary)

## Xosera Reference Information

Xosera has 16 main 16-bit directly accessable bus registers that are used to control its operation.
The even and odd bytes of each 16-bit word are accessed independently (Xosera uses an 8-bit data bus
and "bytesel" signal).  It uses 68000 big-endian convention, so "even" addressed bytes contain the
most significant 8-bits and the "odd" addresses contains the least significant 8-bits of the complete
16-bit value. When writing values, typically the upper 8-bits are saved until the lower 8-bits are written
and then the entire 16-bit value is stored.  For this reason typically you should update the first (even or
high-byte) byte before the second (odd or low-byte) byte or update both with a MOVEP.W write.

Xosera's 128KB of VRAM is organized as 64K x 16-bit words, so a full VRAM address is 16-bits (and an
individual byte is not directly accessible, only 16-bit words). [TODO: nibble masking writes is possible,
but currently not wired up]

In addition to the main registers and VRAM, there is an additional extended register / memory bus that provides
access to many more control registers for system control, video configuration, drawing engines and display
co-processor as well as additional memory regions for tile definitions, color look-up and display coprocessor
instructions.

### Xosera Main Register Summary (16-bit directly accessible)

| Reg # | Reg Name     | R /W  | Description                                                               |
| ----- | ------------ | ----- | ------------------------------------------------------------------------- |
| 0x0   | `XR_ADDR`    | R /W+ | XR register number/address for `XR_DATA` read/write access                |
| 0x1   | `XR_DATA`    | R /W+ | read/write XR register/memory at `XR_ADDR` (`XR_ADDR` incr. on write)     |
| 0x2   | `RD_INCR`    | R /W  | increment value for `RD_ADDR` read from `XDATA`/`XDATA_2`                 |
| 0x3   | `RD_ADDR`    | R /W+ | VRAM address for reading from VRAM when `XDATA`/`XDATA_2` is read         |
| 0x4   | `WR_INCR`    | R /W  | increment value for `WR_ADDR` on write to `XDATA`/`XDATA_2`               |
| 0x5   | `WR_ADDR`    | R /W  | VRAM address for writing to VRAM when `XDATA`/`XDATA_2` is written        |
| 0x6   | `XDATA`      | R+/W+ | read/write VRAM word at `RD_ADDR`/`WR_ADDR` (and add `RD_INCR`/`WR_INCR`) |
| 0x7   | `XDATA_2`    | R+/W+ | 2nd `XVID_DATA`(to allow for 32-bit read/write access)                    |
| 0x8   | `XSYS_CTRL`  | R /W+ | busy status, FPGA reconfig, interrupt status/control, write masking       |
| 0x9   | `XSYS_TIMER` | RO    | read 1/10<sup>th</sup> millisecond timer [TODO]                           |
| 0xA   | `UNUSED_A`   | R /W  | unused direct register 0xA [TODO]                                         |
| 0xB   | `UNUSED_B`   | R /W  | unused direct register 0xB [TODO]                                         |
| 0xC   | `RW_INCR`    | R /W  | `RW_ADDR` increment value on read/write of `RW_DATA`/`RW_DATA_2`          |
| 0xD   | `RW_ADDR`    | R /W+ | read/write address for VRAM access from `RW_DATA`/`RW_DATA_2`             |
| 0xE   | `RW_DATA`    | R+/W+ | read/write VRAM word at `RW_ADDR` (and add `RW_INCR`)                     |
| 0xF   | `RW_DATA_2`  | R+/W+ | 2nd `RW_DATA`(to allow for 32-bit read/write access)                      |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)

## Xosera Main Register Details

**0x0 `XR_ADDR` (R/W+) - eXtended Register / eXtended Region Address**
<img src="./pics/wd_XR_ADDR.svg">  
**Extended register or memory address for data accessed via `XR_DATA`**  
Specifies the XR register or address to be accessed via `XR_DATA`.
The upper 2 bits select XR registers or memory region and the lower 12 bits select the register number or
memory word address within the region (see below for details of XR registers and memory).
When `XR_ADDR` is written, the register/address specified will be read and made available for reading at `XR_DATA`
(`XR_ADDR` needs to be written each time before reading `XR_DATA` or the previously read value will be returned).
After a word is written to `XR_DATA`, the lower 12-bits of `XR_ADDR` will be auto-incremented by 1 which
allows writing to contiguous registers or memory by repeatedly writing to `XR_DATA`.  
The register mapping with `XR_DATA` following `XR_ADDR` allows for M68K code similar to the following to set an
XR register to an immediate value:  
&emsp;&emsp;`MOVE.L #$rrXXXX,D0`  
&emsp;&emsp;`MOVEP.L D0,XR_ADDR(A1)`

**0x1 `XR_DATA` (R/W+) - eXtended Register / eXtended Region Data**
<img src="./pics/wd_XR_DATA.svg">  
**Read or write extended register or memory addressed by `XR_ADDR` register.**  
Allows read/write access to the XR register or memory using address contained in `XR_ADDR` register.  
When `XR_ADDR` is written, the XR register or address specified will be read will be available for reading at `XR_DATA`
(`XR_ADDR` needs to be set each time before reading `XR_DATA` or the previously read value will be returned).  
After a word is written to `XR_DATA`, the lower 12-bits of `XR_ADDR` will be auto-incremented by 1 which
allows writing to contiguous XR registers or memory by repeatedly writing to `XR_DATA`.  

**0x2 `RD_INCR` (R/W) - increment value for `RD_ADDR` when `XDATA`/`XDATA_2` is read from**  
<img src="./pics/wd_RD_INCR.svg">  
**Read or write twos-complement value added to `RD_ADDR` when `XDATA` or `XDATA_2` is read from**  
Allows quickly reading Xosera VRAM from `XDATA`/`XDATA_2` when using a fixed increment.  
Added to `RD_ADDR` when `XDATA` or `XDATA_2` is read from (twos complement, so value can be negative).  

**0x3 `RD_ADDR` (R/W+) - VRAM read address for `XDATA`/`XDATA_2`**  
<img src="./pics/wd_RD_ADDR.svg">  
**Read or write VRAM address that will be read when `XDATA` or `XDATA_2` is read from.**  
Specifies VRAM address used when reading from VRAM via `XDATA`/`XDATA_2`.  
When `RD_ADDR` is written (or incremented by `RD_INCR`) the corresponding word in VRAM is read and made
available for reading at `X_DATA` or `XDATA_2`.  

**0x4 `WR_INCR` (R/W) - increment value for `WR_ADDR` when `XDATA`/`XDATA_2` is written to**  
<img src="./pics/wd_WR_INCR.svg">  
**Read or write twos-complement value added to `WR_ADDR` when `XDATA` or `XDATA_2` is written to.**  
Allows quickly writing to Xosera VRAM via `XDATA`/`XDATA_2` when using a fixed increment.  
Added to `WR_ADDR` when `XDATA` or `XDATA_2` is written to (twos complement, so value can be negative).  

**0x5 `WR_ADDR` (R/W) - VRAM write address for `XDATA`/`XDATA_2`**  
<img src="./pics/wd_WR_ADDR.svg">  
**Read or write VRAM address written when `XDATA` or `XDATA_2` is written to.**  
Specifies VRAM address used when writing to VRAM via `XDATA`/`XDATA_2`.  

**0x6 `XDATA` (R+/W+) - VRAM memory value to read/write at VRAM address `RD_ADDR`/`WR_ADDR`, respectively**  
<img src="./pics/wd_XDATA.svg">  
**Read or write VRAM value from VRAM at `RD_ADDR`/`WR_ADDR` and add `RD_INCR`/`WR_INCR` to `RD_ADDR`/`WR_ADDR`, respectively.**  
When `XDATA` is read data from VRAM at `RD_ADDR` is returned and `RD_INCR` is added to `RD_ADDR` and pre-reading the new VRAM address begins.  
When `XDATA` is written, begins writing value to VRAM at `WR_ADDR` and `WR_INCR` is added to `WR_ADDR`.  

**0x7 `XDATA_2` (R+/W+) - VRAM memory value to read/write at VRAM address `RD_ADDR`/`WR_ADDR`, respectively**  
<img src="./pics/wd_XDATA.svg">  
**Read or write VRAM value from VRAM at `RD_ADDR`/`WR_ADDR` and add `RD_INCR`/`WR_INCR` to `RD_ADDR`/`WR_ADDR`, respectively.**  
When `XDATA_2` is read data from VRAM at `RD_ADDR` is returned and `RD_INCR` is added to `RD_ADDR` and pre-reading the new VRAM address begins.  
When `XDATA_2` is written, begins writing value to VRAM at `WR_ADDR` and adds `WR_INCR` to `WR_ADDR`.  
NOTE: This register is identical to `XDATA` to allow for 32-bit "long" MOVEP.L transfers to/from `XDATA` for additional speed (however, it does have its own nibble write mask).  

**0x8 `XSYS_CTRL` (R/W+) - draw busy status, reconfigure, interrupt control and write masking control [TODO]**  
<img src="./pics/wd_XSYS_CTRL.svg">  
**Read draw busy, write to reboot FPGA or read/write interrupt control/status and `XDATA` nibble write mask.**  
When `XSYS_CTRL` is read:  
&emsp;[15] is draw busy, [11] is interrupt enable, [10-8] is interupt source, [7-0] is `XDATA`/`XDATA_2` nibble write mask.  
When `XSYS_CTRL` is written:  
&emsp;[14] set to reboot FPGA to [13-12] config, [11] set interrupt enable, [10-8] set also _generates_ interrupt, [7-0] set nibble mask.  
NOTE: An interrupt is only generated if an interrupt source is read as 0 then written as 1 (to avoid accidentally generating interrupts).  An interrupt source is only cleared if it is read as 1 and then written as 0 (to avoid accidentally clearing interrupts).  

**0x9 `XSYS_TIMER` (RO) - 1/10<sup>th</sup> of millisecond timer (0 - 6553.5 ms)**
<img src="./pics/wd_XSYS_TIMER.svg">  
**Read-only 16-bit timer, increments every 1/10<sup>th</sup> of a millisecond**  
Can be used for fairly accurate timing.  When value wraps, internal fractional value is maintined (so as accurate as FPGA PLL clock).  

**0xA `UNUSED_A` (R/W) - unused register 0xA**  
Unused direct register 0xA  

**0xB `UNUSED_B` (R/W) - unused register 0xB**  
Unused direct register 0xB  

**0xC `RW_INCR` (R/W) - increment value for `RW_ADDR` when `RW_DATA`/`RW_DATA_2` is read or written**  
<img src="./pics/wd_RW_INCR.svg">  
**Read or write twos-complement value added to `RW_ADDR` when `RW_DATA` or `RW_DATA_2` is read from or written to.**  
Allows quickly reading/writing Xosera VRAM from `RW_DATA`/`RW_DATA_2` when using a fixed `RW_ADDR` increment.  
Added to `RW_ADDR` when `RW_DATA` or `RW_DATA_2` is read from (twos complement so value can be negative).  

**0xD `RW_ADDR` (R/W+) - VRAM read/write address for accessed at `RW_DATA`/`RW_DATA_2`**  
<img src="./pics/wd_RW_ADDR.svg">  
**Read or write VRAM address read when `RW_DATA` or `RW_DATA_2` is read from or written to.**  
Specifies VRAM address used when reading or writing from VRAM via `RW_DATA`/`RW_DATA_2`.  
When `RW_ADDR` is written (or incremented by `RW_INCR`) the corresponding word in VRAM is read and made available for reading at `WR_DATA` or `WR_DATA_2`.  
Since this read always happens (even when only intending to write), prefer using RW_ADDR for
reading (but fairly small VRAM access overhead).  

**0xE `RW_DATA` (R+/W+) - VRAM memory value to read/write at VRAM address `RW_ADDR`**  
<img src="./pics/wd_RW_DATA.svg">  
**Read or write VRAM value in VRAM at `RW_ADDR` and add `RW_INCR` to `RW_ADDR`.**  
When `RW_DATA` is read, returns data from VRAM at `RW_ADDR`, adds `RW_INCR` to `RW_ADDR` and begins reading new VRAM value.  
When `RW_DATA` is written, begins writing value to VRAM at `RW_ADDR` and adds `RW_INCR` to `RW_ADDR` and begins reading new VRAM value.  

**0xF `RW_DATA_2` (R+/W+) - VRAM memory value to read/write at VRAM address `RW_ADDR`**  
<img src="./pics/wd_RW_DATA.svg">  
**Read or write VRAM value in VRAM at `RW_ADDR` and add `RW_INCR` to `RW_ADDR`.**  
When `RW_DATA_2` is read, returns data from VRAM at `RW_ADDR`, adds `RW_INCR` to `RW_ADDR` and begins reading new VRAM value.  
When `RW_DATA_2` is written, begins writing value to VRAM at `RW_ADDR` and adds `RW_INCR` to `RW_ADDR` and begins reading new VRAM value.  
NOTE: This register is identical to `RW_DATA` to allow for 32-bit "long" MOVEP.L transfers to/from `RW_DATA` for additional speed.  

### Xosera Extended Register / Extended Memory Region Summary

| XR Region Name  | XR Region Range | R/W | Description                             |
| --------------- | --------------- | --- | --------------------------------------- |
| XR_REGS         | 0x0000-0x0FFF   | R/W | See below for XR register details       |
| XR_COLOR_MEM    | 0x8000-0x80FF   | WO  | 256 x 16-bit color lookup memory (XRGB) |
| XR_TILE_MEM     | 0x9000-0x9FFF   | WO  | 4096 x 16-bit tile glyph storage memory |
| XR_COPPER_MEM   | 0xA000-0xA7FF   | WO  | 2048 x 16-bit copper program memory     |
| (unused region) | 0xB000-0xFFFF   | -/- | (unused region)                         |

To access an XR register or XR memory address, write the XR register number or address to `XR_ADDR`, then read or write to `XR_DATA` (note that currently only XR registers can be read, not XR memory).  
Each word written to `XR_DATA` will also automatically increment `XR_ADDR` to allows faster consecutive updates (like for color or tile RAM update).  
Note that this is not the case when reading from `XR_DATA`, you _must_ write to `XR_ADDR` in order to trigger a read (or previously read value will remain).

TODO: Investigate a way to read color, tile or copper memory (perhaps not during display time)

### Xosera XR Registers

This XR registers are used to control of most Xosera operation other than CPU VRAM access and a few miscellaneous functions accessed via the main registers.  
To access these registers, write the register address to `XR_ADDR` (with bit [15] zero), then read or write register data to `XR_DATA` (and when _writing only_, the low 12-bits of `XR_ADDR` will be auto-incremented for each word written).

#### Video Config and Copper XR Register Summary

| Reg # | Reg Name        | R /W | Description                                                                             |
| ----- | --------------- | ---- | --------------------------------------------------------------------------------------- |
| 0x00  | `XR_VID_CTRL`   | R /W | display control and border color index                                                  |
| 0x01  | `XR_VID_TOP`    | R /W | top line of active display window (typically 0)                                         |
| 0x02  | `XR_VID_BOTTOM` | R /W | bottom line of active display window (typically 479)                                    |
| 0x03  | `XR_VID_LEFT`   | R /W | left edge of active display window (typically 0)                                        |
| 0x04  | `XR_VID_RIGHT`  | R /W | right edge of active display window (typically 639 or 847)                              |
| 0x05  | `XR_SCANLINE`   | RO   | [15] in V blank, [14] in H blank [10:0] V scanline                                      |
| 0x06  | `XR_COPP_CTRL`  | R /W | display synchronized coprocessor                                                        |
| 0x07  | `XR_UNUSED_07`  | - /- |                                                                                         |
| 0x08  | `XR_VERSION`    | RO   | Xosera optional feature bits [15:8] and version code [7:0] [TODO]                       |
| 0x09  | `XR_GITHASH_H`  | RO   | [15:0] high 16-bits of 32-bit Git hash build identifier                                 |
| 0x0A  | `XR_GITHASH_L`  | RO   | [15:0] low 16-bits of 32-bit Git hash build identifier                                  |
| 0x0B  | `XR_VID_HSIZE`  | RO   | native pixel width of monitor mode (e.g. 640/848)                                       |
| 0x0C  | `XR_VID_VSIZE`  | RO   | native pixel height of monitor mode (e.g. 480)                                          |
| 0x0D  | `XR_VID_VFREQ`  | RO   | update frequency of monitor mode in BCD 1/100<sup>th</sup> Hz (e.g., 0x5997 = 59.97 Hz) |
| 0x0E  | `XR_UNUSED_0E`  | RO   |                                                                                         |
| 0x0F  | `XR_UNUSED_0F`  | RO   |                                                                                         |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)

#### Video Config and Copper XR Register Details

**0x00 `XR_VID_CTRL` (R/W) - video display enable and border color**  
<img src="./pics/wd_XR_VID_CTRL.svg">  
Pixels outside video window (`VID_TOP`, `VID_BOTTOM`, `VID_LEFT`, `VID_RIGHT`) will use border color index.  
All pixels will be black if video is off.

**0x01 `XR_VID_TOP` (R/W) - video display window top line**  
<img src="./pics/wd_XR_VID_TOP.svg">  
Defines top-most line of video display window (normally 0 for full-screen).

**0x02 `XR_VID_BOTTOM` (R/W) - video display window bottom line**  
<img src="./pics/wd_XR_VID_BOTTOM.svg">  
Defines bottom-most line of video display window (normally 479 for full-screen).

**0x03 `XR_VID_LEFT` (R/W) - video display window left edge**  
<img src="./pics/wd_XR_VID_LEFT.svg">  
Defines left-most native pixel of video display window (normally 0 for full-screen).

**0x04 `XR_VID_RIGHT` (R/W) - video display window right edge**  
<img src="./pics/wd_XR_VID_RIGHT.svg">  
Defines right-most native pixel of video display window (normally 639 or 847 for 4:3 or 16:9 full-screen, respectively).

**0x05 `XR_SCANLINE` (RO) - current video display scan line and blanking status**  
<img src="./pics/wd_XR_SCANLINE.svg">  
Continuously updated with the scanline and blanking status during display scanning. Read-only.

**0x06 `XR_COPP_CTRL` (R/W) - copper start address and enable**  
<img src="./pics/wd_XR_COPP_CTRL.svg">  
Display synchronized co-processor enable and starting PC address for each video frame within copper XR memory region.

**0x07 `XR_UNUSED_07` (-/-) - unused XR register 0x07**  
Unused XR register 0x07  

**0x08 `XR_VERSION` (RO) - Xosera version and optional feature bits**  
<img src="./pics/wd_XR_VERSION.svg">  
Decimal coded version (x.xx) and optional feature bits (0 for undefined/not present). Read-only.

**0x09 `XR_GITHASH_H` (RO) - Xosera Git hash identifier (high 16-bits)**  
<img src="./pics/wd_XR_GITHASH_H.svg">  
High 16-bits of Git short hash identifer. Can be used to help identify exact repository version.  
Upper nibble will be 0xD when local modifications have been made. Read-only.

**0x0A `XR_GITHASH_L` (RO) - Xosera Git hash identifier (low 16-bits)**  
<img src="./pics/wd_XR_GITHASH_L.svg">  
Low 16-bits of Git short hash identifer. Can be used to help identify exact repository version. Read-only.

**0x0B `XR_VID_HSIZE` (RO) - monitor display mode native horizontal resolution**  
<img src="./pics/wd_XR_VID_HSIZE.svg">  
Monitor display mode native horizontal resolution (e.g., 640 for 4:3 or 848 for 16:9). Read-only.

**0x0C `XR_VID_VSIZE` (RO) - monitor display mode native vertical resolution**  
<img src="./pics/wd_XR_VID_VSIZE.svg">  
Monitor display mode native vertical resolution (e.g., 480). Read-only.

**0x0D `XR_VID_VFREQ` (RO) - monitor display mode update frequency in BCD 1/100<sup>th</sup> Hz**  
<img src="./pics/wd_XR_VID_VFREQ.svg">  
Monitor display mode update frequency in BCD 1/100<sup>th</sup> Hz (e.g., 0x5997 = 59.97 Hz). Read-only.

**0x0E `XR_UNUSED_0E` (-/-) - unused XR register 0x0E**  
Unused XR register 0x0E  

**0x0F `XR_UNUSED_0F` (-/-) - unused XR register 0x0F**  
Unused XR register 0x0F  

#### Playfield A & B Control XR Register Summary

| Reg # | Name              | R/W | Description                                                  |
| ----- | ----------------- | --- | ------------------------------------------------------------ |
| 0x10  | `XR_PA_GFX_CTRL`  | R/W | playfield A graphics control                                 |
| 0x11  | `XR_PA_TILE_CTRL` | R/W | playfield A tile control                                     |
| 0x12  | `XR_PA_ADDR`      | R/W | playfield A display VRAM start address                       |
| 0x13  | `XR_PA_WIDTH`     | R/W | playfield A display line width in words                      |
| 0x14  | `XR_PA_HV_SCROLL` | R/W | playfield A horizontal and vertical fine scroll              |
| 0x15  | `XR_PA_LINE_ADDR` | R/W | playfield A scanline start address (loaded at start of line) |
| 0x16  | `XR_PA_UNUSED_16` | R/W |                                                              |
| 0x17  | `XR_PA_UNUSED_17` | R/W |                                                              |
| 0x18  | `XR_PB_GFX_CTRL`  | R/W | playfield B graphics control                                 |
| 0x19  | `XR_PB_TILE_CTRL` | R/W | playfield B tile control                                     |
| 0x1A  | `XR_PB_ADDR`      | R/W | playfield B display VRAM start address                       |
| 0x1B  | `XR_PB_WIDTH`     | R/W | playfield B display line width in words                      |
| 0x1C  | `XR_PB_HV_SCROLL` | R/W | playfield B horizontal and vertical fine scroll              |
| 0x1D  | `XR_PB_LINE_ADDR` | R/W | playfield B scanline start address (loaded at start of line) |
| 0x1E  | `XR_PB_UNUSED_1E` | R/W |                                                              |
| 0x1F  | `XR_PB_UNUSED_1F` | R/W |                                                              |

#### Playfield A & B Control XR Register Details

**0x10 `XR_PA_GFX_CTRL` (R/W) - playfield A (foreground) graphics control**  
**0x18 `XR_PB_GFX_CTRL` (R/W) - playfield B (background) graphics control**  
<img src="./pics/wd_XR_GFX_CTRL.svg">  
**playfield A/B graphics control**  
colorbase is used for any color index bits not in source pixel (e.g., the upper 4-bits of 4-bit pixel).  
blank is used to blank the display (solid colorbase color).  
bitmap 0 for tiled character graphics (see `XR_Px_TILE_CTRL`) using display word with attribute and tile index.  
bitmap 1 for bitmapped mode (1-bpp mode uses a 4-bit fore/back color attributes in upper 8-bits of each word).  
bpp selects bits-per-pixel or the number of color index bits per pixel (see "Graphics Modes" [TODO]).  
H repeat selects the number of native pixels wide an Xosera pixel will be (1-4).  
V repeat selects the number of native pixels tall an Xosera pixel will be (1-4).  

**0x11 `XR_PA_TILE_CTRL` (R/W) - playfield A (foreground) tile control**  
**0x19 `XR_PB_TILE_CTRL` (R/W) - playfield B (background) tile control**  
<img src="./pics/wd_XR_TILE_CTRL.svg">  
**playfield A/B tile control**  
tile base address selects the upper bits of tile storage memory on 1KW boundaries.  
mem selects tile XR memory region or VRAM address (only 4KW of tile XR memory, upper bits ignored).  
tile height selects the height for tiles-1 from (0-15 for up to 8x16).  Tiles are stored as either 8 or 16 lines high.  Tile lines past height are truncated when displayed (e.g., tile height of 11 would display 8x12 of 8x16 tile).  

TODO: Describe more stuff here.

#### 2D Blitter Engine XR Register Summary

| Reg # | Name  | R/W | Description |
| ----- | ----- | --- | ----------- |
| 0x20  | [TBD] | R/W |             |
| 0x21  | [TBD] | R/W |             |
| 0x22  | [TBD] | R/W |             |
| 0x23  | [TBD] | R/W |             |
| 0x24  | [TBD] | R/W |             |
| 0x25  | [TBD] | R/W |             |
| 0x26  | [TBD] | R/W |             |
| 0x27  | [TBD] | R/W |             |
| 0x28  | [TBD] | R/W |             |
| 0x29  | [TBD] | R/W |             |
| 0x2A  | [TBD] | R/W |             |
| 0x2B  | [TBD] | R/W |             |
| 0x2C  | [TBD] | R/W |             |
| 0x2D  | [TBD] | R/W |             |
| 0x2E  | [TBD] | R/W |             |
| 0x2F  | [TBD] | R/W |             |

#### Polygon / Line Draw Engine XR Register Summary

| Reg # | Name  | R/W | Description |
| ----- | ----- | --- | ----------- |
| 0x30  | [TBD] | R/W |             |
| 0x31  | [TBD] | R/W |             |
| 0x32  | [TBD] | R/W |             |
| 0x33  | [TBD] | R/W |             |
| 0x34  | [TBD] | R/W |             |
| 0x35  | [TBD] | R/W |             |
| 0x36  | [TBD] | R/W |             |
| 0x37  | [TBD] | R/W |             |
| 0x38  | [TBD] | R/W |             |
| 0x39  | [TBD] | R/W |             |
| 0x3A  | [TBD] | R/W |             |
| 0x3B  | [TBD] | R/W |             |
| 0x3C  | [TBD] | R/W |             |
| 0x3D  | [TBD] | R/W |             |
| 0x3E  | [TBD] | R/W |             |
| 0x3F  | [TBD] | R/W |             |
