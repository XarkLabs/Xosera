# Makefile - Xosera FPGA Makefile
# vim: set noet ts=8 sw=8
#

VIDEO_MODE?=MODE_640x480
AUDIO?=4
PF_B?=true

XOSERA_M68K_API?=../xosera_m68k_api
COPASM=../copper/CopAsm/bin/copasm
DEF_COPPER=default_coppermem.mem

# Build all project targets
all: sim upd iceb

# Program any connected FPGA targets
prog: upd_prog ice_prog

# build copper assembler
$(COPASM):
	@echo === Building copper assembler...
	cd $(XOSERA_M68K_API)/../copper/CopAsm/ && $(MAKE)

%.mem : %.casm $(COPASM)
	$(COPASM) -l -o $@ $<

# build simulation files (Icarus Verilog and Verilator)
sim: $(DEF_COPPER)
	$(MAKE) -f sim.mk all

# build Icarus Verilog simulation files
isim: $(DEF_COPPER)
	$(MAKE) -f sim.mk isim

# build and run Icarus Verilog simulation
irun: $(DEF_COPPER)
	$(MAKE) -f sim.mk irun

# build Verilator native C++ simulation files
vsim: $(DEF_COPPER)
	$(MAKE) -f sim.mk vsim

# build & run Verilator native C++ simulation files
vrun: $(DEF_COPPER)
	$(MAKE) -f sim.mk vrun

# Build Xosera UPduino 3.x FPGA bitstream
upd: $(DEF_COPPER)
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true $(MAKE) -f upduino.mk
	@echo Type \"make upd_prog\" to program the UPduino via USB

# Build combo Xosera UPduino 3.x FPGA VGA bitstream for rosco_m6k board with all supported features
xosera_vga: $(DEF_COPPER)
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 PF_B=true AUDIO=4 FPGA_CONFIG=0 $(MAKE) -f upduino.mk
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_848x480 PF_B=true AUDIO=4 FPGA_CONFIG=1 $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_vga.bin

# Build combo Xosera UPduino 3.x FPGA VGA bitstream for rosco_m6k board (640x480 resolution only)
xosera_vga_640: $(DEF_COPPER)
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_vga_640.bin

xosera_muse_vga: $(DEF_COPPER)
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_MUSE_VGA VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true FPGA_CONFIG=0 $(MAKE) -f upduino.mk
	VIDEO_OUTPUT=PMOD_MUSE_VGA VIDEO_MODE=MODE_848x480 AUDIO=4 PF_B=true FPGA_CONFIG=1 $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_muse_vga.bin

# Build combo Xosera UPduino 3.x FPGA DVI bitstream for rosco_m6k board
xosera_dvi: $(DEF_COPPER)
	$(MAKE) -f upduino.mk clean
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_640x480 AUDIO=4 FPGA_CONFIG=0 $(MAKE) -f upduino.mk
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_848x480 AUDIO=4 FPGA_CONFIG=1 $(MAKE) -f upduino.mk
	mv xosera_upd.bin xosera_board_dvi.bin

# Build Xosera iCEBreaker FPGA bitstream
iceb_dvi: $(DEF_COPPER)
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true FPGA_CONFIG=0 $(MAKE) -f icebreaker.mk
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_848x480 AUDIO=4 PF_B=true FPGA_CONFIG=1 $(MAKE) -f icebreaker.mk
	@echo Type \"make iceb_prog\" to program the iCEBreaker via USB

# Build Xosera iCEBreaker FPGA bitstream
iceb_vga: $(DEF_COPPER)
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true FPGA_CONFIG=0 $(MAKE) -f icebreaker.mk
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_848x480 AUDIO=4 PF_B=true FPGA_CONFIG=1 $(MAKE) -f icebreaker.mk
	@echo Type \"make iceb_prog\" to program the iCEBreaker via USB

# Build and program Xosera UPduino 3.x FPGA bitstream (one config, for development)
upd_prog: $(DEF_COPPER)
	VIDEO_OUTPUT=PMOD_DIGILENT_VGA VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true $(MAKE) -f upduino.mk prog

# Build and program Xosera iCEBreaker FPGA bitstream (ine config, for development)
iceb_prog: $(DEF_COPPER)
	VIDEO_OUTPUT=PMOD_1B2_DVI12 VIDEO_MODE=MODE_640x480 AUDIO=4 PF_B=true $(MAKE) -f icebreaker.mk prog

# Clean all project targets
clean:
	$(MAKE) -f sim.mk clean
	$(MAKE) -f upduino.mk clean
	$(MAKE) -f icebreaker.mk clean

.PHONY: all prog def_files sim isim irun vsim vrun upd iceb xosera_board iceb_prog upd_prog xosera_prog clean
