# Make rosco_pt_player MOD player
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

ifndef XOSERA_EXTRA
$(info Please set XOSERA_EXTRA to the xosera_m68k_extra directory of Xosera repository, e.g. ~/xosera/xosera_m68k_extra)
endif

EXTRA_CFLAGS+=-DPLAY_SAMPLE -I../rosco_pt_lib
EXTRA_LIBS+=../rosco_pt_lib/pt_mod.o

# add common rosco support
EXTRA_CFLAGS+=-I$(XOSERA_EXTRA)
CSOURCES+=$(XOSERA_EXTRA)/rosco_m68k_support.c

CSOURCES+=xenon.mod.raw

all: $(BINARY) $(DISASM)

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_EXTRA)/common_xosera.mk
