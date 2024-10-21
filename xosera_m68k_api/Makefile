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
# LTO is unwanted when making a library (strips things we may want later)
LTO=

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_EXTRA)/common_xosera.mk

CINCLUDES=rosco_m68k/xosera.h rosco_m68k/xosera_defs.h rosco_m68k/xosera_defs.inc
LIBRARY=rosco_m68k/lib$(PROGRAM_BASENAME).a
LINKOBJECTS=$(filter-out api_test.o,$(OBJECTS))

all: prereq $(COPASM) $(LIBRARY) $(BINARY) $(DISASM)

prereq:
	@echo === Building xosera-copasm COPPER assembler...
	cd ../copper/CopAsm/ && $(MAKE)

$(LIBRARY): $(LINKOBJECTS) $(CINCLUDES) $(MAKEFILE_LIST)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
	@if \
	 cmp -s rosco_m68k/xosera.h $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera.h ; \
	then \
	 echo "rosco_m68k/xosera.h unchanged" ; \
	else \
	 echo "rosco_m68k/xosera.h modified" ; \
	fi
	@if \
	 cmp -s rosco_m68k/xosera_defs.h $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera_defs.h ; \
	then \
	 echo "rosco_m68k/xosera_defs.h unchanged" ; \
	else \
	 echo "rosco_m68k/xosera_defs.h modified" ; \
	fi
	@if \
	 cmp -s rosco_m68k/xosera_defs.inc $(ROSCO_M68K_INCLUDES)/rosco_m68k/xosera_defs.inc ; \
	then \
	 echo "rosco_m68k/xosera_defs.inc unchanged" ; \
	else \
	 echo "rosco_m68k/xosera_defs.inc modified" ; \
	fi
	@if \
	 cmp -s -i 0x160 $@ $(ROSCO_M68K_LIBRARIES)/$(LIBRARY) ; \
	then \
	 echo "$(LIBRARY) unchanged" ; \
	else \
	 echo "$(LIBRARY) modified" ; \
	fi
	@echo Update \(overwrite\) toolchain copies [y/N]?
	@read ans && if [ $${ans:-'N'} = 'y' ] ; then \
	 sudo cp -v rosco_m68k/xosera.h $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	 sudo cp -v rosco_m68k/xosera_defs.h $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	 sudo cp -v rosco_m68k/xosera_defs.inc $(ROSCO_M68K_INCLUDES)/rosco_m68k ; \
	 sudo cp -v $(LIBRARY) $(ROSCO_M68K_LIBRARIES) ; \
	fi
