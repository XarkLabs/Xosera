# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Primary tools (official binaries available from https://github.com/YosysHQ/oss-cad-suite-build/releases/latest)
#       Yosys
#       nextpnr-ice40
#       Verilator               (optional)
#       Icarus Verilog          (optional)
#       Built using macOS BigSur and GNU/Linux Ubuntu distribution

# This is a hack to get make to exit if command fails (even if command after pipe succeeds, e.g., tee)
SHELL := /bin/bash -o pipefail

# Version bookkeeping
BUILDDATE := $(shell date -u "+%Y%m%d")
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

# Maximum number of CPU cores to use before waiting with FMAX_TEST
MAX_CPUS := 8

# Xosera video mode selection:
# Supported modes:                           (exact) (actual)
#       MODE_640x400    640x400@70Hz    clock 25.175 (25.125) MHz
#       MODE_640x480    640x480@60Hz    clock 25.175 (25.125) MHz
#       MODE_640x480_75 640x480@75Hz    clock 31.500 (31.500) MHz
#       MODE_640x480_85 640x480@85Hz    clock 36.000 (36.000) MHz
#       MODE_720x400    720x400@70Hz    clock 28.322 (28.500) MHz
#       MODE_848x480    848x480@60Hz    clock 33.750 (33.750) MHz (16:9 480p)
#       MODE_800x600    800x600@60Hz    clock 40.000 (39.750) MHz
#       MODE_1024x768   1024x768@60Hz   clock 65.000 (65.250) MHz [fails timing]
#       MODE_1280x720   1280x720@60Hz   clock 74.176 (73.500) MHz [fails timing]
VIDEO_MODE ?= MODE_640x480
VERILOG_DEFS := -D$(VIDEO_MODE)

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
#VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/true_color_pal.raw -u ../testdata/raw/parrot_320x240_RG8B4.raw -u ../testdata/raw/ST_KingTut_Dpaint_16_pal.raw -u ../testdata/raw/ST_KingTut_Dpaint_16.raw -u ../testdata/raw/ramptable.raw
#VRUN_TESTDATA ?=   -u ../testdata/raw/moto_m_transp_4bpp.raw -u ../testdata/raw/ST_KingTut_Dpaint_16_pal.raw -u ../testdata/raw/ST_KingTut_Dpaint_16.raw
#VRUN_TESTDATA ?=   -u ../testdata/raw/sintable.raw -u ../testdata/raw/ramptable.raw
VRUN_TESTDATA ?=   -u ../testdata/raw/ramptable.raw -u ../testdata/raw/sintable.raw
# Xosera test bed simulation target top (for Icaraus Verilog)
TBTOP := xosera_tb

# Xosera main target top (for Verilator)
VTOP := xosera_main

ifneq ($(strip $(PF_B)),)
VERILOG_DEFS += -DEN_PF_B
endif

ifeq ($(strip $(AUDIO)),)
AUDIO := 0
endif

ifneq ($(strip $(AUDIO)),0)
VERILOG_DEFS += -DEN_AUDIO=$(AUDIO)
endif

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
DEFINES := -DNO_ICE40_DEFAULT_ASSIGNMENTS -DGITCLEAN=$(XOSERA_CLEAN) -DGITHASH=$(XOSERA_HASH) -DBUILDDATE=$(BUILDDATE) $(VERILOG_DEFS) -DICE40UP5K -DUPDUINO

ifeq ($(strip $(BUS_INTERFACE)),1)
DEFINES += -DBUS_INTERFACE
endif

current_dir = $(shell pwd)

LOGS	:= sim/logs

# icestorm tools
YOSYS_CONFIG := yosys-config
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)
VLT_CONFIG := sim/ice40_config.vlt

# Icarus Verilog
IVERILOG := iverilog
VVP := vvp
IVERILOG_ARGS := -g2012 -I$(SRCDIR) -Wall -l $(TECH_LIB)

# Verilator C++ definitions and options
SDL_RENDER := 1
ifeq ($(strip $(SDL_RENDER)),1)
LDFLAGS := -LDFLAGS "$(shell sdl2-config --libs) -lSDL2_image"
SDL_CFLAGS := $(shell sdl2-config --cflags)
endif
# Note: Using -Os seems to provide the fastest compile+run simulation iteration time
# Linux gcc needs -Wno-maybe-uninitialized
CFLAGS		:= -CFLAGS "-std=c++14 -Wall -Wextra -Werror -fomit-frame-pointer -Wno-deprecated-declarations -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wno-int-in-bool-context -D$(VIDEO_MODE) -DSDL_RENDER=$(SDL_RENDER) -DBUS_INTERFACE=$(BUS_INTERFACE) $(SDL_CFLAGS)"

# Verilator tool (used for lint and simulation)
VERILATOR := verilator
VERILATOR_ARGS := --sv --language 1800-2012 --timing -I$(SRCDIR) -v $(TECH_LIB) $(VLT_CONFIG) -Mdir sim/obj_dir -Wall --trace-fst -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-STMTDLY -Wno-fatal

# Verillator C++ source driver
CSRC := sim/xosera_sim.cpp

# default build native simulation executable
all: make_defs vsim isim

# build native simulation executable
vsim: $(VLT_CONFIG) sim/obj_dir/V$(VTOP) sim.mk
	@echo === Verilator simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Verilator simulation, use \"make vrun\" to run.

isim: $(VLT_CONFIG) sim/$(TBTOP) sim.mk
	@echo === Icarus Verilog simulation configured for: $(VIDEO_MODE) ===
	@echo Completed building Icarus Verilog simulation, use \"make irun\" to run.

# run Verilator to build and run native simulation executable
vrun: $(VLT_CONFIG) sim/obj_dir/V$(VTOP) sim.mk
	@mkdir -p $(LOGS)
	sim/obj_dir/V$(VTOP) $(VRUN_TESTDATA)

# run Verilator to build and run native simulation executable
irun: $(VLT_CONFIG) sim/$(TBTOP) sim.mk
	@mkdir -p $(LOGS)
	$(VVP) sim/$(TBTOP) -fst

# disable UNUSED and UNDRIVEN warnings in cells_sim.v library for Verilator lint
$(VLT_CONFIG):
	@echo >$(VLT_CONFIG)
	@echo >>$(VLT_CONFIG) \`verilator_config
	@echo >>$(VLT_CONFIG) lint_off -rule WIDTH  -file \"$(TECH_LIB)\"
	@echo >>$(VLT_CONFIG) lint_off -rule UNUSED  -file \"$(TECH_LIB)\"
	@echo >>$(VLT_CONFIG) lint_off -rule UNDRIVEN  -file \"$(TECH_LIB)\"

# use Verilator to build native simulation executable
sim/obj_dir/V$(VTOP): $(VLT_CONFIG) $(CSRC) $(INC) $(SRC) sim.mk
	$(VERILATOR) $(VERILATOR_ARGS) --cc --exe --trace $(DEFINES) $(CFLAGS) $(LDFLAGS) --top-module $(VTOP) $(SRC) $(current_dir)/$(CSRC)
	cd sim/obj_dir && make -f V$(VTOP).mk

# use Icarus Verilog to build vvp simulation executable
sim/$(TBTOP): $(INC) sim/$(TBTOP).sv $(SRC) sim.mk
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES)  -v $(TECH_LIB) --top-module $(TBTOP) sim/$(TBTOP).sv $(SRC)
	$(IVERILOG) $(IVERILOG_ARGS) $(DEFINES) -D$(VIDEO_MODE) -o sim/$(TBTOP) $(current_dir)/sim/$(TBTOP).sv $(SRC)

# delete all targets that will be re-generated
clean:
	rm -rf sim/obj_dir $(VLT_CONFIG) sim/$(TBTOP)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all vsim isim vrun irun clean
