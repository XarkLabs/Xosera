// xvid_spi.cpp - xvid FTDI SPI test utility
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftdi_spi.h"


typedef enum
{
    SYS_CTRL_MEM_WAIT_B  = 7,
    SYS_CTRL_BLIT_BUSY_B = 6,
    SYS_CTRL_BLIT_FULL_B = 5
} xm_sys_ctrl_t;

// Xosera directly addressable registers (16 x 16-bit words [high/low byte])
enum e_xm_regs
{
    // register 16-bit read/write (no side effects)
    XM_XR_ADDR   = 0x0,        // (R /W+) XR register number/address for XM_XR_DATA read/write access
    XM_XR_DATA   = 0x1,        // (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
    XM_RD_INCR   = 0x2,        // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
    XM_RD_ADDR   = 0x3,        // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
    XM_WR_INCR   = 0x4,        // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
    XM_WR_ADDR   = 0x5,        // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
    XM_DATA      = 0x6,        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
    XM_DATA_2    = 0x7,        // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
    XM_SYS_CTRL  = 0x8,        // (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
    XM_TIMER     = 0x9,        // (RO   ) read 1/10th millisecond timer, write interrupt ack [TODO]
    XM_UNUSED_A  = 0xA,        // (R /W ) unused direct register 0xA [TODO]
    XM_UNUSED_B  = 0xB,        // (R /W ) unused direct register 0xB // TODO: SCANLINE
    XM_RW_INCR   = 0xC,        // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
    XM_RW_ADDR   = 0xD,        // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
    XM_RW_DATA   = 0xE,        // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
    XM_RW_DATA_2 = 0xF         // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)
};

enum e_xr_mem
{
    // XR Register Regions
    XR_CONFIG_REGS = 0x0000,         // 0x0000-0x000F 16 config/video/copper registers
    XR_PA_REGS     = 0x0010,         // 0x0010-0x0017 8 playfield A video registers
    XR_PB_REGS     = 0x0018,         // 0x0000-0x001F 8 playfield B video registers
    XR_BLIT_REGS   = 0x0020,         // 0x0020-0x002F 16 polygon blit registers
    XR_AUDIO_REGS  = 0x0030,         // 0x0030-0x003F 16 audio playback registers      // TODO: audio
                                     // XR Memory Regions
    XR_COLOR_ADDR   = 0x8000,        // 0x8000-0x81FF 256 16-bit 0xXRGB color lookup playfield A & B
    XR_TILE_ADDR    = 0xA000,        // 0xA000-0xB3FF 5K 16-bit words of tile memory
    XR_COPPER_ADDR  = 0xC000,        // 0xC000-0xC7FF 2K 16-bit words copper program memory
    XR_UNUSED_MEM_E = 0xE000         // 0xE000-0xFFFF (unused)
};

// XR read/write registers/memory regions
enum e_xr_regs
{
    // Video Config / Copper XR Registers
    XR_VID_CTRL    = 0x00,         // (R /W) display control and border color index
    XR_COPP_CTRL   = 0x01,         // (R /W) display synchronized coprocessor control
    XR_AUD0_VOL    = 0x02,         // (R /W) // TODO: replace
    XR_AUD0_PERIOD = 0x03,         // (R /W) // TODO: replace
    XR_AUD0_START  = 0x04,         // (R /W) // TODO: replace
    XR_AUD0_LENGTH = 0x05,         // (R /W) // TODO: replace
    XR_VID_LEFT    = 0x06,         // (R /W) left edge of active display window (typically 0)
    XR_VID_RIGHT   = 0x07,         // (R /W) right edge of active display window +1 (typically 640 or 848)
    XR_SCANLINE    = 0x08,         // (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline // TODO: replace
    XR_UNUSED_09   = 0x09,         // (WO  ) // TODO: replace
    XR_UNUSED_0A   = 0x0A,         // (WO  ) // TODO: replace
    XR_UNUSED_0B   = 0x0B,         // (WO  ) // TODO: replace
    XR_UNUSED_0C   = 0x0C,         // (WO  ) // TODO: replace
    XR_VID_HSIZE   = 0x0D,         // (RO  ) native pixel width of monitor mode (e.g. 640/848)// TODO: reorg
    XR_VID_VSIZE   = 0x0E,         // (RO  ) native pixel height of monitor mode (e.g. 480)   // TODO: reorg
    XR_UNUSED_0F   = 0x0F,         // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)
                                   // Playfield A Control XR Registers
    XR_PA_GFX_CTRL  = 0x10,        // (R /W) playfield A graphics control
    XR_PA_TILE_CTRL = 0x11,        // (R /W) playfield A tile control
    XR_PA_DISP_ADDR = 0x12,        // (R /W) playfield A display VRAM start address
    XR_PA_LINE_LEN  = 0x13,        // (R /W) playfield A display line width in words
    XR_PA_HV_SCROLL = 0x14,        // (R /W) playfield A horizontal and vertical fine scroll
    XR_PA_LINE_ADDR = 0x15,        // (R /W) playfield A scanline start address (loaded at start of line)
    XR_PA_HV_FSCALE = 0x16,        // (R /W) playfield A horizontal and vertical pixel scale (reduce pixel res)
    XR_PA_UNUSED_17 = 0x17,        //
                                   // Playfield B Control XR Registers
    XR_PB_GFX_CTRL  = 0x18,        // (R /W) playfield B graphics control
    XR_PB_TILE_CTRL = 0x19,        // (R /W) playfield B tile control
    XR_PB_DISP_ADDR = 0x1A,        // (R /W) playfield B display VRAM start address
    XR_PB_LINE_LEN  = 0x1B,        // (R /W) playfield B display line width in words
    XR_PB_HV_SCROLL = 0x1C,        // (R /W) playfield B horizontal and vertical fine scroll
    XR_PB_LINE_ADDR = 0x1D,        // (R /W) playfield B scanline start address (loaded at start of line)
    XR_PB_HV_FSCALE = 0x1E,        // (R /W) playfield B horizontal and vertical pixel scale (reduce pixel res)
    XR_PB_UNUSED_1F = 0x1F,        //
                                   // Blitter Registers
    XR_BLIT_CTRL  = 0x20,          // (R /W) blit control (transparency control, logic op and op input flags)
    XR_BLIT_MOD_A = 0x21,          // (R /W) blit line modulo added to SRC_A (XOR if A const)
    XR_BLIT_SRC_A = 0x22,          // (R /W) blit A source VRAM read address / constant value
    XR_BLIT_MOD_B = 0x23,          // (R /W) blit line modulo added to SRC_B (XOR if B const)
    XR_BLIT_SRC_B = 0x24,          // (R /W) blit B AND source VRAM read address / constant value
    XR_BLIT_MOD_C = 0x25,          // (R /W) blit line XOR modifier for C_VAL const
    XR_BLIT_VAL_C = 0x26,          // (R /W) blit C XOR constant value
    XR_BLIT_MOD_D = 0x27,          // (R /W) blit modulo added to D destination after each line
    XR_BLIT_DST_D = 0x28,          // (R /W) blit D VRAM destination write address
    XR_BLIT_SHIFT = 0x29,          // (R /W) blit first and last word nibble masks and nibble right shift (0-3)
    XR_BLIT_LINES = 0x2A,          // (R /W) blit number of lines minus 1, (repeats blit word count after modulo calc)
    XR_BLIT_WORDS = 0x2B           // (R /W) blit word count minus 1 per line (write starts blit operation)
};

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

void delay_ms(int ms)
{
    usleep(ms * 1000);
}


// SPI "bus command" message format. Always sends/receives two bytes:
//              +---+---+---+---+---+---+---+---+
// Command byte |CS |WR |RS |BS |     REGNUM    |
//              +---+---+---+---+---+---+---+---+
// CS bit     = 0 for de-selected, 1 for selected
// WR bit     = 0 for read, 1 for write
// RS bit     = 1 for normal, 1 for reset
// BS bit     = 0 for even, 1 for odd byte of register
// REGNUM     = Xosera 4-bit register number
// Command byte SPI reply will be 0xCB (for command byte)
//
//              +---+---+---+---+---+---+---+---+
// Data byte    |Wr 8-bit data / ignored for Rd |
//              +---+---+---+---+---+---+---+---+
//
// Data byte SPI reply will be the Xosera register contents for a read or a "dummy" byte for a write.
//
// NOTE: Actually for writes, the "dummy" reply byte will be the value Xosera "would" have written to the bus had this
// been a read operation.  For read/write registers this will be the previous register contents before the write
// command, or for write-only registers it will be a "mirrored" readable register. This behavior should not be relied
// upon, but can be a handy artifact for debugging/testing.

enum
{
    SPI_CMD_CS      = 0x80,
    SPI_CMD_WR      = 0x40,
    SPI_CMD_RS      = 0x20,
    SPI_CMD_BYTESEL = 0x10,
    SPI_CMD_REGMASK = 0x0F
};

#define DEBUG_HEXDUMP 1

#if 0
#define MAX_SEND    1024
#define FLUSH_QUEUE 1020
#else
#define MAX_SEND    16
#define FLUSH_QUEUE 4
#endif

static uint8_t   send_buffer[MAX_SEND];
static uint8_t   xmit_buffer[MAX_SEND];
static uint8_t * send_ptr = send_buffer;
static uint8_t   read_value;

size_t spi_queue_len()
{
    size_t len = send_ptr - send_buffer;
    return len;
}

inline int spi_queue_flush()
{
    size_t len = spi_queue_len();
    if (len)
    {
        host_spi_cs(false);        // select
        memcpy(xmit_buffer, send_buffer, len);
        host_spi_xfer_bytes(len, xmit_buffer);
        host_spi_cs(true);        // de-select

#if DEBUG_HEXDUMP
        printf("SENT[%02zu]: ", len);
        hexdump(len, send_buffer);
        printf("RCVD[%02zu]: ", len);
        hexdump(len, xmit_buffer);
#endif
        send_ptr = send_buffer;
    }

    return len;
}

inline int spi_queue_cmd(uint8_t cmd, uint8_t data)
{
    int off     = static_cast<int>(send_ptr - send_buffer);
    *send_ptr++ = cmd;
    *send_ptr++ = data;
    return off;
}

void delay(int ms)
{
    spi_queue_flush();
    delay_ms(ms);
}

static inline void xvid_setw(uint8_t r, uint16_t word)
{
    spi_queue_cmd(SPI_CMD_CS | SPI_CMD_WR | (r & SPI_CMD_REGMASK), (word >> 8) & 0xff);
    spi_queue_cmd(SPI_CMD_CS | SPI_CMD_WR | SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), word & 0xff);
    if (spi_queue_len() > FLUSH_QUEUE)
    {
        spi_queue_flush();
    }
}

static inline void xvid_setlb(uint8_t r, uint8_t lsb)
{
    spi_queue_cmd(SPI_CMD_CS | SPI_CMD_WR | SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), lsb & 0xff);
    if (spi_queue_len() > FLUSH_QUEUE)
    {
        spi_queue_flush();
    }
}

static inline void xvid_sethb(uint8_t r, uint8_t msb)
{
    spi_queue_cmd(SPI_CMD_CS | SPI_CMD_WR | (r & SPI_CMD_REGMASK), msb & 0xff);
    if (spi_queue_len() > FLUSH_QUEUE)
    {
        spi_queue_flush();
    }
}

static inline uint16_t xvid_getw(uint8_t r)
{
    int msb = spi_queue_cmd(SPI_CMD_CS | (r & SPI_CMD_REGMASK), 0xff);
    int lsb = spi_queue_cmd(SPI_CMD_CS | SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), 0xff);
    spi_queue_flush();
    assert(xmit_buffer[msb] == 0xcb);
    assert(xmit_buffer[lsb] == 0xcb);

    return (xmit_buffer[msb + 1] << 8) | xmit_buffer[lsb + 1];
}

// bytesel = LSB (default) or 0 for MSB
static inline uint8_t xvid_getb(uint8_t r, uint8_t bytesel = 1)
{
    int off = spi_queue_cmd(SPI_CMD_CS | (bytesel ? SPI_CMD_BYTESEL : 0) | (r & SPI_CMD_REGMASK), 0xff);
    spi_queue_flush();
    assert(xmit_buffer[off] == 0xcb);
    return xmit_buffer[off + 1];
}

static inline uint8_t xvid_getlb(uint8_t r)
{
    return xvid_getb(r, 1);
};
static inline uint8_t xvid_gethb(uint8_t r)
{
    return xvid_getb(r, 0);
}

static void xcolor(uint8_t color)
{
    uint16_t wa = xvid_getw(XM_WR_ADDR);
    xvid_sethb(XM_DATA, color);
    xvid_setw(XM_WR_ADDR, wa);
}

static bool     error_flag;
static uint32_t errors;
static uint8_t  cur_color = 0x02;        // color for status line (green or red after error)
static uint8_t  ln        = 0;           // current line number
static uint16_t width;                   // in pixels
static uint16_t height;                  // in pixels
static uint16_t features;                // feature bits
static uint8_t  columns;                 // in texts chars (words)
static uint8_t  rows;                    // in texts chars (words)
static uint16_t addr;
static uint16_t data;
static uint16_t rdata;

uint32_t mem_buffer[128 * 1024];

#include "buddy_font.h"

static void spi_reset(uint8_t cmd)
{
    spi_queue_flush();
    spi_queue_cmd(cmd, cmd);
    for (int i = 0; i < 100; i++)
    {
        delay_ms(10);
        size_t len = spi_queue_flush();
        if (xmit_buffer[len - 2] == 0xcb)
        {
            break;
        }
        spi_queue_cmd(cmd, cmd);
    }
}

static bool sync_Xosera(bool reset)
{
    printf("Waiting for Xosera SPI sync%s...", reset ? " and reset" : "");
    fflush(stdout);
    xvid_setw(XM_SYS_CTRL, 0x8000);
    host_spi_cs(true);        // de-select
    delay_ms(100);
    bool result = false;
    for (int retry = 0; retry < 10; retry++)
    {
        xvid_setw(XM_RD_INCR, 0xB007);
        uint16_t v = xvid_getw(XM_RD_INCR);
        if (v == 0xb007)
        {
            result = true;
            break;
        }
        host_spi_cs(true);        // de-select
        delay_ms(100);
    }

    printf("%s\n", result ? "okay." : "FAILED!");

    return result;
}

static void reboot_Xosera(int8_t config)
{
    if (config >= 0)
    {
        printf("Xosera reconfiguring to config #%d...\n", config & 0x3);
        host_spi_cs(true);        // de-select
        delay_ms(10);
        //        xvid_setw(XVID_BLIT_CTRL, 0x8080 | ((config & 0x3) << 8));        // reboot FPGA to config
        spi_queue_flush();
        delay_ms(70);
        host_spi_cs(true);        // de-select
    }
    do
    {
        spi_queue_flush();
        host_spi_cs(true);        // de-select
        delay_ms(10);
        xvid_setw(XM_RD_ADDR, 0x1234);
        xvid_setw(XM_RD_INCR, 0xABCD);
        spi_queue_flush();
    } while (xvid_getw(XM_RD_ADDR) != 0x1234 || xvid_getw(XM_RD_INCR) != 0xABCD);

    xvid_setw(XM_XR_ADDR, XR_VID_HSIZE);        // select width
    width = xvid_getw(XM_XR_DATA);
    xvid_setw(XM_XR_ADDR, XR_VID_VSIZE);        // select height
    height = xvid_getw(XM_XR_DATA);
    xvid_setw(XM_XR_ADDR, XR_UNUSED_0F);        // select features
    features = xvid_getw(XM_XR_DATA);
    printf("(%dx%d, features=0x%04x) ready.\n", width, height, features);
    columns = width / 8;
    rows    = height / 16;
    addr    = columns;
}

// read scanline register and wait for non-visible line (bit[15])
static void wait_vsync(uint16_t num = 1)
{
    uint8_t v_flag;
    while (num--)
    {
        do
        {
            xvid_setw(XM_XR_DATA, XR_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XM_XR_DATA);           // read scanline upper byte
        } while (!(v_flag & 0x80));                    // loop if on visible line
    }
}

static void xhome()
{
    // home wr addr
    xvid_setw(XM_WR_INCR, 1);
    xvid_setw(XM_WR_ADDR, 0);
    xcolor(cur_color);        // green-on-black
    ln = 0;
}

static void xpos(uint8_t h, uint8_t v)
{
    xvid_setw(XM_WR_INCR, 1);
    xvid_setw(XM_WR_ADDR, (v * columns) + h);
    xcolor(cur_color);        // green-on-black
    ln = v;
}

static void xcls(uint8_t v = ' ')
{
    // clear screen
    xhome();
    for (uint16_t i = 0; i < columns * rows; i++)
    {
        xvid_setlb(XM_DATA, v);
    }
    xvid_setw(XM_WR_ADDR, 0);
}

static void xprint(const char * s)
{
    uint8_t c;
    while ((c = *s++) != '\0')
    {
        if (c == '\n')
        {
            xvid_setw(XM_WR_ADDR, ++ln * columns);
            continue;
        }
        xvid_setlb(XM_DATA, c);
    }
}

static void xprint_rainbow(uint8_t color, const char * s)
{
    uint8_t c;
    xcolor(color);
    while ((c = *s++) != '\0')
    {
        if (c == '\n')
        {
            ln += 1;
            xvid_setw(XM_WR_ADDR, ln * columns);
            color = (color + 1) & 0xf;
            if (color == 0)        // skip black
                color++;
            xcolor(color);
            continue;
        }
        xvid_setlb(XM_DATA, c);
    }
}

static void xprint_hex(uint16_t v)
{
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    xvid_setw(XM_DATA, (cur_color << 8) | hex[((v >> 12) & 0xf)]);
    xvid_setw(XM_DATA, (cur_color << 8) | hex[((v >> 8) & 0xf)]);
    xvid_setw(XM_DATA, (cur_color << 8) | hex[((v >> 4) & 0xf)]);
    xvid_setw(XM_DATA, (cur_color << 8) | hex[((v >> 0) & 0xf)]);
}

static void xprint_int(uint32_t n)
{
    uint32_t poten = 100000000;
    uint32_t v     = n;
    if (v > 999999999)
        v = 999999999;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XM_DATA, '0' + d);
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

static void xprint_dec(uint16_t n)
{
    uint16_t poten = 10000;
    uint16_t v     = n;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XM_DATA, '0' + d);
        }
        else
        {
            xvid_setlb(XM_DATA, ' ');
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

const uint16_t defpal[16] = {
    0x0000,        // black
    0x000A,        // blue
    0x00A0,        // green
    0x00AA,        // cyan
    0x0A00,        // red
    0x0A0A,        // magenta
    0x0AA0,        // brown
    0x0AAA,        // light gray
    0x0555,        // dark gray
    0x055F,        // light blue
    0x05F5,        // light green
    0x05FF,        // light cyan
    0x0F55,        // light red
    0x0F5F,        // light magenta
    0x0FF5,        // yellow
    0x0FFF         // white
};

const char blurb[] =
    "01234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools, that is being tailored with features appropriate for a Motorola\n"
    "68K era retro computer.\n"
    "\n"
    "  \xf9  VGA or HDMI/DVI output at 848x480 or 640x480 (16:9 or 4:3 @ 60Hz)\n"
    "  \xf9  16 or 256 color palette out of 4096 colors (12-bit RGB)\n"
    "  \xf9  128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "  \xf9  Character tile based modes with color attribute byte\n"
    "  \xf9  Pixel doubled bitmap modes (e.g. 424x240 or 320x240)\n"
    "  \xf9  Smooth horizontal and vertical tile scrolling\n"
    "  \xf9  8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "  \xf9  Register based interface with 16 16-bit registers\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Full speed bus interface (with MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "  \xf9  \"Blitter\" for fast VRAM copy & fill operations (TODO, but used at init)\n"
    "  \xf9  2-D operations \"blitter\" with modulo and shifting/masking (TODO)\n"
    "  \xf9  Dual overlayed \"planes\" of video (TODO)\n"
    "  \xf9  Wavetable stereo audio (TODO, spare debug IO for now)\n"
    "  \xf9  Bit-mapped 16 and 256 color graphics modes (256 color TODO)\n"
    "  \xf9  16-color tile mode with \"game\" attributes (e.g., mirroring) (TODO)\n"
    "  \xf9  At least one \"cursor\" sprite (and likely more, TODO)\n";

void show_blurb()
{
    // Show some text
    printf("Blurb text\n");
    xcls();
    xprint(blurb);
    delay(500);

    // 2nd font (ST 8x8)
    printf("ST 8x8 font\n");
    xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
    xvid_setw(XM_XR_DATA, 0x0207);                 // 2nd font in bank 2, 8 high
    delay(500);

    // 3rd font (hex 8x8 debug)
    printf("hex 8x8 font\n");
    xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
    xvid_setw(XM_XR_DATA, 0x0307);                 // 3st font in bank 3, 8 high
    delay(500);

    // restore 1st font (ST 8x16)
    printf("ST 8x16 font\n");
    xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
    xvid_setw(XM_XR_DATA, 0x000F);                 // back to 1st font in bank 0, 16 high
    delay(500);

    // shrink font height
    printf("Shrink font height\n");
    for (int v = 15; v >= 0; v--)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, (v < 8 ? 0x0200 : 0) | v);

        wait_vsync(1);
    }

    printf("Grow font height\n");
    for (int v = 0; v < 16; v++)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, (v < 8 ? 0x0200 : 0) | v);
        wait_vsync(1);
    }

    // restore 1st font (ST 8x16)
    printf("ST 8x16 font\n");
    xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
    xvid_setw(XM_XR_DATA, 0x000F);                 // back to 1st font in bank 0, 16 high
    delay(500);

    printf("Scroll via video VRAM display address\n");
    int16_t r = 0;
    for (uint16_t i = 0; i < (rows); i++)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // set text start addr
        xvid_setw(XM_XR_DATA, r * columns);            // to one line down
        for (int8_t f = 0; f < 16; f++)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // v fine scroll
            xvid_setw(XM_XR_DATA, f);

            wait_vsync(1);
        }
        if (++r > rows + 10)
        {
            r = -rows;
        }
    }
    xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // reset text start addr
    xvid_setw(XM_XR_DATA, 0x0000);
    xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // reset text start addr
    xvid_setw(XM_XR_DATA, 0x0000);
    delay(500);

    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XM_XR_ADDR, XR_COLOR_ADDR + i);        // use WR address for palette index
        xvid_setw(XM_XR_DATA, defpal[i]);                // set palette data
    }

#if 0
    printf("Horizontal fine scroll\n");
    for (int x = 0; x < 8; x++)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, x);
        delay(100);
    }
    for (int x = 7; x > 0; x--)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, x);
        delay(100);
    }
    delay(1000);
    printf("Vertical fine scroll\n");
    for (int x = 0; x < 16; x++)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, x << 8);
        delay(100);
    }
    for (int x = 15; x > 0; x--)
    {
        xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XM_XR_DATA, x << 8);
        delay(100);
    }
    delay(1000);
#endif
}

static void problem(const char * msg, uint16_t addr, uint16_t rdata, uint16_t vdata)
{
    errors++;
    printf("%s at 0x%04x, rd=%04x, vs %04x, errors %d\n", msg, addr, rdata, vdata, errors);
    error_flag = true;
}


static void test_mono_bitmap(const char * filename)
{
    printf("Loading mono bitmap: \"%s\"", filename);
    FILE * file = fopen(filename, "r");

    xvid_setw(XM_WR_INCR, 0x0001);

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fread(mem_buffer, 1, 128 * 1024, file)) > 0)
        {
            uint8_t * maddr = (uint8_t *)mem_buffer;
            xvid_setw(XM_WR_ADDR, vaddr);
            for (int i = 0; i < cnt; i += 2)
            {
                xvid_sethb(XM_DATA, *maddr++);
                xvid_setlb(XM_DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fclose(file);
        printf(" - done!\n");
    }
    else
    {
        printf(" - FAILED\n");
    }
}


static const uint16_t data_pat[8] = {0xA5A5, 0x5A5A, 0xFFFF, 0x0123, 0x4567, 0x89AB, 0xCDEF, 0x0220};

void test_reg_access()
{
    xcls();
    xprint("Xosera read/write register self-test...\n");

    for (uint8_t r = XM_RD_INCR; r <= XM_WR_ADDR; r++)
    {
        xhome();
        xpos(4, 4 + r);
        xprint("Register: ");
        switch (r)
        {
            case XM_RD_INCR: {
                xprint("XM_XR_ADDR ");
            }
            break;
            case XM_RD_ADDR: {
                xprint("XM_RD_ADDR ");
            }
            break;
            case XM_WR_INCR: {
                xprint("XM_WR_INCR ");
            }
            break;
            case XM_WR_ADDR: {
                xprint("XM_WR_ADDR ");
            }
            break;
        }
        xprint(" <=> ");

        uint16_t cp = xvid_getw(XM_WR_ADDR);
        for (int i = 0; i < 8; i++)
        {
            uint16_t v = data_pat[i];
            xvid_setw(XM_WR_ADDR, cp);
            xcolor(cur_color);
            xprint_hex(v);
            xvid_setw(r, v);
            rdata = xvid_getw(r);
            if (rdata != v)
            {
                problem("reg verify", r, rdata, v);
                break;
            }
        }
        if (!error_flag)
        {
            xvid_setw(XM_WR_ADDR, cp);
            xcolor(0x0a);
            xprint("PASSED");
            xcolor(cur_color);
        }
        else
        {
            xvid_setw(XM_WR_ADDR, cp);
            xcolor(cur_color);
            xprint("FAILED");
        }
    }

    if (error_flag)
    {
        xpos(0, 8);
        xprint("Register self-test FAILED!");
        delay(2000);
    }
    else
    {
        xpos(0, 8);
        xprint("Register self-test passed.");
    }

    xpos(0, 12);
    xprint("VRAM read/write check...");

    delay(1000);

    xpos(4, 14);
    xprint("VRAM[");
    uint16_t ap = xvid_getw(XM_WR_ADDR);
    xprint("    ] <=> ");
    uint16_t vp = xvid_getw(XM_WR_ADDR);

    for (uint8_t i = 0; i < 8; i++)
    {
        uint16_t v = data_pat[i];
        xvid_setw(XM_WR_ADDR, vp);
        xcolor(cur_color);
        xprint_hex(v);

        for (int a = 0x600; a < 0x10000; a++)
        {
            if ((a & 0xfff) == 0xfff)
            {
                xvid_setw(XM_WR_ADDR, ap);
                xcolor(cur_color);
                xprint_hex(a);
            }
            xvid_setw(XM_WR_ADDR, a);
            xvid_setw(XM_DATA, v);
            xvid_setw(XM_RD_ADDR, a);
            rdata = xvid_getw(XM_DATA);
            if (rdata != v)
            {
                problem("VRAM test", a, rdata, v);
                break;
            }
        }
        if (error_flag)
        {
            break;
        }
    }

    if (error_flag)
    {
        xpos(0, 16);
        xprint("VRAM check FAILED!");
    }
    else
    {
        xpos(0, 16);
        xprint("VRAM check passed.");
    }

    delay(2000);
}

void draw_buddy()
{
    xvid_setw(XM_XR_ADDR, XR_PA_TILE_CTRL);        // A_font_ctrl
    //    xvid_setw(XM_XR_DATA, 0x0207);              // 2nd font in bank 2, 8 high
    xvid_setw(XM_XR_DATA, 0x4087);        // 2nd font in VRAM @ 0x4000, 8 high
    rows <<= 1;

    xcls(0xff);
    for (int y = 0; y < 16; y++)
    {
        xvid_setw(XM_WR_ADDR, y * columns);
        for (int x = 0; x < 16; x++)
        {
            xvid_setw(XM_DATA, 0x0f00 | (y * 16 + x));
        }
    }
    for (uint16_t a = 0; a < 1024; a++)
    {
        //        xvid_setw(XM_XR_ADDR, AUX_FONT | 4096 | a);
        //        uint16_t w = (buddy_font[(a << 1) + 1] << 8) | buddy_font[(a << 1)];
        //        xvid_setw(XM_XR_DATA, w);

        xvid_setw(XM_WR_ADDR, 0x4000 | a);
        uint16_t w = (buddy_font[(a << 1) + 1] << 8) | buddy_font[(a << 1)];
        xvid_setw(XM_DATA, w);
    }

    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0005);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0000);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);        // A_font_ctrl
    xvid_setw(XM_XR_DATA, 0x000F);                // back to 1st font in bank 0, 16 high
}


void test_smoothscroll()
{
    xcls();
    xprint_rainbow(1, blurb);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0000);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0001);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0004);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0005);
    delay(2000);
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0000);
    delay(2000);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 8; x++)
        {
            wait_vsync();
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, x << 8);
            delay_ms(150);
        }
        for (int x = 7; x >= 0; x--)
        {
            wait_vsync();
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, x << 8);
            delay_ms(150);
        }
    }

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 8; x++)
        {
            wait_vsync(2);
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, x << 8);
        }
        for (int x = 7; x >= 0; x--)
        {
            wait_vsync(2);
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, x << 8);
        }
    }

    xvid_setw(XM_XR_ADDR, XR_PA_LINE_LEN);        // set width
    xvid_setw(XM_XR_DATA, columns * 2);
    xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // zero fine scroll
    xvid_setw(XM_XR_DATA, 0);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, x >> 3);
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0x7) << 8);
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, x >> 3);
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0x7) << 8);
            wait_vsync(1);
        }
    }

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 4) * (columns * 2)) + (x >> 3));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0x7) << 8 | (x & 0xf));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 4) * (columns * 2)) + (x >> 3));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0x7) << 8 | (x & 0xf));
            wait_vsync(1);
        }
    }

    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0001);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 4) * (columns * 2)) + (x >> 4));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0xf) << 8 | (x & 0xf));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 4) * (columns * 2)) + (x >> 4));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0xf) << 8 | (x & 0xf));
            wait_vsync(1);
        }
    }

    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0005);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 5) * (columns * 2)) + (x >> 4));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0xf) << 8 | (x & 0x1f));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
            xvid_setw(XM_XR_DATA, ((x >> 5) * (columns * 2)) + (x >> 4));
            xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
            xvid_setw(XM_XR_DATA, (x & 0xf) << 8 | (x & 0x1f));
            wait_vsync(1);
        }
    }

    xvid_setw(XM_XR_ADDR, XR_PA_DISP_ADDR);        // start addr
    xvid_setw(XM_XR_DATA, 0x0000);                 // set palette data
    xvid_setw(XM_XR_ADDR, XR_PA_HV_SCROLL);        // fine scroll
    xvid_setw(XM_XR_DATA, 0x0000);                 // set palette data
    xvid_setw(XM_XR_ADDR, XR_PA_LINE_LEN);         // set width
    xvid_setw(XM_XR_DATA, columns);

    delay(5000);

    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);        // use WR address for palette index
    xvid_setw(XM_XR_DATA, 0x0000);                // set palette data

    xvid_setw(XM_XR_ADDR, XR_PA_LINE_LEN);        // set width
    xvid_setw(XM_XR_DATA, columns);

    delay(2000);
}


bool reset_only    = false;
bool no_reset      = false;
int  xosera_config = -1;

#define MAX_CMDS 256
int    num_cmds = 0;
char * cmd_list[MAX_CMDS];

int main(int argc, char ** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-r") == 0)
        {
            reset_only = true;
            continue;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            no_reset = true;
            continue;
        }
        else if (strncmp(argv[i], "-c", 2) == 0)
        {
            if (argv[i][2] < '0' || argv[i][2] > '3')
            {
                printf("Config needs to be 0 - 3\n");
                exit(EXIT_FAILURE);
            }
            xosera_config = argv[i][2] & 0x3;
            continue;
        }
        else if (argv[i][0] == 'R' || argv[i][0] == 'r')
        {
            char * rn = strdup(argv[i]);
            if (num_cmds < MAX_CMDS)
            {
                cmd_list[num_cmds++] = rn;
            }
            else
            {
                printf("Too many commands (> %d)\n", MAX_CMDS);
                exit(EXIT_FAILURE);
            }
            continue;
        }
        printf("Unknown option \"%s\"\n", argv[i]);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_cmds; i++)
    {
        printf("CMD %d: %s\n", i, cmd_list[i]);
    }

    if (host_spi_open() < 0)
    {
        exit(EXIT_FAILURE);
    }

    bool res = sync_Xosera(no_reset ? 0 : 1);

    if (reset_only)
    {
        host_spi_close();
        printf("Exiting after reset (\"-r\" option)\n");

        exit(res ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    //    reboot_Xosera(xosera_config);

    // mono bitmap mode
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0040);
    test_mono_bitmap("space_shuttle_color_small.raw");
    delay(5000);        // let the stunning boot logo display. :)

    // text mode
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0000);

    delay(5000);        // let the stunning boot logo display. :)
#if 1
    xcls();
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint_int(width);
    xprint("x");
    xprint_int(height);
    xprint(" (SPI/FTDI PC tester)\n\n");

    for (int i = 0; i < 409; i++)
    {
        uint8_t c = (i & 0xf) ? (i & 0xf) : 1;
        xcolor(c);
        xprint("Hello! ");
    }

    delay(5000);
#endif

    test_smoothscroll();

    xcolor(0xf);
    xcls();
    draw_buddy();

    for (uint16_t k = 0; k < 2000; k++)
    {
        xvid_setw(XM_XR_ADDR, XR_SCANLINE);               // set scanline reg
        uint16_t l = xvid_getw(XM_XR_DATA);               // read scanline
        l          = l | ((0xf - (l & 0xf)) << 8);        // invert blue for some red
        xvid_setw(XM_XR_ADDR, XR_COLOR_ADDR + 0);         // set palette entry #0
        xvid_setw(XM_XR_DATA, l);                         // set palette data
    }
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XM_XR_ADDR, XR_COLOR_ADDR + i);        // use WR address for palette index
        xvid_setw(XM_XR_DATA, defpal[i]);                // set palette data
    }

    //    test_reg_access();

    show_blurb();

    delay_ms(2000);

    xhome();

    xprint_rainbow(1, blurb);

    delay_ms(2000);

    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);        // use WR address for palette index
    xvid_setw(XM_XR_DATA, 0x0001);                // set palette data

    delay(2000);

    xcolor(0xf);
    xcls();
    draw_buddy();

    // mono bitmap mode
    xvid_setw(XM_XR_ADDR, XR_PA_GFX_CTRL);
    xvid_setw(XM_XR_DATA, 0x0040);
    test_mono_bitmap("space_shuttle_color_small.raw");

    host_spi_close();

    exit(EXIT_SUCCESS);
}
