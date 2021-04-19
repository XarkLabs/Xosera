// ftdi_spi.h - header for FTDI SPI routines
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
#if !defined(HOST_SPI_H)
#define HOST_SPI_H

#include <ftdi.h>
#include <stdint.h>

// Thanks to https://github.com/YosysHQ/icestorm/tree/master/iceprog
// for a great example of FPGA FTDI code.

// FTDI signals used on iCE40UP5K boards
//
// PIN    | UART | PC Dir | Signal | iCEBreaker | UPduino 3.x
// -------+------+--------+--------+------------+----------------
// ADBUS0 | TXD  | output |    SCK | FLASH_SCK  | spi_sck
// ADBUS1 | RXD  | output |   COPI | FLASH_IO0  | spi_copi
// ADBUS2 | RTS# | input  |   CIPO | FLASH_IO1  | spi_cipo
// ADBUS3 | CTS# | output | fpgaCS | LEDR_N **  | led_red ***
// ADBUS4 | DTR# | input* |flashCS | FLASH_SSB  | spi_ssn
// ADBUS5 | DSR# | n/a*   |     nc | nc         | nc
// ADBUS6 | DCD# | input* |  CDONE | configured | configured
// ADBUS7 | RI#  | input* | CRESET | FPGA reset | FPGA reset
//
// *   = Set to input since not needed for Xosera SPI communication
// **  = Connected on iCEBreaker 1.0e or above (SPI lights red LED)
// *** = With TP11 connected to R pin (via ~300Ohm resistor, SPI lights red LED)
//
// NOTE: Since UPduino has single channel FT232H, serial_rxd and serial_txd
//       UART signals are shared with SPI signals spi_sck and spi_cipo.

// FTDI bit definitions to match above
#define SPI_SCK  0x01
#define SPI_COPI 0x02
#define SPI_CIPO 0x04
#define SPI_CS   0x08

#define SPI_OUTPUTS (SPI_SCK | SPI_COPI | SPI_CS)

#define FTDI_VENDOR  0x0403        // USB vendor ID for FTDI
#define FTDI_FT232H  0x6014        // FT232H Hi-Speed Single Channel USB UART/FIFO
#define FTDI_FT2232H 0x6010        // FT2232H Hi-Speed Dual USB UART/FIFO
#define FTDI_FT4232H 0x6011        // FT4232H Hi-Speed Quad USB UART

extern unsigned int chunksize;                   // set on open to the maximum size that can be sent/received per call
int                 host_spi_open();             // open FTDI device for FPGA SPI I/O
int                 host_spi_close();            // close FTDI device
void                host_spi_cs(bool cs);        // cs = false to select FPGA peripheral
int                 host_spi_xfer_bytes(size_t num, uint8_t * buffer);        // send and receive num bytes over SPI

#endif        // HOST_SPI_H
