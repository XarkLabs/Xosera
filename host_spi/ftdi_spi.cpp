// ftdi_spi.cpp - source for FTDI SPI routines
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "ftdi_spi.h"

unsigned int         chunksize;                 // set on open to the maximum size that can be sent/received per call
static bool          ftdi_device_opened;        // true if device was opened (and should be closed at exit)
static bool          ftdi_set_device_latency;        // true if latency was set (and should be restored at exit)
static unsigned char ftdi_original_latency;          // saved original FTDI latency value
static bool          slow_clock = true;

static struct ftdi_context ftdi_ctx;        // context for libftdi

static void ftdi_put_byte(uint8_t data);
static void ftdi_put_word(uint16_t data);
static void host_spi_cleanup();

// Toggle FTDI ADBUS3 (aka CTS) line used as FPGA SS on iCEBreaker (and UPduino 3.x via TP11)
// NOTE: cs = false to select (active low)
void host_spi_cs(bool cs)
{
    uint8_t gpio_pins = 0;

    if (cs)
    {
        gpio_pins |= SPI_CS;
    }

    ftdi_put_byte(SET_BITS_LOW);
    ftdi_put_byte(gpio_pins);
    ftdi_put_byte(SPI_OUTPUTS);
}

[[noreturn]] static void fatal()
{
    host_spi_cs(true);
    host_spi_cleanup();
    printf("EXITING!\n");
    exit(EXIT_FAILURE);
}

// send byte to FTDI device
static void ftdi_put_byte(uint8_t data)
{
    int rc = ftdi_write_data(&ftdi_ctx, &data, 1);
    if (rc != 1)
    {
        fprintf(stderr, "ftdi_put_byte: ftdi_write_data failed (rc=%d).\n", rc);
        fatal();
    }
}

static void ftdi_put_word(uint16_t data)
{
    uint8_t d[2] = {static_cast<uint8_t>(data), static_cast<uint8_t>(data >> 8)};
    int     rc   = ftdi_write_data(&ftdi_ctx, &d[0], 2);
    if (rc != 2)
    {
        fprintf(stderr, "ftdi_put_word: ftdi_put_word failed (rc=%d).\n", rc);
        fatal();
    }
}


// receive byte from FTDI device
static uint8_t ftdi_get_byte()
{
    uint8_t data = 0x00;
    while (1)
    {
        int rc = ftdi_read_data(&ftdi_ctx, &data, 1);
        if (rc < 0)
        {
            fprintf(stderr, "ftdi_get_byte: ftdi_read_data failed (rc=%d).\n", rc);
            fatal();
        }
        if (rc == 1)
        {
            break;
        }
        usleep(100);
    }

    return data;
}


// SPI transfer, reading and writing num bytes from/into inout
int host_spi_xfer_bytes(size_t num, uint8_t * inout)
{
    if (num < 1)
    {
        return -1;
    }

    //    host_spi_cs(false);

    // read CIPO, write COPI, LSB first, update data on negative clock edge
    ftdi_put_byte(MPSSE_DO_READ | MPSSE_DO_WRITE /* | MPSSE_LSB */ | MPSSE_WRITE_NEG);
    ftdi_put_word(static_cast<uint16_t>(num - 1));

    int rc = ftdi_write_data(&ftdi_ctx, inout, static_cast<int>(num));
    if (rc != static_cast<int>(num))
    {
        fprintf(stderr, "host_spi_xfer_bytes: ftdi_write_data failed (c=%d, expected %zu).\n", rc, num);
        fatal();
    }

    for (size_t i = 0; i < num; i++)
    {
        inout[i] = ftdi_get_byte();
    }

    //    host_spi_cs(true);

    return 0;
}

int host_spi_open()
{
    int rc = ftdi_init(&ftdi_ctx);
    if (rc != 0)
    {
        fprintf(stderr, "host_spi_open: ftdi_init failed (rc=%d)\n", rc);
        return -1;
    }

    rc = ftdi_set_interface(&ftdi_ctx, INTERFACE_A);
    if (rc != 0)
    {
        fprintf(stderr, "host_spi_open: ftdi_set_interface failed (rc=%d)\n", rc);
        return -1;
    }

    static int          device_ids[]   = {FTDI_FT2232H, FTDI_FT232H, FTDI_FT4232H};
    static const char * device_name[]  = {"FT2232H (iCEBreaker)", "FT232H (UPduino)", "FT4232H (?)"};
    static unsigned int device_chunk[] = {4096, 1024, 2048};

    int id_num = 0;
    while (id_num < 3)
    {
        if (ftdi_usb_open(&ftdi_ctx, FTDI_VENDOR, device_ids[id_num]) == 0)
        {
            break;
        }
        id_num++;
    }

    if (id_num >= 3)
    {
        fprintf(stderr, "host_spi_open: No FTDI FTx232H USB device found.\n");
        return -1;
    }

    ftdi_device_opened = true;

    printf("Opened FTDI %s...\n", device_name[id_num]);

    chunksize = device_chunk[id_num];

    if (ftdi_usb_reset(&ftdi_ctx))
    {
        fprintf(stderr, "host_spi_open: ftdi_usb_reset failed (%s).\n", ftdi_get_error_string(&ftdi_ctx));
        return -1;
    }

#if 0
    if (ftdi_usb_purge_buffers(&ftdi_ctx))
    {
        fprintf(stderr, "host_spi_open: ftdi_usb_purge_buffers failed (%s).\n", ftdi_get_error_string(&ftdi_ctx));
        return -1;
    }
#endif

    if (ftdi_get_latency_timer(&ftdi_ctx, &ftdi_original_latency) < 0)
    {
        fprintf(stderr, "host_spi_open: ftdi_get_latency_timer failed (%s).\n", ftdi_get_error_string(&ftdi_ctx));
        return -1;
    }

    // set 1kHz latency
    if (ftdi_set_latency_timer(&ftdi_ctx, 1) < 0)
    {
        fprintf(stderr, "host_spi_open: ftdi_set_latency_timer failed (%s).\n", ftdi_get_error_string(&ftdi_ctx));
        return -1;
    }

    ftdi_set_device_latency = true;

    atexit(host_spi_cleanup);

    // enter MPSSE, mask ignored
    if (ftdi_set_bitmode(&ftdi_ctx, 0x00, BITMODE_MPSSE) < 0)
    {
        fprintf(
            stderr, "host_spi_open: ftdi_set_bitmode BITMODE_MPSSE failed (%s)\n", ftdi_get_error_string(&ftdi_ctx));
        fatal();
    }

    if (slow_clock)
    {
        // 12 Mhz / (119 + 1 * 2) = 50 kHz (debug)
        ftdi_put_byte(EN_DIV_5);
        ftdi_put_byte(TCK_DIVISOR);
        ftdi_put_word(119);
    }
    else        // normal
    {
        // 12 Mhz / (0 + 1 * 2) = 6 MHz
        ftdi_put_byte(EN_DIV_5);
        ftdi_put_byte(TCK_DIVISOR);
        ftdi_put_word(0x00);
    }


    sleep(1);

    printf("Success.\n");

    return 0;
}

int host_spi_close()
{
    host_spi_cleanup();

    return 0;
}

static void host_spi_cleanup()
{
    if (ftdi_device_opened)
    {
        if (ftdi_set_device_latency)
        {
            ftdi_set_latency_timer(&ftdi_ctx, ftdi_original_latency);
            ftdi_set_device_latency = false;
        }
        ftdi_disable_bitbang(&ftdi_ctx);
        ftdi_usb_close(&ftdi_ctx);
        ftdi_deinit(&ftdi_ctx);
        ftdi_device_opened = false;
    }
}
