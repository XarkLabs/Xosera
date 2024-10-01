# Make Xosera Boing program for rosco_m68k
#
# vim: set noet ts=8 sw=8
# Copyright (c) 2022 Thomas Jager
# MIT LICENSE

# location of xosera_m68k_extra
XOSERA_EXTRA?=../xosera_m68k_extra

EXTRA_LIBS+=-lm
EXTRA_CFLAGS+=-Os

all: $(BINARY) $(DISASM)

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_EXTRA)/common_xosera.mk
