# Informal list of notable Xosera changes

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
* Removed dead-pin hack ifdef (due to fresh Upduino with a working pin 38 setup)
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
