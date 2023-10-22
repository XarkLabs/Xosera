# Make rosco_pt_player MOD player
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

all:
	cd rosco_pt_lib && make all
	cd rosco_pt_player && make all
	cp -v rosco_pt_player/rosco_pt_player.* .

clean:
	cd rosco_pt_lib && make clean
	cd rosco_pt_player && make clean

linuxtest: all
	cd rosco_pt_player && make linuxtest

mactest: all
	cd rosco_pt_player && make mactest

.PHONY:	all clean
