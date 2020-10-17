# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CCFLAGS += -std=c++11 -Wall -Wextra -Os -I/usr/local/include/libftdi1
LDLIBS += -lftdi1
else
CCFLAGS += -std=c++11 -Wall -Wextra -Os -I/usr/local/include/libftdi1
LDLIBS += -lftdi
endif
 
CFLAGS += 

host_spi: host_spi.cpp ftdi_spi.cpp ftdi_spi.h Makefile
	$(CC) $(CCFLAGS) host_spi.cpp ftdi_spi.cpp -o host_spi $(LDLIBS)

clean:
	rm -f host_spi
