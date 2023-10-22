# Make rosco_pt_player MOD player library
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

ifndef ROSCO_M68K_DIR
$(error Please set ROSCO_M68K_DIR to the top-level rosco_m68k directory to use for rosco_m68k building)
endif

ifndef XOSERA_M68K_API
$(error Please set XOSERA_M68K_API to the Xosera API directory E.g., Xosera/xosera_m68k_api)
endif

EXTRA_CFLAGS=-DPLAY_SAMPLE

# use generic common make rules for Xosera + rosco_m68k build
include $(XOSERA_M68K_API)/common_xosera_m68k.mk
