# Makefile - Xosera master Makefile
# vim: set noet ts=8 sw=8
#
ICEPROG := iceprog

# if XOSERA_M68K_API not set, assume it is from this tree
ifndef XOSERA_M68K_API
XOSERA_M68K_API:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/xosera_m68k_api
endif

# Build all project targets
# NOTE: Xosera DVI PMOD not officially supported yet (image stability issues)
info:
	@echo "NOTE: Requires YosysHQ tools."
	@echo "      (e.g. https://github.com/YosysHQ/oss-cad-suite-build/releases/latest)"
	@echo "      (Sadly, brew and distribution versions seem sadly outdated)"
	@echo "      Simulation requires Icarus Verilog and/or Verilator"
	@echo "      Utilities and Verilator simulation require C++ compiler"
	@echo "      SDL2 and SDL2_Image required for Verilator visual simulation and utils"
	@echo " "
	@echo "Xosera make targets:"
	@echo "   make all             - build everything (RTL, simulation, uitls and host_spi)"
	@echo "   make xosera_vga      - build Xosera VGA Rosco_m68k board firmware"
	@echo "   make xosera_vga_640  - build Xosera VGA 640x480 only Rosco_m68k board firmware"
#	@echo "   make xosera_dvi      - build Xosera DVI Rosco_m68k board firmware"
	@echo "   make xosera_vga_prog - build & program Xosera VGA Rosco_m68k board firmware"
	@echo "   make xosera_vga_640_prog - build Xosera VGA 640x480 only Rosco_m68k board firmware"
#	@echo "   make xosera_dvi_prog - build & program Xosera DVI Rosco_m68k board firmware"
	@echo "   make upduino         - build Xosera for UPduino v3 (see rtl/upduino.mk for options)"
	@echo "   make upd_prog        - build Xosera and program UPduino v3"
	@echo "   make iceb_vga        - build Xosera for iCEBreaker for VGA"
	@echo "   make iceb_dvi         - build Xosera for iCEBreaker for DVI"
	@echo "   make rtl             - build UPduino, iCEBreaker bitstreams and simulation targets"
	@echo "   make sim             - build Icarus Verilog and Verilalator simulation files"
	@echo "   make isim            - build Icarus Verilog simulation files"
	@echo "   make irun            - build and run Icarus Verilog simulation"
	@echo "   make vsim            - build Verilator C++ & SDL2 native visual simulation files"
	@echo "   make vrun            - build and run Verilator C++ & SDL2 native visual simulation"
	@echo "   make count           - build Xosera VGA with Yosys count for module resource usage"
	@echo "   make utils           - build misc C++ image utilities"
	@echo "   make m68k            - build rosco_m68k Xosera test programs"
	@echo "   make clean           - clean most files that can be rebuilt"

# Build all project targets
all: rtl utils host_spi xvid_spi

# Build UPduino combined bitstream for VGA Xosera rosco_m68k board
xosera_vga:
	cd rtl && $(MAKE) xosera_vga
	@echo Type \"make xosera_vga_prog\" to program the UPduino via USB

# Build UPduino combined bitstream for VGA Xosera rosco_m68k board (640x480 only)
xosera_vga_640:
	cd rtl && $(MAKE) xosera_vga_640
	@echo Type \"make xosera_vga_640_prog\" to program the UPduino via USB

# Build UPduino combined bitstream for DVI Xosera rosco_m68k board
xosera_dvi:
	cd rtl && $(MAKE) xosera_dvi
	@echo Type \"make xosera_dvi_prog\" to program the UPduino via USB

# Build UPduino combined bitstream for DVI Xosera rosco_m68k board
xosera_dvi_640:
	cd rtl && $(MAKE) xosera_dvi_640
	@echo Type \"make xosera_dvi_640_prog\" to program the UPduino via USB

# Build and program combo Xosera UPduino 3.x VGA FPGA bitstream for rosco_m6k board
xosera_vga_prog: xosera_vga
	@echo === Programming Xosera board UPduino VGA firmware ===
	$(ICEPROG) -d i:0x0403:0x6014 rtl/xosera_board_vga.bin

# Build and program combo Xosera UPduino 3.x VGA FPGA bitstream for rosco_m6k board (640x480 only)
xosera_vga_640_prog: xosera_vga_640
	@echo === Programming Xosera board UPduino VGA firmware for 640x480 only ===
	$(ICEPROG) -d i:0x0403:0x6014 rtl/xosera_board_vga_640.bin

# Build and program combo Xosera UPduino 3.x DVI FPGA bitstream for rosco_m6k board
xosera_dvi_prog: xosera_dvi
	@echo === Programming Xosera board UPduino DVI firmware ===
	$(ICEPROG) -d i:0x0403:0x6014 rtl/xosera_board_dvi.bin

# Build and program combo Xosera UPduino 3.x DVI FPGA bitstream for rosco_m6k board
xosera_dvi_640_prog: xosera_dvi_640
	@echo === Programming Xosera board UPduino DVI firmware for 640x480 only ===
	$(ICEPROG) -d i:0x0403:0x6014 rtl/xosera_board_dvi_640.bin

# Build UPduino bitstream
upduino:
	cd rtl && $(MAKE) upd

# Build UPduino bitstream
upd: upduino

# Build UPduino bitstream and progam via USB
upd_prog:
	cd rtl && $(MAKE) upd_prog

# Build iCEBreaker bitstream
iceb_dvi:
	cd rtl && $(MAKE) iceb_dvi

# Build iCEBreaker bitstream
iceb_vga:
	cd rtl && $(MAKE) iceb_vga

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

# build Xosera VGA with Yosys count (for module resource usage)
count:
	cd rtl && $(MAKE) -f upduino.mk count

# Build image/font mem utility
utils:
	cd utils && $(MAKE)

# Build m68k tests and demos
m68k:
	cd copper/CopAsm && $(MAKE)
	cd xosera_m68k_api && $(MAKE)
	cd xosera_ansiterm_m68k && $(MAKE)
	cd xosera_audiostream_m68k && $(MAKE)
	cd xosera_boing_m68k && $(MAKE)
	cd xosera_font_m68k && $(MAKE)
	cd xosera_mon_m68k && $(MAKE)
	cd xosera_pointer_m68k && $(MAKE)
	cd xosera_test_m68k && $(MAKE)
	cd xosera_uart_m68k && $(MAKE)
	cd xosera_vramtest_m68k && $(MAKE)
	cd xosera_modplay_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE)
	cd copper/copper_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE)
	cd copper/crop_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE)
	cd copper/splitscreen_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE)

# Build host SPI test utility
host_spi:
	cd host_spi && $(MAKE)

# Build xvid over SPI test utility
xvid_spi:
	cd xvid_spi && $(MAKE)

golden:
	@echo === Last 640x480 bitstream stats:
	@cat rtl/xosera_upd_*vga_640x480_stats.txt
	@echo === Last 848x480 bitstream stats:
	@cat rtl/xosera_upd_*vga_848x480_stats.txt
	@echo Enshrine these bitstreams [y/N]?
	@read ans && if [ $${ans:-'N'} = 'y' ] ; then \
		echo === Copying xosera bitstreams to xosera_gateware... ; \
		cp -v rtl/xosera_upd_*_vga_*_stats.txt ./xosera_gateware && \
		cp -v rtl/upduino/logs/xosera_upd_*_vga_*_yosys.log ./xosera_gateware && \
		cp -v rtl/upduino/logs/xosera_upd_*_vga_*_nextpnr.log ./xosera_gateware && \
		cp -v rtl/upduino/xosera_upd_*_vga_*.json ./xosera_gateware && \
		cp -v rtl/xosera_board_vga.bin ./xosera_gateware ; \
	fi
	@echo === Done

golden_dvi:
	@echo === Last 640x480 dvi bitstream stats:
	@cat rtl/xosera_upd_*dvi_640x480_stats.txt
	@echo === Last 848x480 dvi bitstream stats:
	@cat rtl/xosera_upd_*dvi_848x480_stats.txt
	@echo Enshrine these bitstreams [y/N]?
	@read ans && if [ $${ans:-'N'} = 'y' ] ; then \
		echo === Copying xosera dvi bitstreams to xosera_gateware... ; \
		cp -v rtl/xosera_upd_*_dvi_*_stats.txt ./xosera_gateware && \
		cp -v rtl/upduino/logs/xosera_upd_*_dvi_*_yosys.log ./xosera_gateware && \
		cp -v rtl/upduino/logs/xosera_upd_*_dvi_*_nextpnr.log ./xosera_gateware && \
		cp -v rtl/upduino/xosera_upd_*_dvi_*.json ./xosera_gateware && \
		cp -v rtl/xosera_board_dvi.bin ./xosera_gateware ; \
	fi
	@echo === Done

# Clean all project targets
clean: m68kclean
	cd copper/CopAsm/ && $(MAKE) clean
	cd rtl && $(MAKE) clean
	cd utils && $(MAKE) clean
	cd host_spi && $(MAKE) clean
	cd xvid_spi && $(MAKE) clean

# Clean m68k tests and demos
m68kclean:
	cd xosera_m68k_api && $(MAKE) clean
	cd xosera_ansiterm_m68k/ && $(MAKE) clean
	cd xosera_audiostream_m68k && $(MAKE) clean
	cd xosera_boing_m68k && $(MAKE) clean
	cd xosera_font_m68k && $(MAKE) clean
	cd xosera_mon_m68k && $(MAKE) clean
	cd xosera_test_m68k && $(MAKE) clean
	cd xosera_uart_m68k && $(MAKE) clean
	cd xosera_vramtest_m68k && $(MAKE) clean
	cd xosera_pointer_m68k $(MAKE) clean
	cd xosera_modplay_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE) clean
	cd copper/copper_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE) clean
	cd copper/crop_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE) clean
	cd copper/splitscreen_test_m68k && XOSERA_M68K_API=$(XOSERA_M68K_API) $(MAKE) clean

.PHONY: all upduino upd upd_prog icebreaker iceb iceb_prog rtl sim isim irun vsim vrun utils m68k host_spi xvid_spi clean m68kclean
