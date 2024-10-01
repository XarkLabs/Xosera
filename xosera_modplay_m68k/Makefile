# Make rosco_pt_player MOD player
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

XOSERA_EXTRA:=../../xosera_m68k_extra

all:
	cd rosco_pt_lib && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) all
	cd rosco_pt_player && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) all
	cp -v rosco_pt_player/rosco_pt_player.* .

clean:
	cd rosco_pt_lib && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) clean
	cd rosco_pt_player && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) clean

linuxtest: all
	cd rosco_pt_player && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) linuxtest

mactest: all
	cd rosco_pt_player && XOSERA_EXTRA=$(XOSERA_EXTRA) $(MAKE) mactest

.PHONY:	all clean
