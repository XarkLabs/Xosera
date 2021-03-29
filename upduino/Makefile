# Makefile - Xosera for Upduino v3.x FPGA board
# vim: set noet ts=8 sw=8
#
# NOTE: This assumes Upduino 3.x with PLL and needs either the "OSC" jumper shorted (recommended, but dedicates gpio_20 as a clock).
# Also, since the RGB LED pins are used as GPIO inputs, jumper R28 should be cut to disconnect the RGB LED (which can interfere with input).

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Tools used:
#	Yosys / nextpnr-ice40 -- Synthesis and Next Generation Place and Route (Version nightly-20200609)
#	Verilator -  fast C++ based simulator
#	Icarus Verilog
#	Built on GNU/Linux using Ubuntu 20.04 distribution

# Xosera video mode selection:
# Supported modes:                           (exact) (actual)
#	MODE_640x400	640x400@70Hz 	clock 25.175 (25.125) MHz
#	MODE_640x480	640x480@60Hz	clock 25.175 (25.125) MHz
#	MODE_720x400	720x400@70Hz 	clock 28.322 (28.500) MHz
#	MODE_848x480	848x480@60Hz	clock 33.750 (33.750) MHz (16:9 480p)
#	MODE_800x600	800x600@60Hz	clock 40.000 (39.750) MHz
#	MODE_1024x768	1024x768@60Hz	clock 65.000 (65.250) MHz [fails timing]
#	MODE_1280x720	1280x720@60Hz	clock 74.176 (73.500) MHz [fails timing]
VIDEO_MODE := MODE_848x480

# Xosera project setup for Upduino v3.0
TOP := xosera_upd
SDC := ../rtl/timing/$(VIDEO_MODE).py
PIN_DEF := upduino_v3.pcf
DEVICE := up5k
PACKAGE := sg48

# RTL source and include directory
SRCDIR := ../rtl

# Verilog source directories
VPATH := $(SRCDIR)

# Verilog source files for design
SRC := $(TOP).sv $(wildcard $(SRCDIR)/*.sv)

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.svh)

# memory initialization file for default 8x16 font
FONTMEM := ../fonts/font_8x16.mem

# dot graphic diagram files
DOT := $(SRC:.sv=.dot)

# icestorm tools
YOSYS := yosys
YOSYS_CONFIG := yosys-config
ICEPACK := icepack
ICETIME := icetime
ICEPROG := iceprog

# Yosys synthesis arguments
# TODO: too slow, maybe buggy: YOSYS_SYNTH_ARGS := -dsp -abc9 -abc2 -relut -top $(TOP)
YOSYS_SYNTH_ARGS := -dsp -top $(TOP)

# Verilog preprocessor definitions common to all modules
DEFINES := -DFONT_MEM=$(FONTMEM) -D$(VIDEO_MODE) -DICE40UP5K -DUPDUINO

# Verilator tool (used for "lint")
VERILATOR := verilator
VERILATOR_ARGS := --relative-includes -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSED
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# nextPNR tools
NEXTPNR := nextpnr-ice40
NEXTPNR_ARGS := --pre-pack $(SDC) --placer heap

# defult target is make bitstream
all: $(TOP).bin Makefile
	@echo === Upduino Xosera configured for: $(VIDEO_MODE) ===
	@echo     Type \"make prog\" to program the Upduino via USB

# program Upduino FPGA via USB (may need udev rules or sudo on Linux)
prog: $(TOP).bin Makefile
	@echo === Upduino Xosera configured for: $(VIDEO_MODE) ===
	$(ICEPROG) -d i:0x0403:0x6014 $<

# run icetime to generate a timing report
timing: $(TOP).rpt Makefile
	@echo iCETime timing report: $(TOP).rpt

# run Yosys with "noflatten", which will produce a resource count per module
count: $(SRC) $(INC) $(FONTMEM) Makefile
	@mkdir -p logs
	$(YOSYS) -p 'verilog_defines $(DEFINES) ; read_verilog -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -noflatten' 2>&1 | tee logs/$(TOP)_yosys_count.log

# run Verilator to check for Verilog issues
lint: $(SRC) $(INC) $(FONTMEM) Makefile
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) -DSIMULATE --top-module $(TOP) $(TECH_LIB) $(SRC)

# run Yosys to generate a "dot" graphical representation of each design file
show: $(DOT)

$(DOT): %.dot: %.sv
	mkdir -p dot
	$(YOSYS) -p 'verilog_defines $(DEFINES) -DSHOW ; read_verilog -sv $< ; show -enum -stretch -signed -width -prefix dot/$(basename $(notdir $<)) $(basename $(notdir $<))'

# delete all targets that will be re-generated
clean:
	rm -f *.json *.asc *.rpt *.bin

# synthesize Verilog and create json description
%.json: $(SRC) $(INC) $(MEM) Makefile
	@rm -f $@
	@mkdir -p logs
	-$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) -DSIMULATE --top-module $(TOP) $(TECH_LIB) $(SRC) 2>&1 | tee logs/$(TOP)_verilator.log
	$(YOSYS) -p 'verilog_defines $(DEFINES) ; read_verilog -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -json $@' 2>&1 | tee logs/$(TOP)_yosys.log

# make ASCII bitstream from JSON description and device parameters
%.asc: %.json $(PIN_DEF) $(SDC) $(MEM) Makefile
	@rm -f $@
	@mkdir -p logs
	$(NEXTPNR) $(NEXTPNR_ARGS) --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@ 2>&1 | tee logs/$(TOP)_nextpnr.log
	@echo === Upduino Xosera : $(VIDEO_MODE) | tee $(TOP)_stats.txt
	@$(YOSYS) -V 2>&1 | tee -a $(TOP)_stats.txt
	@$(NEXTPNR) -V 2>&1 | tee -a $(TOP)_stats.txt
	@sed -n '/Device utilisation/,/Info: Placed/p' logs/$(TOP)_nextpnr.log | sed '$$d' | tee -a $(TOP)_stats.txt
	@grep "Max frequency" logs/$(TOP)_nextpnr.log | tail -1 | tee -a $(TOP)_stats.txt

# make binary bitstream from ASCII bitstream
%.bin: %.asc Makefile
	@rm -f $@
	$(ICEPACK) $< $@

# make timing report from ASCII bitstream
%.rpt: %.asc Makefile
	@rm -f $@
	$(ICETIME) -d $(DEVICE) -m -t -r $@ $<

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all prog timing count lint show clean
