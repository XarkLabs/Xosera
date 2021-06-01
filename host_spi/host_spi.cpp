// host_spi.cpp - host FTDI SPI test utility
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

static void hexdump(size_t num, uint8_t * mem)
{
    for (size_t i = 0; i < num; i++)
    {
        printf("%02x", mem[i]);
        if (i != num - 1)
        {
            printf(", ");
        }
    }
    printf("\n");
}

static uint8_t to_send[65536] = {0};
static uint8_t data[65536]    = {0};

int main(int argc, char ** argv)
{
    if (host_spi_open() < 0)
    {
        exit(EXIT_FAILURE);
    }
    size_t len = 0;

    for (int i = 1; i < argc && len < sizeof(to_send); i++)
    {
        char * endptr = nullptr;
        int    value  = static_cast<int>(strtoul(argv[i], &endptr, 0) & 0xffUL);
        if (endptr != nullptr && *endptr == '\0')
        {
            data[len] = static_cast<uint8_t>(value);
            len++;
        }
        else
        {
            break;
        }
    }

    memcpy(to_send, data, len);
    printf("Sending [%zu]: ", len);
    hexdump(len, to_send);
    host_spi_cs(false);
    host_spi_xfer_bytes(len, to_send);
    host_spi_cs(true);
    printf("Reply   [%zu]: ", len);
    hexdump(len, to_send);

    exit(EXIT_SUCCESS);
}
