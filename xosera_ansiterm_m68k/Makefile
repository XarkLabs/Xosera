# Make Xosera test program for rosco_m68k Xosera API
#
# vim: set noet ts=8 sw=8
# Copyright (c) 2023 Xark
# MIT LICENSE

# Makefile "best practices" from https://tech.davis-hansson.com/p/make/ (but not forcing gmake)
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.ONESHELL:
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

# check for rosco_m68k toolchain
ifeq (,$(shell m68k-elf-rosco-gcc --version))
$(info No m68k-elf-rosco-* rosco_m68k build tools found in path)
endif

# NOTE: Uses "private" xosera_m68k_api.h (to be self-contained in firmware)
XOSERA_M68K_API_ORGIN?=../xosera_m68k_api
XOSERA_M68K_XANSI?=videoXoseraANSI

# location of xosera_m68k_extra
XOSERA_EXTRA?=../xosera_m68k_extra

# add common rosco support
EXTRA_CFLAGS+=-I$(XOSERA_EXTRA)
CSOURCES+=$(XOSERA_EXTRA)/rosco_m68k_support.c

DEFINES?=-DTEST_FIRMWARE
FLAGS=-ffunction-sections -fdata-sections -fomit-frame-pointer						\
      -Wall -Wextra -Werror -Wno-unused-function -pedantic							\
      -g -O2 $(DEFINES)

CFLAGS=-std=c2x -Wno-format $(FLAGS)
CXXFLAGS=-std=c++20 -fno-exceptions -fno-rtti $(FLAGS)
EXTRA_CFLAGS?=-I$(XOSERA_M68K_XANSI)
VASMFLAGS=-Felf -m$(CPU) -quiet -Lnf -I$(ROSCO_M68K_INCLUDES) $(DEFINES)
EXTRA_VASMFLAGS?=-showopt -I$(XOSERA_M68K_XANSI) -I$(ROSCO_M68K_INCLUDES)
LDFLAGS=-Wl,--gc-sections -Wl,-Map=$(MAP)
EXTRA_LDFLAGS?=
ARFLAGS?=rDcs

EXTRA_LIBS?=
LIBS=$(EXTRA_LIBS)

CC=m68k-elf-rosco-gcc
CXX=m68k-elf-rosco-g++
LD=m68k-elf-rosco-gcc
AR=m68k-elf-rosco-ar
RANLIB=m68k-elf-rosco-ranlib
NM=m68k-elf-rosco-nm
OBJDUMP=m68k-elf-rosco-objdump
OBJCOPY=m68k-elf-rosco-objcopy
SIZE=m68k-elf-rosco-size
VASM=vasmm68k_mot
CHMOD=chmod
MKDIR=mkdir
LSOF=lsof
RM=rm -f
CP=cp
KERMIT?=kermit
SERIAL?=/dev/setme_rosco_uart
BAUD?=115200

ifeq (,$(shell xosera-copasm -h >/dev/null 2>&1))
COPASM?=$(XOSERA_EXTRA)/../xosera_m68k_api/bin/xosera-copasm
else
COPASM?=xosera-copasm
endif

ROSCO_M68K_BIN?=$(strip $(shell $(CC) 2>&1 -v -x c -E /dev/null | grep "m68k-elf-rosco/include"))
ROSCO_M68K_INCLUDES?=$(strip $(shell $(CC) 2>&1 -v -x c -E /dev/null | grep "m68k-elf-rosco/include"))
ROSCO_M68K_LIBRARIES?=$(strip $(shell $(CC) -print-search-dirs | tr ':' '\n' | grep "../m68k-elf-rosco/lib/$$"))

# GCC-version-specific settings
ifneq ($(findstring GCC,$(shell $(CC) --version 2>/dev/null)),)
CC_VERSION:=$(shell $(CC) -dumpfullversion)
CC_MAJOR:=$(firstword $(subst ., ,$(CC_VERSION)))
# If this is GCC 12 or 13, add flag --param=min-pagesize=0 to CFLAGS
ifeq ($(CC_MAJOR),12)
CFLAGS+=--param=min-pagesize=0
endif
ifeq ($(CC_MAJOR),13)
CFLAGS+=--param=min-pagesize=0
endif
endif

CPU?=68010
# For systems without MMU support, aligning LOAD segments with pages is not needed
# In those cases, provide fake page sizes to both save space and remove RWX warnings
ifeq ($(CPU),68030)
LD_LD_SUPPORT_MMU?=true
endif
ifeq ($(CPU),68040)
LD_SUPPORT_MMU?=true
endif
ifeq ($(CPU),68060)
LD_SUPPORT_MMU?=true
endif
LD_SUPPORT_MMU?=false
ifneq ($(LD_SUPPORT_MMU),true)
# Saves space in binaries, but will break MMU use
LDFLAGS+=-z max-page-size=16 -z common-page-size=16
endif

# Output config (assume name of directory)
PROGRAM_BASENAME?=$(shell basename $(CURDIR))
$(info === Program: $(PROGRAM_BASENAME) in $(CURDIR))

# Set other output files using output basname
ELF=$(PROGRAM_BASENAME).elf
BINARY=$(PROGRAM_BASENAME).bin
DISASM=$(PROGRAM_BASENAME).dis
MAP=$(PROGRAM_BASENAME).map
SYM=$(PROGRAM_BASENAME).sym
SYM_SIZE=$(PROGRAM_BASENAME)_size.sym

# Assume source files in Makefile directory are source files for project
CSOURCES+=$(wildcard *.c)
CXXSOURCES+=$(wildcard *.cpp)
CINCLUDES+=$(wildcard *.h)
SSOURCES+=$(wildcard *.[sS])
ASMSOURCES+=$(wildcard *.asm)
RAWSOURCES+=$(wildcard *.raw)
CASMSOURCES+=$(wildcard *.casm)
CPASMSOURCES+=$(wildcard *.cpasm)
CASMOUTPUT=$(addsuffix .h,$(basename $(CASMSOURCES) $(CPASMSOURCES)))

# Additional build files for Xosera xansi
CSOURCES+=$(wildcard $(XOSERA_M68K_XANSI)/*.c)
CINCLUDES+=$(wildcard $(XOSERA_M68K_XANSI)/*.h)
ASMSOURCES+=$(wildcard $(XOSERA_M68K_XANSI)/*.asm)

SOURCES+=$(CSOURCES) $(CXXSOURCES) $(SSOURCES) $(ASMSOURCES) $(RAWSOURCES)
# Assume each source files makes an object file
OBJECTS+=$(addsuffix .o,$(basename $(SOURCES)))

TO_CLEAN+=$(OBJECTS) $(ELF) $(BINARY) $(MAP) $(SYM) $(SYM_SIZE) $(DISASM) $(addsuffix .lst,$(basename $(ASMSOURCES))) $(CASMOUTPUT) $(addsuffix .lst,$(basename $(CASMSOURCES))) $(addsuffix .casm.ii,$(basename $(CPASMSOURCES))) $(addsuffix .lst,$(basename $(SSOURCES)))

all: update $(BINARY) $(DISASM)

update:
	@echo Updating XANSI private xosera_m68k_api to be self-contained for firmware
	@cp -av $(XOSERA_M68K_API_ORGIN)/xosera_api.c videoXoseraANSI/xosera_api.c
	@mkdir -p videoXoseraANSI/rosco_m68k
	@cp -av $(XOSERA_M68K_API_ORGIN)/rosco_m68k/xosera.h videoXoseraANSI/rosco_m68k/xosera.h
	@cp -av $(XOSERA_M68K_API_ORGIN)/rosco_m68k/xosera_defs.h videoXoseraANSI/rosco_m68k/xosera_defs.h
	@cp -av $(XOSERA_M68K_API_ORGIN)/rosco_m68k/xosera_defs.inc videoXoseraANSI/rosco_m68k/xosera_defs.inc

$(ELF) : $(OBJECTS)
	$(LD) $(LDFLAGS) $(EXTRA_LDFLAGS) $^ $(LIBS) -o $@
	$(NM) --numeric-sort $@ >$(SYM)
	$(NM) --size-sort $@ >$(SYM_SIZE)
	$(SIZE) $@
	-$(CHMOD) a-x $@

$(BINARY) : $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BINARY)

$(DISASM) : $(ELF)
	$(OBJDUMP) --disassemble -S $(ELF) >$(DISASM)

$(OBJECTS): $(CASMOUTPUT) $(MAKEFILE_LIST)

%.o : %.c $(CINCLUDES)
	@$(MKDIR) -p $(@D)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

%.o : %.cpp
	@$(MKDIR) -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(EXTRA_CXXFLAGS) -o $@ $<

%.o : %.S $(CINCLUDES)
	@$(MKDIR) -p $(@D)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

%.o : %.s
	@$(MKDIR) -p $(@D)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

%.o : %.asm
	@$(MKDIR) -p $(@D)
	$(VASM) $(VASMFLAGS) $(EXTRA_VASMFLAGS) -L $(basename $@).lst -o $@ $<

# CopAsm copper source
%.h : %.casm
	@$(MKDIR) -p $(@D)
	$(COPASM) -v -l -i $(XOSERA_M68K_API) -o $@ $<

# preprocessed CopAsm copper source
%.h : %.cpasm
	@$(MKDIR) -p $(@D)
	$(CC) -E -xc -D__COPASM__=1 -I$(XOSERA_M68K_API) $< -o $(basename $<).casm.ii
	$(COPASM) -v -l -i $(XOSERA_M68K_API) -o $@ $(basename $<).casm.ii

# link raw binary file into executable (with symbols _binary_<name>_raw_start/*_end/*_size)
%.o: %.raw
	@$(MKDIR) -p $(@D)
	$(OBJCOPY) -I binary -O elf32-m68k -B m68k:$(CPU) $< $@

# remove targets that can be generated by this Makefile
clean:
	$(RM) $(TO_CLEAN)

disasm: $(DISASM)

# hexdump of program binary
dump: $(BINARY)
	hexdump -C $(BINARY)

# upload binary to rosco (if ready and kermit present)
load: $(BINARY)
	-$(LSOF) -t $(SERIAL) | (read oldscreen ; [ ! -z "$$oldscreen" ] && kill -3 $$oldscreen ; sleep 1)
	$(KERMIT) -i -l $(SERIAL) -b $(BAUD) -s $(BINARY)

# Linux (etc.) upload binary and connect with screen (free SERIAL port, kermit upload, open screen in shell window/tab)
linuxtest: $(BINARY) $(DISASM)
	-$(LSOF) -t $(SERIAL) | (read oldscreen ; [ ! -z "$$oldscreen" ] && kill -3 $$oldscreen ; sleep 1)
	$(KERMIT) -i -l $(SERIAL) -b $(BAUD) -s $(BINARY)
	gnome-terminal --geometry=106x30 --title="rosco_m68k $(SERIAL)" -- screen $(SERIAL) $(BAUD)

# Linux (etc.) connect with screen (free SERIAL port, opens screen in shell window/tab)
linuxterm: $(BINARY) $(DISASM)
	-$(LSOF) -t $(SERIAL) | (read oldscreen ; [ ! -z "$$oldscreen" ] && kill -3 $$oldscreen ; sleep 1)
	gnome-terminal --geometry=106x30 --title="rosco_m68k $(SERIAL)" -- screen $(SERIAL) $(BAUD)

# macOS upload binary and connect with screen (free SERIAL port, kermit upload, open screen in shell window/tab)
mactest: $(BINARY) $(DISASM)
	-$(LSOF) -t $(SERIAL) | (read oldscreen ; [ ! -z "$$oldscreen" ] && kill -3 $$oldscreen ; sleep 1)
	$(KERMIT) -i -l $(SERIAL) -b $(BAUD) -s $(BINARY)
	echo "#! /bin/sh" > $(TMPDIR)/rosco_screen.sh
	echo "/usr/bin/screen $(SERIAL) $(BAUD)" >> $(TMPDIR)/rosco_screen.sh
	-$(CHMOD) +x $(TMPDIR)/rosco_screen.sh
	sleep 1
	open -b com.apple.terminal $(TMPDIR)/rosco_screen.sh

# macOS connect with screen (free SERIAL port, open screen in shell window/tab)
macterm:
	-$(LSOF) -t $(SERIAL) | (read oldscreen ; [ ! -z "$$oldscreen" ] && kill -3 $$oldscreen ; sleep 1)
	echo "#! /bin/sh" > $(TMPDIR)/rosco_screen.sh
	echo "/usr/bin/screen $(SERIAL) $(BAUD)" >> $(TMPDIR)/rosco_screen.sh
	-$(CHMOD) +x $(TMPDIR)/rosco_screen.sh
	sleep 1
	open -b com.apple.terminal $(TMPDIR)/rosco_screen.sh

# Makefile magic (for "phony" targets that are not real files)
.PHONY: all update clean disasm dump load linuxtest linuxterm mactest macterm
