# Makefile - Xosera Read/write Xosera registers via FTDI SPI
# (mostly iCEBreaker, but can work on UPduino)
# vim: set noet ts=8 sw=8
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),x86_64)
# MacOS x86_64
CCFLAGS += -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Os -I/usr/local/include/libftdi1
LDLIBS += -L/usr/local/lib -lftdi1 -lstdc++
else
# MacOS arm64
CCFLAGS += -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Os -I/opt/homebrew/include/libftdi1
LDLIBS += -L/opt/homebrew/lib -lftdi1 -lstdc++
endif
else
# Linux
CCFLAGS += -std=c++11 -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Os -I/usr/include/libftdi1
LDLIBS += -lftdi1 -lstdc++ -lm
endif

xvid_spi: xvid_spi.cpp ftdi_spi.cpp ftdi_spi.h Makefile
	$(CC) $(CCFLAGS) xvid_spi.cpp ftdi_spi.cpp -o xvid_spi $(LDLIBS)

clean:
	rm -f xvid_spi
