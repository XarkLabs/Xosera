// C++ "driver" for Xosera Verilator simulation
//
// vim: set et ts=4 sw=4
//
// Thanks to Dan "drr" Rodrigues for the amazing icestation-32 project which
// has a nice example of how to use Verilator with Yosys and SDL.  This code
// was created starting with that (so drr gets most of the credit).

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../xosera_m68k_api/xosera_m68k_defs.h"
#include "video_mode_defs.h"

#include "verilated.h"

#include "Vxosera_main.h"

#include "Vxosera_main_colormem.h"
#include "Vxosera_main_vram.h"
#include "Vxosera_main_vram_arb.h"
#include "Vxosera_main_xosera_main.h"
#include "Vxosera_main_xrmem_arb.h"

#define USE_FST 1
#if USE_FST
#include "verilated_fst_c.h"        // for VM_TRACE
#else
#include "verilated_vcd_c.h"        // for VM_TRACE
#endif
#include <SDL.h>        // for SDL_RENDER
#include <SDL_image.h>

#define LOGDIR "sim/logs/"

#define MAX_TRACE_FRAMES 30        // video frames to dump to VCD file (and then screen-shot and exit)
#define MAX_UPLOADS      8         // maximum number of "payload" uploads

// Current simulation time (64-bit unsigned)
vluint64_t main_time         = 0;
vluint64_t first_frame_start = 0;
vluint64_t frame_start_time  = 0;

volatile bool done;
bool          sim_render = SDL_RENDER;
bool          sim_bus    = BUS_INTERFACE;
bool          wait_close = false;

bool vsync_detect = false;
bool vtop_detect  = false;
bool hsync_detect = false;

int          num_uploads;
int          next_upload;
const char * upload_name[MAX_UPLOADS];
uint8_t *    upload_payload[MAX_UPLOADS];
int          upload_size[MAX_UPLOADS];
uint8_t      upload_buffer[128 * 1024];

uint16_t last_read_val;

static FILE * logfile;
static char   log_buff[16384];

static void log_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, stdout);
    fputs(log_buff, logfile);
    va_end(args);
}

static void logonly_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, logfile);
    va_end(args);
}

class BusInterface
{
    const int   BUS_START_TIME = 1000000;        // after init
    const float BUS_CLOCK_DIV  = 5;              // min 5

    static const char * reg_name[];
    enum
    {
        BUS_START,
        BUS_HOLD,
        BUS_STROBEOFF,
        BUS_END
    };

    bool    enable;
    int64_t last_time;
    int     state;
    int     index;
    bool    wait_vsync;
    bool    wait_hsync;
    bool    wait_vtop;
    bool    wait_blit;
    bool    data_upload;
    int     data_upload_mode;
    int     data_upload_num;
    int     data_upload_count;
    int     data_upload_index;

    static int      test_data_len;
    static uint16_t test_data[32768];

public:
public:
    void set_cmdline_data(int argc, char ** argv, int & nextarg)
    {
        size_t len = 0;
        for (int i = nextarg; i < argc && len < sizeof(test_data); i++)
        {
            char * endptr = nullptr;
            int    value  = static_cast<int>(strtoul(argv[i], &endptr, 0) & 0x1fffUL);
            if (endptr != nullptr && *endptr == '\0')
            {
                test_data[len] = value;
                len++;
            }
            else
            {
                break;
            }
        }

        if (len != 0)
        {
            test_data_len = len;
        }
    }

    void init(Vxosera_main * top, bool _enable)
    {
        enable            = _enable;
        index             = 0;
        state             = BUS_START;
        wait_vsync        = false;
        wait_hsync        = false;
        wait_vtop         = false;
        wait_blit         = false;
        data_upload       = false;
        data_upload_mode  = 0;
        data_upload_num   = 0;
        data_upload_count = 0;
        data_upload_index = 0;
        top->bus_cs_n_i   = 1;
    }

    void process(Vxosera_main * top)
    {
        char tempstr[256];
        // bus test
        if (enable && main_time >= BUS_START_TIME)
        {
            if (wait_vsync)
            {
                if (vsync_detect)
                {
                    logonly_printf("[@t=%8lu  ... VSYNC arrives]\n", main_time);
                    wait_vsync   = false;
                    vsync_detect = false;
                }
                return;
            }

            if (wait_vtop)
            {
                if (vtop_detect)
                {
                    logonly_printf("[@t=%8lu  ... VSYNC end arrives]\n", main_time);
                    wait_vtop   = false;
                    vtop_detect = false;
                }
                return;
            }

            if (wait_hsync)
            {
                if (hsync_detect)
                {
                    logonly_printf("[@t=%8lu  ... HSYNC arrives]\n", main_time);
                    wait_hsync = false;
                }
                return;
            }

            int64_t bus_time = (main_time - BUS_START_TIME) / BUS_CLOCK_DIV;

            if (bus_time >= last_time)
            {
                last_time = bus_time + 1;

                // logonly_printf("%5d >= %5d [@bt=%lu] INDEX=%9d 0x%04x%s\n",
                //                bus_time,
                //                last_time,
                //                main_time,
                //                index,
                //                test_data[index],
                //                data_upload ? " UPLOAD" : "");

                // REG_END
                if (!data_upload && test_data[index] == 0xffff)
                {
                    logonly_printf("[@t=%8lu] REG_END hit\n", main_time);
                    done      = true;
                    enable    = false;
                    last_time = bus_time - 1;
                    logonly_printf("%5d >= new last_time = %5d\n", bus_time, last_time);
                    return;
                }
                // REG_WAITVSYNC
                if (!data_upload && test_data[index] == 0xfffe)
                {
                    logonly_printf("[@t=%8lu] Wait VSYNC...\n", main_time);
                    wait_vsync = true;
                    index++;
                    return;
                }
                // REG_WAITVTOP
                if (!data_upload && test_data[index] == 0xfffd)
                {
                    logonly_printf("[@t=%8lu] Wait VTOP (VSYNC end)...\n", main_time);
                    //                    logonly_printf("[@t=%8lu] belayed!\n", main_time);
                    wait_vtop   = true;
                    vtop_detect = false;
                    index++;
                    return;
                }
                // REG_WAIT_BLIT_READY
                if (!data_upload && test_data[index] == 0xfffc)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & (0x0100 << SYS_CTRL_BLIT_FULL_B)))        // blit_full bit
                    {
                        logonly_printf("[@t=%8lu] blit_full clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%8lu] Waiting until SYS_CTRL.L blit_full is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
                    return;
                }
                // REG_WAIT_BLIT_DONE
                if (!data_upload && test_data[index] == 0xfffb)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & (0x0100 << SYS_CTRL_BLIT_BUSY_B)))        // blit_busy bit
                    {
                        logonly_printf("[@t=%8lu] blit_busy clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        logonly_printf(
                            "%5d WB >= [@bt=%lu] INDEX=%9d 0x%04x\n", bus_time, main_time, index, test_data[index]);
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%8lu] Waiting until SYS_CTRL.L blit_busy is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
                    return;
                }
                // REG_WAITHSYNC
                if (!data_upload && test_data[index] == 0xfffa)
                {
                    logonly_printf("[@t=%8lu] Wait HSYNC...\n", main_time);
                    wait_hsync = true;
                    index++;
                    return;
                }

                if (!data_upload && (test_data[index] & 0xfffe) == 0xfff0)
                {
                    data_upload       = upload_size[data_upload_num] > 0;
                    data_upload_mode  = test_data[index] & 0x1;
                    data_upload_count = upload_size[data_upload_num];        // byte count
                    data_upload_index = 0;
                    logonly_printf("[Upload #%d started, %d bytes, mode %s]\n",
                                   data_upload_num + 1,
                                   data_upload_count,
                                   data_upload_mode ? "XR_DATA" : "VRAM_DATA");

                    index++;
                }
                int rd_wr   = (test_data[index] & 0xC000) == 0x8000 ? 1 : 0;
                int bytesel = (test_data[index] & 0x1000) ? 1 : 0;
                int reg_num = (test_data[index] >> 8) & 0xf;
                int data    = test_data[index] & 0xff;

                if (data_upload && state == BUS_START)
                {
                    bytesel = data_upload_index & 1;
                    reg_num = data_upload_mode ? XM_XDATA : XM_DATA;
                    data    = upload_payload[data_upload_num][data_upload_index++];
                }

                switch (state)
                {
                    case BUS_START:

                        top->bus_cs_n_i    = 1;
                        top->bus_bytesel_i = bytesel;
                        top->bus_rd_nwr_i  = rd_wr;
                        top->bus_reg_num_i = reg_num;
                        top->bus_data_i    = data;
                        if (data_upload && data_upload_index < 16)
                        {
                            logonly_printf("[@t=%8lu] ", main_time);
                            snprintf(tempstr,
                                     sizeof(tempstr),
                                     "r[0x%x] %s.%3s",
                                     reg_num,
                                     reg_name[reg_num],
                                     bytesel ? "lsb*" : "msb");
                            logonly_printf("  %-25.25s <= %s%02x%s\n",
                                           tempstr,
                                           bytesel ? "__" : "",
                                           data & 0xff,
                                           bytesel ? "" : "__");
                            if (data_upload_index == 15)
                            {
                                logonly_printf("  ...\n");
                            }
                        }
                        break;
                    case BUS_HOLD:
                        break;
                    case BUS_STROBEOFF:
                        if (rd_wr)
                        {
                            if (!wait_blit)
                            {
                                logonly_printf("[@t=%8lu] Read  Reg %s (#%02x.%s) => %s%02x%s\n",
                                               main_time,
                                               reg_name[reg_num],
                                               reg_num,
                                               bytesel ? "L" : "H",
                                               bytesel ? "__" : "",
                                               top->bus_data_o,
                                               bytesel ? "" : "__");
                            }
                            if (bytesel)
                            {
                                last_read_val = (last_read_val & 0xff00) | top->bus_data_o;
                            }
                            else
                            {
                                last_read_val = (last_read_val & 0x00ff) | (top->bus_data_o << 8);
                            }
                        }
                        else if (!data_upload)
                        {
                            logonly_printf("[@t=%8lu] Write Reg %s (#%02x.%s) <= %s%02x%s\n",
                                           main_time,
                                           reg_name[reg_num],
                                           reg_num,
                                           bytesel ? "L" : "H",
                                           bytesel ? "__" : "",
                                           top->bus_data_i,
                                           bytesel ? "" : "__");
                        }
                        top->bus_cs_n_i = 0;
                        break;
                    case BUS_END:
                        top->bus_cs_n_i    = 0;
                        top->bus_bytesel_i = 0;
                        top->bus_rd_nwr_i  = 0;
                        top->bus_reg_num_i = 0;
                        top->bus_data_i    = 0;
                        //                        last_time          = bus_time + 9;
                        if (data_upload)
                        {
                            if (data_upload_index >= data_upload_count)
                            {
                                data_upload = false;
                                logonly_printf("[Upload #%d completed]\n", data_upload_num + 1);
                                data_upload_num++;
                            }
                        }
                        else if (++index >= test_data_len)
                        {
                            logonly_printf("*** END of test_data_len ***\n");
                            enable = false;
                        }
                        break;
                    default:
                        assert(false);
                }
                state = state + 1;
                if (state > BUS_END)
                {
                    state = BUS_START;
                }
            }
            else
            {
                //                logonly_printf("%5d < %5d INDEX=%9d\n", bus_time, last_time, index);
            }
        }
    }
};

const char * BusInterface::reg_name[] = {"XM_SYS_CTRL ",
                                         "XM_INT_CTRL ",
                                         "XM_TIMER    ",
                                         "XM_RD_XADDR ",
                                         "XM_WR_XADDR ",
                                         "XM_XDATA    ",
                                         "XM_RD_INCR  ",
                                         "XM_RD_ADDR  ",
                                         "XM_WR_INCR  ",
                                         "XM_WR_ADDR  ",
                                         "XM_DATA     ",
                                         "XM_DATA_2   ",
                                         "XM_PIXEL_X  ",
                                         "XM_PIXEL_Y  ",
                                         "XM_UART",
                                         "XM_FEATURE  "};

#define REG_BH(r, v)     (((XM_##r) | 0x00) << 8) | ((v) & 0xff)
#define REG_BL(r, v)     (((XM_##r) | 0x10) << 8) | ((v) & 0xff)
#define REG_W(r, v)      ((XM_##r) << 8) | (((v) >> 8) & 0xff), (((XM_##r) | 0x10) << 8) | ((v) & 0xff)
#define REG_RW(r)        (((XM_##r) | 0x80) << 8), (((XM_##r) | 0x90) << 8)
#define XREG_SETW(xr, v) REG_W(WR_XADDR, XR_##xr), REG_W(XDATA, (v))
#define XREG_GETW(xr)    REG_W(RD_XADDR, (XR_##xr) | XRMEM_READ), REG_RW(XDATA)

#define XMEM_SETW(xrmem, v) REG_W(WR_XADDR, xrmem), REG_W(XDATA, (v))

#define REG_UPLOAD()          0xfff0
#define REG_UPLOAD_AUX()      0xfff1
#define REG_WAITHSYNC()       0xfffa
#define REG_WAIT_BLIT_READY() (((XM_SYS_CTRL) | 0x80) << 8), 0xfffc
#define REG_WAIT_BLIT_DONE()  (((XM_SYS_CTRL) | 0x80) << 8), 0xfffb
#define REG_WAITVTOP()        0xfffd
#define REG_WAITVSYNC()       0xfffe
#define REG_END()             0xffff

#define X_COLS 80
#define W_4BPP (320 / 4)
#define H_4BPP (240)

#define W_LOGO (32 / 4)
#define H_LOGO (16)

BusInterface bus;
int          BusInterface::test_data_len    = 32767;
uint16_t     BusInterface::test_data[32768] = {
    // test data

    REG_WAITHSYNC(),
    REG_WAITVTOP(),
    REG_WAIT_BLIT_DONE(),
    // initialize non-zero Xosera registers
    XREG_SETW(VID_CTRL, 0x0008),
    XREG_SETW(VID_LEFT, 0),
    XREG_SETW(VID_RIGHT, VISIBLE_WIDTH),

    XREG_SETW(PA_GFX_CTRL, 0x0080),
    XREG_SETW(PA_TILE_CTRL, 0x000F),
    XREG_SETW(PA_LINE_LEN, VISIBLE_WIDTH / 8),
    XREG_SETW(PB_GFX_CTRL, 0x0080),
    XREG_SETW(PB_TILE_CTRL, 0x000F),
    XREG_SETW(PB_LINE_LEN, VISIBLE_WIDTH / 8),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#if 0        // this clears all VRAM
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_ANDC, 0x0000),              // no ANDC
    XREG_SETW(BLIT_XOR, 0x0000),               // no XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),             // no S modulo
    XREG_SETW(BLIT_SRC_S, 0x0000),             // fill const
    XREG_SETW(BLIT_MOD_D, 0x0000),             // no B modulo (contiguous output)
    XREG_SETW(BLIT_DST_D, 0x0000),             // VRAM display start address line 0
    XREG_SETW(BLIT_SHIFT, 0xFF00),             // no edge masking or shifting
    XREG_SETW(BLIT_LINES, 0x0000),             // repeat 1 time
    XREG_SETW(BLIT_WORDS, 0x10000 - 1),        // size of VRAM
    REG_WAIT_BLIT_DONE(),
#endif

    XREG_SETW(POINTER_H, OFFSCREEN_WIDTH + 390),
    XREG_SETW(POINTER_V, 0xF000 | 100),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#if 0
    REG_W(PIXEL_X, 0xC000),        // base
    REG_W(PIXEL_Y, 0x0100),        // width
    REG_BH(SYS_CTRL, 0x00),         // set base, width

    REG_W(PIXEL_X, 0x0000),
    REG_W(PIXEL_Y, 0x0000),
    REG_W(DATA, 0xAAAA),

    REG_W(PIXEL_X, 0x0000),
    REG_W(PIXEL_Y, 0x0005),
    REG_W(DATA, 0x5555),

    REG_W(PIXEL_X, 0x0005),
    REG_W(PIXEL_Y, 0x0000),
    REG_W(DATA, 0xAAAA),

    REG_W(PIXEL_X, 0x0005),
    REG_W(PIXEL_Y, 0x0005),
    REG_W(DATA, 0x5555),

    REG_W(PIXEL_X, 0xC000),        // base
    REG_W(PIXEL_Y, 0x0100),        // width
    REG_BH(FEATURE, 0x01),         // set base, width

    REG_W(PIXEL_X, 0x0000),
    REG_W(PIXEL_Y, 0x0000),
    REG_W(DATA, 0xAAAA),

    REG_W(PIXEL_X, 0x0000),
    REG_W(PIXEL_Y, 0x0005),
    REG_W(DATA, 0x5555),

    REG_W(PIXEL_X, 0x0005),
    REG_W(PIXEL_Y, 0x0000),
    REG_W(DATA, 0xAAAA),

    REG_W(PIXEL_X, 0x0004),
    REG_W(PIXEL_Y, 0x0005),
    REG_W(DATA, 0x5555),

    REG_W(PIXEL_X, 0x0005),
    REG_W(PIXEL_Y, 0x0005),
    REG_W(DATA, 0x5555),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#endif

#if 0
    REG_W(WR_INCR, 0x10),
    REG_W(WR_ADDR, 0x2000),
    REG_W(DATA, 1234),
    REG_W(DATA, 5678),

    REG_W(WR_XADDR, XR_COLOR_A_ADDR),
    REG_W(XDATA, 0x1234),
    REG_W(XDATA, 0x5678),

    REG_W(RD_XADDR, XR_COLOR_A_ADDR),
    REG_RW(XDATA),
    REG_RW(XDATA),
#endif

#if 0
    // copper bar sample
    XREG_SETW(POINTER_V, 0xF000 | 480),
    XREG_SETW(PA_GFX_CTRL, 0x0000),        // blank screen
    XREG_SETW(PB_GFX_CTRL, 0x0080),        // blank screen
    XREG_SETW(VID_CTRL, 0x0000),           // border color #0

#include "color_bar_table.vsim.h"

    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_W(DATA, 0x0F00 | 'X'),
    REG_BL(DATA, 'o'),
    REG_BL(DATA, 's'),
    REG_BL(DATA, 'e'),
    REG_BL(DATA, 'r'),
    REG_BL(DATA, 'a'),
    REG_BL(DATA, ' '),
    REG_BL(DATA, 'c'),
    REG_BL(DATA, 'o'),
    REG_BL(DATA, 'p'),
    REG_BL(DATA, 'p'),
    REG_BL(DATA, 'e'),
    REG_BL(DATA, 'r'),
    REG_BL(DATA, ' '),
    REG_BL(DATA, 'r'),
    REG_BL(DATA, 'a'),
    REG_BL(DATA, 's'),
    REG_BL(DATA, 't'),
    REG_BL(DATA, 'e'),
    REG_BL(DATA, 'r'),
    REG_BL(DATA, ' '),
    REG_BL(DATA, 'b'),
    REG_BL(DATA, 'a'),
    REG_BL(DATA, 'r'),
    REG_BL(DATA, ' '),
    REG_BL(DATA, 'e'),
    REG_BL(DATA, 'x'),
    REG_BL(DATA, 'a'),
    REG_BL(DATA, 'm'),
    REG_BL(DATA, 'p'),
    REG_BL(DATA, 'l'),
    REG_BL(DATA, 'e'),

    XREG_SETW(COPP_CTRL, 0x8000),        // enable copper
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    XREG_SETW(COPP_CTRL, 0x0000),        // disable copper
#endif


#if 1
    REG_W(SYS_CTRL, 0x000F),                // write mask
    XREG_SETW(PA_GFX_CTRL, 0x005F),         // bitmap, 4-bpp, Hx4, Vx4
    XREG_SETW(PA_TILE_CTRL, 0x000F),        // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),        // display start address
    XREG_SETW(PA_LINE_LEN, W_4BPP),         // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    // REG_W(WR_XADDR, XR_COLOR_ADDR),        // upload color palette
    // REG_UPLOAD_AUX(),
    // upload moto logo to 0xF000
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0xF000),
    REG_UPLOAD(),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    // fill screen with dither with 0 = transparency
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0001),             // constS, no transp
    XREG_SETW(BLIT_ANDC, 0x0000),             // no ANDC
    XREG_SETW(BLIT_XOR, 0x0000),              // no XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),            // no S modulo
    XREG_SETW(BLIT_SRC_S, 0x8888),            // fill const
    XREG_SETW(BLIT_MOD_D, 0x0000),            // no B modulo (contiguous output)
    XREG_SETW(BLIT_DST_D, 0x0000),            // VRAM display start address line 0
    XREG_SETW(BLIT_SHIFT, 0xFF00),            // no edge masking or shifting
    XREG_SETW(BLIT_LINES, H_4BPP - 1),        // screen height -1
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),        // screen width in words -1

    REG_WAIT_BLIT_DONE(),
    REG_WAITVTOP(),

    // fill screen with dither with 0 = opaque
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0011),                   // constS, enable 4-bit transp=0x00
    XREG_SETW(BLIT_ANDC, 0x0000),                   // no ANDC
    XREG_SETW(BLIT_XOR, 0x0000),                    // no XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                  // no S modulo
    XREG_SETW(BLIT_SRC_S, 0x1010),                  // fill const
    XREG_SETW(BLIT_MOD_D, W_4BPP),                  // modulo line width (to skip every other line)
    XREG_SETW(BLIT_DST_D, 0x0000),                  // VRAM display start address line 0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                  // no edge masking or shifting
    XREG_SETW(BLIT_LINES, (H_4BPP / 2) - 1),        // (screen height/2) -1
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),              // screen width in words -1

    REG_WAIT_BLIT_READY(),

    XREG_SETW(BLIT_CTRL, 0x0011),                   // constS, enable 4-bit transp=0x00
    XREG_SETW(BLIT_ANDC, 0x0000),                   // no ANDC
    XREG_SETW(BLIT_XOR, 0x0000),                    // no XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                  // C line XOR (toggle dither pattern)
    XREG_SETW(BLIT_SRC_S, 0x0101),                  // fill const
    XREG_SETW(BLIT_MOD_D, W_4BPP),                  // modulo line width (to skip every other line)
    XREG_SETW(BLIT_DST_D, 0x0000 + W_4BPP),         // VRAM display start address line 1
    XREG_SETW(BLIT_SHIFT, 0xFF00),                  // no edge masking or shifting
    XREG_SETW(BLIT_LINES, (H_4BPP / 2) - 1),        // (screen height/2) -1
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),              // screen width in words -1

    REG_WAITVSYNC(),
    REG_WAITVTOP(),

    // 2D moto blit 0, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000),                             // enable 4-bit transp=0x00
    XREG_SETW(BLIT_ANDC, 0x0000),                             // no ANDC
    XREG_SETW(BLIT_XOR, 0x0000),                              // no XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                            // no S modulo (contiguous source)
    XREG_SETW(BLIT_SRC_S, 0xF000),                            // S = start of moto logo
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                   // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0xFF00),                            // no masking or shifting
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                        // moto graphic width

    // 2D moto blit 1, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                             // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x7801),                            // shift/mask 1 nibble
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 2, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                             // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x3C02),                            // shift/mask 2 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 3, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                             // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x1E03),                            // shift/mask 3 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 0, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, BLIT_CTRL_TRANSP_F),                  // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                             // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0xFF00),                             // no masking or shifting
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                         // moto graphic width

    // 2D moto blit 1, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, BLIT_CTRL_TRANSP_F),                  // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x7801),                             // shift/mask 1 nibble
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 2, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, BLIT_CTRL_TRANSP_F),                  // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x3C02),                             // shift/mask 2 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 3, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, BLIT_CTRL_TRANSP_F),                  // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x1E03),                             // shift/mask 3 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width


    // 2D moto blit 0, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF00 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                             // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0xFF00),                             // no masking or shifting
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                         // moto graphic width

    // 2D moto blit 1, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF00 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x7801),                             // shift/mask 1 nibble
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 2, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF00 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x3C02),                             // shift/mask 2 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 3, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF00 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x0000),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x0000),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x1E03),                             // shift/mask 3 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width


    // 2D moto blit 0, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x3333),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x1111),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, 0x0000),                             // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 28),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0xFF00),                             // no masking or shifting
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                         // moto graphic width

    // 2D moto blit 1, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x3333),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x2222),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 28),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x7801),                             // shift/mask 1 nibble
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 2, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x3333),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x3333),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 28),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x3C02),                             // shift/mask 2 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 3, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0000 | BLIT_CTRL_TRANSP_F),         // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_ANDC, 0x4444),                              // no A line XOR
    XREG_SETW(BLIT_XOR, 0x8888),                               // no A line XOR
    XREG_SETW(BLIT_MOD_S, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_SRC_S, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 28),        // D = start dest address
    XREG_SETW(BLIT_SHIFT, 0x1E03),                             // shift/mask 3 nibbles
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width




#if 1

    // 16-color 320x200 color tut
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    XREG_SETW(PA_GFX_CTRL, 0x0065),           // bitmap, 8-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),          // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),          // display start address
    XREG_SETW(PA_LINE_LEN, (320 / 2)),        // display line word length (320 pixels with 2 pixels per word at 8-bpp)
    XREG_SETW(PB_GFX_CTRL, 0x0080),           // disable

    REG_W(WR_XADDR, XR_COLOR_ADDR),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#endif


    REG_WAIT_BLIT_DONE(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#endif

#if 0
    // true color hack test

    XREG_SETW(PA_GFX_CTRL, 0x0065),         // PA bitmap, 8-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),        // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),        // display start address (start of 8-bpp line data)
    XREG_SETW(PA_LINE_LEN,
              (320 / 2) +
                  (320 / 4)),        // display line word length (combined 8-bit and 4-bit for interleaved lines)

    XREG_SETW(PB_GFX_CTRL, 0x0055),                     // PB bitmap, 4-bpp, Hx2, Vx2
    XREG_SETW(PB_TILE_CTRL, 0x000F),                    // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PB_DISP_ADDR, 0x0000 + (320 / 2)),        // display start address (start of 4-bpp line data)
    XREG_SETW(PB_LINE_LEN,
              (320 / 2) +
                  (320 / 4)),        // display line word length (combined 8-bit and 4-bit for interleaved lines)

    REG_W(WR_XADDR, XR_COLOR_ADDR),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),        // 16x16 logo to 0xF000
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),        // RG 8-bpp + 4-bpp B

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#endif

#if 0
    // slim copper wait test
    XREG_SETW(PA_GFX_CTRL, 0x0080),        // blank screen
    XREG_SETW(PB_GFX_CTRL, 0x0080),        // blank screen
    XREG_SETW(VID_CTRL, 0x0000),           // border color #0

#include "cop_wait_test.vsim.h"

    XREG_SETW(COPP_CTRL, 0x8000),        // enable copper
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#endif

#if 1
    // slim copper test
    XREG_SETW(PA_GFX_CTRL, 0x0080),        // blank screen
    XREG_SETW(PB_GFX_CTRL, 0x0080),        // blank screen
    XREG_SETW(VID_CTRL, 0x0000),           // border color #0

#include "cop_blend_test.vsim.h"


    XREG_SETW(COPP_CTRL, 0x8000),        // enable copper
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#endif


#if 0

    // 16-color 320x200 color tut
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    XREG_SETW(PA_HV_FSCALE, 0x0005),        // 400 line scale
    XREG_SETW(PA_HV_FSCALE, 0x0005),        // 400 line scale

    XREG_SETW(PA_GFX_CTRL, 0x0055),           // bitmap, 8-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),          // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),          // display start address
    XREG_SETW(PA_LINE_LEN, (320 / 4)),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    XREG_SETW(PB_GFX_CTRL, 0x0080),        // disable

    REG_W(WR_XADDR, XR_COLOR_ADDR),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),        // tut
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),        // RG 8-bpp + 4-bpp B

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

#endif

#if 0        // lame audio test
    XREG_SETW(VID_CTRL, 0x0000),
    XREG_SETW(VID_RIGHT, 640 - 4),

    REG_RW(INT_CTRL),
    REG_W(INT_CTRL, 0x000F),
    REG_RW(INT_CTRL),

    REG_W(WR_INCR, 0x0001),        // sample at 0xFF00
    REG_W(WR_ADDR, 0xFF00),
    REG_UPLOAD(),        // upload sample data

    REG_W(WR_INCR, 0x0001),        // sample at 0xFE00
    REG_W(WR_ADDR, 0xFE00),
    REG_UPLOAD(),        // upload sample data

#include "cop_audio_evil.vsim.h"

//    XREG_SETW(PA_GFX_CTRL, 0x0080),        // pf a blank
//    XREG_SETW(PB_GFX_CTRL, 0x0080),        // pf b blank

#define SILENCE_ADDR (XR_TILE_ADDR + XR_TILE_SIZE - 1)
#define PERIOD_TEST  0x300
#define WAVE_TEST    0xFF00
#define WAVE_LEN     0x0001
#define WAVE_LEN3    0x007F

    XMEM_SETW(SILENCE_ADDR, 0xE3E4),
    XREG_SETW(COPP_CTRL, 0x8000),        // enable copper

    XREG_SETW(AUD0_VOL, 0x0000),
    XREG_SETW(AUD0_LENGTH, 0x8000 | 0),
    XREG_SETW(AUD0_START, SILENCE_ADDR),
    XREG_SETW(AUD0_PERIOD, 0x7FFF),

    XREG_SETW(AUD1_VOL, 0x0000),
    XREG_SETW(AUD1_LENGTH, 0x8000 | 0),
    XREG_SETW(AUD1_START, SILENCE_ADDR),
    XREG_SETW(AUD1_PERIOD, 0x7FFF),

    XREG_SETW(AUD2_VOL, 0x0000),
    XREG_SETW(AUD2_LENGTH, 0x8000 | 0),
    XREG_SETW(AUD2_START, SILENCE_ADDR),
    XREG_SETW(AUD2_PERIOD, 0x7FFF),

    XREG_SETW(AUD3_VOL, 0x0000),
    XREG_SETW(AUD3_LENGTH, 0x8000 | 0),
    XREG_SETW(AUD3_START, SILENCE_ADDR),
    XREG_SETW(AUD3_PERIOD, 0x7FFF),

    XREG_SETW(AUD_CTRL, 0x0001),

    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),
    REG_RW(INT_CTRL),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_VOL, 0x2020),
    XREG_SETW(AUD1_VOL, 0x2020),
    XREG_SETW(AUD2_VOL, 0x2020),
    XREG_SETW(AUD3_VOL, 0x2020),
    XREG_SETW(AUD0_PERIOD, 0x8000 | PERIOD_TEST),
    XREG_SETW(AUD1_PERIOD, 0x8000 | (PERIOD_TEST + 1)),
    XREG_SETW(AUD2_PERIOD, 0x8000 | (PERIOD_TEST + 3)),
    XREG_SETW(AUD3_PERIOD, 0x8000 | (PERIOD_TEST - 3)),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    // spam

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),


    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),


    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),


    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),

    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),


    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
#if 0
    REG_WAITVTOP(),
    XREG_SETW(AUD0_PERIOD, 0x8000 | PERIOD_TEST),
    XREG_SETW(AUD0_LENGTH, WAVE_LEN),
    XREG_SETW(AUD0_START, WAVE_TEST),

    XREG_SETW(AUD1_PERIOD, 0x8000 | (PERIOD_TEST * 2)),
    XREG_SETW(AUD1_LENGTH, WAVE_LEN),
    XREG_SETW(AUD1_START, WAVE_TEST),

    XREG_SETW(AUD2_PERIOD, 0x8000 | (PERIOD_TEST * 3)),
    XREG_SETW(AUD2_LENGTH, WAVE_LEN),
    XREG_SETW(AUD2_START, WAVE_TEST),

    XREG_SETW(AUD3_PERIOD, 0x8000 | (PERIOD_TEST * 4)),
    XREG_SETW(AUD3_LENGTH, WAVE_LEN3),
    XREG_SETW(AUD3_START, WAVE_TEST),
    XREG_SETW(VID_CTRL, 0x0001),

    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#endif
    // XREG_SETW(AUD0_VOL, 0x2020),
    // XREG_SETW(AUD1_VOL, 0x2020),
    // XREG_SETW(AUD2_VOL, 0x2020),
    // XREG_SETW(AUD3_VOL, 0x2020),

    // XREG_SETW(AUD_CTRL, 0x0000),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // XREG_SETW(AUD_CTRL, 0x000F),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    REG_RW(INT_CTRL),

    // XREG_SETW(AUD0_VOL, 0x8040),
    // XREG_SETW(AUD1_VOL, 0x8040),
    // XREG_SETW(AUD2_VOL, 0x8040),
    // XREG_SETW(AUD3_VOL, 0x0000),

    // XREG_SETW(AUD_CTRL, 0x0000),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // XREG_SETW(AUD_CTRL, 0x000F),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    // XREG_SETW(AUD0_VOL, 0x8040),
    // XREG_SETW(AUD1_VOL, 0x8040),
    // XREG_SETW(AUD2_VOL, 0x8040),
    // XREG_SETW(AUD3_VOL, 0x8040),

    // XREG_SETW(AUD_CTRL, 0x0000),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // REG_WAITHSYNC(),
    // XREG_SETW(AUD_CTRL, 0x000F),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#if 0        // min vol test
    XREG_SETW(AUD0_VOL, 0x0200),        // minimum
    XREG_SETW(AUD1_VOL, 0x0000),
    XREG_SETW(AUD2_VOL, 0x0000),
    XREG_SETW(AUD3_VOL, 0x0000),

    XREG_SETW(AUD_CTRL, 0x0000),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    XREG_SETW(AUD_CTRL, 0x000F),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#endif
    XREG_SETW(AUD_CTRL, 0x0000),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
    REG_WAITHSYNC(),
#endif

    REG_W(INT_CTRL, 0x8100),

    REG_END(),
    // end test data
};

#if 0
uint16_t     BusInterface::test_stfont[1024] = {REG_W(WR_ADDR, 0x3),
                                          REG_W(WR_INC, 0x1),
                                          REG_W(DATA, 0x0200 | 'H'),
                                          REG_BL(DATA, 'e'),
                                          REG_BL(DATA, 'l'),
                                          REG_BL(DATA, 'l'),
                                          REG_BL(DATA, 'o'),
                                          REG_BL(DATA, '!'),
                                          REG_W(WR_ADDR, 0x0),
                                          REG_W(DATA, 0x0e00 | '\x0e'),
                                          REG_W(DATA, 0x0e00 | '\x0f'),
                                          REG_W(WR_ADDR, X_COLS * 5),
                                          REG_W(DATA, 0x0200 | 'A'),
                                          REG_BL(DATA, 't'),
                                          REG_BL(DATA, 'a'),
                                          REG_BL(DATA, 'r'),
                                          REG_BL(DATA, 'i'),
                                          REG_BL(DATA, ' '),
                                          REG_BL(DATA, 'S'),
                                          REG_BL(DATA, 'T'),
                                          REG_BL(DATA, ' '),
                                          REG_BL(DATA, '8'),
                                          REG_BL(DATA, 'x'),
                                          REG_BL(DATA, '1'),
                                          REG_BL(DATA, '6'),
                                          REG_BL(DATA, ' '),
                                          REG_BL(DATA, 'F'),
                                          REG_BL(DATA, 'o'),
                                          REG_BL(DATA, 'n'),
                                          REG_BL(DATA, 't'),
                                          REG_BL(DATA, ' '),
                                          REG_BL(DATA, 'T'),
                                          REG_BL(DATA, 'e'),
                                          REG_BL(DATA, 's'),
                                          REG_BL(DATA, 't'),
                                          REG_BL(DATA, ' '),
                                          REG_BL(DATA, '\x1c'),
                                          REG_BL(WR_INC, X_COLS - 1),
                                          REG_BL(DATA, '\x1d'),
                                          REG_BL(WR_INC, 1),
                                          REG_BL(DATA, '\x1e'),
                                          REG_BL(DATA, '\x1f'),
                                          REG_END()};
#endif

void ctrl_c(int s)
{
    (void)s;
    done = true;
}

// Called by $time in Verilog
double sc_time_stamp()
{
    return main_time;
}

int main(int argc, char ** argv)
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = ctrl_c;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    if ((logfile = fopen("sim/logs/xosera_vsim.log", "w")) == NULL)
    {
        if ((logfile = fopen("xosera_vsim.log", "w")) == NULL)
        {
            printf("can't create xosera_vsim.log (in \"sim/logs/\" or current directory)\n");
            exit(EXIT_FAILURE);
        }
    }

    double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
    log_printf("\nXosera simulation. Video Mode: %dx%d @%0.02fHz clock %0.03fMhz\n",
               VISIBLE_WIDTH,
               VISIBLE_HEIGHT,
               Hz,
               PIXEL_CLOCK_MHZ);

    int nextarg = 1;

    while (nextarg < argc && (argv[nextarg][0] == '-' || argv[nextarg][0] == '/'))
    {
        if (strcmp(argv[nextarg] + 1, "n") == 0)
        {
            sim_render = false;
        }
        else if (strcmp(argv[nextarg] + 1, "b") == 0)
        {
            sim_bus = true;
        }
        else if (strcmp(argv[nextarg] + 1, "w") == 0)
        {
            wait_close = true;
        }
        if (strcmp(argv[nextarg] + 1, "u") == 0)
        {
            nextarg += 1;
            if (nextarg >= argc)
            {
                printf("-u needs filename\n");
                exit(EXIT_FAILURE);
            }
            // upload_data = true;
            upload_name[num_uploads] = argv[nextarg];
            num_uploads++;
        }
        nextarg += 1;
    }

    if (num_uploads)
    {
        for (int u = 0; u < num_uploads; u++)
        {
            logonly_printf("Reading upload data #%d: \"%s\"...", u + 1, upload_name[u]);
            FILE * bfp = fopen(upload_name[u], "r");
            if (bfp != nullptr)
            {
                int read_size = fread(upload_buffer, 1, sizeof(upload_buffer), bfp);
                fclose(bfp);

                if (read_size > 0)
                {
                    logonly_printf("read %d bytes.\n", read_size);
                    upload_size[u]    = read_size;
                    upload_payload[u] = (uint8_t *)malloc(read_size);
                    memcpy(upload_payload[u], upload_buffer, read_size);
                }
                else
                {
                    fprintf(stderr, "Reading upload data \"%s\" error ", upload_name[u]);
                    perror("fread failed");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fprintf(stderr, "Reading upload data \"%s\" error ", upload_name[u]);
                perror("fopen failed");
                exit(EXIT_FAILURE);
            }
        }
    }


#if BUS_INTERFACE
    // bus test data init
    bus.set_cmdline_data(argc, argv, nextarg);
#endif

    Verilated::commandArgs(argc, argv);

#if VM_TRACE
    Verilated::traceEverOn(true);
#endif

    Vxosera_main * top = new Vxosera_main;

#if SDL_RENDER
    SDL_Renderer * renderer = nullptr;
    SDL_Window *   window   = nullptr;
    if (sim_render)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
        {
            fprintf(stderr, "IMG_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }

        window = SDL_CreateWindow(
            "Xosera-sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TOTAL_WIDTH, TOTAL_HEIGHT, SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        SDL_RenderSetScale(renderer, 1, 1);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    bool shot_all  = true;        // screenshot all frames
    bool take_shot = false;

#endif        // SDL_RENDER
    int  current_x          = 0;
    int  current_y          = 0;
    bool vga_hsync_previous = false;
    bool vga_vsync_previous = false;
    bool vga_dv_previous    = false;
    int  frame_num          = -1;
    int  x_max              = 0;
    int  y_max              = 0;
    int  hsync_count = 0, hsync_min = 0, hsync_max = 0;
    int  vsync_count  = 0;
    bool image_loaded = false;

#if VM_TRACE
#if USE_FST
    const auto trace_path = LOGDIR "xosera_vsim.fst";
    logonly_printf("Writing FST waveform file to \"%s\"...\n", trace_path);
    VerilatedFstC * tfp = new VerilatedFstC;
#else
    const auto trace_path = LOGDIR "xosera_vsim.vcd";
    logonly_printf("Writing VCD waveform file to \"%s\"...\n", trace_path);
    VerilatedVcdC * tfp = new VerilatedVcdC;
#endif

    top->trace(tfp, 99);        // trace to heirarchal depth of 99
    tfp->open(trace_path);
#endif

    top->reset_i = 1;        // start in reset

    bus.init(top, sim_bus);

    while (!done && !Verilated::gotFinish())
    {
        if (main_time == 4)
        {
            top->reset_i = 0;        // tale out of reset after 2 cycles
        }

#if BUS_INTERFACE
        bus.process(top);
#endif

        top->eval();         // see https://lawrie.github.io/blackicemxbook/Simulation/Simulation.html
        top->clk = 1;        // clock rising
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif

        if (top->reconfig_o)
        {
            log_printf("FPGA RECONFIG: config #0x%x\n", top->boot_select_o);
            done = true;
        }

        if (top->bus_intr_o)
        {
            logonly_printf("[@t=%8lu FPGA INTERRUPT]\n", main_time);
        }

        if (frame_num > 1)
        {
            if (top->xosera_main->vram_arb->regs_ack_o)
            {
                if (top->xosera_main->vram_arb->regs_wr_i)
                {
                    logonly_printf(" => regs write VRAM[0x%04x]<=0x%04x\n",
                                   top->xosera_main->vram_arb->regs_addr_i,
                                   top->xosera_main->vram_arb->regs_data_i);
                }
                else
                {
                    logonly_printf(" <= regs read VRAM[0x%04x]=>0x%04x\n",
                                   top->xosera_main->vram_arb->regs_addr_i,
                                   top->xosera_main->vram_arb->vram_data_o);
                }
            }
#if 0
            if (top->xosera_main->xrmem_arb->xr_ack_o)
            {
                if (top->xosera_main->xrmem_arb->xr_wr_i)
                {
                    logonly_printf(" => regs write XR[0x%04x]<=0x%04x\n",
                                   top->xosera_main->xrmem_arb->xr_addr_i,
                                   top->xosera_main->xrmem_arb->xr_data_i);
                }
                else
                {
                    logonly_printf(" <= regs read XR[0x%04x]=>0x%04x\n",
                                   top->xosera_main->xrmem_arb->xr_addr_i,
                                   top->xosera_main->xrmem_arb->xr_data_o);
                }
            }

            if (top->xosera_main->xrmem_arb->copp_xr_sel_i)
            {
                logonly_printf(" => COPPER XR write XR[0x%04x]<=0x%04x\n",
                               top->xosera_main->xrmem_arb->copp_xr_addr_i,
                               top->xosera_main->xrmem_arb->copp_xr_data_i);
            }
#endif
        }

        bool hsync = H_SYNC_POLARITY ? top->hsync_o : !top->hsync_o;
        bool vsync = V_SYNC_POLARITY ? top->vsync_o : !top->vsync_o;

#if SDL_RENDER
        if (sim_render)
        {
            if (top->dv_de_o)
            {
                // sim_render current VGA output pixel (4 bits per gun)
                SDL_SetRenderDrawColor(renderer,
                                       (top->red_o << 4) | top->red_o,
                                       (top->green_o << 4) | top->green_o,
                                       (top->blue_o << 4) | top->blue_o,
                                       255);
            }
            else
            {
                if (top->red_o != 0 || top->green_o != 0 || top->blue_o != 0)
                {
                    log_printf("Frame %3u pixel %d, %d RGB is 0x%02x 0x%02x 0x%02x when NOT visible\n",
                               frame_num,
                               current_x,
                               current_y,
                               top->red_o,
                               top->green_o,
                               top->blue_o);
                }

                // sim_render dithered border area
                if (((current_x ^ current_y) & 1) == 1)        // non-visible
                {
                    // dither with dimmed color 0 // TODO: fix border
                    //                    auto       vmem    = top->xosera_main->xrmem_arb->colormem->bram;
                    //                    uint16_t * color0p = &vmem[0];
                    uint16_t color0 = 0;        //*color0p;
                    SDL_SetRenderDrawColor(
                        renderer, ((color0 & 0x0f00) >> 5), ((color0 & 0x00f0) >> 1), ((color0 & 0x000f) << 7), 255);
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, 0x21, vsync ? 0x41 : 0x21, hsync ? 0x41 : 0x21, 0xff);
                }
            }

            if (frame_num > 0)
            {
                SDL_RenderDrawPoint(renderer, current_x, current_y);
            }
        }
#endif
        current_x++;

        if (hsync)
            hsync_count++;

        hsync_detect = false;

        // end of hsync
        if (!hsync && vga_hsync_previous)
        {
            hsync_detect = true;
            if (hsync_count > hsync_max)
                hsync_max = hsync_count;
            if (hsync_count < hsync_min || !hsync_min)
                hsync_min = hsync_count;
            hsync_count = 0;

            if (current_x > x_max)
                x_max = current_x;

            current_x = 0;
            current_y++;

            if (vsync)
                vsync_count++;
        }
        vga_hsync_previous = hsync;

        vsync_detect = false;

        if (vsync && !vga_vsync_previous)
        {
            vtop_detect = true;
        }

        if (!vsync && vga_vsync_previous)
        {
            vsync_detect = true;
            if (current_y - 1 > y_max)
                y_max = current_y - 1;

            if (frame_num > 0)
            {
                if (frame_num == 1)
                {
                    first_frame_start = main_time;
                }
                vluint64_t frame_time = (main_time - frame_start_time) / 2;
                logonly_printf(
                    "[@t=%8lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
                    main_time,
                    frame_num,
                    frame_time,
                    ((1.0 / PIXEL_CLOCK_MHZ) * frame_time) / 1000.0,
                    x_max,
                    y_max + 1,
                    hsync_max,
                    vsync_count);

#if SDL_RENDER

                if (sim_render)
                {
                    if (shot_all || take_shot || frame_num == MAX_TRACE_FRAMES)
                    {
                        int  w = 0, h = 0;
                        char save_name[256] = {0};
                        SDL_GetRendererOutputSize(renderer, &w, &h);
                        SDL_Surface * screen_shot =
                            SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                        SDL_RenderReadPixels(
                            renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screen_shot->pixels, screen_shot->pitch);
                        snprintf(save_name,
                                 sizeof(save_name),
                                 LOGDIR "xosera_vsim_%dx%d_f%02d.png",
                                 VISIBLE_WIDTH,
                                 VISIBLE_HEIGHT,
                                 frame_num);
                        IMG_SavePNG(screen_shot, save_name);
                        SDL_FreeSurface(screen_shot);
                        float fnum = ((1.0 / PIXEL_CLOCK_MHZ) * ((main_time - first_frame_start) / 2)) / 1000.0;
                        log_printf("[@t=%8lu] %8.03f ms frame #%3u saved as \"%s\" (%dx%d)\n",
                                   main_time,
                                   fnum,
                                   frame_num,
                                   save_name,
                                   w,
                                   h);
                        take_shot = false;
                    }

                    SDL_RenderPresent(renderer);
                    SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 0xff);
                    SDL_RenderClear(renderer);
                }
#endif
            }
            frame_start_time = main_time;
            hsync_min        = 0;
            hsync_max        = 0;
            vsync_count      = 0;
            current_y        = 0;

            if (frame_num == MAX_TRACE_FRAMES)
            {
                break;
            }

            if (TOTAL_HEIGHT == y_max + 1)
            {
                frame_num += 1;
            }
            else if (TOTAL_HEIGHT <= y_max)
            {
                log_printf("line %d >= TOTAL_HEIGHT\n", y_max);
            }
        }

        vga_vsync_previous = vsync;

        main_time++;

        top->clk = 0;        // clock falling
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

#if SDL_RENDER
        if (sim_render)
        {
            SDL_Event e;
            SDL_PollEvent(&e);

            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.mod == 0))
            {
                log_printf("Window closed\n");
                break;
            }
        }
#endif
    }

#if 0
    FILE * mfp = fopen(LOGDIR "xosera_vsim_text.txt", "w");
    if (mfp != nullptr)
    {
        auto       vmem = top->xosera_main->vram_arb->vram->memory;
        uint16_t * mem  = &vmem[0];

        for (int y = 0; y < VISIBLE_HEIGHT / 16; y++)
        {
            fprintf(mfp, "%04x: ", y * (VISIBLE_WIDTH / 8));
            for (int x = 0; x < VISIBLE_WIDTH / 8; x++)
            {
                auto m      = mem[y * (VISIBLE_WIDTH / 8) + x];
                char str[4] = {0};
                if (isprint(m & 0xff))
                {
                    sprintf(str, "'%c", m & 0xff);
                }
                else
                {
                    sprintf(str, "%02x", m & 0xff);
                }

                fprintf(mfp, "%02x%s ", m >> 8, str);
            }
            fprintf(mfp, "\n");
        }
        fclose(mfp);
    }

    {
        FILE * bfp = fopen(LOGDIR "xosera_vsim_vram.bin", "w");
        if (bfp != nullptr)
        {
            auto       vmem = top->xosera_main->vram_arb->vram->memory;
            uint16_t * mem  = &vmem[0];
            fwrite(mem, 128 * 1024, 1, bfp);
            fclose(bfp);
        }
    }

    {
        FILE * tfp = fopen(LOGDIR "xosera_vsim_vram_hex.txt", "w");
        if (tfp != nullptr)
        {
            auto       vmem = top->xosera_main->vram_arb->vram->memory;
            uint16_t * mem  = &vmem[0];
            for (int i = 0; i < 65536; i += 16)
            {
                fprintf(tfp, "%04x:", i);
                for (int j = i; j < i + 16; j++)
                {
                    fprintf(tfp, " %04x", mem[j]);
                }
                fprintf(tfp, "\n");
            }
            fclose(tfp);
        }
    }
#endif

    top->final();

#if VM_TRACE
    tfp->close();
#endif

#if SDL_RENDER
    if (sim_render)
    {
        if (!wait_close)
        {
            SDL_Delay(1000);
        }
        else
        {
            fprintf(stderr, "Press RETURN:\n");
            fgetc(stdin);
        }

        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
#endif

    log_printf("Simulation ended after %d frames, %lu pixel clock ticks (%.04f milliseconds)\n",
               frame_num,
               (main_time / 2),
               ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);

    return EXIT_SUCCESS;
}
