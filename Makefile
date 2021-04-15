# Makefile - Xosera master Makefile
# vim: set noet ts=8 sw=8
#

# Build all project targets
all: rtl utils host_spi

iceb:
	cd rtl && $(MAKE) iceb

upd:
	cd rtl && $(MAKE) upd

# Build Xosera Verilator native simulation target
rtl:
	cd rtl && $(MAKE) all

# Build all project targets
sim:
	cd rtl && $(MAKE) sim

# Build all project targets
isim:
	cd rtl && $(MAKE) isim

# Build all project targets
irun:
	cd rtl && $(MAKE) irun

# Build all project targets
vsim:
	cd rtl && $(MAKE) vsim

# Build all project targets
vrun:
	cd rtl && $(MAKE) vrun

# Build image/font mem utility
utils:
	cd utils && $(MAKE)

# Build host SPI command utility
host_spi:
	cd host_spi && $(MAKE)

# Clean all project targets
clean:
	cd rtl && $(MAKE) clean
	cd utils && $(MAKE) clean
	cd host_spi && $(MAKE) clean

.PHONY: all rtl sim utils host_spi clean
