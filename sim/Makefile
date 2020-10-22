# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Tool versions used:
#	Yosys 45cd323055270ff414419ddf8a9b5d08f40628b5 (open-tool-forge build) (git sha1 926d4d1, gcc 9.3.0-10ubuntu2 -Os)
#	nextpnr-ice40 -- Next Generation Place and Route (Version nightly-20200602)
#	Verilator 4.028 2020-02-06 rev v4.026-92-g890cecc1
#	Built on GNU/Linux using Ubuntu 20.04 distribution

# Xosera video mode selection:
# Supported modes:
#	MODE_640x400	640x400@70Hz 	clock 25.175 MHz
#	MODE_640x480	640x480@60Hz	clock 25.175 MHz
#	MODE_720x400	720x400@70Hz 	clock 28.322 MHz 
#	MODE_848x480	848x480@60Hz	clock 33.750 MHz (16:9 480p)
#	MODE_800x600	800x600@60Hz	clock 40.000 MHz
#	MODE_1024x768	1024x768@60Hz	clock 65.000 MHz
#	MODE_1280x720	1280x720@60Hz	clock 74.176 MHz
VIDEO_MODE := MODE_640x480

# Xosera test bed simulation target top (for Icaraus Verilog)
TBTOP := xosera_tb

# Xosera main target top (for Verilator)
VTOP := xosera_main

# RTL source and include directory
SRCDIR := ../rtl

# Verilog source directories
VPATH := $(SRCDIR)

# Verilog source files for design
SRC := $(SRCDIR)/xosera_main.v $(filter-out $(SRCDIR)/xosera_main.v,$(wildcard $(SRCDIR)/*.v))

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.vh)

# memory initialization file for default 8x16 font
FONTMEM := ../fonts/font_8x16.mem

# Verilog preprocessor definitions common to all modules
DEFINES := -DFONT_MEM=$(FONTMEM) -D$(VIDEO_MODE) -DICE40UP5K

# icestorm tools
YOSYS_CONFIG := yosys-config
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# Icarus Verilog
IVERILOG	:= iverilog
IVERILOG_ARGS	:= -g2005-sv -I ../rtl/ -l $(TECH_LIB)

# Verilator SDL2 simulation frame rendering (1 enables SDL2 video rendering)
SDL_RENDER	:= 1

# Simulate SPI commands diring simulation (results in log)
SPI_INTERFACE	:= 1

# Verilator C++ definitions and options
ifeq ($(strip $(SDL_RENDER)),1)
LDFLAGS		:= -LDFLAGS "$(shell sdl2-config --libs)"
SDL_CFLAGS	:= $(shell sdl2-config --cflags)
endif
# Note: Using -Os seems to provide the fastest compile+run simulation iteration time
CFLAGS		:= -CFLAGS "-std=c++14 -Wall -Wextra -fomit-frame-pointer -Wno-sign-compare -D$(VIDEO_MODE) -DSDL_RENDER=$(SDL_RENDER) -DSPI_INTERFACE=$(SPI_INTERFACE) $(SDL_CFLAGS)"

# Verilator tool (used for lint and simulation)
VERILATOR := verilator
VERILATOR_ARGS := --relative-includes -Wall -Wno-fatal -Wno-VARHIDDEN -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY

# Verillator C++ source driver
CSRC := xosera_sim.cpp

# default build native simulation executable
all: vsim isim

# build native simulation executable
vsim: obj_dir/V$(VTOP)
	@echo === Verilator simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Verilator simulation, use \"make vrun\" to run.

isim: $(TBTOP) Makefile
	@echo === Icarus Verilog simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Icarus Verilog simulation, use \"make irun\" to run.

# run Verilator to build and run native simulation executable
vrun: obj_dir/V$(VTOP) Makefile
	@mkdir -p logs
	obj_dir/V$(VTOP) | tee logs/xosera_vsim.log

# run Verilator to build and run native simulation executable
irun: $(TBTOP)
	./$(TBTOP)

# use Verilator to build native simulation executable
obj_dir/V$(VTOP): $(CSRC) $(INC) $(SRC) Makefile
	rm -rf obj_dir
	$(VERILATOR) $(VERILATOR_ARGS) --cc --exe --trace -DSIMULATE $(DEFINES) $(CFLAGS) $(LDFLAGS) --top-module $(VTOP) $(TECH_LIB) $(SRC) xosera_sim.cpp
	cd obj_dir && make -f V$(VTOP).mk -j$(shell nproc)

# use Icarus Verilog to build vvp simulation executable
$(TBTOP): $(INC) $(TBTOP).v $(SRC) Makefile
	$(IVERILOG) $(IVERILOG_ARGS) -DSIMULATE -D$(VIDEO_MODE) -o $(TBTOP) $(TBTOP).v $(SRC)

# delete all targets that will be re-generated
clean:
	rm -rf obj_dir $(TBTOP)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all vsim isim vrun irun clean
