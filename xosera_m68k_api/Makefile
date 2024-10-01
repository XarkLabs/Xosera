# Make Xosera m68k API
# vim: set noet ts=8 sw=8
# Copyright (c) 2022 Xark
# MIT LICENSE

# check for rosco_m68k toolchain
ifeq (,$(shell m68k-elf-rosco-gcc --version))
$(info No m68k-elf-rosco-* rosco_m68k build tools found in path)
endif

# location of xosera_m68k_extra
XOSERA_EXTRA?=../xosera_m68k_extra

PROGRAM_BASENAME=xosera
BUILDING_XOSERA_API=true

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_EXTRA)/common_xosera.mk

LIBRARY=lib$(PROGRAM_BASENAME).a
LINKOBJECTS=$(filter-out api_test.o,$(OBJECTS))

all: prereq $(COPASM) $(LIBRARY) $(BINARY) $(DISASM)

prereq:
	rm -f $(COPASM)
	cd ../copper/CopAsm/ && $(MAKE) clean

$(COPASM): ../copper/CopAsm/bin/xosera-copasm
	@mkdir -p bin
	cp -v ../copper/CopAsm/bin/xosera-copasm $(COPASM)
	@echo === Installed xosera-copasm to "$(COPASM)"

../copper/CopAsm/bin/xosera-copasm:
	@echo === Building xosera-copasm COPPER assembler...
	cd ../copper/CopAsm/ && $(MAKE)

$(LIBRARY) : $(LINKOBJECTS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
	@echo rosco_m68k toolchain includes: $(ROSCO_M68K_INCLUDES)/rosco_m68k
	@if \
	 cmp -s xosera.h $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera.h ; \
	then \
	 echo "xosera.h unchanged" ; \
	else \
	 echo "xosera.h modified" ; \
	 sudo cp -v xosera.h $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	fi
	@if \
	 cmp -s xosera_defs.h $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera_defs.h ; \
	then \
	 echo "xosera_defs.h unchanged" ; \
	else \
	 echo "xosera_defs.h modified" ; \
	 sudo cp -v xosera_defs.h $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	fi
	@if \
	 cmp -s xosera_defs.inc $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera_defs.inc ; \
	then \
	 echo "xosera_defs.inc unchanged" ; \
	else \
	 echo "xosera_defs.inc modified" ; \
	 sudo cp -v xosera_defs.inc $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	fi
	@echo rosco_m68k toolchain libraries: $(ROSCO_M68K_LIBRARIES)
	@if \
	 cmp -s -i 0x160 $@ $(ROSCO_M68K_LIBRARIES)/$@ ; \
	then \
	 echo "$@ unchanged" ; \
	else \
	 echo "$@ modified" ; \
	 sudo cp -v $@ $(ROSCO_M68K_LIBRARIES) ; \
	fi
