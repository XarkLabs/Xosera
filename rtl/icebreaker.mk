# icebreaker.mk - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Primary tools (official binaries available from https://github.com/YosysHQ/oss-cad-suite-build/releases/latest)
#	Yosys
#	nextpnr-ice40
#	Verilator		(optional)
#	Icarus Verilog		(optional)
#	Built using macOS BigSur 11.2 and GNU/Linux Ubuntu 20.04 distribution

# Version bookkeeping
GITSHORTHASH := $(shell git rev-parse --short HEAD)
DIRTYFILES := $(shell git status --porcelain --untracked-files=no | grep -v _stats.txt | cut -d " " -f 3-)
ifeq ($(strip $(DIRTYFILES)),)
# prepend 0 for "clean"
XOSERA_HASH := 0$(GITSHORTHASH)
$(info === Xosera iCEBreaker [$(XOSERA_HASH)] is CLEAN from git)
else
# prepend d for "dirty"
XOSERA_HASH := D$(GITSHORTHASH)
$(info === Xosera iCEBreaker [$(XOSERA_HASH)] is DIRTY: $(DIRTYFILES))
endif

# Xosera video mode selection:
# Supported modes:                           (exact) (actual)
#	MODE_640x400	640x400@70Hz 	clock 25.175 (25.125) MHz
#	MODE_640x480	640x480@60Hz	clock 25.175 (25.125) MHz
#	MODE_720x400	720x400@70Hz 	clock 28.322 (28.500) MHz
#	MODE_848x480	848x480@60Hz	clock 33.750 (33.750) MHz (16:9 480p)
#	MODE_800x600	800x600@60Hz	clock 40.000 (39.750) MHz
#	MODE_1024x768	1024x768@60Hz	clock 65.000 (65.250) MHz [fails timing]
#	MODE_1280x720	1280x720@60Hz	clock 74.176 (73.500) MHz [fails timing]
VIDEO_MODE ?= MODE_848x480

# Xosera video output selection:
# Supported video outputs:
#   PMOD_1B2_DVI12		12-bit DVI, PMOD 1A&1B	https://1bitsquared.com/products/pmod-digital-video-interface
#   PMOD_DIGILENT_VGA		12-bit VGA, PMOD 1A&1B	https://store.digilentinc.com/pmod-vga-video-graphics-array/
#   PMOD_XESS_VGA		 9-bit VGA, PMOD 1A&1B	http://www.xess.com/shop/product/stickit-vga/
#   PMOD_XESS_VGA_SINGLE	 6-bit VGA, PMOD 1B	http://www.xess.com/shop/product/stickit-vga/ (half used)
#
VIDEO_OUTPUT ?= PMOD_1B2_DVI12

FONTFILES := $(wildcard ../fonts/*.mem)

# RTL source and include directory
SRCDIR := .

# log output directory
LOGS	:= icebreaker/logs

# Xosera project setup for iCEBreaker FPGA target
TOP := xosera_iceb
PIN_DEF := icebreaker/icebreaker.pcf
DEVICE := up5k
PACKAGE := sg48

# Verilog source directories
VPATH := $(SRCDIR) icebreaker

# Verilog source files for design
SRC := icebreaker/$(TOP).sv $(wildcard $(SRCDIR)/*.sv)

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.svh)

# dot graphic diagram files
DOT := $(SRC:.sv=.dot)

# icestorm tools
YOSYS := yosys
YOSYS_CONFIG := yosys-config
ICEPACK := icepack
ICETIME := icetime
ICEPROG := iceprog
ICEMULTI := icemulti

# Yosys synthesis arguments
# TODO: too slow, maybe buggy: YOSYS_SYNTH_ARGS := -dsp -abc9 -abc2 -relut -top $(TOP)
YOSYS_SYNTH_ARGS := -dsp -abc2 -relut -top $(TOP)

# Verilog preprocessor definitions common to all modules
DEFINES := -DNO_ICE40_DEFAULT_ASSIGNMENTS -DGITHASH=$(XOSERA_HASH) -D$(VIDEO_MODE) -D$(VIDEO_OUTPUT) -DICE40UP5K -DICEBREAKER -DSPI_INTERFACE

# Verilator tool (used for "lint")
VERILATOR := verilator
VERILATOR_ARGS := -I$(SRCDIR) -Wall -Wno-DECLFILENAME -Wno-UNUSED
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# nextPNR tools
NEXTPNR := nextpnr-ice40
NEXTPNR_ARGS := --placer heap --opt-timing

# defult target is make bitstream
all: icebreaker/$(TOP)_$(VIDEO_MODE).bin icebreaker.mk
	@echo === Finished Building iCEBreaker Xosera: $(VIDEO_OUTPUT) $(SPI_INTERFACE) ===

# program iCEBreaker FPGA via USB (may need udev rules or sudo on Linux)
prog: icebreaker/$(TOP)_$(VIDEO_MODE).bin icebreaker.mk
	@echo === Programming iCEBreaker Xosera: $(VIDEO_OUTPUT) $(SPI_INTERFACE) ===
	-$(ICEPROG) -d i:0x0403:0x6010 $(TOP).bin

# run icetime to generate a timing report
timing: icebreaker/$(TOP)_$(VIDEO_MODE).rpt icebreaker.mk
	@echo iCETime timing report: $(TOP).rpt

# run Yosys to generate a "dot" graphical representation of each design file
 show: $(DOT) icebreaker.mk

# run Yosys with "noflatten", which will produce a resource count per module
count: $(SRC) $(INC) $(FONTFILES) icebreaker.mk
	@mkdir -p $(LOGS)
	$(YOSYS) -l $(LOGS)/$(TOP)_$(VIDEO_MODE)_yosys_count.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -I$(SRCDIR) -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -noflatten'

# run Verilator to check for Verilog issues
lint: $(SRC) $(INC) $(FONTFILES) icebreaker.mk
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC)

$(DOT): %.dot: %.sv icebreaker.mk
	mkdir -p icebreaker/dot
	$(YOSYS) -l $(LOGS)/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) -DSHOW ; read_verilog -I$(SRCDIR) -sv $< ; show -enum -stretch -signed -width -prefix icebreaker/dot/$(basename $(notdir $<)) $(basename $(notdir $<))'

# synthesize Verilog and create json description
%.json: $(SRC) $(INC) $(FONTFILES) icebreaker.mk
	@echo === Building iCEBreaker Xosera ===
	@rm -f $@
	@mkdir -p $(LOGS)
	-$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC) 2>&1 | tee $(LOGS)/$(TOP)_verilator.log
	$(YOSYS) -l $(LOGS)/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -I$(SRCDIR) -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -json $@'

# make ASCII bitstream from JSON description and device parameters
icebreaker/%_$(VIDEO_MODE).asc: icebreaker/%_$(VIDEO_MODE).json $(PIN_DEF) icebreaker.mk
	@rm -f $@
	@mkdir -p $(LOGS)
	@-cp $(TOP)_stats.txt $(LOGS)/$(TOP)_stats_last.txt
	$(NEXTPNR) -l $(LOGS)/$(TOP)_nextpnr.log -q $(NEXTPNR_ARGS) --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@
	@echo === iCEBreaker Xosera: $(VIDEO_OUTPUT) $(VIDEO_MODE) $(SPI_INTERFACE) | tee $(TOP)_stats.txt
	@$(YOSYS) -V 2>&1 | tee -a $(TOP)_stats.txt
	@$(NEXTPNR) -V 2>&1 | tee -a $(TOP)_stats.txt
	@sed -n '/Device utilisation/,/Info: Placed/p' $(LOGS)/$(TOP)_nextpnr.log | sed '$$d' | grep -v ":     0/" | tee -a $(TOP)_stats.txt
	@grep "Max frequency" $(LOGS)/$(TOP)_nextpnr.log | tail -1 | tee -a $(TOP)_stats.txt
	@-diff -U0 $(LOGS)/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+\|^---\)" | while read line; do echo "PREVIOUS: $$line" ; done >$(LOGS)/$(TOP)_stats_delta.txt
	@-diff -U0 $(LOGS)/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+++\|^-\)" | while read line; do echo "CURRENT : $$line" ; done >>$(LOGS)/$(TOP)_stats_delta.txt
	@echo
	@-cat $(LOGS)/$(TOP)_stats_delta.txt

# make binary bitstream from ASCII bitstream
icebreaker/%_$(VIDEO_MODE).bin: icebreaker/%_$(VIDEO_MODE).asc icebreaker.mk
	@rm -f $@
	$(ICEPACK) $< $@
	$(ICEMULTI) -v -v -p0 icebreaker/*.bin -o $(TOP).bin

# make timing report from ASCII bitstream
icebreaker/%_$(VIDEO_MODE).rpt: icebreaker/%_$(VIDEO_MODE).asc icebreaker.mk
	@rm -f $@
	$(ICETIME) -d $(DEVICE) -m -t -r $@ $<

# delete all targets that will be re-generated
clean:
	rm -f xosera_iceb.bin $(wildcard icebreaker/*.json) $(wildcard icebreaker/*.asc) $(wildcard icebreaker/*.rpt) $(wildcard icebreaker/*.bin)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all prog timing count lint show clean
