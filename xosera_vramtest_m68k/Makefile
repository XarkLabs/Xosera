# Make Xosera test program for rosco_m68k
#
# vim: set noet ts=8 sw=8
# Copyright (c) 2021 Xark
# MIT LICENSE

# location of xosera_m68k_extra
XOSERA_EXTRA?=../xosera_m68k_extra

# add common rosco support
EXTRA_CFLAGS+=-I$(XOSERA_EXTRA)
CSOURCES+=$(XOSERA_EXTRA)/rosco_m68k_support.c

all: $(BINARY) $(DISASM)

# use common make rules for m68k-elf-rosco + Xosera build
include $(XOSERA_EXTRA)/common_xosera.mk
