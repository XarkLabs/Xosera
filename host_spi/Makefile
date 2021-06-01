# Makefile - Xosera for iCEBreaker FPGA board
# vim: set noet ts=8 sw=8
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),x86_64)
# MacOS x86_64
CCFLAGS += -std=c++11 -Wall -Wextra -Os -I/usr/local/include/libftdi1
LDLIBS += -L/usr/local/lib -lftdi1
else
# MacOS arm64
CCFLAGS += -std=c++11 -Wall -Wextra -Os -I/opt/homebrew/include/libftdi1
LDLIBS += -L/opt/homebrew/lib -lftdi1
endif
else
# Linux
CCFLAGS += -std=c++11 -Wall -Wextra -Os -I/usr/include/libftdi1
LDLIBS += -lftdi1
endif

host_spi: host_spi.cpp ftdi_spi.cpp ftdi_spi.h Makefile
	$(CC) $(CCFLAGS) host_spi.cpp ftdi_spi.cpp -o host_spi $(LDLIBS)

clean:
	rm -f host_spi
