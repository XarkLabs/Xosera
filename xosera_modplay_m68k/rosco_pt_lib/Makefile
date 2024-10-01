# Make rosco_pt_player MOD player library
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

ifndef XOSERA_EXTRA
$(info Please set XOSERA_EXTRA to the xosera_m68k_extra directory of Xosera repository, e.g. ~/xosera/xosera_m68k_extra)
endif

DEFINES=-DPLAY_SAMPLE
# add common rosco support
EXTRA_CFLAGS+=-I$(XOSERA_EXTRA)
CSOURCES+=$(XOSERA_EXTRA)/rosco_m68k_support.c

all: $(BINARY) $(DISASM)

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_EXTRA)/common_xosera.mk
