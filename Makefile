# Makefile - Xosera master Makefile
# vim: set noet ts=8 sw=8
#

# Build all project targets
info:
	@echo "NOTE: Requires YosysHQ tools (https://github.com/YosysHQ/fpga-toolchain)"
	@echo "      (Sadly, brew and distribution versions seem sadly outdated)"
	@echo "      Simulation requires Icarus Verilog and/or Verilator"
	@echo "      Utilities and Verilator simulation require C++ compiler"
	@echo "      SDL2 and SDL2_Image required for Verilator visual simulation"
	@echo " "
	@echo "Xosera make targets:"
	@echo "   make all        - build everything (RTL, simulation, uitls and host_spi)"
	@echo "   make upduino    - build Xosera for UPduino v3 (see rtl/upduino.mk for options)"
	@echo "   make upd_prog   - build Xosera and program UPduino v3 (see rtl/upduino.mk for options)"
	@echo "   make icebreaker - build Xosera for iCEBreaker (see rtl/icebreaker.mk for options)"
	@echo "   make iceb_prog  - build Xosera and program iCEBreaker (see rtl/icebreaker.mk for options)"
	@echo "   make rtl        - build UPduino and iCEBreaker bitstream"
	@echo "   make sim        - build Icarus Verilog and Verilalator simulation files"
	@echo "   make isim       - build Icarus Verilog simulation files"
	@echo "   make irun       - build and run Icarus Verilog simulation"
	@echo "   make vsim       - build Verilator C++ & SDL2 native visual simulation files"
	@echo "   make vrun       - build and run Verilator C++ & SDL2 native visual simulation"
	@echo "   make utils      - build utilities (currently image_to_mem font converter)"
	@echo "   make host_spi   - build PC side of FTDI SPI test utility (needs libftdi1)"
	@echo "   make clean      - clean files that can be rebuilt"

all: rtl utils host_spi

upduino:
	cd rtl && $(MAKE) upd

upd: upduino

upd_prog:
	cd rtl && $(MAKE) upd_prog

icebreaker:
	cd rtl && $(MAKE) iceb

iceb: icebreaker

iceb_prog:
	cd rtl && $(MAKE) iceb_prog

# Build all project targets
sim:
	cd rtl && $(MAKE) sim

# Build all project targets
isim:
	cd rtl && $(MAKE) isim

# Build all project targets
irun:
	cd rtl && $(MAKE) irun

# Build all project targets
vsim:
	cd rtl && $(MAKE) vsim

# Build all project targets
vrun:
	cd rtl && $(MAKE) vrun

# Build image/font mem utility
utils:
	cd utils && $(MAKE)

# Build host SPI command utility
host_spi:
	cd host_spi && $(MAKE)

# Clean all project targets
clean:
	cd rtl && $(MAKE) clean
	cd utils && $(MAKE) clean
	cd host_spi && $(MAKE) clean

.PHONY: all rtl sim utils host_spi clean
