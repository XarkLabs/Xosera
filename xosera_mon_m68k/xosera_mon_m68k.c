/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

#include "rosco_m68k_support.h"

#include "xosera_mon_m68k.h"

#define DEBUG 1

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;

xosera_info_t initinfo;
uint32_t      mem_buffer[128 * 1024];

// timer helpers
static uint32_t start_tick;

void timer_start()
{
    uint32_t ts = XFrameCount;
    uint32_t t;
    // this waits for a "fresh tick" to reduce timing jitter
    while ((t = XFrameCount) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = XFrameCount;

    return ((stop_tick - start_tick) * 1667) / 100;
}

static inline void wait_vblank_start()
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();
}

static inline void check_vblank()
{
    xv_prep();

    if (!xm_getb_sys_ctrl(VBLANK) || xreg_getw(SCANLINE) > 520)
    {
        wait_vblank_start();
    }
}

void reset_vid(void)
{
    xv_prep();

    remove_intr();

    wait_vblank_start();

    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x08));
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));        // disable copper
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xosera_vid_width());
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_1_BPP, 0, 0, 0));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);        // line len
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));
    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_1_BPP, 0, 0, 0));

    printf("\033c");        // reset XANSI

    while (mcCheckInput())
    {
        mcInputchar();
    }
}

// From https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
/*-
 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
 *  code or tables extracted from it, as desired without restriction.
 *
 * CRC32 code derived from work by Gary S. Brown.
 */

/*
 * A function that calculates the CRC-32 based on the table above is
 * given below for documentation purposes. An equivalent implementation
 * of this function that's actually used in the kernel can be found
 * in sys/libkern.h, where it can be inlined.
 */

unsigned int crc32b(unsigned int crc, const void * buf, size_t size)
{
    static const unsigned int crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
        0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
        0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
        0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
        0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
        0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
        0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
        0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
        0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
        0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
        0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
        0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
        0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
        0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
        0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

    const uint8_t * p = buf;
    crc               = crc ^ ~0U;
    while (size--)
    {
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

// read line from keyboard with basic editing
int dreadline(char * buf, int buf_size)
{
    memset(buf, 0, buf_size);

    int len = 0;
    while (true)
    {
        char c = mcInputchar();

        // accept string
        if (c == '\r')
        {
            break;
        }

        switch (c)
        {
            // backspace
            case '\b': /* ^H */
            case 0x7F: /* DEL */
                if (len > 0)
                {
                    buf[--len] = '\0';
                    debug_puts("\b \b");
                }
                break;
            // clear string
            case '\x3':  /* ^C */
            case '\x18': /* ^X */
                while (len > 0)
                {
                    buf[--len] = '\0';
                    debug_puts("\b \b");
                }
                break;

            // add non-control character
            default:
                if (len < (buf_size - 1) && c >= ' ')
                {
                    debug_putc(c);
                    buf[len++] = c;
                }
        }
    }
    debug_puts("\n");
    // make sure string is terminated
    buf[len] = 0;

    return len;
}

// return next argument from string
char * next_token(char ** next_token)
{
    if (next_token == NULL || *next_token == NULL || **next_token == '\0')
    {
        return "";
    }

    char * token = *next_token;
    // skip leading spaces
    while (*token == ' ')
    {
        token++;
    }
    bool quoted = false;
    if (*token == '"')
    {
        quoted = true;
        token++;
    }
    // arg string
    char * end_token = token;
    // either empty or starts after space (unless quoted)
    while (*end_token && *end_token != '"' && !(*end_token == ' ' && quoted == false))
    {
        end_token++;
    }
    // end cmd_ptr string
    *end_token++ = '\0';
    *next_token  = end_token;

    return token;
}

const char * val_name(const addr_range_t * ar, uint16_t v)
{
    static char str[64];
    for (int i = 0; ar[i].name != NULL; i++)
    {
        if (v >= ar[i].addr && v < ar[i].addr + ar[i].size)
        {
            if (ar[i].size > 1)
            {
                uint16_t o = v - ar[i].addr;
                sprintf(str, "%s+0x%x", ar[i].name, o);
            }
            else
            {
                sprintf(str, "%s", ar[i].name);
            }
            return str;
        }
    }

    return "";
}

int name_val(const addr_range_t * ar, const char * name)
{
    for (int i = 0; ar[i].name != NULL; i++)
    {
        if (strcmp(name, ar[i].name) == 0)
        {
            return (int)ar[i].addr;
        }
    }

    return -1;
}

void str_upper(char * str)
{
    char c;
    while (str && (c = *str) != '\0')
    {
        *str = toupper(*str);
        str++;
    }
}

void print_xr_reg(int xreg_num)
{
    xv_prep();
    xwait_mem_ready();
    uint16_t v = xmem_getw_wait(xreg_num);

    debug_printf("%-13.13s= 0x%04x", val_name(xr_mem, xreg_num), v);
}

void print_xm_reg(int reg_num)
{
    xv_prep();

    debug_printf("%-10.10s= ", val_name(xm_regs, reg_num));

    uint16_t v;
    uint32_t l;
    uint16_t prev;
    uint16_t read;
    uint16_t incr;

    (void)read;

    switch (reg_num << 2)
    {
        case XM_SYS_CTRL:
            v = xm_getw(SYS_CTRL);
            debug_printf("0x%04x", v);
            for (int b = 7; b >= 0; b--)
            {
                if (v & (0x100 << b))
                {
                    debug_printf(" %s", val_name(sys_ctrl_status, b));
                }
            }
            debug_printf(" WM:%x%x%x%x", (v >> 3) & 1, (v >> 2) & 1, (v >> 1) & 1, (v >> 0) & 1);
            break;
        case XM_INT_CTRL:
            v = xm_getw(INT_CTRL);
            debug_printf("0x%04x", v);
            debug_printf(" IM:%s %s %s %s %s %s %s",
                         (v >> 14) & 1 ? "BL" : "- ",
                         (v >> 13) & 1 ? "TI" : "- ",
                         (v >> 12) & 1 ? "VI" : "- ",
                         (v >> 11) & 1 ? "A3" : "- ",
                         (v >> 10) & 1 ? "A2" : "- ",
                         (v >> 9) & 1 ? "A1" : "- ",
                         (v >> 8) & 1 ? "A0" : "- ");
            debug_printf(" IP:%s %s %s %s %s %s %s",
                         (v >> 6) & 1 ? "BL" : "- ",
                         (v >> 5) & 1 ? "TI" : "- ",
                         (v >> 4) & 1 ? "VI" : "- ",
                         (v >> 3) & 1 ? "A3" : "- ",
                         (v >> 2) & 1 ? "A2" : "- ",
                         (v >> 1) & 1 ? "A1" : "- ",
                         (v >> 0) & 1 ? "A0" : "- ");
            break;
        case XM_TIMER:
            v = xm_getw(TIMER);
            debug_printf("0x%04x [%d.%04d s]", v, v / (uint16_t)10000, v % (uint16_t)10000);
            break;
        case XM_RD_XADDR:
            v = xm_getw(RD_XADDR);
            debug_printf("0x%04x %s", v, val_name(xr_mem, v));
            break;
        case XM_WR_XADDR:
            v = xm_getw(WR_XADDR);
            debug_printf("0x%04x %s", v, val_name(xr_mem, v));
            break;
        case XM_XDATA:
            xwait_mem_ready();
            prev = xm_getw(RD_XADDR);
            v    = xm_getw(XDATA);
            debug_printf("[0x%04x]", v);
            read = xm_getw(RD_XADDR);
            ASSERT(read == (prev + 1), "0x%04x vs 0x%04x + 1", read, prev);
            xm_setw(RD_XADDR, prev - 1);
            xwait_mem_ready();
            break;
        case XM_RD_INCR:
            v = xm_getw(RD_INCR);
            debug_printf("0x%04x", v);
            break;
        case XM_RD_ADDR:
            v = xm_getw(RD_ADDR);
            debug_printf("0x%04x", v);
            break;
        case XM_WR_INCR:
            v = xm_getw(WR_INCR);
            debug_printf("0x%04x", v);
            break;
        case XM_WR_ADDR:
            v = xm_getw(WR_ADDR);
            debug_printf("0x%04x", v);
            break;
        case XM_DATA:
            xwait_mem_ready();
            prev = xm_getw(RD_ADDR);
            incr = xm_getw(RD_INCR);
            v    = xm_getw(DATA);
            debug_printf("[0x%04x]", v);
            read = xm_getw(RD_ADDR);
            ASSERT(read == (prev + incr), "0x%04x vs 0x%04x + 0x%04x", read, prev, incr);
            xm_setw(RD_ADDR, prev - incr);
            xwait_mem_ready();
            break;
        case XM_DATA_2:
            xwait_mem_ready();
            prev = xm_getw(RD_ADDR);
            incr = xm_getw(RD_INCR) << 1;
            l    = xm_getl(DATA);
            v    = (uint16_t)l;
            debug_printf("[0x%04x]", v);
            read = xm_getw(RD_ADDR);
            ASSERT(read == (prev + incr), "0x%04x vs 0x%04x + 0x%04x", read, prev, incr);
            xm_setw(RD_ADDR, prev - incr);
            xwait_mem_ready();
            break;
        case XM_PIXEL_X:
            v = xm_getw(PIXEL_X);
            debug_printf("0x%04x", v);
            break;
        case XM_PIXEL_Y:
            v = xm_getw(PIXEL_Y);
            debug_printf("0x%04x", v);
            break;
        case XM_UART:
            v = xm_getw(UART);
            debug_printf("0x%04x", v);
            break;
        case XM_FEATURE:
            v = xm_getw(FEATURE);
            debug_printf("0x%04x", v);
            break;
    }
}

void print_xm_regs()
{
    for (int r = 0; r < 16; r++)
    {
        print_xm_reg(r);
        debug_printf("\n");
    }
}

void print_xr_regs()
{
    for (int r = 0; r < 0x20; r++)
    {
        if (r >= XR_UNUSED_08 && r <= XR_UNUSED_0F)
            continue;
        print_xr_reg(r);
        debug_printf("\n");
    }
}

char line[4096];

int main()
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }
    debug_printf("Xosera_mon_m68k\n");

    debug_printf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))        // check for XANSI (and disable input cursor if present)
    {
        debug_printf("detected.\n");
    }
    else
    {
        debug_printf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }

#if 0
    debug_printf("Installing Xosera test interrupt handler...");
    install_intr();
    debug_printf("done.\n");
#endif

    debug_printf("\nNOTE: This program is a WIP.\n");
    debug_printf("\n");

    if (xosera_sync())
    {
        print_xm_regs();
    }
    else
    {
        debug_printf("*** Xosera not responding.\n\n");
    }

    bool exit = false;
    do
    {
        debug_printf("\n*");
        dreadline(line, sizeof(line));

        char * line_ptr = line;

        char * cmd = next_token(&line_ptr);

        if (strcmp(cmd, "xm") == 0)
        {
            char * reg = next_token(&line_ptr);

            if (*reg == '\0')
            {
                print_xm_regs();
            }
            else
            {
                str_upper(reg);
                int r = name_val(xm_regs, reg);
                if (r < 0 && isdigit((unsigned char)*reg))
                {
                    r = strtol(reg, NULL, 0);
                }
                else
                {
                    debug_printf("Bad register: \"%s\"\n", reg);
                    continue;
                }

                print_xm_reg(r);

                char * assign = next_token(&line_ptr);
                if (assign[0] == '=')
                {
                    assign++;
                    if (*assign == '\0')
                    {
                        assign = next_token(&line_ptr);
                    }

                    if (isdigit((unsigned char)*assign))
                    {
                        int v = strtol(assign, NULL, 0);

                        debug_printf(" = 0x%04x, ", v);

                        print_xm_reg(r);
                    }
                }
            }
        }
        else if (strcmp(cmd, "xr") == 0)
        {
            print_xr_regs();
        }
        else if (strcmp(cmd, "Z") == 0)
        {
            char * num = next_token(&line_ptr);
            int    n   = strtol(num, NULL, 0);
            if (!isdigit((unsigned char)*num))
            {
                n = -1;
            }
            bool result = xosera_init(n);
            debug_printf("xosera_init(%d) %s\n", n, result ? "succeeded" : "failed");
        }
        else if (strcmp(cmd, "exit") == 0)
        {
            exit = true;
        }
        else
        {
            debug_printf("Commands:\n");
            debug_printf(" xm       - dump xm registers\n");
            debug_printf(" xr       - dump xr registers\n");
            debug_printf(" Z [c]    - detect/init Xosera w/optional reset config #\n");
            debug_printf(" exit     - exit and warm boot\n");
        }
    } while (!exit);

    debug_printf("\nExit...\n");

    reset_vid();
}
