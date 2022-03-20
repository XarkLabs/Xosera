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
      - [0x0 **`XM_XR_ADDR`** (R/W+) - XR Register / Memory Address](#0x0-xm_xr_addr-rw---xr-register--memory-address)
      - [0x1 **`XM_XR_DATA`** (R/W+) - eXtended Register / eXtended Region Data](#0x1-xm_xr_data-rw---extended-register--extended-region-data)
      - [0x2 **`XM_RD_INCR`** (R/W) - increment value for `XM_RD_ADDR` when `XM_DATA`/`XM_DATA_2` is read from](#0x2-xm_rd_incr-rw---increment-value-for-xm_rd_addr-when-xm_dataxm_data_2-is-read-from)
      - [0x3 **`XM_RD_ADDR`** (R/W+) - VRAM read address for `XM_DATA`/`XM_DATA_2`](#0x3-xm_rd_addr-rw---vram-read-address-for-xm_dataxm_data_2)
      - [0x4 **`XM_WR_INCR`** (R/W) - increment value for `XM_WR_ADDR` when `XM_DATA`/`XM_DATA_2` is written](#0x4-xm_wr_incr-rw---increment-value-for-xm_wr_addr-when-xm_dataxm_data_2-is-written)
      - [0x5 **`XM_WR_ADDR`** (R/W) - VRAM write address for `XM_DATA`/`XM_DATA_2`](#0x5-xm_wr_addr-rw---vram-write-address-for-xm_dataxm_data_2)
      - [0x6 **`XM_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`](#0x6-xm_data-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)
      - [0x7 **`XM_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`](#0x7-xm_data_2-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)
      - [0x8 **`XM_SYS_CTRL`** (R/W+) - draw busy status, read wait, reconfigure, interrupt control and write masking control](#0x8-xm_sys_ctrl-rw---draw-busy-status-read-wait-reconfigure-interrupt-control-and-write-masking-control)
      - [0x9 **`XM_TIMER`** (R/W) - tenth millisecond timer (0 - 6553.5 ms) / interrupt clear](#0x9-xm_timer-rw---tenth-millisecond-timer-0---65535-ms--interrupt-clear)
      - [0xA **`XM_LFSR`** (RO) - LFSR pseudo-random number](#0xa-xm_lfsr-ro---lfsr-pseudo-random-number)
      - [0xB **`XM_UNUSED_B`** (R/W) - unused register 0xB](#0xb-xm_unused_b-rw---unused-register-0xb)
      - [0xC **`XM_RW_INCR`** (R/W) - increment value for `XM_RW_ADDR` when `XM_RW_DATA`/`XM_RW_DATA_2`is accessed](#0xc-xm_rw_incr-rw---increment-value-for-xm_rw_addr-when-xm_rw_dataxm_rw_data_2is-accessed)
      - [0xD **`XM_RW_ADDR`** (R/W+) - VRAM read/write address for accessed at `XM_RW_DATA`/`XM_RW_DATA_2`](#0xd-xm_rw_addr-rw---vram-readwrite-address-for-accessed-at-xm_rw_dataxm_rw_data_2)
      - [0xE **`XM_RW_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address`XM_RW_ADDR`](#0xe-xm_rw_data-rw---vram-memory-value-to-readwrite-at-vram-addressxm_rw_addr)
      - [0xF **`XM_RW_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address`XM_RW_ADDR`](#0xf-xm_rw_data_2-rw---vram-memory-value-to-readwrite-at-vram-addressxm_rw_addr)
  - [Xosera Extended Register / Extended Memory Region Summary](#xosera-extended-register--extended-memory-region-summary)
    - [Xosera Extended Registers Details (XR Registers)](#xosera-extended-registers-details-xr-registers)
    - [Video Config and Copper XR Registers Summary](#video-config-and-copper-xr-registers-summary)
    - [Video Config and Copper XR Registers Details](#video-config-and-copper-xr-registers-details)
    - [Playfield A & B Control XR Registers Summary](#playfield-a--b-control-xr-registers-summary)
    - [Playfield A & B Control XR Registers Details](#playfield-a--b-control-xr-registers-details)
    - [2D Blitter Engine Operation](#2d-blitter-engine-operation)
      - [Logic Operation Applied to Blitter Operations](#logic-operation-applied-to-blitter-operations)
      - [Transparency Testing nd Masking Applied to Blitter Operations](#transparency-testing-nd-masking-applied-to-blitter-operations)
      - [Nibble Shifting and First/Last Word Edge Masking](#nibble-shifting-and-firstlast-word-edge-masking)
      - [Decrement Mode](#decrement-mode)
      - [Blitter Operation Status](#blitter-operation-status)
    - [2D Blitter Engine XR Registers Summary](#2d-blitter-engine-xr-registers-summary)
      - [2D Blitter Engine XR Registers Details](#2d-blitter-engine-xr-registers-details)
  - [Video Synchronized Co-Processor Details](#video-synchronized-co-processor-details)
    - [Programming the Co-processor](#programming-the-co-processor)
    - [Co-processor Instruction Set](#co-processor-instruction-set)
      - [Notes on the MOVE variants](#notes-on-the-move-variants)
    - [Co-processor Assembler](#co-processor-assembler)

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

[#TODO: **NOTE:** It is planned to swap registers 0 and 1 with registers 8 and 9 along with remapping some `XM_SYS_CTRL` bits to
provide more efficient access to Xosera status flags (like busy and timeout)]

<!--
This table is ugly, but worth it for clickable links
-->
| Reg # | Reg Name       | R /W  | Description                                                                           |
| ----- | -------------- | ----- | ------------------------------------------------------------------------------------- |
| 0x0   | [**`XM_XR_ADDR`**](#0x0-xm_xr_addr-rw---xr-register--memory-address)   | R /W+ | XR register number/address for `XM_XR_DATA` read/write access                         |
| 0x1   | [**`XM_XR_DATA`**](#0x1-xm_xr_data-rw---extended-register--extended-region-data)   | R /W+ | read/write XR register/memory at `XM_XR_ADDR` (`XM_XR_ADDR` incr. on write)           |
| 0x2   | [**`XM_RD_INCR`**](#0x2-xm_rd_incr-rw---increment-value-for-xm_rd_addr-when-xm_dataxm_data_2-is-read-from)   | R /W  | increment value for `XM_RD_ADDR` read from `XM_DATA`/`XM_DATA_2`                      |
| 0x3   | [**`XM_RD_ADDR`**](#0x3-xm_rd_addr-rw---vram-read-address-for-xm_dataxm_data_2)   | R /W+ | VRAM address for reading from VRAM when `XM_DATA`/`XM_DATA_2` is read                 |
| 0x4   | [**`XM_WR_INCR`**](#0x4-xm_wr_incr-rw---increment-value-for-xm_wr_addr-when-xm_dataxm_data_2-is-written)   | R /W  | increment value for `XM_WR_ADDR` on write to `XM_DATA`/`XM_DATA_2`                    |
| 0x5   | [**`XM_WR_ADDR`**](#0x5-xm_wr_addr-rw---vram-write-address-for-xm_dataxm_data_2)   | R /W  | VRAM address for writing to VRAM when `XM_DATA`/`XM_DATA_2` is written                |
| 0x6   | [**`XM_DATA`**](#0x6-xm_data-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)      | R+/W+ | read/write VRAM word at `XM_RD_ADDR`/`XM_WR_ADDR` (and add `XM_RD_INCR`/`XM_WR_INCR`) |
| 0x7   | [**`XM_DATA_2`**](#0x7-xm_data_2-rw---vram-memory-value-to-readwrite-at-vram-address-xm_rd_addrxm_wr_addr)    | R+/W+ | 2nd `XM_DATA`(to allow for 32-bit read/write access)                                  |
| 0x8   | [**`XM_SYS_CTRL`**](#0x8-xm_sys_ctrl-rw---draw-busy-status-read-wait-reconfigure-interrupt-control-and-write-masking-control)  | R /W+ | busy status, FPGA reconfig, interrupt status/control, write masking                   |
| 0x9   | [**`XM_TIMER`**](#0x9-xm_timer-rw---tenth-millisecond-timer-0---65535-ms--interrupt-clear)     | R /W+ | read tenth millisecond timer                                              |
| 0xA   | [**`XM_LFSR`**](#0xa-xm_lfsr-ro---lfsr-pseudo-random-number)      | RO    | read LFSR pseudo random number (internally 19-bit)                                    |
| 0xB   | [**`XM_UNUSED_B`**](#0xb-xm_unused_b-rw---unused-register-0xb)  | R /W  | unused direct register 0xB [**#TODO**]                                                    |
| 0xC   | [**`XM_RW_INCR`**](#0xc-xm_rw_incr-rw---increment-value-for-xm_rw_addr-when-xm_rw_dataxm_rw_data_2is-accessed)   | R /W  | `XM_RW_ADDR` increment value on read/write of `XM_RW_DATA`/`XM_RW_DATA_2`             |
| 0xD   | [**`XM_RW_ADDR`**](#0xd-xm_rw_addr-rw---vram-readwrite-address-for-accessed-at-xm_rw_dataxm_rw_data_2)   | R /W+ | read/write address for VRAM access from `XM_RW_DATA`/`XM_RW_DATA_2`                   |
| 0xE   | [**`XM_RW_DATA`**](#0xe-xm_rw_data-rw---vram-memory-value-to-readwrite-at-vram-addressxm_rw_addr)   | R+/W+ | read/write VRAM word at `XM_RW_ADDR` (and add `XM_RW_INCR`)                           |
| 0xF   | [**`XM_RW_DATA_2`**](#0xf-xm_rw_data_2-rw---vram-memory-value-to-readwrite-at-vram-addressxm_rw_addr) | R+/W+ | 2nd `XM_RW_DATA`(to allow for 32-bit read/write access)                               |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)
___

### Xosera Main Register Details (XM Registers)

#### 0x0 **`XM_XR_ADDR`** (R/W+) - XR Register / Memory Address

<img src="./pics/wd_XM_XR_ADDR.svg">

**Extended register or memory address for data accessed via `XM_XR_DATA`**  
Specifies the XR register or XR region address to be accessed via `XM_XR_DATA`.
The upper bits select XR registers or memory region and the lower 13 bits select the register number or
memory word address within the region (see below for details of XR registers and memory).  
When `XM_XR_DATA` is written, the register/address specified will be read and made available for reading at `XM_XR_DATA`.
`XM_XR_DATA` needs to be written each time before *reading* `XM_XR_DATA` (or the previously read value will be returned).
After a word is *written* to `XM_XR_DATA`, the lower 13-bits of `XM_XR_DATA` will be auto-incremented which
allows writing to contiguous registers or memory by repeatedly writing to `XM_XR_DATA` (without needing to increment to
`XM_XR_ADDR` each time).  
The register mapping with `XM_XR_DATA` following `XM_XR_ADDR` allows for M68K code similar to the following to set an
XR register (or XR memory word) to an immediate word value:
&emsp;&emsp;`MOVE.L #$rrXXXX,D0`
&emsp;&emsp;`MOVEP.L D0,XR_ADDR(A1)`

#### 0x1 **`XM_XR_DATA`** (R/W+) - eXtended Register / eXtended Region Data  

<img src="./pics/wd_XM_XR_DATA.svg">

**Read or write extended register or memory addressed by `XM_XR_DATA` register.**  
Allows read/write access to the XR register or memory using address contained in `XM_XR_DATA` register.
When `XM_XR_DATA` is *written*, the XR register or address specified will be read will be available for *reading* at `XM_XR_DATA`
(`XM_XR_ADDR` needs to be set each time before reading `XM_XR_DATA` or the previously read value will be returned).  
After a word is *written* to `XM_XR_DATA`, the lower 13-bits of `XM_XR_DATA` will be auto-incremented which allows
writing to  contiguous XR registers or memory by repeatedly writing to `XM_XR_DATA`.

#### 0x2 **`XM_RD_INCR`** (R/W) - increment value for `XM_RD_ADDR` when `XM_DATA`/`XM_DATA_2` is read from

<img src="./pics/wd_XM_RD_INCR.svg">

**Read or write a twos-complement value added to `XM_RD_ADDR` when `XM_DATA` or `XM_DATA_2` is read from**  
Allows quickly reading Xosera VRAM from `XM_DATA`/`XM_DATA_2` when using a fixed increment.  
Added to `XM_RD_ADDR` when `XM_DATA` or `XM_DATA_2` is read from (twos complement, so value can be negative).

#### 0x3 **`XM_RD_ADDR`** (R/W+) - VRAM read address for `XM_DATA`/`XM_DATA_2`

<img src="./pics/wd_XM_RD_ADDR.svg">

**Read or write VRAM address that will be read when `XM_DATA` or `XM_DATA_2` is read from.**  
Specifies VRAM address used when reading from VRAM via `XM_DATA`/`XM_DATA_2`.  
When `XM_RD_ADDR` is written (or when auto incremented) the corresponding word in VRAM is read and made
available for reading at `XM_DATA` or `XM_DATA_2`.

#### 0x4 **`XM_WR_INCR`** (R/W) - increment value for `XM_WR_ADDR` when `XM_DATA`/`XM_DATA_2` is written

<img src="./pics/wd_XM_WR_INCR.svg">

**Read or write a twos-complement value added to `XM_WR_ADDR` when `XM_DATA` or `XM_DATA_2` is written to.**  
Allows quickly writing to Xosera VRAM via `XM_DATA`/`XM_DATA_2` when using a fixed increment.  
Added to `XM_WR_ADDR` when `XM_DATA` or `XM_DATA_2` is written to (twos complement, so value can be negative).

#### 0x5 **`XM_WR_ADDR`** (R/W) - VRAM write address for `XM_DATA`/`XM_DATA_2`

<img src="./pics/wd_XM_WR_ADDR.svg">

**Read or write VRAM address written when `XM_DATA` or `XM_DATA_2` is written to.**  
Specifies VRAM address used when writing to VRAM via `XM_DATA`/`XM_DATA_2`. Writing a value here does
not cause any VRAM access (which happens when data *written* to `XM_DATA` or `XM_DATA_2`).

#### 0x6 **`XM_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`

<img src="./pics/wd_XM_DATA.svg">

**Read or write VRAM value from VRAM at `XM_RD_ADDR`/`XM_WR_ADDR` and add `XM_RD_INCR`/`XM_WR_INCR` to `XM_RD_ADDR`/`XM_WR_ADDR`,
respectively.**  
When `XM_DATA` is read data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the
new VRAM address begins.  
When `XM_DATA` is written, begins writing value to VRAM at `XM_WR_ADDR` and `XM_WR_INCR` is added to `XM_WR_ADDR`.

#### 0x7 **`XM_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address `XM_RD_ADDR`/`XM_WR_ADDR`

<img src="./pics/wd_XM_DATA.svg">

**Read or write VRAM value from VRAM at `XM_RD_ADDR`/`XM_WR_ADDR` and add `XM_RD_INCR`/`XM_WR_INCR` to `XM_RD_ADDR`/`XM_WR_ADDR`,
respectively.**  
When `XM_DATA_2` is read data from VRAM at `XM_RD_ADDR` is returned and `XM_RD_INCR` is added to `XM_RD_ADDR` and pre-reading the
new VRAM address begins.  
When `XM_DATA_2` is written, begins writing value to VRAM at `XM_WR_ADDR` and adds `XM_WR_INCR` to `XM_WR_ADDR`.  
**NOTE:** This register is identical to `XM_DATA` to allow for 32-bit "long" MOVEP.L transfers to/from `XM_DATA` for
additional transfer speed.

#### 0x8 **`XM_SYS_CTRL`** (R/W+) - draw busy status, read wait, reconfigure, interrupt control and write masking control

<img src="./pics/wd_XM_SYS_CTRL.svg">  

**Read draw busy, read wait, write to reboot FPGA or read/write interrupt control/status and `XM_DATA` nibble write mask.**  
Read:  
&nbsp;&nbsp;&nbsp;&nbsp;`[11:8]`  interrupt enables  
&nbsp;&nbsp;&nbsp;&nbsp;`[7]` memory operation pending (when memory contention)  
&nbsp;&nbsp;&nbsp;&nbsp;`[6]` blit busy  (operation in progress or queued)
&nbsp;&nbsp;&nbsp;&nbsp;`[5]` blit queue full (don't write to blit registers)  
&nbsp;&nbsp;&nbsp;&nbsp;`[4]` value of flag `RW_RD_INC` flag
&nbsp;&nbsp;&nbsp;&nbsp;`[3:0]` `XM_DATA`/`XM_DATA_2`/`RW_DATA` nibble write masks  
Write:  
&nbsp;&nbsp;&nbsp;&nbsp;`[15]` reboot FPGA to `[14:13]` configuration 0-3
&nbsp;&nbsp;&nbsp;&nbsp;`[14:13]` reboot configuration
&nbsp;&nbsp;&nbsp;&nbsp;`[11:8]` `XM_DATA`/`XM_DATA_2` nibble write masks  
&nbsp;&nbsp;&nbsp;&nbsp;`[4]` set `RW_RD_INC` flag (controls if a *read* of  `RW_DATA` adds `RW_INCR` to `RW_ADDR`)
&nbsp;&nbsp;&nbsp;&nbsp;`[3:0]` interrupt mask (1 allows corresponding interrupt source to generate CPU interrupt).  
[#TODO optimize layout for 68k status polling]

#### 0x9 **`XM_TIMER`** (R/W) - tenth millisecond timer (0 - 6553.5 ms) / interrupt clear

<img src="./pics/wd_XM_TIMER.svg">
<img src="./pics/wd_XM_TIMER_W.svg">

**Read 16-bit timer, increments every 1/10<sup>th</sup> of a millisecond (10,000 Hz)**  
**Write to clear interrupt status**  
Can be used for fairly accurate timing. When value wraps, internal fractional value is maintined (so as accurate as FPGA PLL
clock).  Can be used for elapsed time up to ~6.5 seconds (or unlimited, if the cumulative elapsed time is updated at least that often).
**NOTE:** To assure an atomic incrementing 16-bit value, when the high byte of TIMER is read, the low byte is saved into an internal register and returned when TIMER low byte is read. Because of this reading the full 16-bit TIMER register is recommended (or first even byte, then odd byte, or odd byte value may not update).

#### 0xA **`XM_LFSR`** (RO) - LFSR pseudo-random number

<img src="./pics/wd_XM_LFSR.svg">

**Read 16-bit LFSR pseudo-random value**  
Read 16-bits from internal 19-bit LFSR (linear feedback shift-register). All values are possible and the value changes
every cycle asynchronus at the display pixel clock, it should provide "quite random" numbers (at least for most game
and graphics purposes).

#### 0xB **`XM_UNUSED_B`** (R/W) - unused register 0xB  

Unused direct register 0xB

#### 0xC **`XM_RW_INCR`** (R/W) - increment value for `XM_RW_ADDR` when `XM_RW_DATA`/`XM_RW_DATA_2`is accessed  

<img src="./pics/wd_XM_RW_INCR.svg">

**Read or write twos-complement value added to`XM_RW_ADDR` when `XM_RW_DATA` or `XM_RW_DATA_2`is written to (or read if `RW_RD_INC` flag set).**  
Allows quickly reading/writing Xosera VRAM from`XM_RW_DATA`/`XM_RW_DATA_2` when using a fixed `XM_RW_ADDR` increment.
Added to `XM_RW_ADDR` when `XM_RW_DATA` or `XM_RW_DATA_2` is written (twos complement so value can be negative).   If the `RW_RD_INC` flag is set in `XM_SYS_CTRL`, then a read of `XM_RW_DATA` or`XM_RW_DATA_2` will also add `XM_RW_INCR` to `XM_RW_ADDR` (this is useful when using `XM_RW_DATA` *only* for reading).

#### 0xD **`XM_RW_ADDR`** (R/W+) - VRAM read/write address for accessed at `XM_RW_DATA`/`XM_RW_DATA_2`

<img src="./pics/wd_XM_RW_ADDR.svg">

**Read or write VRAM address read when`XM_RW_DATA` or `XM_RW_DATA_2`is read from or written to.**  
Specifies VRAM address used when reading or writing from VRAM via`XM_RW_DATA`/`XM_RW_DATA_2`.
When `XM_RW_ADDR` is written (or incremented by `XM_RW_INCR`) the corresponding word in VRAM is read and made available for
reading at `XM_RW_DATA` or `XM_RW_DATA_2`.  
Since this read always happens (even when only a write was intended), prefer using `RW_ADDR` only when
reading is needed (however, it is a *tiny* VRAM access overhead).  The `RW_RD_INC` flag in `XM_SYS_CTRL` controls if `XM_RW_INCR` is added to `XM_RW_ADDR` when `XM_RW_DATA` or `XM_RW_DATA_2` are *read* (it is always added on a *write*).

#### 0xE **`XM_RW_DATA`** (R+/W+) - VRAM memory value to read/write at VRAM address`XM_RW_ADDR`

<img src="./pics/wd_XM_RW_DATA.svg">

**Read or write VRAM value in VRAM at`XM_RW_ADDR` and add `XM_RW_INCR` to `XM_RW_ADDR`.**  
When`XM_RW_DATA`is read, returns data from VRAM at `XM_RW_ADDR`.  If `RW_RD_INC` flag in `XM_SYS_CTRL`is set it also adds `XM_RW_INCR` to `XM_RW_ADDR` and immediately begins pre-reading new the new VRAM address.  
When `XM_RW_DATA` is written, begins writing value to VRAM at `XM_RW_ADDR` and adds `XM_RW_INCR` to `XM_RW_ADDR` and begins
reading new VRAM value.  
The `RW_RD_INC` flag in `XM_SYS_CTRL` controls if `XM_RW_INCR` is added to `XM_RW_ADDR` when `XM_RW_DATA` is *read* (it is always added on a *write*).

#### 0xF **`XM_RW_DATA_2`** (R+/W+) - VRAM memory value to read/write at VRAM address`XM_RW_ADDR`

<img src="./pics/wd_XM_RW_DATA.svg">

**Read or write VRAM value in VRAM at`XM_RW_ADDR` and add `XM_RW_INCR` to `XM_RW_ADDR`.**  
When`XM_RW_DATA_2`is read, returns data from VRAM at `XM_RW_ADDR`.  If `RW_RD_INC` flag in `XM_SYS_CTRL`is set it also adds `XM_RW_INCR` to `XM_RW_ADDR` and immediately begins pre-reading new the new VRAM address.
When `XM_RW_DATA_2` is written, begins writing value to VRAM at `XM_RW_ADDR` and adds `XM_RW_INCR` to `XM_RW_ADDR` and begins
reading new VRAM value.  
The `RW_RD_INC` flag in `XM_SYS_CTRL` controls if `XM_RW_INCR` is added to `XM_RW_ADDR` when `XM_RW_DATA_2` is *read* (it is always added on a *write*).  
**NOTE:** This register is identical to `XM_RW_DATA` to allow for 32-bit "long" MOVEP.L transfers to/from`XM_RW_DATA` for additional transfer speed.
___

## Xosera Extended Register / Extended Memory Region Summary

| XR Region Name   | XR Region Range | R/W | Description                                |
| ---------------- | --------------- | --- | ------------------------------------------ |
| XR video config  | 0x0000-0x000F   | R/W | config XR registers                        |
| XR playfield A   | 0x0010-0x0017   | R/W | playfield A XR registers                   |
| XR playfield B   | 0x0018-0x001F   | R/W | playfield B XR registers                   |
| XR blit engine   | 0x0020-0x002F   | R/W | 2D-blit XR registers                       |
| `XR_COLOR_ADDR`  | 0x8000-0x81FF   | R/W | 2 x 256W 16-bit color lookup memory (XRGB) |
| `XR_TILE_ADDR`   | 0xA000-0xB3FF   | R/W | 5KW 16-bit tile glyph storage memory       |
| `XR_COPPER_ADDR` | 0xC000-0xC7FF   | R/W | 2KW 16-bit copper program memory           |
| (unused region)  | 0xE000-0xFFFF   | -/- | (unused region)                            |

To access an XR register or XR memory address, write the XR register number or address to `XM_XR_ADDR`, then read or write to
`XM_XR_DATA`. Each word *written* to `XM_XR_DATA` will also automatically increment `XM_XR_DATA` to allows faster
consecutive updates (like for color or tile memory update). Note that this is not the case when reading from
`XM_XR_DATA`, you *must* write to `XM_XR_ADDR` in order to trigger a read (or the previously read value will be
returned again). [#TODO: revisit this restriction now with `mem_wait` bit]
While all XR registers and memory regions can be read, when there is high memory contention (e.g., it is being used for
video generation or other use), there is a `mem_wait` bit in `XM_SYS_CTRL` that will indicate when the last memory
operation is still pending.  
Also note that unlike the main 16 `XM` registers, the XR region can only be accessed as full 16-bit words (either
reading or writing). The full 16-bits of the `XM_XR_DATA` value are pre-read when `XM_XR_ADDR` is written and a full 16-bit
word is written when the odd (low-byte) of `XM_XR_DATA` is written.
___

### Xosera Extended Registers Details (XR Registers)

This XR registers are used to control of most Xosera operation other than CPU VRAM access and a few miscellaneous
control functions (which accessed directly via the main registers).  
To access these XR registers, first write the register address to `XM_XR_ADDR`, then read or write register data to `XM_XR_DATA`
(and when *writing only*, the low 13-bits of `XM_XR_DATA` will be auto-incremented for each word written).

### Video Config and Copper XR Registers Summary

| Reg # | Reg Name       | R /W | Description                                                                             |
| ----- | -------------- | ---- | --------------------------------------------------------------------------------------- |
| 0x00  | `XR_VID_CTRL`  | R /W | display control and border color index                                                  |
| 0x01  | `XR_COPP_CTRL` | R /W | display synchronized coprocessor control                                                |
| 0x02  | `XR_UNUSED_02` | R /W | [#TODO]                                                                                 |
| 0x03  | `XR_UNUSED_03` | R /W | [#TODO]                                                                                 |
| 0x04  | `XR_UNUSED_04` | R /W | [#TODO]                                                                                 |
| 0x05  | `XR_UNUSED_05` | R /W | [#TODO]                                                                                 |
| 0x06  | `XR_VID_LEFT`  | R /W | left edge start of active display window (normally 0)                                   |
| 0x07  | `XR_VID_RIGHT` | R /W | right edge + 1 end of active display window (normally 640 or 848)                       |
| 0x08  | `XR_SCANLINE`  | RO   | [15] in V blank, [14] in H blank [10:0] V scanline                                      |
| 0x09  | `XR_UNUSED_09` | RO   | [#TODO]                                                                                 |
| 0x0A  | `XR_VERSION`   | RO   | optional feature bits [15:12] and 3 digit BCD version [11:0]                            |
| 0x0B  | `XR_GITHASH_H` | RO   | [15:0] high 16-bits of 32-bit Git hash build identifier                                 |
| 0x0C  | `XR_GITHASH_L` | RO   | [15:0] low 16-bits of 32-bit Git hash build identifier                                  |
| 0x0D  | `XR_VID_HSIZE` | RO   | native pixel width of monitor mode (e.g. 640/848)                                       |
| 0x0E  | `XR_VID_VSIZE` | RO   | native pixel height of monitor mode (e.g. 480)                                          |
| 0x0F  | `XR_VID_VFREQ` | RO   | update frequency of monitor mode in BCD 1/100<sup>th</sup> Hz (e.g., 0x5997 = 59.97 Hz) |

(`R+` or `W+` indicates that reading or writing this register has additional "side effects", respectively)

___

### Video Config and Copper XR Registers Details

**0x00 `XR_VID_CTRL` (R/W) - interrupt status/signal and border color**  
<img src="./pics/wd_XR_VID_CTRL.svg">
Pixels outside video window (`VID_TOP`, `VID_BOTTOM`, `VID_LEFT`, `VID_RIGHT`) will use border color index. Sprite cursor will
also use upper 4-bits of border color (with lower 4-bits from sprite data).  
Writing 1 to interrupt bit will generate CPU interrupt (if not already pending). Read will give pending interrupts (which CPU can
clear writing to `XM_TIMER`).

**0x01 `XR_COPP_CTRL` (R/W) - copper start address and enable**  
<img src="./pics/wd_XR_COPP_CTRL.svg">
Display synchronized co-processor enable and starting PC address for each video frame within copper XR memory region.

**0x02 `XR_UNUSED_02` (--) - unused XR register 0x02**  
Unused XR register 0x02

**0x03 `XR_UNUSED_03` (--) - unused XR register 0x03**  
Unused XR register 0x03

**0x04 `XR_UNUSED_04` (--) - unused XR register 0x04**  
Unused XR register 0x04

**0x05 `XR_UNUSED_05` (--) - unused XR register 0x05**  
Unused XR register 0x05

**0x06 `XR_VID_LEFT` (R/W) - video display window left edge**  
<img src="./pics/wd_XR_VID_LEFT.svg">
Defines left-most native pixel of video display window (normally 0 for full-screen).

**0x07 `XR_VID_RIGHT` (R/W) - video display window right edge**  
<img src="./pics/wd_XR_VID_RIGHT.svg">
Defines right-most native pixel of video display window (normally 639 or 847 for 4:3 or 16:9 full-screen, respectively).

**0x08 `XR_SCANLINE` (RO) - current video display scan line and blanking status**  
<img src="./pics/wd_XR_SCANLINE.svg">
Continuously updated with the scanline and blanking status during display scanning. Read-only.

**0x09 `XR_UNUSED_09` (--) - unused XR register 0x09**  
Unused XR register 0x09

**0x0A `XR_VERSION` (RO) - Xosera version and optional feature bits**  
<img src="./pics/wd_XR_VERSION.svg">
BCD coded version (x.xx) and optional feature bits (0 for undefined/not present). Read-only.  
Bit 15 will be set if the bitstream design was "clean" Git hash (locally unmodified).

**0x0B `XR_GITHASH_H` (RO) - Xosera Git hash identifier (high 16-bits)**  
<img src="./pics/wd_XR_GITHASH_H.svg">
High 16-bits of Git short hash identifier. Can be used to help identify exact repository version.  
Upper nibble will be 0xD when local modifications have been made. Read-only.

**0x0C `XR_GITHASH_L` (RO) - Xosera Git hash identifier (low 16-bits)**  
<img src="./pics/wd_XR_GITHASH_L.svg">
Low 16-bits of Git short hash identifier. Can be used to help identify exact repository version. Read-only.

**0x0D `XR_VID_HSIZE` (RO) - monitor display mode native horizontal resolution**  
<img src="./pics/wd_XR_VID_HSIZE.svg">
Monitor display mode native horizontal resolution (e.g., 640 for 4:3 or 848 for 16:9). Read-only.

**0x0E `XR_VID_VSIZE` (RO) - monitor display mode native vertical resolution**  
<img src="./pics/wd_XR_VID_VSIZE.svg">
Monitor display mode native vertical resolution (e.g., 480). Read-only.

**0x0F `XR_VID_VFREQ` (RO) - monitor display mode update frequency in BCD 1/100<sup>th</sup> Hz**  
<img src="./pics/wd_XR_VID_VFREQ.svg">
Monitor display mode update frequency in BCD 1/100<sup>th</sup> Hz (e.g., 0x5997 = 59.97 Hz). Read-only.
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
blank is used to blank the display (solid colorbase color).  
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

The Xosera blitter is a VRAM data read/write "engine" that operates on 16-bit words in VRAM to copy, fill and do logic operations on arbitrary two-dimensional rectangular areas of Xosera VRAM (i.e., to draw, copy and manipulate "chunky" 4/8 bit pixel bitmap images). It supports a VRAM destination and up to three data sources. Two of which can be read from VRAM and one set to a constant (and it can operate either incrementing or decrementing VRAM addresses with and right and left shiting pixels). It repeats a basic word length operation for each "line" of a rectanglar area, and at the end of each line adds "modulo" values to source and destination addresses (to position source and destination for next line). It also has a "nibble shifter" that can shift a line of word data 0 to 3 nibbles to allow for pixel level positioning and drawing. There are also first and last word edge transparency masks, to remove unwanted pixels from the words at the start end end line allowing for arbitrary pixel sized rectangular operations. There is a fixed logic equation, but with several ways to vary input terms. This also works in conjunction with "transparency" testing that can mask-out specified 4-bit or 8-bit pixel values when writing. This transparency masking allows specified nibbles in a word to remain unaltered when the word is overwritten (without read-modify-write VRAM access, so no speed penalty). The combination of thes features allow for many useful graphical "pixel" operations to be performed in a single pass (copy, masking, shifting, logical operations AND, OR, XOR etc.).

___

#### Logic Operation Applied to Blitter Operations

&nbsp;&nbsp;&nbsp;`D = A & B ^ C`    (or destination word `D` = word `A` AND'd with word `B` and XOR'd with word `C`)

- `D` result word
  - written to VRAM (starting address set by `XR_BLIT_DST_D` and incrementing/decrementing)
- `A` primary source word, can be one of:
  - word read from VRAM (starting VRAM address set by `XR_BLIT_SRC_A` and incrementing/decrementing)
  - word constant (set by `XR_BLIT_SRC_A` when `A_CONST` set in `XR_BLIT_CTRL`)
- `B` secondary AND source word and word used for transparecy test, can be one of:
  - word read from VRAM (starting at VRAM address set by `XR_BLIT_SRC_B` and incrementing/decrementing)
  - word constant (set by `XR_BLIT_SRC_B` when `B_CONST` set in `XR_BLIT_CTRL`)
  - if `NOT_B` flag set in `XR_BLIT_CTRL`, then B will inverted for effective operation: `D = A & ~B ^ C`
- `C` constant XOR source word:
  - word constant set by `XR_BLIT_VAL_C`
  - if `C_USE_B` set in `XR_BLIT_CTRL` and `B_CONST` is not set, `C` will assume value of source term `B` (unaffected by `NOT_B`).
    - `C_USE_B` makes effective operation: `D = ~A & B` and using both `C_USE_B` and `NOT_B` gives: `D = A | B` (logical OR)

When set as constants, address values (`XR_BLIT_SRC_A` , `XR_BLIT_SRC_B` , `XR_BLIT_DST_D`) with be incremented when used unless the `DECR` flag set in `XR_BLIT_CTRL` (in which case they will instead be decremented). This allows blit operations where source and destination VRAM addresses overlap to be handled appropriately (otherwise source may be overwritten mid-operation).

#### Transparency Testing nd Masking Applied to Blitter Operations

&nbsp;&nbsp;&nbsp;4-bit mask `M = (B != {T})` where `T` is even/odd transparency nibbles or an 8-bit transparent value (for 8-bit pixel mode)

- `M` transparency result is 0 for transparent nibbles (ones that will not be altered in the destination word).
- `B` secondary source word used in logical operation above
  - Either read from VRAM or a constant (`NOT_B` flag does not affect transparency test)
- `T` transparency constant nibble pair/byte set in upper byte of `XR_BLIT_CTRL`

The 4-bit mask will be set in two-bit pairs for 8-bit pixels when `TRANSP_8B` set in `XR_BLIT_CTRL` (both bits zero only
when both nibbles of the transparency test are zero, otherwise both one). This allows any 4-bit or 8-bit pixel value in
`B` to be considered transparent (by beng XOR'd with a constant value `T` that makes it produce zero).  
Transparency testing cannot be disabled, but if it is not desired (all pixels values opaque), one method is to set `B`
to a constant (`B_CONST` in `XR_BLIT_CTRL`) that when XOR'd with `T` byte value (to both bytes of word) from
`XR_BLIT_CTRL` produces a value with no 4-bit or 8-bit pixels that have a zero value.

#### Nibble Shifting and First/Last Word Edge Masking

The `BLIT_SHIFT` register controls the nibble shifter that can shift 0 to 3 pixels to the right (or to the left in
`DECR` mode). Pixels shifted out of a word, will be shifted into the next word. There are also first and last word 4-bit
masks to mask unwanted pixels on the edges of a rectangle (when not word aligned, and pixels shifted off the end of the
previous line). When drawing in normal increment mode, the first word mask will trim pixels on the left edge and the
last word mask the right edge (or both at once for single word width).

#### Decrement Mode

Normally during blitter operations pointers will be incremented when used to read or write VRAM and when shifted, pixels
move from left to the right (from MSB to LSB). If the `DECR` bit is set in `XR_BLIT_CTRL`, then `XR_BLIT_SRC_B`,
`XR_BLIT_SRC_A` and `XR_BLIT_DST_D` pointers will decrement on each use and shifting will be to the right (LSB to MSB).
So this means when setting up a blitter operation they need to be pointing to the last word of the image (inclusive,
since not pre-decremented). Also `DECR` will also reverse the edge used by the first and last word masks (to be the
right and left edges respectively, sicne each line will be drawn right to left). However, even in `DECR` mode, the
`XR_BLIT_MOD_A`, `XR_BLIT_MOD_B` and `XR_BLIT_MOD_D` values will still be added each line (so typically they would be
negated).

`DECR` mode can be useful for overlapping blitter operations and "scrolling" VRAM, when the destination would overwrite the source mid-operation. For example, when copying an overlapping source and destination image rectangle a few pixels directly to the right (where the first pixels of each line copied would overwrite uncopied source pixels on each line).  
If lines of an image operation don't overlap, then the `XR_BLIT_MOD_A`, `XR_BLIT_MOD_B` and `XR_BLIT_MOD_D` can be negated to copy lines in reverse order, but keeping pixels right to left).

**NOTE:** Full `DECR` mode, with a left-shift has turned out to be rather "expensive" (in area and speed) in the FPGA implementation. If this continues to be an issue, then the ability to "left shift" in `DECR` mode may be removed. This is only strictly needed in cases similar to the example outlined above, with overlap on each line (and there are workarounds).  The ability to decrement `XR_BLIT_SRC_A`, `XR_BLIT_SRC_B` and `XR_BLIT_DST_D` pointers each word is planned to be retained.

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

| Reg # | Name            | R/W  | Description                                                           |
| ----- | --------------- | ---- | --------------------------------------------------------------------- |
| 0x20  | `XR_BLIT_CTRL`  | -/W  | blitter control (transparency, decrement mode, logic operation flags) |
| 0x21  | `XR_BLIT_MOD_A` | -/W  | end of line modulo value added to `A` address (or XOR'd if `A_CONST`) |
| 0x22  | `XR_BLIT_SRC_A` | -/W  | `A` source read VRAM address (or constant value if `A_CONST`)         |
| 0x23  | `XR_BLIT_MOD_B` | -/W  | end of line modulo value added to `B` address (or XOR'd if `B_CONST`) |
| 0x24  | `XR_BLIT_SRC_B` | -/W  | `B` source read VRAM address (or constant value if `B_CONST`)         |
| 0x25  | `XR_BLIT_MOD_C` | -/W  | end of line XOR value for `C` constant                                |
| 0x26  | `XR_BLIT_VAL_C` | -/W  | `C` constant value                                                    |
| 0x27  | `XR_BLIT_MOD_D` | -/W  | end of line modulo added to `D` destination address                   |
| 0x28  | `XR_BLIT_DST_D` | -/W  | `D` destination write VRAM address                                    |
| 0x29  | `XR_BLIT_SHIFT` | -/W  | first and last word nibble masks and nibble shift amount (0-3)        |
| 0x2A  | `XR_BLIT_LINES` | -/W  | number of lines minus 1, (repeats blit word count after modulo calc)  |
| 0x2B  | `XR_BLIT_WORDS` | -/W+ | word count minus 1 per line (write starts blit operation)             |
| 0x2C  | `XR_BLIT_2C`    | -/-  | RESERVED                                                              |
| 0x2D  | `XR_BLIT_2D`    | -/-  | RESERVED                                                              |
| 0x2E  | `XR_BLIT_2E`    | -/-  | RESERVED                                                              |
| 0x2F  | `XR_BLIT_2F`    | -/-  | RESERVED                                                              |

**NOTE:** None of the blitter registers are readable (reads will return unpredictable values). However, registers will
not be altered between blitter operations so only registers that need new values need be written before writing
`XR_BLIT_WORDS` to start a blit operation. This necessitates careful consideration/coordination when using the blitter
from both interrupts and mainline code.

#### 2D Blitter Engine XR Registers Details

**0x20 `XR_BLIT_CTRL` (WO) - control bits (logic ops, A addr/const, B addr/const, transparent/opaque)**  
<img src="./pics/wd_XR_BLIT_CTRL.svg">

**blitter operation control**  
The basic logic operation applied to all operations: `D = A & B ^ C`
The operation has four options that can be independently specified in `XR_BLIT_CTRL`:

- `A_CONST` specifies `A` term is a constant (`XR_BLIT_SRC_A` used as constant instead of VRAM address to read)
  - When `A` is a constant the value of `XR_BLIT_MOD_A` will be XORed with `XR_BLIT_SRC_A` instead of added and then end
    of each line.
- `B_CONST` specifies `B` term is a constant (`XR_BLIT_SRC_B` used as constant instead of VRAM address to read)
- `NOT_B` specifies that `B` term takes on an inverted value in the 2nd term (this does not affect transparency using `B`)
  - When `B` is a constant the value of `XR_BLIT_MOD_B` will be XORed with `XR_BLIT_SRC_B` instead of added and then end
    of each line.
- `C_USE_B` specifies that `C` term takes on same value as `B` term (unaffected by `NOT_B` flag)
- `DECR` specifies the blit operation will be carried out with all address decrementing. Source and destination
  addresses would be set to start at the last word (highest address) of image buffers and will decrement during each
  word of the operation (instead of incrementing). This also reverses the left/right edge for first/last word mask and
  will shift pixels to the left. Note that `DECR` does *not* subtract `XR_BLIT_MOD_A`, `XR_BLIT_MOD_B` and
  `XR_BLIT_MOD_D` values when set (so they typically need to be negated).

Additionally, a transparency test is applied to all operations: `4-bit mask M = (B != T)` (testing either per nibble or byte)

- `TRANSP8` can be set so 8-bit pixels are tested for transparency and masked only when the both nibbles in a byte match the transparency value
- `T` value is set in with the upper 8 bits of `XR_BLIT_CTRL` register and is the transparent value for even and odd 4-bit pixels or a single 8-bit pixel value (when `TRANSP8` set).

**0x21 `XR_BLIT_MOD_A` (WO) - modulo added to `BLIT_SRC_A` address at end of line (or XOR'd if `A_CONST` set)**  
<img src="./pics/wd_XR_BLIT_MOD_A.svg">

**modulo added to `BLIT_SRC_A` address at end of a line (or XOR'd with A constant value if `A_CONST` set)**  
Arbitrary twos complement value added to `A` address at the end of each line (or XOR'd with A constant value when `A_CONST` set). Typically zero when `BLIT_SRC_A` image data is contiguous, or -1 when shift amount is non-zero  (or 1 if `DECR` set when shifting).

**0x22 `XR_BLIT_SRC_A` (WO) - source `A` term (read from VRAM address or constant value)**  
<img src="./pics/wd_XR_BLIT_SRC_A.svg">

**source term `A` VRAM address (with term read from VRAM) or a constant value if `A_CONST` set**  
Address of source VRAM image data or arbitrary constant if `A_CONST` set in `XR_BLIT_CTRL`. This value will be shifted by `XR_BLIT_SHIFT` nibble shift amount (when not a constant with `A_CONST` set)

**0x23 `XR_BLIT_MOD_B` (WO) - modulo added to `BLIT_SRC_B` address at end of line (or XOR'd if `B_CONST` set)**  
<img src="./pics/wd_XR_BLIT_MOD_B.svg">

**modulo added to `BLIT_SRC_B` address at end of a line (or XOR'd with B constant value if `B_CONST` set)**  
Arbitrary twos complement value added to `B` address at the end of each line (or XOR'd with B constant value when `B_CONST` set). Typically zero when `BLIT_SRC_B` image data is contiguous, or -1 when shift amount is non-zero (or 1 if `DECR` set when shifting).

**0x24 `XR_BLIT_SRC_B` (WO) - source term `B` (read from VRAM address or constant value)**  
<img src="./pics/wd_XR_BLIT_SRC_B.svg">

**source term `B` VRAM address (with term read from VRAM) or a constant value if `B_CONST` set**  
Address of source VRAM image data or arbitrary constant if `B_CONST` set in `XR_BLIT_CTRL`. This value will be shifted by `XR_BLIT_SHIFT` nibble shift amount (when not a constant with `B_CONST` set)

**0x25 `XR_BLIT_MOD_C` (WO) - XOR'd with source constant term `C` at end of line**  
<img src="./pics/wd_XR_BLIT_MOD_C.svg">

**XOR'd with source constant term `C` at end of a line**  
Arbitrary 16-bit word value XOR'd with source constant term `C` at the end of each line. This can be useful to produce a "dither" or other pattern that alternates each line.

**0x26 `XR_BLIT_VAL_C` (WO) - source term C (constant XOR value)**  
<img src="./pics/wd_XR_BLIT_VAL_C.svg">

**source constant term `C` value**  
Arbitrary value for source constant term `C`. If `C_USE_B` set in `XR_BLIT_CTRL` then term `C` will assume the value of `B` (ignoring `NOT_B` flag) and this value will not be used.

**0x27 `XR_BLIT_MOD_D` (WO) - modulo added to `BLIT_DST_D` address at end of line**  
<img src="./pics/wd_XR_BLIT_MOD_D.svg">

**modulo added to `BLIT_DST_D` destination address at end of a line**  
Arbitrary twos complement value added to `D` destination address at the end of each line. Typically the *destination_width*-*source_width* (in words) to adjust the destination pointer to the start of the next rectangular image line.

**0x28 `XR_BLIT_DST_D` (WO) - destination D VRAM write address**  
<img src="./pics/wd_XR_BLIT_DST_D.svg">

**destination D VRAM write address**  
Destination VRAM address.  Set to the first word of the destination address for the blit operation (or the last word of the destination, if in `DECR` mode).

**0x29 `XR_BLIT_SHIFT` (WO - first and last word nibble masks and nibble shift**  
<img src="./pics/wd_XR_BLIT_SHIFT.svg">

**first and last word nibble masks and nibble shift**  
The first word nibble mask will unconditionally mask out (make transparent) any zero nibbles on the first word of each
line (left edge incrementing, right decrementing). Similarly, the last word nibble mask will unconditionally mask out
(make transparent) any zero nibbles on the last word of each line (the right edge incrementing, left decrementing). The
nibble shift specifies a 0 to 3 nibble shift on all words drawn (a right shift normally, but a left shift when in `DECR`
mode). Nibbles shifted out the right of a word will be shifted into the the next word (to provide a seamless pixel shift
with word aligned writes). In "normal" use (and not using `DECR` decrement mode), when the nibble shift is non-zero, the
left and right nibble mask would typically contain the value `0xF0` nibble shifted (e.g., `0xF0 >> nibble_shift`). The
right edge mask hides "garbage" pixlels wrapping from left edge, and right edge mask hides excess pixels when images is
not a exact word width. When shifting, typically also you need to add an extra word to `BLIT_WORDS` and subtract one
from `BLIT_MOD_A` (to avoid skewing the image). When not shifting (nibble shift of zero) and your source image width is
word aligned, you typically want the complete last word so both left and right mask `0xFF` (no edge masking). If your
source image is not an exact multiple word width, you would typically want to trim the excess pixels from the right edge
(e.g., for a 7 nibble wide image, `0xFE`). For images 1 word wide, both left and right edge masks will be AND'd
together. Also this masking is AND'd with the normal transparency control (so if a pixel is masked by either the left
mask, right mask or is considered transparent it will not be drawn).

**0x2A `XR_BLIT_LINES` (WO) - 15-bit number of lines heigh - 1 (1 to 32768)**  
<img src="./pics/wd_XR_BLIT_LINES.svg">

**15-bit number of lines high - 1 (1 to 32768)**  
Number of times to repeat blit operation minus one. Typically source image height with modulo values advancing addresses
for the next line to be drawn).

**0x2B `XR_BLIT_WORDS` (WO) - write starts operation, word width - 1 (1 to 65536, repeats `XR_BLIT_LINES` times)**  
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
