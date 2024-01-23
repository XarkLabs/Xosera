# Make Xosera test program for rosco_m68k
#
# vim: set noet ts=8 sw=8
# Copyright (c) 2021 Xark
# MIT LICENSE

# Makefile "best practices" from https://tech.davis-hansson.com/p/make/ (but not forcing gmake)
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.ONESHELL:
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

# check for rosco_m68k toolchain
ifeq (,$(shell m68k-elf-gcc --version))
$(info No m68k-elf-* build tools found in path)
else
# check for rosco_m68k build dir
ifndef ROSCO_M68K_DIR
$(info Please set ROSCO_M68K_DIR to the top-level rosco_m68k directory to use for rosco_m68k building, e.g. ~/rosco_m68k)
endif
endif

# include special user rosco_m68k build riles
-include $(ROSCO_M68K_DIR)/user.mk

# NOTE: Uses "private" xosera_m68k_api.h (to be self-contained in firmware)
XOSERA_M68K_API_ORGIN?=../xosera_m68k_api
XOSERA_M68K_API?=videoXoseraANSI
XOSERA_M68K_XANSI?=videoXoseraANSI

EXTRA_CFLAGS?=-g -O2 -fomit-frame-pointer -Wno-variadic-macros -DTEST_FIRMWARE
EXTRA_LDFLAGS?=
EXTRA_LIBS?=
EXTRA_VASMFLAGS?=-showopt -I$(XOSERA_M68K_XANSI) -I$(ROSCO_M68K_DIR)/code/shared -DTEST_FIRMWARE

EXTRA_CFLAGS+=-I$(XOSERA_M68K_XANSI)
# EXTRA_LDFLAGS+=-L$(XOSERA_M68K_API)
# EXTRA_LIBS+=-lxosera_m68k_api

CPU?=68010
ARCH?=$(CPU)
TUNE?=$(CPU)

SYSINCDIR?=$(ROSCO_M68K_DIR)/code/software/libs/build/include
SYSLIBDIR?=$(ROSCO_M68K_DIR)/code/software/libs/build/lib

ifeq (,$(wildcard $(SYSLIBDIR)/librosco_m68k.a))
$(info === Please build the rosco_m68k libraries using:    cd $(ROSCO_M68K_DIR)/code/software/libs && make clean all install && cd ~-)
$(error Need rosco_m68k libraries)
endif

DEFINES=-DROSCO_M68K
FLAGS=-ffreestanding -ffunction-sections -fdata-sections -fomit-frame-pointer	\
      -Wall -Wextra -Werror -Wno-unused-function -pedantic -I$(SYSINCDIR)			\
      -mcpu=$(CPU) -march=$(ARCH) -mtune=$(TUNE) -g -O2 $(DEFINES)
CFLAGS=-std=c11 $(FLAGS)
# EVIL    	CFLAGS += -Wall -Wextra -Wpedantic -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2 -Wformat-security -Wnull-dereference -Wstack-protector -Wtrampolines -Walloca -Wvla -Warray-bounds=2 -Wimplicit-fallthrough=3 -Wshift-overflow=2 -Wcast-qual -Wstringop-overflow=4 -Wconversion -Warith-conversion -Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wformat-signedness -Wshadow -Wstrict-overflow=4 -Wswitch-default -Wswitch-enum -Wstack-usage=1000000 -Wcast-align=strict
# Too-EVIL 	-Wundef -Wstrict-prototypes  -Wtraditional-conversion
CXXFLAGS=-std=c++20 -fno-exceptions -fno-rtti $(FLAGS)
GCC_LIBS?=$(shell $(CC) --print-search-dirs \
          | grep libraries:\ = \
          | sed 's/libraries: =/-L/g' \
          | sed 's/:/m68000\/ -L/g')m68000/
LIBS=$(EXTRA_LIBS) -lrosco_m68k -lgcc
ASFLAGS=-mcpu=$(CPU) -march=$(ARCH)

ROSCO_M68K_HUGEROM?=
ifneq ($(ROSCO_M68K_HUGEROM),false)
LDSCRIPT?=$(SYSLIBDIR)/ld/serial/hugerom_rosco_m68k_program.ld
else
LDSCRIPT?=$(SYSLIBDIR)/ld/serial/rosco_m68k_program.ld
endif


VASMFLAGS=-Felf -m$(CPU) -quiet -Lnf $(DEFINES)
LDFLAGS=-T $(LDSCRIPT) -L $(SYSLIBDIR) -Map=$(MAP) --gc-sections --oformat=elf32-m68k $(EXTRA_LDFLAGS)

CC=m68k-elf-gcc
CXX=m68k-elf-g++
AS=m68k-elf-as
LD=m68k-elf-ld
AR=m68k-elf-ar
RANLIB=m68k-elf-ranlib
NM=m68k-elf-nm
LD=m68k-elf-ld
OBJDUMP=m68k-elf-objdump
OBJCOPY=m68k-elf-objcopy
SIZE=m68k-elf-size
VASM=vasmm68k_mot
CHMOD=chmod
MKDIR=mkdir
LSOF=lsof
RM=rm -f
CP=cp
KERMIT=kermit
SERIAL?=/dev/modem
BAUD?=115200
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
PROGRAM_BASENAME=$(shell basename $(CURDIR))

# Set other output files using output basname
ELF=$(PROGRAM_BASENAME).elf
BINARY=$(PROGRAM_BASENAME).bin
DISASM=$(PROGRAM_BASENAME).dis
MAP=$(PROGRAM_BASENAME).map
SYM=$(PROGRAM_BASENAME).sym
SYM_SIZE=$(PROGRAM_BASENAME)_size.sym

# Assume source files in Makefile directory are source files for project
CSOURCES=$(wildcard *.c)
CXXSOURCES=$(wildcard *.cpp)
CINCLUDES=$(wildcard *.h)
SSOURCES=$(wildcard *.S)
ASMSOURCES=$(wildcard *.asm)

# Additional build files for Xosera xansi
CSOURCES+=$(wildcard $(XOSERA_M68K_XANSI)/*.c)
CINCLUDES+=$(wildcard $(XOSERA_M68K_XANSI)/*.h)
ASMSOURCES+=$(wildcard $(XOSERA_M68K_XANSI)/*.asm)

SOURCES=$(CSOURCES) $(CXXSOURCES) $(SSOURCES) $(ASMSOURCES)

# Assume each source files makes an object file
OBJECTS=$(addsuffix .o,$(basename $(SOURCES)))

TO_CLEAN=$(OBJECTS) $(ELF) $(BINARY) $(MAP) $(SYM) $(SYM_SIZE) $(DISASM) $(addsuffix .lst,$(basename $(SSOURCES) $(ASMSOURCES)))

all: update $(BINARY) $(DISASM)

update:
	@echo Updating XANSI private xosera_m68k_api to be self-contained for firmware
	@cp -av $(XOSERA_M68K_API_ORGIN)/xosera_m68k_api.c videoXoseraANSI/xosera_m68k_api.c
	@cp -av $(XOSERA_M68K_API_ORGIN)/xosera_m68k_api.h videoXoseraANSI/xosera_m68k_api.h
	@cp -av $(XOSERA_M68K_API_ORGIN)/xosera_m68k_defs.h videoXoseraANSI/xosera_m68k_defs.h
	@cp -av $(XOSERA_M68K_API_ORGIN)/xosera_m68k_defs.inc videoXoseraANSI/xosera_m68k_defs.inc

$(ELF) : $(OBJECTS)
	$(LD) $(LDFLAGS) $(GCC_LIBS) $^ $(LIBS) -o $@
	$(NM) --numeric-sort $@ >$(SYM)
	$(NM) --size-sort $@ >$(SYM_SIZE)
	$(SIZE) $@
	-$(CHMOD) a-x $@

$(BINARY) : $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BINARY)

$(DISASM) : $(ELF)
	$(OBJDUMP) --disassemble -S $(ELF) >$(DISASM)

$(OBJECTS): $(MAKEFILE_LIST)

%.o : %.c $(CINCLUDES)
	@$(MKDIR) -p $(@D)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

%.o : %.cpp
	@$(MKDIR) -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(EXTRA_CXXFLAGS) -o $@ $<

%.o : %.asm
	@$(MKDIR) -p $(@D)
	$(VASM) $(VASMFLAGS) $(EXTRA_VASMFLAGS) -L $(basename $@).lst -o $@ $<

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
