# Informal list of notable Xosera changes

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
