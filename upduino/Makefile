# Makefile - Xosera for Upduino v3.x FPGA board
# vim: set noet ts=8 sw=8

# NOTE: Upduino 2.x should be the same as 3.x except for the clock input.
#	However, it is known to suffer from significant problems when using the
#	PLL (without board modifications, e.g., adding a capacitor to PLL VCC).  See:
#	https://tinyvision.ai/blogs/processing-at-the-edge/ground-trampolines-and-phase-locked-loops
#       Use -DEXTLK35 option below to enable external clock to gpio_35.  You can try running a wire from J8/12Mhz
#	to gpio_35, but I was unable to get PLL stable.  However, using a 25Mhz 3.3v oscillator or oscillator circuit
#	into gpio_35 and -DNOPLL does work (25Mhz for 640x480, others frequencies/modes from table below may also work)
#
# NOTE: Upduino 3.x PLL is fine, but needs either the "OSC" jumper shorted (recommended, but dedicates gpio_10 as a clock)
#	or add the -DCLKGPIO35 option below and run a wire from 12Mhz pin to gpio_35 (same as Upduino 2.x with PLL)

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Tool versions used:
#	Yosys 74e93e083ff23f3381fe2409e5847f9843840b17 (open-tool-forge build) (git sha1 bb6d04d, gcc 9.3.0-10ubuntu2 -Os)
#	nextpnr-ice40 -- Next Generation Place and Route (Version nightly-20200609)
#	Verilator 4.028 2020-02-06 rev v4.026-92-g890cecc1
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
VIDEO_MODE := MODE_640x480

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
SRC := $(TOP).v $(wildcard $(SRCDIR)/*.v)

# Verilog include files for design
INC := $(wildcard $(SRCDIR)/*.vh)

# memory initialization file for default 8x16 font
FONTMEM := ../fonts/font_8x16.mem

# dot graphic diagram files
DOT := $(SRC:.v=.dot)

# icestorm tools
YOSYS := yosys
YOSYS_CONFIG := yosys-config
ICEPACK := icepack
ICETIME := icetime
ICEPROG := iceprog		# WSL Users: I read Windows "iceprog.exe" works from WSL (otherwise no USB)

# Yosys synthesis arguments
YOSYS_SYNTH_ARGS := -dsp -top $(TOP)

# Verilog preprocessor definitions common to all modules
#       
# NOTE:	CLKGPIO35 - use gpio_35 for external clock source (12Mhz with PLL or pixel clock)
#       Add -DCLKGPIO35 for Upduino V2.x (or V3.x without OSC jumper shorted)
#       (Also route J8/12Mhz signal or external clock to gpio_35 pin for clock input)
# NOTE: NOPLL    - assume external clock is proper pixel clock frequency (don't use PLL)
#	Add -DNOPLL to -DCLKGPIO35 if external clock needs no PLL (e.g. 25Mhz)
# NOTE: SPI_DEBUG - outputs FTDI SPI signals and some SPI module signals to GPIO
#	pins to allow use of logic analyzer (only Upduino 3.x)
#       
DEFINES := -DFONT_MEM=$(FONTMEM) -D$(VIDEO_MODE) -DICE40UP5K -DUPDUINO #-DNOPLL -DCLKGPIO35 #-DSPI_DEBUG

# Verilator tool (used for "lint")
VERILATOR := verilator
VERILATOR_ARGS := --relative-includes -Wall -Wno-fatal -Wno-DECLFILENAME
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# nextPNR tools
NEXTPNR := nextpnr-ice40
NEXTPNR_ARGS := --pre-pack $(SDC) --placer heap

# defult target is make bitstream
all: $(TOP).bin Makefile
	@echo === Upduino Xosera configured for: $(VIDEO_MODE) $(findstring CLKGPIO35,$(DEFINES)) $(findstring NOPLL,$(DEFINES)) ===
	@echo     Type \"make prog\" to program the Upduino via USB

# program Upduino FPGA via USB (may need udev rules or sudo on Linux)
prog: $(TOP).bin Makefile
	@echo === Upduino Xosera configured for: $(VIDEO_MODE) $(findstring CLKGPIO35,$(DEFINES)) $(findstring NOPLL,$(DEFINES)) ===
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
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC)

# run Yosys to generate a "dot" graphical representation of each design file
show: $(DOT)

$(DOT): %.dot: %.v
	mkdir -p dot
	$(YOSYS) -p 'verilog_defines $(DEFINES) -DSHOW ; read_verilog $< ; show -enum -stretch -signed -width -prefix dot/$(basename $(notdir $<)) $(basename $(notdir $<))'

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
	@echo === Upduino Xosera : $(VIDEO_MODE) $(findstring CLKGPIO35,$(DEFINES)) $(findstring NOPLL,$(DEFINES)) | tee $(TOP)_stats.txt	
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
.PHONY: all prog timing lint show clean
