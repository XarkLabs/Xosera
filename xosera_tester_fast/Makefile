# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

# NOTE: This Make "hack" uses the "old" IDE (I used 1.8.13)
ARDUINO_PATH ?= /Applications/Arduino.app/Contents/MacOS/
AVR_SERIAL ?= /dev/tty.usbserial-AJ5OKW84

# NOTE: To exit "screen" (almost worse than vim...)
#       ^A : quit

all: build

build:
	mkdir -p temp
	cat xosera_tester_arduino.cpp xosera_tests.cpp > temp/xosera_tester_fast.ino
	arch -x86_64 /bin/bash -c "$(ARDUINO_PATH)/Arduino --verify ./temp/xosera_tester_fast.ino"

upload:
	arch -x86_64 /bin/bash -c "$(ARDUINO_PATH)/Arduino --upload ./temp/xosera_tester_fast.ino"

test: upload
	screen $(AVR_SERIAL) 115200

term:
	screen $(AVR_SERIAL) 115200

clean:
	rm -rf temp

.PHONY: all prog test term clean
