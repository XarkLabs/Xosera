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
	@echo "   make all          - build everything (RTL, simulation, uitls and host_spi)"
	@echo "   make upduino      - build Xosera for UPduino v3 (see rtl/upduino.mk for options)"
	@echo "   make upd_prog     - build Xosera and program UPduino v3 (see rtl/upduino.mk for options)"
	@echo "   make xosera_board - build Xosera Rosco_m68k board image"
	@echo "   make xosera_prog  - build and program Xosera Rosco_m68k board image"
	@echo "   make icebreaker   - build Xosera for iCEBreaker (see rtl/icebreaker.mk for options)"
	@echo "   make iceb_prog    - build Xosera and program iCEBreaker (see rtl/icebreaker.mk for options)"
	@echo "   make rtl          - build UPduino, iCEBreaker bitstreams and simulation targets"
	@echo "   make sim          - build Icarus Verilog and Verilalator simulation files"
	@echo "   make isim         - build Icarus Verilog simulation files"
	@echo "   make irun         - build and run Icarus Verilog simulation"
	@echo "   make vsim         - build Verilator C++ & SDL2 native visual simulation files"
	@echo "   make vrun         - build and run Verilator C++ & SDL2 native visual simulation"
	@echo "   make utils        - build utilities (currently image_to_mem font converter)"
	@echo "   make host_spi     - build PC side of FTDI SPI test utility (needs libftdi1)"
	@echo "   make xvid_spi     - build PC XVID API over FTDI SPI test utility (needs libftdi1)"
	@echo "   make clean        - clean files that can be rebuilt"

# Build all project targets
all: rtl utils host_spi xvid_spi

# Build UPduino combined bitstream for Xosera rosco_m68k board
xosera_board:
	cd rtl && $(MAKE) xosera_board

# Build UPduino combined bitstream for Xosera rosco_m68k board and program via USB
xosera_prog:
	cd rtl && $(MAKE) xosera_prog

# Build UPduino bitstream
upduino:
	cd rtl && $(MAKE) upd

# Build UPduino bitstream
upd: upduino

# Build UPduino bitstream and progam via USB
upd_prog:
	cd rtl && $(MAKE) upd_prog

# Build iCEBreaker bitstream
icebreaker:
	cd rtl && $(MAKE) iceb

# Build iCEBreaker bitstream
iceb: icebreaker

# Build iCEBreaker bitstream and program via USB
iceb_prog:
	cd rtl && $(MAKE) iceb_prog

# Build all RTL synthesis and simulation targets
rtl:
	cd rtl && $(MAKE) all

# Build all simulation targets
sim:
	cd rtl && $(MAKE) sim

# Build Icarus Verilog simulation targets
isim:
	cd rtl && $(MAKE) isim

# Build Icarus and run Verilog simulation
irun:
	cd rtl && $(MAKE) irun

# Build Verilator simulation targets
vsim:
	cd rtl && $(MAKE) vsim

# Build and run Verilator simulation targets
vrun:
	cd rtl && $(MAKE) vrun

# Build image/font mem utility
utils:
	cd utils && $(MAKE)

# Build host SPI test utility
host_spi:
	cd host_spi && $(MAKE)

# Build xvid over SPI test utility
xvid_spi:
	cd xvid_spi && $(MAKE)

# Clean all project targets
clean:
	cd rtl && $(MAKE) clean
	cd utils && $(MAKE) clean
	cd host_spi && $(MAKE) clean
	cd xvid_spi && $(MAKE) clean

.PHONY: all upduino upd upd_prog icebreaker iceb iceb_prog xosera_board xosera_prog rtl sim isim irun vsim vrun utils host_spi xvid_spi clean
