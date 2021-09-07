# Makefile - Xosera FPGA Makefile
# vim: set noet ts=8 sw=8
#

# Build all project targets
all: sim upd iceb

# Program any connected FPGA targets
prog: upd_prog ice_prog

# build simulation files (Icarus Verilog and Verilator)
sim:
	$(MAKE) -f sim.mk all

# build Icarus Verilog simulation files
isim:
	$(MAKE) -f sim.mk isim

# build and run Icarus Verilog simulation
irun:
	$(MAKE) -f sim.mk irun

# build Verilator native C++ simulation files
vsim:
	$(MAKE) -f sim.mk vsim

# build & run Verilator native C++ simulation files
vrun:
	$(MAKE) -f sim.mk vrun

# Build Xosera UPduino 3.x FPGA bitstream
upd:
	$(MAKE) -f upduino.mk
	@echo Type \"make upd_prog\" to program the UPduino via USB

# Build combo Xosera UPduino 3.x FPGA VGA bitstream for rosco_m6k board
xosera_vga:
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 $(MAKE) -f upduino.mk
	#VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_848x480 $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_vga.bin

# Build combo Xosera UPduino 3.x FPGA DVI bitstream for rosco_m6k board
xosera_dvi:
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_640x480 $(MAKE) -f upduino.mk
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_848x480 $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_dvi.bin

# Build Xosera iCEBreaker FPGA bitstream
iceb:
	$(MAKE) -f icebreaker.mk
	@echo Type \"make iceb_prog\" to program the iCEBreaker via USB

# Build and program Xosera UPduino 3.x FPGA bitstream
upd_prog:
	$(MAKE) -f upduino.mk prog

# Build and program Xosera iCEBreaker FPGA bitstream
iceb_prog:
	$(MAKE) -f icebreaker.mk prog

# Clean all project targets
clean:
	$(MAKE) -f sim.mk clean
	$(MAKE) -f upduino.mk clean
	$(MAKE) -f icebreaker.mk clean

.PHONY: all prog sim isim irun vsim vrun upd iceb xosera_board iceb_prog upd_prog xosera_prog clean
