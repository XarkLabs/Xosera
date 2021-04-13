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

# Version bookkeeping
GITSHORTHASH := $(shell git rev-parse --short HEAD)
DIRTYFILES := $(shell git status --porcelain --untracked-files=no . ../rtl | grep -v _stats.txt | cut -d " " -f 3-)
ifeq ($(strip $(DIRTYFILES)),)
# prepend 0 for "clean"
XOSERA_HASH := 0$(GITSHORTHASH)
$(info === Xosera Upduino [$(XOSERA_HASH)] is CLEAN from git)
else
# prepend d for "dirty"
XOSERA_HASH := d$(GITSHORTHASH)
$(info === Xosera Upduino [$(XOSERA_HASH)] is DIRTY: $(DIRTYFILES))
endif

# Xosera video mode selection:
# Supported modes:
#	MODE_640x400	640x400@70Hz 	clock 25.175 MHz
#	MODE_640x480	640x480@60Hz	clock 25.175 MHz
#	MODE_720x400	720x400@70Hz 	clock 28.322 MHz
#	MODE_848x480	848x480@60Hz	clock 33.750 MHz (16:9 480p)
#	MODE_800x600	800x600@60Hz	clock 40.000 MHz
#	MODE_1024x768	1024x768@60Hz	clock 65.000 MHz
#	MODE_1280x720	1280x720@60Hz	clock 74.176 MHz
VIDEO_MODE := MODE_848x480

# Xosera test bed simulation target top (for Icaraus Verilog)
TBTOP := xosera_tb

# Xosera main target top (for Verilator)
VTOP := xosera_main

# RTL source and include directory
SRCDIR := ../rtl

# Verilog source directories
VPATH := $(SRCDIR)

# Verilog source files for design
SRC := $(SRCDIR)/xosera_main.sv $(filter-out $(SRCDIR)/xosera_main.sv,$(wildcard $(SRCDIR)/*.sv))

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.svh)

# Simulate BUS commands diring simulation (results in log)
BUS_INTERFACE	:= 1

# Verilog preprocessor definitions common to all modules
DEFINES := -DGITHASH=$(XOSERA_HASH) -DFONT_MEM=$(FONTMEM) -D$(VIDEO_MODE) -DICE40UP5K

ifeq ($(strip $(BUS_INTERFACE)),1)
DEFINES += -DBUS_INTERFACE
endif

current_dir = $(shell pwd)

# icestorm tools
YOSYS_CONFIG := yosys-config
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# Icarus Verilog
IVERILOG	:= iverilog
IVERILOG_ARGS	:= -g2005-sv -Wall -I../rtl/ -l $(TECH_LIB)

# Verilator C++ definitions and options
SDL_RENDER	:= 1
ifeq ($(strip $(SDL_RENDER)),1)
LDFLAGS		:= -LDFLAGS "$(shell sdl2-config --libs) -lsdl2_image"
SDL_CFLAGS	:= $(shell sdl2-config --cflags)
endif
# Note: Using -Os seems to provide the fastest compile+run simulation iteration time
# Linux gcc needs -Wno-maybe-uninitialized
CFLAGS		:= -CFLAGS "-std=c++14 -Wall -Wextra -Werror -fomit-frame-pointer -Wno-sign-compare -Wno-unused-parameter -D$(VIDEO_MODE) -DSDL_RENDER=$(SDL_RENDER) -DBUS_INTERFACE=$(BUS_INTERFACE) $(SDL_CFLAGS)"

# Verilator tool (used for lint and simulation)
VERILATOR := verilator
VERILATOR_ARGS := --relative-includes -I../rtl --timescale-override 1ns/1ps -Wall -Wno-UNUSED -Wno-VARHIDDEN -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-STMTDLY

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
	./$(TBTOP) 2>&1 | tee logs/$(TBTOP)_isim.log

# use Verilator to build native simulation executable
obj_dir/V$(VTOP): $(CSRC) $(INC) $(SRC) Makefile
	rm -rf obj_dir
	$(VERILATOR) $(VERILATOR_ARGS) --cc --exe --trace $(DEFINES) $(CFLAGS) $(LDFLAGS) --top-module $(VTOP) $(TECH_LIB) $(SRC) $(current_dir)/$(CSRC)
	cd obj_dir && make -f V$(VTOP).mk

# -j$(shell nproc)

# use Icarus Verilog to build vvp simulation executable
$(TBTOP): $(INC) $(TBTOP).sv $(SRC) Makefile
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TBTOP) $(TBTOP).sv $(SRC)
	$(IVERILOG) $(IVERILOG_ARGS) $(DEFINES) -D$(VIDEO_MODE) -o $(TBTOP) $(current_dir)/$(TBTOP).sv $(SRC)

# delete all targets that will be re-generated
clean:
	rm -rf obj_dir $(TBTOP)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all vsim isim vrun irun clean
