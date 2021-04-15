# upduino.mk - Xosera for Upduino v3.x FPGA board
# vim: set noet ts=8 sw=8
#
# NOTE: This assumes Upduino 3.x with PLL and needs either the "OSC" jumper shorted (recommended, but dedicates gpio_20 as a clock).
# Also, since the RGB LED pins are used as GPIO inputs, jumper R28 should be cut to disconnect the RGB LED (which can interfere with input).

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Primary tools (official binaries available from https://github.com/YosysHQ/fpga-toolchain)
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
$(info === Xosera Upduino [$(XOSERA_HASH)] is CLEAN from git)
else
# prepend d for "dirty"
XOSERA_HASH := D$(GITSHORTHASH)
$(info === Xosera Upduino [$(XOSERA_HASH)] is DIRTY: $(DIRTYFILES))
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

# RTL source and include directory
SRCDIR := .

# log output directory
LOGS	:= upduino/logs

# Xosera project setup for Upduino v3.0
TOP := xosera_upd
SDC := $(SRCDIR)/timing/$(VIDEO_MODE).py
PIN_DEF := upduino/upduino_v3.pcf
DEVICE := up5k
PACKAGE := sg48

# Verilog source directories
VPATH := $(SRCDIR) upduino

# Verilog source files for design
SRC := upduino/$(TOP).sv $(wildcard $(SRCDIR)/*.sv)

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
YOSYS_SYNTH_ARGS := -dsp -top $(TOP)

# Verilog preprocessor definitions common to all modules
DEFINES := -DGITHASH=$(XOSERA_HASH) -D$(VIDEO_MODE) -DICE40UP5K -DUPDUINO

# Verilator tool (used for "lint")
VERILATOR := verilator
VERILATOR_ARGS := -I$(SRCDIR) -Iupduino -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSED
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# nextPNR tools
NEXTPNR := nextpnr-ice40
NEXTPNR_ARGS := --pre-pack $(SDC) --placer heap

# defult target is make bitstream
all: $(TOP).bin upduino.mk
	@echo === Upduino Xosera: $(VIDEO_MODE) ===

# program Upduino FPGA via USB (may need udev rules or sudo on Linux)
prog: $(TOP).bin upduino.mk
	@echo === Upduino Xosera: $(VIDEO_MODE) ===
	$(ICEPROG) -d i:0x0403:0x6014 $<

# run icetime to generate a timing report
timing: $(TOP).rpt upduino.mk
	@echo iCETime timing report: $(TOP).rpt

# run Yosys with "noflatten", which will produce a resource count per module
count: $(SRC) $(INC) $(FONTMEM) upduino.mk
	@mkdir -p $(LOGS)
	$(YOSYS) -l $(LOGS)/$(TOP)_yosys_count.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -I$(SRCDIR) -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -noflatten'

# run Verilator to check for Verilog issues
lint: $(SRC) $(INC) $(FONTMEM) upduino.mk
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC)

# run Yosys to generate a "dot" graphical representation of each design file
show: $(DOT)

$(DOT): %.dot: %.sv
	mkdir -p dot
	$(YOSYS) -l $(LOGS)/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) -DSHOW ; read_verilog -I$(SRCDIR) -sv $< ; show -enum -stretch -signed -width -prefix dot/$(basename $(notdir $<)) $(basename $(notdir $<))'

# synthesize Verilog and create json description
%.json: $(SRC) $(INC) $(MEM) upduino.mk
	@rm -f $@
	@mkdir -p $(LOGS)
	-$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC) 2>&1 | tee $(LOGS)/$(TOP)_verilator.log
	$(YOSYS) -l $(LOGS)/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -I$(SRCDIR) -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -json $@'

# make ASCII bitstream from JSON description and device parameters
%.asc: %.json $(PIN_DEF) $(SDC) $(MEM) upduino.mk
	@rm -f $@
	@mkdir -p $(LOGS)
	@-cp $(TOP)_stats.txt $(LOGS)/$(TOP)_stats_last.txt
	$(NEXTPNR) -l $(LOGS)/$(TOP)_nextpnr.log -q $(NEXTPNR_ARGS) --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@
	@echo === Upduino Xosera: $(VIDEO_MODE) | tee $(TOP)_stats.txt
	@$(YOSYS) -V 2>&1 | tee -a $(TOP)_stats.txt
	@$(NEXTPNR) -V 2>&1 | tee -a $(TOP)_stats.txt
	@sed -n '/Device utilisation/,/Info: Placed/p' $(LOGS)/$(TOP)_nextpnr.log | sed '$$d' | grep -v ":     0/" | tee -a $(TOP)_stats.txt
	@grep "Max frequency" $(LOGS)/$(TOP)_nextpnr.log | tail -1 | tee -a $(TOP)_stats.txt
	@-diff -U0 $(LOGS)/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+\|^---\)" | while read line; do echo "PREVIOUS: $$line" ; done >$(LOGS)/$(TOP)_stats_delta.txt
	@-diff -U0 $(LOGS)/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+++\|^-\)" | while read line; do echo "CURRENT : $$line" ; done >>$(LOGS)/$(TOP)_stats_delta.txt
	@echo
	@-cat $(LOGS)/$(TOP)_stats_delta.txt

# make binary bitstream from ASCII bitstream
%.bin: %.asc upduino.mk
	@rm -f $@
	$(ICEPACK) $< upduino/$(basename $@)_$(VIDEO_MODE).bin
	$(ICEMULTI) -p0 upduino/*.bin -o $@

# make timing report from ASCII bitstream
%.rpt: %.asc upduino.mk
	@rm -f $@
	$(ICETIME) -d $(DEVICE) -m -t -r $@ $<

# delete all targets that will be re-generated
clean:
	rm -f xosera_upd.json xosera_upd.asc xosera_upd.rpt xosera_upd.bin $(wildcard upduino/*.bin)

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all prog timing count lint show clean
