# Make Xosera m68k API
# vim: set noet ts=8 sw=8
# Copyright (c) 2022 Xark
# MIT LICENSE

# check for rosco_m68k toolchain
ifeq (,$(shell m68k-elf-gcc --version))
$(info No m68k-elf-* build tools found in path)
else
# check for rosco_m68k build dir
ifndef ROSCO_M68K_DIR
$(info Please set ROSCO_M68K_DIR to the top-level rosco_m68k directory to use for rosco_m68k building, e.g. ~/rosco_m68k)
endif
endif

BUILDING_XOSERA_API=true

# use generic common make rules for Xosera + rosco_m68k build
include common_xosera_m68k.mk

$(info Using ROSCO_M68K_DIR=$(ROSCO_M68K_DIR))
$(info Using XOSERA_M68K_API=$(XOSERA_M68K_API))

LIBRARY=lib$(PROGRAM_BASENAME).a
LINKOBJECTS=$(filter-out api_test.o,$(OBJECTS))

all: prereq $(COPASM) $(LIBRARY) $(BINARY) $(DISASM)

prereq:
	rm -f $(COPASM)
	cd ../copper/CopAsm/ && $(MAKE) clean

$(COPASM): ../copper/CopAsm/bin/copasm
	@mkdir -p bin
	cp -v ../copper/CopAsm/bin/copasm $(COPASM)
	@echo === Installed CopAsm to "$(COPASM)"

../copper/CopAsm/bin/copasm:
	@echo === Building CopASm COPPER assembler...
	cd ../copper/CopAsm/ && $(MAKE)

$(LIBRARY) : $(LINKOBJECTS)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

