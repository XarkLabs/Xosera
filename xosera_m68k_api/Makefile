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
$(info Please set ROSCO_M68K_DIR to the rosco_m68k root directory to use for rosco_m68k building)
endif
endif

# location of xosera_m68k_api
XOSERA_M68K_API?=../xosera_m68k_api

BUILDING_XOSERA_API=true

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_M68K_API)/common_xosera_m68k.mk

LIBRARY=lib$(PROGRAM_BASENAME).a
LINKOBJECTS=$(filter-out api_test.o,$(OBJECTS))

all: prereq $(COPASM) $(XOSERA_M68K_API)/$(LIBRARY) $(BINARY) $(DISASM)

prereq:
	rm -f $(COPASM)
	cd $(XOSERA_M68K_API)/../copper/CopAsm/ && $(MAKE) clean

$(COPASM): $(XOSERA_M68K_API)/../copper/CopAsm/bin/copasm
	@mkdir -p $(XOSERA_M68K_API)/bin
	cp -v $(XOSERA_M68K_API)/../copper/CopAsm/bin/copasm $(COPASM)
	@echo === Installed CopAsm to "$(COPASM)"

$(XOSERA_M68K_API)/../copper/CopAsm/bin/copasm:
	@echo === Building CopASm COPPER assembler...
	cd $(XOSERA_M68K_API)/../copper/CopAsm/ && $(MAKE)

$(XOSERA_M68K_API)/$(LIBRARY) : $(LINKOBJECTS)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

