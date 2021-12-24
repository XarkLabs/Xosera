# Informal list of notable Xosera changes

2021-12-22+ Xark

* Still badly neglecting this file. :sweat_smile:
* Xosera is nearing completion, main things still needed:
  * mouse cursor sprite (and probably some others)
  * audio playback with mixing (aiming for similar to Amiga, but stereo samples [so 8 channels])
* Stretch goals - some things that might be doable
  * UART support
    * to give another fast serial port to rosco_m68k
    * allow iCEBreaker and debug control via easy UART
  * Draw support - add as much Danoidus stuff as will fix :)
    * Line draw
    * Maybe add "poly render" focused config?
      * Rosco_pro changes equation (maybe with '882)
  * Fix memory arbitration to not need "ACK" cycle
    * This would pretty much double blitter speed
  * Treat 4 VRAM banks separately to allow parallel access
    * Avoid scanout/blitter contention
  * Unify all memory VRAM and XR memories in one address space
    * Might reduce redundancy and make most memories "orthagonal"
    * Better for larger versions in future (ECP5)
  * RL7 video mode for simple decompress from VRAM

2021-10-25+ Xark

* I have not been good about updating this. Sorry. :)  Here are some recent ones...
* No more 2-bpp/4-color mode, now just 1-bpp, 4-bpp, 8-bpp and "mode X" (also 8-bpp)
* 8x16 high characters are only supported in 1-bpp mode (others are always 8x8
  and upper bit of height ignored)
* 4-bpp and 8-bpp tiled modes support 1024 glyphs and H&V mirroring
* nibble write mask moved to low four bits of SYS_CTRL and can be updated with
  byte write (without affecting interrupts/sysconfig)

2021-05-01+ Xark

* Fonts can now be in VRAM or the 8KB of font RAM
* Fonts are now stored as two bytes in a 16-bit word (same for VRAM or font RAM)
* Revamped pixel fetch "loop"
* Made most AUX video registers read/write (makes things easier, software can query display addr, width etc.)
* Started work on bitmap mode, currently testing with 8x1 text mode (with color attribute byte and bitmap byte) - needs more flexible pixel fetch
* Fixed RD_ADDR and RD_INC to work properly
* Received rosco_m68k prototype PCB boards. Assembled one, works great.  Dangled 74HCT245 pin out to allow re-flash of Xosera w/o powering down rosco_m68k (vs hard reset with rosco)
* Did a fair bit of preliminary work on Xosera rosco_m68k, mostly experimenting with GCC to get it to use the MOVEP opcode for Xosera registers optimally.
* Found a pretty good solution (with a minimal amount if inline asm)
* Fleshed out API, wrote some test code.  Scrutinizing disassembly and it appears to be working nicely.
* Did small test and 8MHz rosco_m68k can write to Xosera VRAM at ~820 KB/sec vs main memory at ~1040 KB/sec (8-bit bus and MOVEP.L vs 16-bit and MOVE.L)

2021-04-22 Xark

* Cleaned up tester, did Q&D 4096 colors-at-once test (looks okay - needs a copper to be solid)
* Updated "even register byte" buffer logic.  Now zeros only on switching register for "shared" buffer.  This allows single byte AUX_ADDR updates for (e.g.) COLORTBL.
* Updated VRAM fetch/output logic to support a "clean" horizontal pixel double (in conjunction with horizontal scrolling)
* To do this "right" I needed to add another bit to horizontal scroll register (now 0-15 for double wide chars). Pixel doubled modes can still single pixel scroll now.
* Updated smooth scroll test to also test pixel doubled.  Buttery smooth and (AFAICT) pixel perfect.

2021-04-21 Xark

* Re-did VRAM fetch/output logic.  This fixed some "column 0" issues (and made things cleaner).
* Verified horizontal smooth scroll is spot on.  Adjusted memory fetch boundaries to be as tight as possible.
* Disabled pixel double because it is not "playing nice" yet

2021-04-20 Xark

* Added another bit to AUX_VID registers for GFX_CTRL register with horizontal and vertical pixel doubling (horizontal only wired up)
* Moved global definitions into a Verilog package
* Fixed up image_to_mem utility to also output C code
* Used utility to add 128x128 monochome "font pic" as a test.  Worked, but still want a real bitmap mode. :)

2021-04-18 Xark

* Added SPI controller to iCEBreaker that allows low-level "bus" access from PC
* Wrote xvid_spi test program that runs about the same register API as AVR, but on PC ove SPI (retro eGPU :) )
* Added some tests for SPI to xvid
* Fixed SPI flush issue
* Added "remote reset" to help sync SPI when starting (works well, but FTDI SPI gets funky after ^C) - trying UART

2021-04-15 Xark

* Fix AUX reads.  Now it is necessary to write register number to AUX_ADDR before each read (this primes the read for AUX_DATA)
* Fixed reboot issues.  Now selecting FPGA reconfig 0-3 seems to work as expected.  Updated Make to show config slots populated. E.g. `VIDEO_MODE=MODE_640x480 make upd` and then `make upd_prog` (which defaults to 848x480) will program both configs into flash.
* Other make fixes.

2021-04-12 Xark

* Recfactored registers (again).  Now more flexible with less "LUTs" using multi purpose "AUX" memory space.
* Added WIDTH HEIGHT and SCANLINE vgen registers
* Font height, 8x8 or 8x16 font, font memory writing (8KB, 2KB per bank, 1 bank for 8x8, 2 for 8x16)
* Vertical smooth scroll
* Palette updates via palette BRAM TODO: One pixel "delayed color" to fix on left edge of display (leaking from right edge)
* Improved even byte/MSB.  Now reg 0-3 you can read/write arbitrary bytes.  DATA/DATA_2 have dedicated MSB and the rest share one that is zeroed after LSB written.
* Added 8x8 hexfont
* Vsim uses PNG (few things touch BMP on Mac)

2021-04-11 Xark

* Wrote `image_to_mem` font convrsion utility.
* Converted 8x16 and 8x8 Atari ST fonts.  ST 8x16 is default in bank 0 & 1, ST 8x8 in bank 2, and V9958 in bank 3.
* Added FPGA reconfigure ability (self reboot, got tired of manually resetting it and Arduino).  A write of the "number of the boot" (0x808x) to XVID_VID_CTRL will reboot (low two bits firmware configurations, 0=normal).  This is great to reset fonts & mode and can easily be used to swtich video modes (e.g., 640x480, 848x480 or other options).

2021-04-10 #2 - Xark

* Added 4 font banks (for 8x8, 2 for 8x16)
* Added font fine scroll (TODO untested)
* Added V9958 8x8 font in bank 2 (3rd)

2021-04-10 - Xark

* Added git "short hash" to build.  The 7-digit short hash is preceded with "0" for a clean build, or "d" for a "dirty" build (ignoring stat file changes) forming a 32-bit hex number.  This is provided to the Verilog build as a define, so the intent is to display this briefly on screen as part of the boot message (and knowing exact firmware may be important to help diagnose issues).  I don't  think I want to provide the hash as a register, since it would encourage code to only run on specific versions (but I will add something like "a bit per feature", once things get going).
* NOTE: In testing I found 16MHz AVR is "too fast" for 640x480 Xosera (without some NOPs) due to it having a 25.125MHz bus sampling/pixel clock vs 33.75MHz at 848x480.  8MHz AVR seems fine (as does 8/10MHz 68k+GAL interface).
* Added some config settings  VID_CTRL selects, 0=text start addr, 1=line length, 3=palette[0], palette[1], and then write to VID_DATA to set.

2021-04-08 - Xark

* Refactored bus interface to live under blitter module (less cross module traffic, access to blit internals)
* Reorganized registers so registers 0-6 are fully read/write
* Seperated blitter state machine from register handling logic (now registers always active/responsive)
* Now DATA reg stores MSB written to it (so e.g., DATA MSB will not be cleared when other regs written)
* Regs 8-16 are "virtual" and will not be normal read/write (e.g., status, font RAM)
* VID_CTRL now hooked up so low two bits select VID_DATA info for 00=width, 01=height, 02=???, 03=??? (TBD)
* Some infrastructure/ideas to get async copy/fill operations working.
* Updated AVR tester, faster 16MHz or 8MHz (now ~66 msec for 128KB write).  Added res query and reg r/w test.
* Rebuilt DV PMOD breadboard prototype on quality breadboard (removed NOPs due to flakiness/capacitance? of previous one). :)

2021-04-05 - Xark

* Redid Arduino tester with direct port access for ~75x speedup (seriously - 164 vs 12,350 msec for 65535 word write)
* Added stats "diff" to show any FPGA tool, resource or FMAX change between current and previous build

2021-04-03 #4 - Xark

* Glitch filter seemed to do no harm, so removed ifdef
* Removed dead-pin hack ifdef (due to fresh UPduino with a working pin 38 setup)
* Reversed bit order on Arduino tester (easier to make fast)

2021-04-03 #3 - Xark

* Added "free" glitch filter to CS detect (which even made design faster and smaller)

2021-04-03 #2 - Xark

* Added speculative fix for bus sampling delay for bus settling problem observed on real 68k bus [t worked!].

2021-04-03 - Xark

* Added this "primitive" changelog
* Improved Makefiles so they don't "spam" so badly.  Now they mostly show warnings/errors and FPGA usage summary.
* Renamed signal `DV_EN` to `DV_DE` to match schematic (and 1BitSquared)
* Remove `R_` from `R_XVID` register names (`XVID_*` seems unique enough they are a bit long)
* Renamed and moved around some registers a bit.  See [REFERENCE.md](REFERENCE.md)
* Slightly improved blitter init (removed redundant lines and set WR_ADDR to 0 when done)
* Fixed `XVID_RD_INC` and `XVID_WR_INC` to be modifyable.  They still default to 0x0001 at init.
* Improved Icarus Verilog testbench to show VRAM access
* Changed `timescale` in simulations to be more accurate (so simulation clock frequency matches real design frequency)
* Change sim to 848x480 video mode (nominal target mode IMHO)
* Fixed 16K bank problem reading VRAM (address lines could change cycle after value read, selecting wrong bank)
* Improved VRAM Verilog design a bit (faster)
* Improved Arduino xosera_tester (now seems to never get a mismatchs reading/writing entire VRAM space)
