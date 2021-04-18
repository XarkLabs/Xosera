# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

AVR_SERIAL ?= /dev/tty.usbserial-AJ5OKW84

all: xosera_tester_fast.ino
	/Applications/Arduino.app/Contents/MacOS/Arduino --verify ./xosera_tester_fast.ino

prog: xosera_tester_fast.ino
	/Applications/Arduino.app/Contents/MacOS/Arduino --upload ./xosera_tester_fast.ino

test: prog
	-killall screen && sleep 5
	echo "/usr/bin/screen $(AVR_SERIAL) 115200" > $(TMPDIR)/rosco_screen.sh
	chmod +x $(TMPDIR)/rosco_screen.sh
	sleep 2
	open -b com.apple.terminal $(TMPDIR)/rosco_screen.sh

clean:
	echo clean
