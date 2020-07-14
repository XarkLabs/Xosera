# Makefile - Xosera master Makefile
# vim: set noet ts=8 sw=8
#

# Build all project targets
all: sim icebreaker upduino host_spi

# Build Xosera Verilator native simulation target
sim:
	cd sim && make

# Build Xosera iCEBreaker FPGA bitstream
icebreaker:
	cd icebreaker && make

# Build Xosera Upduino 3.x FPGA bitstream
upduino:
	cd upduino && make

# Build host SPI command utility
host_spi:
	cd host_spi && make

# Clean all project targets
clean:
	cd sim && make clean
	cd icebreaker && make clean
	cd upduino && make clean
	cd host_spi && make clean

.PHONY: all sim icebreaker upduino host_spi
