# Xosera - Xark's Open Source Embedded Retro Adapter

Xosera is a Verilog design currently for iCE40UltraPlus5K FPGA that implements an "Embedded Retro Adapter"
designed primarily for the rosco_m68K series of retro computers (but adaptable to others). It provides
color video text and graphics generation similar to late 80s 68K era home computers (along with other
capabilities).

This document is meant to provide the low-level reference information to operate it (and ideally
matches the actual Verilog implementation). Please mention it if you spot a discrepency.

**Section Index:**  

- [Xosera - Xark's Open Source Embedded Retro Adapter](#xosera---xarks-open-source-embedded-retro-adapter)
  - [Xosera Reference Information](#xosera-reference-information)
    - [Xosera Main Registers (XM Registers) Summary](#xosera-main-registers-xm-registers-summary)
    - [Xosera Main Register Details (XM Registers)](#xosera-main-register-details-xm-registers)
      - [0x0 **`XM_SYS_CTRL`** (R/W) - status for memory, blitter, hblank, vblank, RD\_RW\_INCR bit and VRAM write masking](#0x0-xm_sys_ctrl-rw---status-for-memory-blitter-hblank-vblank-rd_rw_incr-bit-and-vram-write-masking)
      - [0x1 **`XM_INT_CTRL`** (R/W+) - FPGA reconfigure, interrupt masking and interrupt status](#0x1-xm_int_ctrl-rw---fpga-reconfigure-interrupt-masking-and-interrupt-status)
      - [0x2 **`XM_TIMER`** (R/W) - tenth millisecond timer (0 - 6553.5 ms) / set countdown interrupt timer](#0x2-xm_timer-rw---tenth-millisecond-timer-0---65535-ms--set-countdown-interrupt-timer)
      - [0x3 **`XM_RD_XADDR`** (R/W+) - Read XR Register / Memory Address](#0x3-xm_rd_xaddr-rw---read-xr-register--memory-address)
      - [0x4 **`XM_WR_XADDR`** (R/W) - Write XR Register / Memory Address](#0x4-xm_wr_xaddr-rw---write-xr-register--memory-address)
      - [0x5 **`XM_XDATA`** (R+/W+) - eXtended Register / eXtended Region Data](#0x5-xm_xdata-rw---extended-register--extended-region-data)
      - [0x6 **`XM_RD_INCR`** (R/W) - increment value for `XM_RD_ADDR` when `XM_DATA`/`XM_DATA_2` is read from](#0x6-xm_rd_incr-rw---increment-value-for-xm_rd_addr-when-xm_dataxm_data_2-is-read-from)
      - [0x7 **`XM_RD_ADDR`** (R/W+) - VRAM read address for `XM_DATA`/`XM_DATA_2`](#0x7-xm_rd_addr-rw---vram-read-address-for-xm_dataxm_data_2)
      - [0x8 **`XM_WR_INCR`** (R/W) - increment value for `XM_WR_ADDR` when `XM_DATA`/`XM_DATA_2` is written](#0x8-xm_wr_incr-rw---increment-value-for-xm_wr_addr-when-xm_dataxm_data_2-is-written)
      - [0x9 **`XM_WR_ADDR`** (R/W) - VRAM write address for `XM_DATA`/`XM_DATA_2`](#0x9-xm_wr_addr-rw---vram-write-address-for-xm_dataxm_data_2)
      - [0xA **`XM_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`](#0xa-xm_data-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)
      - [0xB **`XM_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`](#0xb-xm_data_2-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)
      - [0xC **`UNUSED_C`** (-/-)](#0xc-unused_c---)
      - [0xD **`UNUSED_D`** (-/-)](#0xd-unused_d---)
      - [0xE **`UNUSED_E`** (-/-)](#0xe-unused_e---)
      - [0xF **`UNUSED_F`** (-/-)](#0xf-unused_f---)
  - [Xosera Extended Register / Extended Memory Region Summary](#xosera-extended-register--extended-memory-region-summary)
    - [Xosera Extended Registers Details (XR Registers)](#xosera-extended-registers-details-xr-registers)
    - [Video Config and Copper XR Registers Summary](#video-config-and-copper-xr-registers-summary)
    - [Video Config and Copper XR Registers Details](#video-config-and-copper-xr-registers-details)
      - [0x00 **`XR_VID_CTRL` (R/W) - interrupt status/signal and border color**](#0x00-xr_vid_ctrl-rw---interrupt-statussignal-and-border-color)
      - [0x01 **`XR_COPP_CTRL` (R/W) - copper enable**](#0x01-xr_copp_ctrl-rw---copper-enable)
      - [0x02 **`XR_AUD_CTRL` (R/W) - audio channel control register 0x02**](#0x02-xr_aud_ctrl-rw---audio-channel-control-register-0x02)
      - [0x03 **`XR_SCANLINE` (R/W) - current video scan line/trigger Xosera host CPU video interrupt**](#0x03-xr_scanline-rw---current-video-scan-linetrigger-xosera-host-cpu-video-interrupt)
      - [0x04 **`XR_VID_LEFT` (R/W) - video display window left edge**](#0x04-xr_vid_left-rw---video-display-window-left-edge)
      - [0x05 **`XR_VID_RIGHT` (R/W) - video display window right edge**](#0x05-xr_vid_right-rw---video-display-window-right-edge)
      - [0x06 **`XR_UNUSED_06` (--) - unused XR register 0x06**](#0x06-xr_unused_06------unused-xr-register-0x06)
      - [0x07 **`XR_UNUSED_07` (--) - unused XR register 0x07**](#0x07-xr_unused_07------unused-xr-register-0x07)
      - [0x08 **`XR_UNUSED_08` (--) - unused XR register 0x08**](#0x08-xr_unused_08------unused-xr-register-0x08)
      - [0x09 **`XR_UNUSED_09` (--) - unused XR register 0x09**](#0x09-xr_unused_09------unused-xr-register-0x09)
      - [0x0A **`XR_UNUSED_0A` (--) - unused XR register 0x0A**](#0x0a-xr_unused_0a------unused-xr-register-0x0a)
      - [0x0B **`XR_UNUSED_0B` (--) - unused XR register 0x0B**](#0x0b-xr_unused_0b------unused-xr-register-0x0b)
      - [0x0C **`XR_UNUSED_0C` (--) - unused XR register 0x0C**](#0x0c-xr_unused_0c------unused-xr-register-0x0c)
      - [0x0D **`XR_UNUSED_0D` (--) - unused XR register 0x0D**](#0x0d-xr_unused_0d------unused-xr-register-0x0d)
      - [0x0E **`XR_UNUSED_0E` (--) - unused XR register 0x0E**](#0x0e-xr_unused_0e------unused-xr-register-0x0e)
      - [0x0F **`XR_UNUSED_0F` (--) - unused XR register 0x0F**](#0x0f-xr_unused_0f------unused-xr-register-0x0f)
    - [Playfield A \& B Control XR Registers Summary](#playfield-a--b-control-xr-registers-summary)
    - [Playfield A \& B Control XR Registers Details](#playfield-a--b-control-xr-registers-details)
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

Xosera uses an 8-bit parallel bus with 4 bits to select from one of 16 main 16-bit bus accessable registers (and a
bit to select the even or odd register half, using 68000 big-endian convention of high byte at even addresses).
Since Xosera uses an 8-bit bus, for low-level access on 68K systems multi-byte operations can utilize the `MOVEP.W` or
`MOVEP.L` instructions that skip the unused half of the 16-bit data bus (also known as a 8-bit "6800" style peripheral
bus). When this document mentions, for example "writing a word", it means writing to the even and then odd bytes of a
register (usually with `MOVE.P` but multiple `MOVE.B` instructions work also). For some registers there is a "special
action" that happens on a 16-bit word when the odd (low byte) is either read or written (generally noted).

Xosera's 128KB of VRAM is organized as 64K x 16-bit words, so a full VRAM address is 16-bits (and an
individual byte is not directly accessible, only 16-bit words, however write "nibble masking" is available).

In addition to the main registers and VRAM, there is an additional extended register / memory XR bus that provides
access to control registers for system control, video configuration, drawing engines and display co-processor as
well as additional memory regions for tile definitions, color look-up and display coprocessor instructions.
___

### Xosera Main Registers (XM Registers) Summary

<!--
This table is ugly, but worth it for clickable links
-->
| Reg # | Reg Name                                                                                                                      | Access | Description                                                                           |
| ----- | ----------------------------------------------------------------------------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------- |
| 0x0   | [**`XM_SYS_CTRL`**](#0x8-xm_sys_ctrl-rw---draw-busy-status-read-wait-reconfigure-interrupt-control-and-write-masking-control) | R /W   | status and option flags, VRAM write masking                                           |
| 0x1   | [**`XM_INT_CTRL`**](#0x8-xm_sys_ctrl-rw---draw-busy-status-read-wait-reconfigure-interrupt-control-and-write-masking-control) | R /W+  | FPGA reconfigure, interrupt masking, interrupt status                                 |
| 0x2   | [**`XM_TIMER`**](#0x9-xm_timer-rw---tenth-millisecond-timer-0---65535-ms--interrupt-clear)                                    | RO     | read 1/10th millisecond (1/10,000 second) timer                                       |
| 0x3   | [**`XM_RD_XADDR`**](#0x0-xm_xr_addr-rw---xr-register--memory-address)                                                         | R /W+  | XR register/address used for `XM_XDATA` read access                                   |
| 0x4   | [**`XM_WR_XADDR`**](#0x0-xm_xr_addr-rw---xr-register--memory-address)                                                         | R /W   | XR register/address used for `XM_XDATA` write access                                  |
| 0x5   | [**`XM_XDATA`**](#0x1-xm_xr_data-rw---extended-register--extended-region-data)                                                | R+/W+  | read/write XR register/memory at `XM_RD_XADDR`/`XM_WR_XADDR` (and increment address)  |
| 0x6   | [**`XM_RD_INCR`**](#0x2-xm_rd_incr-rw---increment-value-for-xm_rd_addr-when-xm_dataxm_data_2-is-read-from)                    | R /W   | increment value for `XM_RD_ADDR` read from `XM_DATA`/`XM_DATA_2`                      |
| 0x7   | [**`XM_RD_ADDR`**](#0x3-xm_rd_addr-rw---vram-read-address-for-xm_dataxm_data_2)                                               | R /W+  | VRAM address for reading from VRAM when `XM_DATA`/`XM_DATA_2` is read                 |
| 0x8   | [**`XM_WR_INCR`**](#0x4-xm_wr_incr-rw---increment-value-for-xm_wr_addr-when-xm_dataxm_data_2-is-written)                      | R /W   | increment value for `XM_WR_ADDR` on write to `XM_DATA`/`XM_DATA_2`                    |
| 0x9   | [**`XM_WR_ADDR`**](#0x5-xm_wr_addr-rw---vram-write-address-for-xm_dataxm_data_2)                                              | R /W   | VRAM address for writing to VRAM when `XM_DATA`/`XM_DATA_2` is written                |
| 0xA   | [**`XM_DATA`**](#0x6-xm_data-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)                        | R+/W+  | read/write VRAM word at `XM_RD_ADDR`/`XM_WR_ADDR` (and add `XM_RD_INCR`/`XM_WR_INCR`) |
| 0xB   | [**`XM_DATA_2`**](#0x7-xm_data_2-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)                    | R+/W+  | 2nd `XM_DATA`(to allow for 32-bit read/write access)                                  |
| 0xC   | **`UNUSED_0C`**                                                                                                               |        |                                                                                       |
| 0xD   | **`UNUSED_0D`**                                                                                                               |        |                                                                                       |
| 0xE   | **`UNUSED_0E`**                                                                                                               |        |                                                                                       |
| 0xF   | **`XM_FEATURES`**                                                                                                             | RO     | Bits indicating features present in Xosera                                            |

(`R+` or `W+` indicates that reading or writing this register can have additional "side effects", respectively)
___

### Xosera Main Register Details (XM Registers)

#### 0x0 **`XM_SYS_CTRL`** (R/W) - status for memory, blitter, hblank, vblank, RD_RW_INCR bit and VRAM write masking

<img src="./pics/wd_XM_SYS_CTRL.svg">  

**Read memory busy, blit full, blit busy, vblank, hblank, read wait. Read/write RD_RW_INCR bit and VRAM nibble write mask.**  
Read-only:  
&nbsp;&nbsp;&nbsp;&nbsp;`[15]` memory read/write operation still in progress (for contended memory)  
&nbsp;&nbsp;&nbsp;&nbsp;`[14]` blit queue full (can't safely write to blit registers when set)  
&nbsp;&nbsp;&nbsp;&nbsp;`[13]` blit busy (blit operations not fully completed, but queue may be empty)  
&nbsp;&nbsp;&nbsp;&nbsp;`[12]` unused  
&nbsp;&nbsp;&nbsp;&nbsp;`[11]` horizontal blank flag (i.e., current pixel is not visible, off left/right edge)  
&nbsp;&nbsp;&nbsp;&nbsp;`[10]` vertical blank flag (i.e., current line is not visible, off top/bottom edge)  
&nbsp;&nbsp;&nbsp;&nbsp;`[9]` unused  
Read/Write:  
&nbsp;&nbsp;&nbsp;&nbsp;`[3:0]` `XM_DATA`/`XM_DATA_2` nibble write masks.  When a bit corresponding to a given nibble is zero, writes to that nibble are ignored and the original value is retained. For example, if the nibble mask is `0010` then only bits `[7:3]` of VRAM would be written.  This can be useful to isolate a pixel within a word of VRAM.

#### 0x1 **`XM_INT_CTRL`** (R/W+) - FPGA reconfigure, interrupt masking and interrupt status

<img src="./pics/wd_XM_INT_CTRL.svg">  

**Reconfigure (write=1)**:  
&nbsp;&nbsp;&nbsp;&nbsp;`[15]` Reconfig FPGA using `XM_INT_CTRL` bits [9:8] for new configuration  **NOTE:** Xosera
reconfiguration takes ~100ms and during this time Xosera will be unresponsive and no display signal will be generated  
**Interrupt mask (1=enabled, 0=masked)**:  
&nbsp;&nbsp;&nbsp;&nbsp;`[14]` blitter queue empty interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[13]` countdown timer interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[12]` video (vblank/COPPER) interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[11]` audio channel 3 ready for `XR_AUD3_START`/`XR_AUD3_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[10]` audio channel 3 ready for `XR_AUD2_START`/`XR_AUD2_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[9]` audio channel 3 ready for `XR_AUD1_START`/`XR_AUD1_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[8]` audio channel 3 ready for `XR_AUD0_START`/`XR_AUD0_LENGTH`  
**Interrupt status (read 1=pending, write 1=acknowledge & clear)**:  
&nbsp;&nbsp;&nbsp;&nbsp;`[6]` blitter queue empty interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[5]` countdown timer interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[4]` video (vblank/COPPER) interrupt  
&nbsp;&nbsp;&nbsp;&nbsp;`[3]` audio channel 3 ready for `XR_AUD3_START`/`XR_AUD3_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[2]` audio channel 3 ready for `XR_AUD2_START`/`XR_AUD2_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[1]` audio channel 3 ready for `XR_AUD1_START`/`XR_AUD1_LENGTH`  
&nbsp;&nbsp;&nbsp;&nbsp;`[0]` audio channel 3 ready for `XR_AUD0_START`/`XR_AUD0_LENGTH`  

#### 0x2 **`XM_TIMER`** (R/W) - tenth millisecond timer (0 - 6553.5 ms) / set countdown interrupt timer

<img src="./pics/wd_XM_TIMER.svg">

**Read 16-bit timer, increments every 1/10<sup>th</sup> of a millisecond (10,000 Hz)**  
Can be used for fairly accurate timing. Internal fractional value is maintined (so as accurate as FPGA PLL
clock).  Can be used for elapsed time up to ~6.5 seconds (or unlimited, if the cumulative elapsed time is updated at least as often as timer wrap value).
**NOTE:** To assure an atomic incrementing 16-bit value, when the high byte of TIMER is read, the low byte is saved into an internal register and returned when TIMER low byte is read. Because of this reading the full 16-bit TIMER register is recommended (or first even byte, then odd byte, or odd byte value may not update).  
**Write to set countdown interrupt interval**  
When written sets countdown timer value.  Timer is decremented every 1/10<sup>th</sup> millisecond and when timer reaches 0x0000 an Xosera timer interrupt (5) will be generated and the count will be reset.  This countdown timer value can only be written (as a read returns the free running TIMER).

#### 0x3 **`XM_RD_XADDR`** (R/W+) - Read XR Register / Memory Address

<img src="./pics/wd_XM_RD_XADDR.svg">

**Extended register or memory address for data read via `XM_XDATA`**  
Specifies the XR register or XR region address to be read via `XM_XDATA`.  As soon as this register is written, a 16-bit read operation will take place on the register or memory region specified (and the data wil be available from `XM_XDATA`).

#### 0x4 **`XM_WR_XADDR`** (R/W) - Write XR Register / Memory Address

<img src="./pics/wd_XM_WR_XADDR.svg">

**Extended register or memory address for data written to `XM_XDATA`**  
Specifies the XR register or XR region address to be accessed via `XM_XDATA`.
The register mapping with `XM_XDATA` following `XM_WR_XADDR` allows for M68K code similar to the following to set an
XR register (or XR memory word) to an immediate word value:
&emsp;&emsp;`MOVE.L #$rrXXXX,D0`
&emsp;&emsp;`MOVEP.L D0,XR_WR_XADDR(A1)`

#### 0x5 **`XM_XDATA`** (R+/W+) - eXtended Register / eXtended Region Data  

<img src="./pics/wd_XM_XDATA.svg">

**Read or write XR register/memory value from/to `XM_RD_XADDR`/`XM_WR_XADDR` and increment `XM_RD_XADDR`/`XM_WR_XADDR`,
respectively.**  
When `XM_XDATA` is read, data from XR address `XM_RD_XADDR` is returned and `XM_RD_XADDR` is incremented and pre-reading the
next word begins.  
When `XM_XDATA` is written, value is written to XR address `XM_WR_XADDR` and `XM_WR_XADDR` is incremented.

#### 0x6 **`XM_RD_INCR`** (R/W) - increment value for `XM_RD_ADDR` when `XM_DATA`/`XM_DATA_2` is read from

<img src="./pics/wd_XM_RD_INCR.svg">

**Read or write a twos-complement value added to `XM_RD_ADDR` when `XM_DATA` or `XM_DATA_2` is read from**  
Allows quickly reading Xosera VRAM from `XM_DATA`/`XM_DATA_2` when using a fixed increment.  
Added to `XM_RD_ADDR` when `XM_DATA` or `XM_DATA_2` is read from (twos complement, so value can be negative).

#### 0x7 **`XM_RD_ADDR`** (R/W+) - VRAM read address for `XM_DATA`/`XM_DATA_2`

<img src="./pics/wd_XM_RD_ADDR.svg">

**Read or write VRAM address that will be read when `XM_DATA` or `XM_DATA_2` is read from.**  
Specifies VRAM address used when reading from VRAM via `XM_DATA`/`XM_DATA_2`.  
When `XM_RD_ADDR` is written (or when auto incremented) the corresponding word in VRAM is read and made
available for reading at `XM_DATA` or `XM_DATA_2`.

#### 0x8 **`XM_WR_INCR`** (R/W) - increment value for `XM_WR_ADDR` when `XM_DATA`/`XM_DATA_2` is written

<img src="./pics/wd_XM_WR_INCR.svg">

**Read or write a twos-complement value added to `XM_WR_ADDR` when `XM_DATA` or `XM_DATA_2` is written to.**  
Allows quickly writing to Xosera VRAM via `XM_DATA`/`XM_DATA_2` when using a fixed increment.  
Added to `XM_WR_ADDR` when `XM_DATA` or `XM_DATA_2` is written to (twos complement, so value can be negative).

#### 0x9 **`XM_WR_ADDR`** (R/W) - VRAM write address for `XM_DATA`/`XM_DATA_2`

<img src="./pics/wd_XM_WR_ADDR.svg">

**Read or write VRAM address written when `XM_DATA` or `XM_DATA_2` is written to.**  
Specifies VRAM address used when writing to VRAM via `XM_DATA`/`XM_DATA_2`. Writing a value here does
not cause any VRAM access (which happens when data *written* to `XM_DATA` or `XM_DATA_2`).

#### 0xA **`XM_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`

<img src="./pics/wd_XM_DATA.svg">

**Read or write VRAM value from VRAM at `XM_RD_ADDR`/`XM_WR_ADDR` and add `XM_RD_INCR`/`XM_WR_INCR` to `XM_RD_ADDR`/`XM_WR_ADDR`,
respectively.**  
When `XM_DATA` is read data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the
new VRAM address begins.  
When `XM_DATA` is written, begins writing value to VRAM at `XM_WR_ADDR` and `XM_WR_INCR` is added to `XM_WR_ADDR`.

#### 0xB **`XM_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`

<img src="./pics/wd_XM_DATA.svg">

**Read or write VRAM value from VRAM at `XM_RD_ADDR`/`XM_WR_ADDR` and add `XM_RD_INCR`/`XM_WR_INCR` to `XM_RD_ADDR`/`XM_WR_ADDR`,
respectively.**  
When `XM_DATA_2` is read data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the
new VRAM address begins.  
When `XM_DATA_2` is written, begins writing value to VRAM at `XM_WR_ADDR` and adds `XM_WR_INCR` to `XM_WR_ADDR`.  
**NOTE:** This register is identical to `XM_DATA` to allow for 32-bit "long" MOVEP.L transfers to/from `XM_DATA` for
additional transfer speed.

#### 0xC **`UNUSED_C`** (-/-)  

#### 0xD **`UNUSED_D`** (-/-)  

#### 0xE **`UNUSED_E`** (-/-)  

#### 0xF **`UNUSED_F`** (-/-)  

___

## Xosera Extended Register / Extended Memory Region Summary

| XR Region Name    | XR Region Range | R/W | Description                                |
| ----------------- | --------------- | --- | ------------------------------------------ |
| XR video config   | 0x0000-0x000F   | R/W | config XR registers                        |
| XR playfield A    | 0x0010-0x0017   | R/W | playfield A XR registers                   |
| XR playfield B    | 0x0018-0x001F   | R/W | playfield B XR registers                   |
| XR audio channels | 0x0020-0x002F   | WO  | audio channel XR registers                 |
| XR blit engine    | 0x0040-0x004B   | WO  | 2D-blit XR registers                       |
| `XR_TILE_ADDR`    | 0x4000-0x53FF   | R/W | 5KW 16-bit tilemap/tile storage memory     |
| `XR_COLOR_ADDR`   | 0x8000-0x81FF   | R/W | 2 x 256W 16-bit color lookup memory (XRGB) |
| `XR_COPPER_ADDR`  | 0xC000-0xC3FF   | R/W | 1KW 16-bit copper program memory           |

To access an XR register or XR memory address, write the XR register number or address to `XM_RD_XADDR` or `XM_WR_XADDR` then read or write (respectively) to `XM_XDATA`. Each word *written* to `XM_XDATA` will also automatically increment `XM_WR_XADDR` to allow faster consecutive updates (like for color or tile memory update). The address in `XM_WR_XADDR` will similarly be incremented when reading from `XM_XDATA`.
While all XR registers and memory regions can be read, when there is high memory contention (e.g., it is being used for
video generation or other use), there is a `mem_wait` bit in `XM_SYS_CTRL` that will indicate when the last memory
operation is still pending.  
Also note that unlike the main 16 `XM` registers, the XR region can only be accessed as full 16-bit words (either
reading or writing). The full 16-bits of the `XM_XDATA` value are pre-read when `XM_RD_XADDR` is written or incremented and a full 16-bit word is written when the odd (low-byte) of `XM_XDATA` is written (the even/upper byte is latched).
___

### Xosera Extended Registers Details (XR Registers)

This XR registers are used to control of most Xosera operation other than CPU VRAM access and a few miscellaneous
control functions (which accessed directly via the main registers).  
To write to these XR registers, write an XR address to `XM_WR_XADDR` then write data to `XM_XDATA`. Each write to `XM_XDATA` will increment `XM_WR_XADDR` address by one (so you can repeatedly write to `XR_XDATA` for contiguous registers or XR memory).
Reading is similar, write the address to `XM_RD_XADDR` then read `XM_XDATA`. Each read of `XM_XDATA` will increment `XM_RD_XADDR` address by one (so you can repeatedly read `XR_XDATA` for contiguous registers or XR memory).

### Video Config and Copper XR Registers Summary

| Reg # | Reg Name                                                                                        | R /W | Description                                                       |
| ----- | ----------------------------------------------------------------------------------------------- | ---- | ----------------------------------------------------------------- |
| 0x00  | [**`XR_VID_CTRL`**](#0x00-xr_vid_ctrl-rw---interrupt-statussignal-and-border-color)             | R /W | display control and border color index                            |
| 0x01  | [**`XR_COPP_CTRL`**](#0x01-xr_copp_ctrl-rw---copper-start-address-and-enable)                   | R /W | display synchronized coprocessor control                          |
| 0x02  | [**`XR_AUD_CTRL`**](#0x02-xr_unused_02------unused-xr-register-0x02)                            | R /W | [#TODO]                                                           |
| 0x03  | [**`XR_UNUSED_03`**](#0x03-xr_unused_03------unused-xr-register-0x03)                           | R /W | [#TODO]                                                           |
| 0x04  | [**`XR_VID_LEFT`**](#0x06-xr_vid_left-rw---video-display-window-left-edge)                      | R /W | left edge start of active display window (normally 0)             |
| 0x05  | [**`XR_VID_RIGHT`**](#0x07-xr_vid_right-rw---video-display-window-right-edge)                   | R /W | right edge + 1 end of active display window (normally 640 or 848) |
| 0x04  | [**`XR_UNUSED_04`**](#0x04-xr_unused_04------unused-xr-register-0x04)                           | R /W | [#TODO]                                                           |
| 0x06  | [**`XR_UNUSED_06`**](#0x05-xr_unused_05------unused-xr-register-0x05)                           | R /W | [#TODO]                                                           |
| 0x07  | [**`XR_UNUSED_07`**](#0x05-xr_unused_05------unused-xr-register-0x05)                           | R /W | [#TODO]                                                           |
| 0x08  | [**`XR_SCANLINE`**](#0x08-xr_scanline-ro---current-video-display-scan-line-and-blanking-status) | RO   | current scanline (including off-screen)                           |
| 0x09  | [**`FEATURES`**](#0x09-xr_unused_09------unused-xr-register-0x09)                               | RO   | [#TODO] bits for optional features                                |
| 0x0A  | [**`XR_VID_HSIZE`**](#0x0d-xr_vid_hsize-ro---monitor-display-mode-native-horizontal-resolution) | RO   | native pixel width of monitor mode (e.g. 640/848)                 |
| 0x0B  | [**`XR_VID_VSIZE`**](#0x0e-xr_vid_vsize-ro---monitor-display-mode-native-vertical-resolution)   | RO   | native pixel height of monitor mode (e.g. 480)                    |
| 0x0C  | [**`XR_UNUSED_0C`**](#0x0A-xr_unused_0A------unused-xr-register-0x0A)                           | RO   | [#TODO]                                                           |
| 0x0D  | [**`XR_UNUSED_0D`**](#0x0B-xr_unused_0B------unused-xr-register-0x0B)                           | RO   | [#TODO]                                                           |
| 0x0E  | [**`XR_UNUSED_0E`**](#0x0C-xr_unused_0C------unused-xr-register-0x0C)                           | RO   | [#TODO]                                                           |
| 0x0F  | [**`XR_UNUSED_0F`**](#0x0F-xr_unused_0F------unused-xr-register-0x0F)                           | RO   | [#TODO]                                                           |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)

___

### Video Config and Copper XR Registers Details

#### 0x00 **`XR_VID_CTRL` (R/W) - interrupt status/signal and border color**  

<img src="./pics/wd_XR_VID_CTRL.svg">  

Pixels outside video window (`VID_LEFT`, `VID_RIGHT`) or when blanked will use border color 8-bit index.  This index is used with playfield A colormap (playfield B always uses color index 0 for border or blanked pixels).
When bit [15] colormap `swap` is set then playfields will "swap" which colormaps they use (so the 8-bit color index from playfield A will be used with colormap B and vice versa).  This effectively changes the playfield layer order, so playfield A will be "on top" and playfield B will be "underneath" (for blending purposes).

#### 0x01 **`XR_COPP_CTRL` (R/W) - copper enable**  

<img src="./pics/wd_XR_COPP_CTRL.svg">

Display synchronized co-processor enable/disable.  When enabled, the copper will be reset and the program restarted at the start of each frame (at start of line 0, offscreen left of first visible pixel).  When reset copper register `RA` will be `0x0000` and the `B` flag set and the copper PC will be set to copper address `0xC000` (first word of copper memory). See the below for details about copper operation.

#### 0x02 **`XR_AUD_CTRL` (R/W) - audio channel control register 0x02**  

<img src="./pics/wd_XR_AUD_CTRL.svg">

Enable audio channel DMA and output processing (if present in design, typically 2 or 4 channels present).

#### 0x03 **`XR_SCANLINE` (R/W) - current video scan line/trigger Xosera host CPU video interrupt**  

<img src="./pics/wd_XR_SCANLINE.svg">

Continuously updated with the scanline, lines 0-479 are visible (others are in vertical blank).  Each line starts with some non-visible pixels off the left edge of the display (160 pixels in 640x480 or 240 in 848x480).

A write to this register will trigger Xosera host CPU video interrupt (if unmasked and one is not already pending in `XM_INT_CTRL`).  This is mainly useful to allow COPPER to generate an arbitrary screen position synchronized CPU interrupt (in addition to the normal end of visible display v-blank interrupt).

#### 0x04 **`XR_VID_LEFT` (R/W) - video display window left edge**  

<img src="./pics/wd_XR_VID_LEFT.svg">

Defines left-most native pixel of video display window (normally 0 for full-screen).

#### 0x05 **`XR_VID_RIGHT` (R/W) - video display window right edge**  

<img src="./pics/wd_XR_VID_RIGHT.svg">

Defines right-most native pixel of video display window +1 (normally 640 or 848 for 4:3 or 16:9 full-screen, respectively).

#### 0x06 **`XR_UNUSED_06` (--) - unused XR register 0x06**  

Unused XR register 0x06

#### 0x07 **`XR_UNUSED_07` (--) - unused XR register 0x07**  

Unused XR register 0x07

#### 0x08 **`XR_UNUSED_08` (--) - unused XR register 0x08**  

Unused XR register 0x08

#### 0x09 **`XR_UNUSED_09` (--) - unused XR register 0x09**  

Unused XR register 0x09

#### 0x0A **`XR_UNUSED_0A` (--) - unused XR register 0x0A**  

Unused XR register 0x0A

#### 0x0B **`XR_UNUSED_0B` (--) - unused XR register 0x0B**  

Unused XR register 0x0B

#### 0x0C **`XR_UNUSED_0C` (--) - unused XR register 0x0C**  

Unused XR register 0x0C

#### 0x0D **`XR_UNUSED_0D` (--) - unused XR register 0x0D**  

Unused XR register 0x0D

#### 0x0E **`XR_UNUSED_0E` (--) - unused XR register 0x0E**  

Unused XR register 0x0E

#### 0x0F **`XR_UNUSED_0F` (--) - unused XR register 0x0F**  

Unused XR register 0x0F
___

### Playfield A & B Control XR Registers Summary

| Reg # | Name              | R/W | Description                                                  |
| ----- | ----------------- | --- | ------------------------------------------------------------ |
| 0x10  | `XR_PA_GFX_CTRL`  | R/W | playfield A graphics control                                 |
| 0x11  | `XR_PA_TILE_CTRL` | R/W | playfield A tile control                                     |
| 0x12  | `XR_PA_DISP_ADDR` | R/W | playfield A display VRAM start address                       |
| 0x13  | `XR_PA_LINE_LEN`  | R/W | playfield A display line width in words                      |
| 0x14  | `XR_PA_HV_SCROLL` | R/W | playfield A horizontal and vertical fine scroll              |
| 0x15  | `XR_PA_LINE_ADDR` | WO  | playfield A scanline start address (loaded at start of line) |
| 0x16  | `XR_PA_HV_FSCALE` | R/W | playfield A horizontal and vertical fractional scaling       |
| 0x17  | `XR_PA_UNUSED_17` | -/- |                                                              |
| 0x18  | `XR_PB_GFX_CTRL`  | R/W | playfield B graphics control                                 |
| 0x19  | `XR_PB_TILE_CTRL` | R/W | playfield B tile control                                     |
| 0x1A  | `XR_PB_DISP_ADDR` | R/W | playfield B display VRAM start address                       |
| 0x1B  | `XR_PB_LINE_LEN`  | R/W | playfield B display line width in words                      |
| 0x1C  | `XR_PB_HV_SCROLL` | R/W | playfield B horizontal and vertical fine scroll              |
| 0x1D  | `XR_PB_LINE_ADDR` | WO  | playfield B scanline start address (loaded at start of line) |
| 0x16  | `XR_PB_HV_FSCALE` | R/W | playfield B horizontal and vertical fractional scaling       |
| 0x1F  | `XR_PB_UNUSED_1F` | -/- |                                                              |
___

### Playfield A & B Control XR Registers Details

**0x10 `XR_PA_GFX_CTRL` (R/W) - playfield A (base) graphics control**  
**0x18 `XR_PB_GFX_CTRL` (R/W) - playfield B (overlay) graphics control**  
<img src="./pics/wd_XR_Px_GFX_CTRL.svg">

**playfield A/B graphics control**  
colorbase is used for any color index bits not in source pixel (e.g., the upper 4-bits of 4-bit pixel).  
blank bit set will disable display memory fetch and "blank" the display to a solid color (`XR_VID_CTRL` bordercolor, XOR'd with colorbase).  
bitmap 0 for tiled character graphics (see `XR_Px_TILE_CTRL`) using display word with attribute and tile index.  
bitmap 1 for bitmapped mode (1-bpp mode uses a 4-bit fore/back color attributes in upper 8-bits of each word).  
bpp selects bits-per-pixel or the number of color index bits per pixel (see "Graphics Modes" [#TODO]).  
H repeat selects the number of native pixels wide an Xosera pixel will be (1-4).  
V repeat selects the number of native pixels tall an Xosera pixel will be (1-4).  

**0x11 `XR_PA_TILE_CTRL` (R/W) - playfield A (base) tile control**  
**0x19 `XR_PB_TILE_CTRL` (R/W) - playfield B (overlay) tile control**  
<img src="./pics/wd_XR_Px_TILE_CTRL.svg">

**playfield A/B tile control**  
tile base address selects the upper bits of tile storage memory on 1KW boundaries.  
disp selects tilemap data (tile index and attributes) in VRAM or XR TILEMAP memory (5KW of tile XR memory, upper bits
ignored).  
tile selects tile definitions in XR TILEMAP memory or VRAM (5KW of tile XR memory, upper bits ignored).  
tile height selects the tile height-1 from (0-15 for up to 8x16). Tiles are stored as either 8 or 16 lines high. Tile lines past
height are truncated when displayed (e.g., tile height of 11 would display 8x12 of 8x16 tile).

**0x12 `XR_PA_DISP_ADDR` (R/W) - playfield A (base) display VRAM start address**  
**0x1A `XR_PB_DISP_ADDR` (R/W) - playfield B (overlay) display VRAM start address**  
<img src="./pics/wd_XR_Px_DISP_ADDR.svg">

**playfield A/B display start address**  
Address in VRAM for start of playfield display (tiled or bitmap).

**0x13 `XR_PA_LINE_LEN` (R/W) - playfield A (base) display line word length**  
**0x1B `XR_PB_LINE_LEN` (R/W) - playfield B (overlay) display line word length**  
<img src="./pics/wd_XR_Px_LINE_LEN.svg">

**playfield A/B display line word length**  
Length in words for each display line (i.e., the amount added to line start address for the start of the next line - not the width
of the display).  
Twos complement, so negative values are okay (for reverse scan line order in memory).

**0x14 `XR_PA_HV_SCROLL` (R/W) - playfield A (base) horizontal and vertical fine scroll**  
**0x1C `XR_PB_HV_SCROLL` (R/W) - playfield B (overlay) horizontal and vertical fine scroll**  
<img src="./pics/wd_XR_Px_HV_SCROLL.svg">

**playfield A/B  horizontal and vertical fine scroll**  
Horizontal fine scroll should be constrained to the scaled width of 8 pixels or 1 tile (e.g., `H_REPEAT` 1x = 0-7, 2x = 0-15, 3x =
0-23 and 4x = 0-31).  
vertical fine scroll should be constrained to the scaled height of a tile or (one less than the tile-height times `V_REPEAT`).
(But hey, we will see what happens, it might be fine...)

**0x15 `XR_PA_LINE_ADDR` (WO) - playfield A (base) display VRAM line address**  
**0x1D `XR_PB_LINE_ADDR` (WO) - playfield B (overlay) display VRAM line address**  
<img src="./pics/wd_XR_Px_LINE_ADDR.svg">

**playfield A/B display line address**  
Address in VRAM for start of next scanline (tiled or bitmap). This is generally used to allow the copper to change the display
address per scanline. Write-only.

**0x16 `XR_PA_HV_FSCALE` (R/W) - playfield A (base) horizontal and vertical fractional scale 0x16**  
**0x1E `XR_PB_HV_FSCALE` (R/W) - playfield B (overlay) horizontal and vertical fractional scale 0x1E**  
<img src="./pics/wd_XR_Px_HV_FSCALE.svg">  
Fractional scale factor to *repeat* a native pixel column and/or scanline to reduce pixel resolution (the "fractional" part accumulates until an entire row/column is repeated).  
Will repeat the color of a native column or scan-line every N+1<sup>th</sup> column or line (see native resolution scaling table above, values rounded to integer values).  
This repeat scaling is applied in addition to the integer pixel repeat (so a repeat value of 3x and fractional scale of 1 [repeat every line], would make 6x effective scale).  
NOTE: Handy to reduce VRAM consumption a bit and to get the correct aspect ratio on "classic" 320x200 or 640x400 artwork.

vertical fine scroll should be constrained to the scaled height of a tile or (one less than the tile-height times VSCALE).
(But hey, we will see what happens, it might be fine...)

**0x17 `XR_PA_UNUSED_17` (-/-) - unused XR PA register 0x17**  
**0x1F `XR_PB_UNUSED_1F` (-/-) - unused XR PB register 0x1F**  
Unused XR playfield registers 0x17, 0x1F
___

### 2D Blitter Engine Operation

The Xosera blitter is a VRAM data read/write "engine" that operates on 16-bit words in VRAM to copy, fill and do logic operations on arbitrary two-dimensional rectangular areas of Xosera VRAM (i.e., to draw, copy and manipulate "chunky" 4/8 bit pixel bitmap images). It supports a VRAM source and destination. The source can also be set to a constant. It repeats a basic word length operation for each "line" of a rectanglar area, and at the end of each line adds "modulo" values to source and destination addresses (to position source and destination for next line). It also has a "nibble shifter" that can shift a line of word data 0 to 3 nibbles to allow for pixel level positioning and drawing. There are also first and last word edge transparency masks, to remove unwanted pixels from the words at the start end end line allowing for arbitrary pixel sized rectangular operations. There is a fixed logic equation with ANDC and XOR terms (a combination of which can set, clear, invert or pass through any source color bits). This also works in conjunction with "transparency" testing that can mask-out specified 4-bit or 8-bit pixel values when writing. This transparency masking allows specified nibbles in a word to remain unaltered when the word is overwritten (without read-modify-write VRAM access, and no speed penalty). The combination of these features allow for many useful graphical "pixel" operations to be performed in a single pass (copying, masking, shifting and logical operations).

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

The `BLIT_SHIFT` register controls the nibble shifter that can shift 0 to 3 pixels to the right (or to the left in
`DECR` mode). Pixels shifted out of a word, will be shifted into the next word. There are also first and last word 4-bit
masks to mask unwanted pixels on the edges of a rectangle (when not word aligned, and pixels shifted off the end of the
previous line). When drawing in normal increment mode, the first word mask will trim pixels on the left edge and the
last word mask the right edge (or both at once for single word width).

#### Blitter Operation Status

The blitter provides two bits of status information that can be read from the XM register `XM_SYS_CTRL`.

- `BLIT_BUSY` set when blitter is busy with an operation (or has an operaion queued). Use this bit to check if blitter
  has completed all operations.
- `BLIT_FULL` set when the blitter queue is full and it cannot accept new operations. The blitter supports one blit
  operation in progress and one queued in registers. Use this bit to check if the blitter is ready to have a new
  operation stored into its registers (so it can stay busy, without idle pauses while a new operation is setup).

The biltter will also generate an Xosera interrupt with interrupt source #1 each time each blit operation is completed
(when interrupt source #1 mask is 1 in `XM_SYS_CTRL`).

### 2D Blitter Engine XR Registers Summary

| Reg # | Name            | R/W  | Description                                                          |
| ----- | --------------- | ---- | -------------------------------------------------------------------- |
| 0x20  | `XR_BLIT_CTRL`  | -/W  | blitter control (transp value, transp_8b, transp, S_const)           |
| 0x21  | `XR_BLIT_ANDC`  | -/W  | `ANDC` AND-COMPLEMENT constant value                                 |
| 0x22  | `XR_BLIT_XOR`   | -/W  | `XOR` XOR constant value                                             |
| 0x23  | `XR_BLIT_MOD_S` | -/W  | end of line modulo value added to `S` address                        |
| 0x24  | `XR_BLIT_SRC_S` | -/W  | `S` source read VRAM address (or constant value if `S_CONST`)        |
| 0x25  | `XR_BLIT_MOD_D` | -/W  | end of line modulo added to `D` destination address                  |
| 0x26  | `XR_BLIT_DST_D` | -/W  | `D` destination write VRAM address                                   |
| 0x27  | `XR_BLIT_SHIFT` | -/W  | first and last word nibble masks and nibble shift amount (0-3)       |
| 0x28  | `XR_BLIT_LINES` | -/W  | number of lines minus 1, (repeats blit word count after modulo calc) |
| 0x29  | `XR_BLIT_WORDS` | -/W+ | word count minus 1 per line (write starts blit operation)            |
| 0x2A  | `XR_BLIT_2A`    | -/-  | RESERVED                                                             |
| 0x2B  | `XR_BLIT_2B`    | -/-  | RESERVED                                                             |
| 0x2C  | `XR_BLIT_2C`    | -/-  | RESERVED                                                             |
| 0x2D  | `XR_BLIT_2D`    | -/-  | RESERVED                                                             |
| 0x2E  | `XR_BLIT_2E`    | -/-  | RESERVED                                                             |
| 0x2F  | `XR_BLIT_2F`    | -/-  | RESERVED                                                             |

**NOTE:** None of the blitter registers are readable (reads will return unpredictable values). However, registers will
not be altered between blitter operations so only registers that need new values need be written before writing
`XR_BLIT_WORDS` to start a blit operation. This necessitates careful consideration/coordination when using the blitter
from both interrupts and mainline code.

#### 2D Blitter Engine XR Registers Details

**0x20 `XR_BLIT_CTRL` (WO) - control bits (transparency control, S const)**  
<img src="./pics/wd_XR_BLIT_CTRL.svg">

**blitter operation control**  
The basic logic operation applied to all operations: `D = A & B ^ C`
The operation has four options that can be independently specified in `XR_BLIT_CTRL`:

- `S_CONST` specifies `S` term is a constant (`XR_BLIT_SRC_S` used as constant instead of VRAM address to read)
  - NOTE: When `S` is a constant the value of `XR_BLIT_MOD_S` will still be added to it at the end of each line.

Additionally, a transparency testing can be to the source data: `4-bit mask M = (S != T)` (testing either per nibble or byte, for 4-bit or 8-bit modes).  When a nibble/byte is masked, the existing pixel in VRAM will not be modified.

- `TRANSP` will enable transparency testing when set (when zero, no pixel values will be masked, but start and end of line masking will still apply)
- `TRANSP8` can be set so 8-bit transparency test (vs 4-bit).  Pixels are tested for transparency and masked only when the both nibbles in a byte match the transparency value
- `T` value is set in with the upper 8 bits of `XR_BLIT_CTRL` register and is the transparent value for even and odd 4-bit pixels or a single 8-bit pixel value (when `TRANSP8` set).

**0x21 `XR_BLIT_ANDC` (WO) - source term ANDC value constant**  
<img src="./pics/wd_XR_BLIT_ANDC.svg">

**source constant term `ANDC` (AND-complement) value**  
Arbitrary value for used for `ANDC` AND-complement with source term `S`, in equation: `D = S & (~ANDC) ^ XOR`

**0x22 `XR_BLIT_XOR` (WO) - source term XOR value constant**  
<img src="./pics/wd_XR_BLIT_XOR.svg">

**source constant term `XOR` (exclusive OR) value**  
Arbitrary value for used for `XOR` exclusive-OR with source term `S`, in equation: `D = S & (~ANDC) ^ XOR`

**0x23 `XR_BLIT_MOD_S` (WO) - modulo added to `BLIT_SRC_S` address at end of line**  
<img src="./pics/wd_XR_BLIT_MOD_S.svg">

**modulo added to `BLIT_SRC_S` address at end of a line**  
Arbitrary twos complement value added to `S` address/constant at the end of each line. Typically zero when `BLIT_SRC_S` image data is contiguous, or -1 when shift amount is non-zero.

**0x24 `XR_BLIT_SRC_S` (WO) - source `S` term (read from VRAM address or constant value)**  
<img src="./pics/wd_XR_BLIT_SRC_S.svg">

**source term `S` VRAM address (with term read from VRAM) or a constant value if `S_CONST` set**  
Address of source VRAM image data or arbitrary constant if `S_CONST` set in `XR_BLIT_CTRL`. This value will be shifted by `XR_BLIT_SHIFT` nibble shift amount (when not a constant with `S_CONST` set)

**0x25 `XR_BLIT_MOD_D` (WO) - modulo added to `BLIT_DST_D` address at end of line**  
<img src="./pics/wd_XR_BLIT_MOD_D.svg">

**modulo added to `BLIT_DST_D` destination address at end of a line**  
Arbitrary twos complement value added to `D` destination address at the end of each line. Typically the *destination_width*-*source_width* (in words) to adjust the destination pointer to the start of the next rectangular image line.

**0x26 `XR_BLIT_DST_D` (WO) - destination D VRAM write address**  
<img src="./pics/wd_XR_BLIT_DST_D.svg">

**destination D VRAM write address**  
Destination VRAM address.  Set to the first word of the destination address for the blit operation (or the last word of the destination, if in `DECR` mode).

**0x27 `XR_BLIT_SHIFT` (WO - first and last word nibble masks and nibble shift**  
<img src="./pics/wd_XR_BLIT_SHIFT.svg">

**first and last word nibble masks and nibble shift**  
The first word nibble mask will unconditionally mask out (make transparent) any zero nibbles on the first word of each
line (left edge). Similarly, the last word nibble mask will unconditionally mask out
(make transparent) any zero nibbles on the last word of each line (the right edge). The
nibble shift specifies a 0 to 3 nibble right shift on all words drawn. Nibbles shifted out the right of a word will be shifted into the the next word (to provide a seamless pixel shift
with word aligned writes). In "normal" use, when the nibble shift is non-zero, the
left and right nibble mask would typically contain the value `0xF0` nibble shifted (e.g., `0xF0 >> nibble_shift`). The
right edge mask hides "garbage" pixels wrapping from left edge, and right edge mask hides excess pixels when image is
not a exact word width. When shifting, typically also you need to add an extra word to `BLIT_WORDS` and subtract one
from `BLIT_MOD_S` (to avoid skewing the image). When not shifting (nibble shift of zero) and your source image width is
word aligned, you typically want the complete last word so both left and right mask `0xFF` (no edge masking). If your
source image is not an exact multiple word width, you would typically want to trim the excess pixels from the right edge
(e.g., for a 7 nibble wide image, `0xFE`). For images 1 word wide, both left and right edge masks will be AND'd
together. Also this masking is AND'd with the normal transparency control (so if a pixel is masked by either the left
mask, right mask or is considered transparent it will not be modified).

**0x28 `XR_BLIT_LINES` (WO) - 15-bit number of lines heigh - 1 (1 to 32768)**  
<img src="./pics/wd_XR_BLIT_LINES.svg">

**15-bit number of lines high - 1 (1 to 32768)**  
Number of times to repeat blit operation minus one. Typically source image height with modulo values advancing addresses
for the next line to be drawn).

**0x29 `XR_BLIT_WORDS` (WO) - write starts operation, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times)**  
<img src="./pics/wd_XR_BLIT_WORDS.svg">

**write starts blit, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times))**  
Write starts blit operation, you should check `BLIT_FULL` bit in `XM_SYS_CTRL` to make sure the blitter is ready to
accept a new operation (it supports one operation in progress and one operation queued to start - after that it reports
`BLIT_FULL` true and you should wait before altering any BLIT registers).  
To check if all blit operation has completely finished (and no operations queued), you can check the `BLIT_BUSY` bit in `XM_SYS_CTRL` which will read true until the blit operation has come to a complete stop.

___

## Video Synchronized Co-Processor Details

Xosera provides a dedicated video co-processor, synchronized to the pixel clock. The co-processor (or "copper") can be
programmed to perform video register manipulation on a frame-by-frame basis in sync with the video beam. With careful
programming this enables many advanced effects to be achieved, such as multi-resolution displays and simultaneous
display of more colors than would otherwise be possible.

Interaction with the copper involves two Xosera memory areas:

- XR Register 0x01 - The **`XR_COPP_CTRL`** register
- Xosera XR memory area **`0xC000-0xC7FF`** - The **`XR_COPPER_ADDR`** area

In general, programming the copper comprises loading a copper program (or 'copper list') into
the `XR_COPPER_ADDR` area, and then setting the starting PC (if necessary) and enable bit in
the `XR_COPP_CTRL` register.

**NOTE:** The PC contained in the control register is the *initial* PC, used to initialize
the copper at the next vertical blanking interval. It is **not** read during a frame, and
cannot be used to perform jumps - see instead the `JMP` instruction.

### Programming the Co-processor

As mentioned, copper programs (aka 'The Copper List') live in XR memory at `0xC000`. This memory
segment is 2K in size, and all copper instructions are 32-bits, meaning there is space for a
maximum of 512 instructions at any one time.

When enabled, the copper will run through as much of the program as time allows within each frame.
At the end of each frame (during the vertical blanking interval) the program is restarted at the
PC contained in the control register.

You should **always** set up a program for the copper before you enable it. Once enabled, it is
always running in sync with the pixel clock. There is no specific 'stop' or 'done' instruction
(though the wait instruction can be used with both X and Y ignored as a "wait for end of frame"
instruction).

Although illegal instructions are ignored, the undefined nature of memory at start up means that
if the copper is enabled without a program being loaded into its memory segment, undefined
behaviour and display corruption may result.

As far as the copper is concerned, all coordinates are in native resolution (i.e. 640x480 or
848x480). They do not account for any pixel doubling or other settings you may have made.

The copper broadly follows the Amiga copper in terms of instructions (though we may add more
as time progresses). There are multiple variants of the MOVE instruction that each move to a
different place (see next section).
___

### Co-processor Instruction Set

There are four basic copper instructions: `WAIT`, `SKIP`, `MOVE` and `JUMP`. Briefly, their function
is:

| Instruction | Description                                                                  |
| ----------- | ---------------------------------------------------------------------------- |
| `WAIT`      | Wait until the video beam reaches (or exceeds) a specified position          |
| `SKIP`      | Skip the next instruction if the video beam has reached a specified position |
| `MOVE`      | Move immediate data to a target destination                                  |
| `JUMP`      | Change the copper program-counter to the given immediate value               |

Copper instructions take multiple pixels to execute. The timing for each instruction is
detailed below, along with the binary format.

The `MOVE` instruction is actually subdivided into four specific types of move, as detailed
below.

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
| ------------- | ------------------------------------------------------- |
| 0x4000-0x47FF | 8x16 ST font (derived from Atari ST 8x16 font)          |
| 0x4800-0x4BFF | 8x8 ST font (derived from Atari ST 8x8 font             |
| 0x4C00-0x4FFF | 8x8 PC font (derived from IBM CGA 8x8)                  |
| 0x5000-0x53FF | 8x8 hexadecimal debug font (showing TILE number in hex) |

<!-- TODO: Add COLOR and COPPER info -->
