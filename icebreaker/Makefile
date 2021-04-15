# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

# Using icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution for
# up5k_vga by E. Brombaugh (emeb) and further extensively
# hacked by Xark for Xosera purposes

# Tool versions used:
#	Yosys 74e93e083ff23f3381fe2409e5847f9843840b17 (open-tool-forge build) (git sha1 bb6d04d, gcc 9.3.0-10ubuntu2 -Os)
#	nextpnr-ice40 -- Next Generation Place and Route (Version nightly-20200609)
#	Verilator 4.028 2020-02-06 rev v4.026-92-g890cecc1
#	Built on GNU/Linux using Ubuntu 20.04 distribution

# Version bookkeeping
GITSHORTHASH := $(shell git rev-parse --short HEAD)
DIRTYFILES := $(shell git status --porcelain --untracked-files=no | grep -v _stats.txt | cut -d " " -f 3-)
ifeq ($(strip $(DIRTYFILES)),)
# prepend 0 for "clean"
XOSERA_HASH := 0$(GITSHORTHASH)
$(info === Xosera iCEBreaker [$(XOSERA_HASH)] is CLEAN from git)
else
# prepend d for "dirty"
XOSERA_HASH := d$(GITSHORTHASH)
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
VIDEO_MODE := MODE_848x480

# Xosera video output selection:
# Supported video outputs:
#   PMOD_1B2_DVI12		12-bit DVI, PMOD 1A&1B	https://1bitsquared.com/products/pmod-digital-video-interface
#   PMOD_DIGILENT_VGA		12-bit VGA, PMOD 1A&1B	https://store.digilentinc.com/pmod-vga-video-graphics-array/
#   PMOD_XESS_VGA		 9-bit VGA, PMOD 1A&1B	http://www.xess.com/shop/product/stickit-vga/
#   PMOD_XESS_VGA_SINGLE	 6-bit VGA, PMOD 1B	http://www.xess.com/shop/product/stickit-vga/ (half used)
#
VIDEO_OUTPUT := PMOD_1B2_DVI12

# Xosera project setup for iCEBreaker FPGA target
TOP := xosera_iceb
SDC := ../rtl/timing/$(VIDEO_MODE).py
PIN_DEF := icebreaker.pcf
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
YOSYS_SYNTH_ARGS := -dsp -relut -top $(TOP)

# Verilog preprocessor definitions common to all modules
DEFINES := -DGITHASH=$(XOSERA_HASH) -DFONT_MEM=$(FONTMEM) -D$(VIDEO_MODE) -D$(VIDEO_OUTPUT) -DICE40UP5K -DICEBREAKER -DSPI_INTERFACE

# Verilator tool (used for "lint")
VERILATOR := verilator
VERILATOR_ARGS := --relative-includes -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSED
TECH_LIB := $(shell $(YOSYS_CONFIG) --datdir/ice40/cells_sim.v)

# nextPNR tools
NEXTPNR := nextpnr-ice40
NEXTPNR_ARGS := --pre-pack $(SDC) --placer heap

# defult target is make bitstream
all: $(TOP).bin Makefile
	@echo === iCEBreaker Xosera: $(VIDEO_OUTPUT) $(VIDEO_MODE) $(SPI_INTERFACE) ===
	@echo     Type \"make prog\" to program the iCEBreaker via USB

# program iCEBreaker FPGA via USB (may need udev rules or sudo on Linux)
prog: $(TOP).bin Makefile
	@echo === iCEBreaker Xosera: $(VIDEO_OUTPUT) $(VIDEO_MODE) $(SPI_INTERFACE) ===
	$(ICEPROG) -d i:0x0403:0x6010 $<

# run icetime to generate a timing report
timing: $(TOP).rpt Makefile
	@echo iCETime timing report: $(TOP).rpt

# run Yosys with "noflatten", which will produce a resource count per module
count: $(SRC) $(INC) $(FONTMEM) Makefile
	@mkdir -p logs
	$(YOSYS) -l logs/$(TOP)_yosys_count.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -noflatten'

# run Verilator to check for Verilog issues
lint: $(SRC) $(INC) $(FONTMEM) Makefile
	$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC)

# run Yosys to generate a "dot" graphical representation of each design file
show: $(DOT)

$(DOT): %.dot: %.sv
	mkdir -p dot
	$(YOSYS) -l logs/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) -DSHOW ; read_verilog -sv $< ; show -enum -stretch -signed -width -prefix dot/$(basename $(notdir $<)) $(basename $(notdir $<))'

# delete all targets that will be re-generated
clean:
	rm -f *.json *.asc *.rpt *.bin

# synthesize Verilog and create json description
%.json: $(SRC) $(INC) $(MEM) Makefile
	@rm -f $@
	@mkdir -p logs
	-$(VERILATOR) $(VERILATOR_ARGS) --lint-only $(DEFINES) --top-module $(TOP) $(TECH_LIB) $(SRC) 2>&1 | tee logs/$(TOP)_verilator.log
	$(YOSYS) -l logs/$(TOP)_yosys.log -w ".*" -q -p 'verilog_defines $(DEFINES) ; read_verilog -sv $(SRC) ; synth_ice40 $(YOSYS_SYNTH_ARGS) -json $@'

# make ASCII bitstream from JSON description and device parameters
%.asc: %.json $(PIN_DEF) $(SDC) $(MEM) Makefile
	@rm -f $@
	@mkdir -p logs
	@-cp $(TOP)_stats.txt logs/$(TOP)_stats_last.txt
	$(NEXTPNR) -l logs/$(TOP)_nextpnr.log -q $(NEXTPNR_ARGS) --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@
	@echo === iCEBreaker Xosera: $(VIDEO_OUTPUT) $(VIDEO_MODE) $(SPI_INTERFACE) | tee $(TOP)_stats.txt
	@$(YOSYS) -V 2>&1 | tee -a $(TOP)_stats.txt
	@$(NEXTPNR) -V 2>&1 | tee -a $(TOP)_stats.txt
	@sed -n '/Device utilisation/,/Info: Placed/p' logs/$(TOP)_nextpnr.log | sed '$$d' | grep -v ":     0/" | tee -a $(TOP)_stats.txt
	@grep "Max frequency" logs/$(TOP)_nextpnr.log | tail -1 | tee -a $(TOP)_stats.txt
	@-diff -U0 logs/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+\|^---\)" | while read line; do echo "PREVIOUS: $$line" ; done >logs/$(TOP)_stats_delta.txt
	@-diff -U0 logs/$(TOP)_stats_last.txt $(TOP)_stats.txt | grep -v "\(^@\|^+++\|^-\)" | while read line; do echo "CURRENT : $$line" ; done >>logs/$(TOP)_stats_delta.txt
	@echo
	@-cat logs/$(TOP)_stats_delta.txt

# make binary bitstream from ASCII bitstream
%.bin: %.asc Makefile
	@rm -f $@
	$(ICEPACK) $< $@.temp
	$(ICEMULTI) -p0 $@.temp -o $@
	@rm -f $@.temp

# make timing report from ASCII bitstream
%.rpt: %.asc Makefile
	@rm -f $@
	$(ICETIME) -d $(DEVICE) -m -t -r $@ $<

# prevent make from deleting any intermediate files
.SECONDARY:

# inform make about "phony" convenience targets
.PHONY: all prog timing count lint show clean
