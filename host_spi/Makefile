# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8

host_spi: host_spi.cpp ftdi_spi.cpp ftdi_spi.h
	g++ host_spi.cpp ftdi_spi.cpp -o host_spi -lftdi

clean:
	rm -f host_spi
