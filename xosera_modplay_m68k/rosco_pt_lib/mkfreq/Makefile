# Make rosco_pt_lib frequency tables
#
# Copyright (c) 2021 Ross Bamford
# MIT LICENSE (See LICENSE file)
#
# vim: set noet ts=8 sw=8

all: xosera_freq.h

mkfreq: mkfreq.c ../pt_mod.h
	$(CC) -o mkfreq mkfreq.c

xosera_freq.h: mkfreq
	./mkfreq

.PHONY clean:
	rm -rf mkfreq xosera_freq.h
