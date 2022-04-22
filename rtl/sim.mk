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

# This is a hack to get make to exit if command fails (even if command after pipe succeeds, e.g., tee)
SHELL := /bin/bash -o pipefail

# Version bookkeeping
GITSHORTHASH := $(shell git rev-parse --short HEAD)
DIRTYFILES := $(shell git status --porcelain --untracked-files=no | grep rtl/ | grep -v _stats.txt | cut -d " " -f 3-)
ifeq ($(strip $(DIRTYFILES)),)
# "clean" (unmodified) from git
XOSERA_HASH := $(GITSHORTHASH)
XOSERA_CLEAN := 1
$(info === Xosera simulation [$(XOSERA_HASH)] is CLEAN from git)
else
# "dirty" (HDL modified) from git
XOSERA_HASH := $(GITSHORTHASH)
XOSERA_CLEAN := 0
$(info === Xosera simulation [$(XOSERA_HASH)] is DIRTY: $(DIRTYFILES))
endif

# Xosera video mode selection:
# Supported modes:
#	MODE_640x400	640x400@70Hz 	clock 25.175 MHz
#	MODE_640x480	640x480@60Hz	clock 25.175 MHz
#	MODE_640x480_75	640x480@75Hz	clock 31.500 MHz
#	MODE_640x480_85	640x480@85Hz	clock 36.000 MHz
#	MODE_720x400	720x400@70Hz 	clock 28.322 MHz
#	MODE_848x480	848x480@60Hz	clock 33.750 MHz (16:9 480p)
#	MODE_800x600	800x600@60Hz	clock 40.000 MHz
#	MODE_1024x768	1024x768@60Hz	clock 65.000 MHz
#	MODE_1280x720	1280x720@60Hz	clock 74.176 MHz
VIDEO_MODE ?= MODE_640x480

# monochrome + color attribute byte
#VRUN_TESTDATA ?= -u ../testdata/raw/space_shuttle_color_640x480.raw

#4bpp
#VRUN_TESTDATA ?= -u ../testdata/raw/color_bars_test_pal.raw -u ../testdata/raw/color_bars_test.raw
#VRUN_TESTDATA ?= -u ../testdata/raw/escher-relativity_320x240_16_pal.raw -u ../testdata/raw/escher-relativity_320x240_16.raw
#VRUN_TESTDATA ?= -u ../testdata/raw/ST_KingTut_Dpaint_16_pal.raw -u ../testdata/raw/ST_KingTut_Dpaint_16.raw
#8bpp
#VRUN_TESTDATA ?= -u ../testdata/raw/VGA_Balloon_320x200_256_pal.raw -u ../testdata/raw/VGA_Balloon_320x200_256.raw
#VRUN_TESTDATA ?= -u ../testdata/raw/256_colors_pal.raw -u ../testdata/raw/256_colors.raw
#VRUN_TESTDATA ?= -u ../testdata/raw/xosera_r1_pal.raw -u ../testdata/raw/xosera_r1.raw
#VRUN_TESTDATA ?= -u ../testdata/raw/space_shuttle_color_small.raw
#VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/pacbox-320x240_pal.raw -u ../testdata/raw/pacbox-320x240.raw ../testdata/raw/xosera_r1_pal.raw -u ../testdata/raw/xosera_r1.raw -u ../testdata/raw/color_cube_320x240_256_pal_alpha.raw -u ../testdata/raw/color_cube_320x240_256.raw -u ../testdata/raw/pacbox-320x240_pal.raw -u ../testdata/raw/pacbox-320x240.raw -u ../testdata/raw/mountains_mono_640x480w.raw
#VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/true_color_pal.raw -u ../testdata/raw/parrot_320x240_RG8B4.raw
VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/true_color_pal.raw -u ../testdata/raw/parrot_320x240_RG8B4.raw -u ../testdata/raw/ST_KingTut_Dpaint_16_pal.raw -u ../testdata/raw/ST_KingTut_Dpaint_16.raw
#VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/ST_KingTut_Dpaint_16_pal.raw -u ../testdata/raw/ST_KingTut_Dpaint_16.raw
# Xosera test bed simulation target top (for Icaraus Verilog)
TBTOP := xosera_tb

DEFTOP := xosera_def_files

# Xosera main target top (for Verilator)
VTOP := xosera_main

# RTL source and include directory
SRCDIR := .

# Verilog source directories
VPATH := $(SRCDIR)

# Verilog source files for design
SRC := $(SRCDIR)/xosera_main.sv $(filter-out $(SRCDIR)/xosera_main.sv,$(wildcard $(SRCDIR)/*.sv))

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.svh)

# Simulate BUS commands diring simulation (results in log)
BUS_INTERFACE	:= 1

# Verilog preprocessor definitions common to all modules
DEFINES := -DNO_ICE40_DEFAULT_ASSIGNMENTS -DGITCLEAN=$(XOSERA_CLEAN) -DGITHASH=$(XOSERA_HASH) -D$(VIDEO_MODE) -DICE40UP5K

ifeq ($(strip $(BUS_INTERFACE)),1)
DEFINES += -DBUS_INTERFACE
endif

current_dir = $(shell pwd)

LOGS	:= sim/logs

# icestorm tools
YOSYS_CONFIG := yosys-config
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# Icarus Verilog
IVERILOG := iverilog
IVERILOG_ARGS := -g2012 -I$(SRCDIR) -Wall -l $(TECH_LIB)

# Verilator C++ definitions and options
SDL_RENDER := 1
ifeq ($(strip $(SDL_RENDER)),1)
LDFLAGS := -LDFLAGS "$(shell sdl2-config --libs) -lSDL2_image"
SDL_CFLAGS := $(shell sdl2-config --cflags)
endif
# Note: Using -Os seems to provide the fastest compile+run simulation iteration time
# Linux gcc needs -Wno-maybe-uninitialized
CFLAGS		:= -CFLAGS "-std=c++14 -Wall -Wextra -Werror -fomit-frame-pointer -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wno-int-in-bool-context -D$(VIDEO_MODE) -DSDL_RENDER=$(SDL_RENDER) -DBUS_INTERFACE=$(BUS_INTERFACE) $(SDL_CFLAGS)"

# Verilator tool (used for lint and simulation)
VERILATOR := verilator
VERILATOR_ARGS := --sv --language 1800-2012 -I$(SRCDIR) -Mdir sim/obj_dir -Wall --trace-fst -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-STMTDLY

# Verillator C++ source driver
CSRC := sim/xosera_sim.cpp

# default build native simulation executable
all: make_defs vsim isim

def_files: sim/$(DEFTOP) sim.mk
	@echo === Icarus Verilog creating C and asm definition files ===
	@mkdir -p $(LOGS)
	sim/$(DEFTOP)

# build native simulation executable
vsim: sim/obj_dir/V$(VTOP) sim.mk
	@echo === Verilator simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Verilator simulation, use \"make vrun\" to run.

isim: sim/$(TBTOP) sim.mk
	@echo === Icarus Verilog simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Icarus Verilog simulation, use \"make irun\" to run.

# run Verilator to build and run native simulation executable
vrun: sim/obj_dir/V$(VTOP) sim.mk
	@mkdir -p $(LOGS)
	sim/obj_dir/V$(VTOP) $(VRUN_TESTDATA)

# run Verilator to build and run native simulation executable
irun: sim/$(TBTOP) sim.mk
	@mkdir -p $(LOGS)
	sim/$(TBTOP) -fst

# use Icarus Verilog to build vvp simulation executable
sim/$(DEFTOP): $(INC) sim/$(DEFTOP).sv sim.mk
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TBTOP) sim/$(TBTOP).sv
	$(IVERILOG) $(IVERILOG_ARGS) $(DEFINES) -o sim/$(DEFTOP) $(current_dir)/sim/$(DEFTOP).sv

# use Verilator to build native simulation executable
sim/obj_dir/V$(VTOP): $(CSRC) $(INC) $(SRC) sim.mk
	$(VERILATOR) $(VERILATOR_ARGS) --cc --exe --trace $(DEFINES) $(CFLAGS) $(LDFLAGS) --top-module $(VTOP) $(TECH_LIB) $(SRC) $(current_dir)/$(CSRC)
	cd sim/obj_dir && make -f V$(VTOP).mk

# use Icarus Verilog to build vvp simulation executable
sim/$(TBTOP): $(INC) sim/$(TBTOP).sv $(SRC) sim.mk
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES)  -v $(TECH_LIB) --top-module $(TBTOP) sim/$(TBTOP).sv $(SRC)
	$(IVERILOG) $(IVERILOG_ARGS) $(DEFINES) -D$(VIDEO_MODE) -o sim/$(TBTOP) $(current_dir)/sim/$(TBTOP).sv $(SRC)

# delete all targets that will be re-generated
clean:
	rm -rf sim/obj_dir sim/$(TBTOP)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all vsim isim vrun irun clean
