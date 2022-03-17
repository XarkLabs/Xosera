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

#include "xosera_defs.h"

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

#define MAX_TRACE_FRAMES 10        // video frames to dump to VCD file (and then screen-shot and exit)
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
    const float BUS_CLOCK_DIV  = 5;              // min 4

    enum
    {
        XM_XR_ADDR = 0x0,        // (R /W+) XR register number/address for XM_XR_DATA read/write access
        XM_XR_DATA = 0x1,        // (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
        XM_RD_INCR = 0x2,        // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
        XM_RD_ADDR = 0x3,        // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
        XM_WR_INCR = 0x4,        // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
        XM_WR_ADDR = 0x5,        // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
        XM_DATA   = 0x6,        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR (and add XM_RD_INCR/XM_WR_INCR)
        XM_DATA_2 = 0x7,        // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
        XM_SYS_CTRL  = 0x8,        // (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
        XM_TIMER     = 0x9,        // (RO   ) read 1/10th millisecond timer
        XM_LFSR      = 0xA,        // (R /W ) LFSR pseudo-random register // TODO: keep this?
        XM_UNUSED_B  = 0xB,        // (R /W ) unused direct register 0xB // TODO: Use for XM_XR_DATA_2
        XM_RW_INCR   = 0xC,        // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
        XM_RW_ADDR   = 0xD,        // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
        XM_RW_DATA   = 0xE,        // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
        XM_RW_DATA_2 = 0xF         // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)
    };

    enum
    {
        XR_COLOR_ADDR   = 0x8000,        // (R/W) 0x8000-0x81FF 2 x A & B color lookup memory
        XR_COLOR_SIZE   = 0x0200,        //                      2 x 256 x 16-bit words  (0xARGB)
        XR_COLOR_A_ADDR = 0x8000,        // (R/W) 0x8000-0x80FF A 256 entry color lookup memory
        XR_COLOR_A_SIZE = 0x0100,        //                      256 x 16-bit words (0xARGB)
        XR_COLOR_B_ADDR = 0x8100,        // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
        XR_COLOR_B_SIZE = 0x0100,        //                      256 x 16-bit words (0xARGB)
        XR_TILE_ADDR    = 0xA000,        // (R/W) 0xA000-0xB3FF tile glyph/tile map memory
        XR_TILE_SIZE    = 0x1400,        //                      5120 x 16-bit tile glyph/tile map memory
        XR_COPPER_ADDR  = 0xC000,        // (R/W) 0xC000-0xC7FF copper program memory (32-bit instructions)
        XR_COPPER_SIZE  = 0x0800,        //                      2048 x 16-bit copper program memory addresses
        XR_UNUSED_ADDR  = 0xE000         // (-/-) 0xE000-0xFFFF unused
    };

    enum
    {
        // Video Config / Copper XR Registers
        XR_VID_CTRL  = 0x00,        // (R /W) display control and border color index
        XR_COPP_CTRL = 0x01,        // (R /W) display synchronized coprocessor control
        XR_UNUSED_02 = 0x02,        // (R /W) // TODO:
        XR_UNUSED_03 = 0x03,        // (R /W) // TODO:
        XR_UNUSED_04 = 0x04,        // (R /W) // TODO:
        XR_UNUSED_05 = 0x05,        // (R /W) // TODO:
        XR_VID_LEFT  = 0x06,        // (R /W) left edge of active display window (typically 0)
        XR_VID_RIGHT = 0x07,        // (R /W) right edge of active display window (typically 639 or 847)
        XR_SCANLINE  = 0x08,        // (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline
        XR_UNUSED_09 = 0x09,        // (RO  )
        XR_VERSION   = 0x0A,        // (RO  ) Xosera optional feature bits [15:8] and version code [7:0] [TODO]
        XR_GITHASH_H = 0x0B,        // (RO  ) [15:0] high 16-bits of 32-bit Git hash build identifier
        XR_GITHASH_L = 0x0C,        // (RO  ) [15:0] low 16-bits of 32-bit Git hash build identifier
        XR_VID_HSIZE = 0x0D,        // (RO  ) native pixel width of monitor mode (e.g. 640/848)
        XR_VID_VSIZE = 0x0E,        // (RO  ) native pixel height of monitor mode (e.g. 480)
        XR_VID_VFREQ = 0x0F,        // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)

        // Playfield A Control XR Registers
        XR_PA_GFX_CTRL  = 0x10,        //  playfield A graphics control
        XR_PA_TILE_CTRL = 0x11,        //  playfield A tile control
        XR_PA_DISP_ADDR = 0x12,        //  playfield A display VRAM start address
        XR_PA_LINE_LEN  = 0x13,        //  playfield A display line width in words
        XR_PA_HV_SCROLL = 0x14,        //  playfield A horizontal and vertical fine scroll
        XR_PA_LINE_ADDR = 0x15,        //  playfield A scanline start address (loaded at start of line)
        XR_PA_HV_FSCALE = 0x16,        //  playfield A horizontal and vertical fractional scale
        XR_PA_UNUSED_17 = 0x17,        //

        // Playfield B Control XR Registers
        XR_PB_GFX_CTRL  = 0x18,        //  playfield B graphics control
        XR_PB_TILE_CTRL = 0x19,        //  playfield B tile control
        XR_PB_DISP_ADDR = 0x1A,        //  playfield B display VRAM start address
        XR_PB_LINE_LEN  = 0x1B,        //  playfield B display line width in words
        XR_PB_HV_SCROLL = 0x1C,        //  playfield B horizontal and vertical fine scroll
        XR_PB_LINE_ADDR = 0x1D,        //  playfield B scanline start address (loaded at start of line)
        XR_PB_HV_FSCALE = 0x1E,        //  playfield B horizontal and vertical fractional scale
        XR_PB_UNUSED_1F = 0x1F,        //

        // Blitter Registers
        XR_BLIT_CTRL  = 0x20,        // (R /W) blit control (transparency control, logic op and op input flags)
        XR_BLIT_MOD_A = 0x21,        // (R /W) blit line modulo added to SRC_A (XOR if A const)
        XR_BLIT_SRC_A = 0x22,        // (R /W) blit A source VRAM read address / constant value
        XR_BLIT_MOD_B = 0x23,        // (R /W) blit line modulo added to SRC_B (XOR if B const)
        XR_BLIT_SRC_B = 0x24,        // (R /W) blit B AND source VRAM read address / constant value
        XR_BLIT_MOD_C = 0x25,        // (R /W) blit line XOR modifier for C_VAL const
        XR_BLIT_VAL_C = 0x26,        // (R /W) blit C XOR constant value
        XR_BLIT_MOD_D = 0x27,        // (R /W) blit modulo added to D destination after each line
        XR_BLIT_DST_D = 0x28,        // (R /W) blit D VRAM destination write address
        XR_BLIT_SHIFT = 0x29,        // (R /W) blit first and last word nibble masks and nibble right shift (0-3)
        XR_BLIT_LINES = 0x2A,        // (R /W) blit number of lines minus 1, (repeats blit word count after modulo calc)
        XR_BLIT_WORDS = 0x2B         // (R /W) blit word count minus 1 per line (write starts blit operation)
    };

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
                    logonly_printf("[@t=%lu  ... VSYNC arrives]\n", main_time);
                    wait_vsync = false;
                }
                return;
            }

            if (wait_vtop)
            {
                if (vtop_detect)
                {
                    logonly_printf("[@t=%lu  ... VTOP arrives]\n", main_time);
                    wait_vtop = false;
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
                    logonly_printf("[@t=%lu] REG_END hit\n", main_time);
                    done      = true;
                    enable    = false;
                    last_time = bus_time - 1;
                    logonly_printf("%5d >= new last_time = %5d\n", bus_time, last_time);
                    return;
                }
                // REG_WAITVSYNC
                if (!data_upload && test_data[index] == 0xfffe)
                {
                    logonly_printf("[@t=%lu] Wait VSYNC...\n", main_time);
                    wait_vsync = true;
                    index++;
                    return;
                }
                // REG_WAITVTOP
                if (!data_upload && test_data[index] == 0xfffd)
                {
                    logonly_printf("[@t=%lu] Wait VTOP...\n", main_time);
                    wait_vtop = true;
                    index++;
                    return;
                }
                // REG_WAIT_BLIT_READY
                if (!data_upload && test_data[index] == 0xfffc)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & 0x20))        // blit_full bit
                    {
                        logonly_printf("[@t=%lu] blit_full clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%lu] Waiting until SYS_CTRL.L blit_full is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
                    return;
                }
                // REG_WAIT_BLIT_DONE
                if (!data_upload && test_data[index] == 0xfffb)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & 0x40))        // blit_busy bit
                    {
                        logonly_printf("[@t=%lu] blit_busy clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        logonly_printf(
                            "%5d WB >= [@bt=%lu] INDEX=%9d 0x%04x\n", bus_time, main_time, index, test_data[index]);
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%lu] Waiting until SYS_CTRL.L blit_busy is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
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
                    reg_num = data_upload_mode ? XM_XR_DATA : XM_DATA;
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
                            logonly_printf("[@t=%lu] ", main_time);
                            sprintf(tempstr, "r[0x%x] %s.%3s", reg_num, reg_name[reg_num], bytesel ? "lsb*" : "msb");
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
                                logonly_printf("[@t=%lu] Read  Reg %s (#%02x.%s) => %s%02x%s\n",
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
                            logonly_printf("[@t=%lu] Write Reg %s (#%02x.%s) <= %s%02x%s\n",
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

const char * BusInterface::reg_name[] = {"XM_XR_ADDR  ",
                                         "XM_XR_DATA  ",
                                         "XM_RD_INCR  ",
                                         "XM_RD_ADDR  ",
                                         "XM_WR_INCR  ",
                                         "XM_WR_ADDR  ",
                                         "XM_DATA     ",
                                         "XM_DATA_2   ",
                                         "XM_SYS_CTRL ",
                                         "XM_TIMER    ",
                                         "XM_LFSR     ",
                                         "XM_UNUSED_B ",
                                         "XM_RW_INCR  ",
                                         "XM_RW_ADDR  ",
                                         "XM_RW_DATA  ",
                                         "XM_RW_DATA_2"};

#define REG_B(r, v) (((BusInterface::XM_##r) | 0x10) << 8) | ((v)&0xff)
#define REG_W(r, v)                                                                                                    \
    ((BusInterface::XM_##r) << 8) | (((v) >> 8) & 0xff), (((BusInterface::XM_##r) | 0x10) << 8) | ((v)&0xff)
#define REG_RW(r)        (((BusInterface::XM_##r) | 0x80) << 8), (((BusInterface::XM_##r) | 0x90) << 8)
#define XREG_SETW(xr, v) REG_W(XR_ADDR, XR_##xr), REG_W(XR_DATA, (v))
#define XREG_GETW(xr)    REG_W(XR_ADDR, XR_##xr), REG_RW(XR_DATA)

#define REG_UPLOAD()          0xfff0
#define REG_UPLOAD_AUX()      0xfff1
#define REG_WAIT_BLIT_READY() (((BusInterface::XM_SYS_CTRL) | 0x90) << 8), 0xfffc
#define REG_WAIT_BLIT_DONE()  (((BusInterface::XM_SYS_CTRL) | 0x90) << 8), 0xfffb
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

    REG_WAITVSYNC(),
    REG_WAITVTOP(),

    REG_RW(LFSR),
    REG_RW(LFSR),


    REG_WAITVSYNC(),        // show boot screen
                                //    REG_WAITVTOP(),         // show boot screen


#if 0        // copper torture test
    REG_W(XR_ADDR, XR_COPPER_ADDR),        // setup copper program

    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0000),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0001),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0002),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0003),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0004),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0005),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0006),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0007),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0008),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0009),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x000f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0010),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0011),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0012),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0013),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0014),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0015),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0016),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0017),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0018),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0019),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x001f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0020),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0021),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0022),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0023),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0024),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0025),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0026),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0027),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0028),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0029),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x002f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0030),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0031),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0032),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0033),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0034),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0035),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0036),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0037),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0038),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0039),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x003f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0040),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0041),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0042),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0043),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0044),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0045),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0046),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0047),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0048),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0049),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x004f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0050),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0051),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0052),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0053),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0054),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0055),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0056),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0057),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0058),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0059),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x005f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0060),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0061),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0062),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0063),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0064),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0065),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0066),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0067),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0068),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0069),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x006f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0070),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0071),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0072),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0073),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0074),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0075),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0076),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0077),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0078),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0079),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x007f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0080),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0081),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0082),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0083),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0084),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0085),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0086),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0087),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0088),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0089),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x008f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0090),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0091),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0092),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0093),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0094),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0095),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0096),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0097),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0098),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0099),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x009f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00a9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00aa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ab),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ac),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ad),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ae),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00af),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00b9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ba),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00bb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00bc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00bd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00be),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00bf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00c9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ca),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00cb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00cc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00cd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ce),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00cf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00d9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00da),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00db),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00dc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00dd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00de),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00df),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00e9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ea),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00eb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ec),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ed),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ee),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ef),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00f9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00fa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00fb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00fc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00fd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00fe),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x00ff),

    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0400),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0401),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0402),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0403),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0404),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0405),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0406),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0407),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0408),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0409),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x040f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0410),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0411),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0412),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0413),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0414),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0415),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0416),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0417),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0418),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0419),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x041f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0420),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0421),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0422),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0423),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0424),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0425),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0426),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0427),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0428),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0429),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x042f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0430),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0431),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0432),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0433),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0434),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0435),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0436),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0437),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0438),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0439),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x043f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0440),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0441),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0442),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0443),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0444),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0445),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0446),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0447),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0448),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0449),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x044f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0450),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0451),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0452),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0453),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0454),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0455),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0456),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0457),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0458),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0459),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x045f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0460),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0461),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0462),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0463),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0464),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0465),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0466),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0467),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0468),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0469),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x046f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0470),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0471),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0472),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0473),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0474),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0475),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0476),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0477),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0478),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0479),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x047f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0480),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0481),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0482),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0483),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0484),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0485),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0486),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0487),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0488),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0489),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x048f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0490),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0491),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0492),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0493),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0494),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0495),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0496),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0497),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0498),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0499),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x049f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04a9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04aa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ab),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ac),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ad),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ae),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04af),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04b9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ba),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04bb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04bc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04bd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04be),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04bf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04c9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ca),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04cb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04cc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04cd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ce),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04cf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04d9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04da),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04db),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04dc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04dd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04de),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04df),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04e9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ea),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04eb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ec),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ed),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ee),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ef),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04f9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04fa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04fb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04fc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04fd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04fe),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x04ff),


    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0800),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0801),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0802),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0803),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0804),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0805),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0806),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0807),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0808),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0809),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x080f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0810),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0811),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0812),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0813),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0814),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0815),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0816),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0817),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0818),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0819),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x081f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0820),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0821),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0822),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0823),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0824),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0825),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0826),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0827),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0828),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0829),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x082f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0830),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0831),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0832),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0833),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0834),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0835),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0836),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0837),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0838),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0839),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x083f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0840),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0841),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0842),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0843),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0844),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0845),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0846),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0847),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0848),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0849),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x084f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0850),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0851),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0852),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0853),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0854),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0855),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0856),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0857),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0858),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0859),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x085f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0860),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0861),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0862),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0863),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0864),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0865),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0866),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0867),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0868),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0869),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x086f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0870),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0871),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0872),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0873),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0874),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0875),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0876),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0877),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0878),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0879),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x087f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0880),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0881),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0882),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0883),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0884),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0885),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0886),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0887),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0888),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0889),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x088f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0890),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0891),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0892),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0893),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0894),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0895),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0896),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0897),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0898),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0899),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x089f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08a9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08aa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ab),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ac),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ad),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ae),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08af),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08b9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ba),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08bb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08bc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08bd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08be),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08bf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08c9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ca),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08cb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08cc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08cd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ce),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08cf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08d9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08da),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08db),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08dc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08dd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08de),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08df),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08e9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ea),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08eb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ec),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ed),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ee),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ef),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08f9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08fa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08fb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08fc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08fd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08fe),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x08ff),

    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C00),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C01),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C02),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C03),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C04),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C05),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C06),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C07),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C08),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C09),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C0f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C10),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C11),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C12),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C13),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C14),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C15),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C16),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C17),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C18),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C19),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C1f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C20),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C21),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C22),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C23),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C24),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C25),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C26),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C27),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C28),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C29),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C2f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C30),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C31),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C32),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C33),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C34),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C35),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C36),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C37),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C38),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C39),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C3f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C40),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C41),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C42),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C43),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C44),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C45),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C46),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C47),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C48),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C49),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C4f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C50),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C51),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C52),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C53),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C54),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C55),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C56),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C57),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C58),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C59),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C5f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C60),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C61),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C62),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C63),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C64),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C65),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C66),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C67),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C68),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C69),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C6f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C70),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C71),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C72),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C73),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C74),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C75),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C76),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C77),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C78),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C79),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C7f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C80),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C81),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C82),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C83),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C84),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C85),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C86),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C87),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C88),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C89),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C8f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C90),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C91),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C92),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C93),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C94),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C95),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C96),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C97),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C98),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C99),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9a),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9b),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9c),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9d),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9e),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0C9f),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ca9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Caa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cab),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cac),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cad),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cae),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Caf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cb9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cba),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cbb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cbc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cbd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cbe),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cbf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cc9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cca),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ccb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ccc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ccd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cce),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ccf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cd9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cda),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cdb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cdc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cdd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cde),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cdf),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ce9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cea),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ceb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cec),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Ced),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cee),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cef),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf0),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf1),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf2),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf3),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf4),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf5),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf6),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf7),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf8),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cf9),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cfa),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cfb),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cfc),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cfd),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cfe),
    REG_W(XR_DATA, 0xA000),
    REG_W(XR_DATA, 0x0Cff),

    XREG_SETW(COPP_CTRL, 0x8000),

#endif

    REG_W(RW_INCR, 0x1),
    REG_W(RW_ADDR, 0x1234),
    REG_RW(RW_DATA),
    REG_RW(RW_DATA),
    REG_B(SYS_CTRL, 0x1F),
    REG_W(RW_INCR, 0x1),
    REG_W(RW_ADDR, 0x1234),
    REG_RW(RW_DATA),
    REG_RW(RW_DATA),

    XREG_SETW(PA_GFX_CTRL, 0x005F),         // bitmap, 4-bpp, Hx4, Vx4
    XREG_SETW(PA_TILE_CTRL, 0x000F),        // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),        // display start address
    XREG_SETW(PA_LINE_LEN, 320 / 4),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    // D = A & B ^ C;
    // flags:
    //   notB   - changes B in 2nd term to NOT B
    //   CuseB  - changes C in 3rd term to B value (without notB applied)
    // fill screen with dither with 0 = transparency
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0003),                  // constA, constB, 4-bit trans=0
    XREG_SETW(BLIT_MOD_A, 0x0000),                 // no A line XOR
    XREG_SETW(BLIT_MOD_B, 0x8080 ^ 0x0808),        // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                 // C line XOR (toggle dither pattern)
    XREG_SETW(BLIT_MOD_D, 0x0000),                 // no B modulo (contiguous output)
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                 // nop A const
    XREG_SETW(BLIT_SRC_B, 0x8080),                 // color B const (B also used for transparency test)
    XREG_SETW(BLIT_VAL_C, 0x0000),                 // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000),                 // VRAM display start address line 0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                 // no edge masking or shifting
    XREG_SETW(BLIT_LINES, H_4BPP - 1),             // screen height -1
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),             // screen width in words -1
    REG_WAIT_BLIT_DONE(),

    // fill screen with dither with 0 = opaque
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xEE03),                  // constA, constB, 4-bit trans=E
    XREG_SETW(BLIT_MOD_A, 0x0000),                 // no A line XOR
    XREG_SETW(BLIT_MOD_B, 0x0000),                 // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x8080 ^ 0x0808),        // C line XOR (toggle dither pattern)
    XREG_SETW(BLIT_MOD_D, 0x0000),                 // no B line modulo (contiguous output)
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                 // nop A const
    XREG_SETW(BLIT_SRC_B, 0x0000),                 // color B const (B also used for transparency test)
    XREG_SETW(BLIT_VAL_C, 0x8080),                 // C const initial dither
    XREG_SETW(BLIT_DST_D, 0x0000),                 // VRAM display start address line 0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                 // no edge masking or shifting
    XREG_SETW(BLIT_LINES, H_4BPP - 1),             // screen height -1
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),             // screen width in words -1
    REG_WAIT_BLIT_DONE(),

    REG_W(WR_INCR, 0x0001),        // 16x16 logo to 0xF000
    REG_W(WR_ADDR, 0xF000),
    REG_UPLOAD(),
    // REG_WAITVTOP(),
    REG_WAITVSYNC(),
    //    REG_END(),

    // 2D moto blit 0, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0001),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                            // no masking or shifting
    XREG_SETW(BLIT_MOD_A, 0x0000),                            // no A line modulo (contiguous source)
    XREG_SETW(BLIT_MOD_B, 0x0000),                            // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                            // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                   // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                            // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                            // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                        // moto graphic width

    // 2D moto blit 1, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0001),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x7801),                            // shift/mask 1 nibble
    XREG_SETW(BLIT_MOD_A, 0x000),                             // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                            // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                            // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                            // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 2, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0001),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x3C02),                            // shift/mask 2 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                             // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                            // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                            // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                            // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 3, 0
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0001),                             // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x1E03),                            // shift/mask 3 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                             // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                            // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                            // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                            // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                            // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                        // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                    // moto graphic width

    // 2D moto blit 0, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0002),                              // read A, const B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                             // no masking or shifting
    XREG_SETW(BLIT_MOD_A, 0x0000),                             // no A line modulo (contiguous source)
    XREG_SETW(BLIT_MOD_B, 0x0000),                             // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xF000),                             // moto graphic src A
    XREG_SETW(BLIT_SRC_B, 0xFFFF),                             // nop B const (w/o transparent nibble)
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                         // moto graphic width

    // 2D moto blit 1, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0002),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x7801),                             // shift/mask 1 nibble
    XREG_SETW(BLIT_MOD_A, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_B, 0x0000),                             // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xF000),                             // moto graphic src A
    XREG_SETW(BLIT_SRC_B, 0xFFFF),                             // nop B const (w/o transparent nibble)
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 2, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0002),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x3C02),                             // shift/mask 2 nibbles
    XREG_SETW(BLIT_MOD_A, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_B, 0x0000),                             // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xF000),                             // moto graphic src A
    XREG_SETW(BLIT_SRC_B, 0xFFFF),                             // nop B const (w/o transparent nibble)
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 3, 1
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0002),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x1E03),                             // shift/mask 3 nibbles
    XREG_SETW(BLIT_MOD_A, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_B, 0x0000),                             // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xF000),                             // moto graphic src A
    XREG_SETW(BLIT_SRC_B, 0xFFFF),                             // nop B const (w/o transparent nibble)
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 10),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 0, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF01),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0xFF00),                             // no masking or shifting
    XREG_SETW(BLIT_MOD_A, 0x0000),                             // no A line modulo (contiguous source)
    XREG_SETW(BLIT_MOD_B, 0x0000),                             // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                             // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                         // moto graphic width

    // 2D moto blit 1, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF01),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x7801),                             // shift/mask 1 nibble
    XREG_SETW(BLIT_MOD_A, 0x000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                             // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 2, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF01),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x3C02),                             // shift/mask 2 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                             // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width

    // 2D moto blit 3, 2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0xFF01),                              // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x1E03),                             // shift/mask 3 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                              // no A line XOR
    XREG_SETW(BLIT_MOD_B, -1),                                 // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                             // no C line XOR
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                             // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000),                             // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                             // nop C const
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 19),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                         // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                     // moto graphic width


    // 2D moto blit 0, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0011),                                 // const A, read B, 8-bit trans=33
    XREG_SETW(BLIT_SHIFT, 0xFF03),                                // no masking or shifting
    XREG_SETW(BLIT_MOD_A, 0x0000),                                // no A line modulo (contiguous source)
    XREG_SETW(BLIT_MOD_B, 0x0000),                                // no B line XOR
    XREG_SETW(BLIT_MOD_C, 0x0000),                                // no C line XOR
    XREG_SETW(BLIT_MOD_D, -(W_4BPP - W_LOGO)),                    // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                                // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000 + (H_LOGO * W_LOGO) - 1),        // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                                // nop C const
    XREG_SETW(BLIT_DST_D,
              0x0000 + ((20 + (H_LOGO - 1)) * W_4BPP) + (W_LOGO - 1) + 28),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                                             // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),                                             // moto graphic width

    // 2D moto blit 1, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0011),                                 // const A, read B, 8-bit trans=33
    XREG_SETW(BLIT_SHIFT, 0xE102),                                // shift/mask 3 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                                 // no A line XOR
    XREG_SETW(BLIT_MOD_B, +1),                                    // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                                // no C line XOR
    XREG_SETW(BLIT_MOD_D, -(W_4BPP - W_LOGO - 1)),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                                // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000 + (H_LOGO * W_LOGO) - 1),        // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                                // nop C const
    XREG_SETW(BLIT_DST_D,
              0x0000 + ((40 + (H_LOGO - 1)) * W_4BPP) + (W_LOGO - 1) + 28 + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                                                 // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                                             // moto graphic width

    // 2D moto blit 2, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0011),                                 // const A, read B, 8-bit trans=33
    XREG_SETW(BLIT_SHIFT, 0xC301),                                // shift/mask 2 nibbles
    XREG_SETW(BLIT_MOD_A, 0x000),                                 // no A line XOR
    XREG_SETW(BLIT_MOD_B, +1),                                    // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                                // no C line XOR
    XREG_SETW(BLIT_MOD_D, -(W_4BPP - W_LOGO - 1)),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                                // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000 + (H_LOGO * W_LOGO) - 1),        // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                                // nop C const
    XREG_SETW(BLIT_DST_D,
              0x0000 + ((60 + (H_LOGO - 1)) * W_4BPP) + (W_LOGO - 1) + 28 + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                                                 // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                                             // moto graphic width

    // 2D moto blit 3, 3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0011),                                 // const A, read B, 4-bit trans=0
    XREG_SETW(BLIT_SHIFT, 0x8700),                                // shift/mask 1 nibble
    XREG_SETW(BLIT_MOD_A, 0),                                     // no A line XOR
    XREG_SETW(BLIT_MOD_B, +1),                                    // line A modulo adjust for added width
    XREG_SETW(BLIT_MOD_C, 0x0000),                                // no C line XOR
    XREG_SETW(BLIT_MOD_D, -(W_4BPP - W_LOGO - 1)),                // D modulo = dest width - source width
    XREG_SETW(BLIT_SRC_A, 0xFFFF),                                // nop A const
    XREG_SETW(BLIT_SRC_B, 0xF000 + (H_LOGO * W_LOGO) - 1),        // moto graphic src B
    XREG_SETW(BLIT_VAL_C, 0x0000),                                // nop C const
    XREG_SETW(BLIT_DST_D,
              0x0000 + ((80 + (H_LOGO - 1)) * W_4BPP) + (W_LOGO - 1) + 28 + 1),        // D = start dest address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),                                                 // moto graphic height
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),                                             // moto graphic width

    REG_WAIT_BLIT_DONE(),

#if 1        // XREG read torture test

    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),

    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),

    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),

    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),
    XREG_GETW(VID_HSIZE),

    XREG_SETW(COPP_CTRL, 0x0000),
#endif


    REG_WAITVTOP(),
    REG_WAITVSYNC(),
#if 1
    // true color hack test

    XREG_SETW(PA_GFX_CTRL, 0x0065),         // bitmap, 8-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),        // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),        // display start address
    XREG_SETW(PA_LINE_LEN,
              (320 / 2) + (320 / 4)),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    XREG_SETW(PB_GFX_CTRL, 0x0055),                     // bitmap, 4-bpp, Hx2, Vx2
    XREG_SETW(PB_TILE_CTRL, 0x000F),                    // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PB_DISP_ADDR, 0x0000 + (320 / 2)),        // display start address
    XREG_SETW(PB_LINE_LEN,
              (320 / 2) + (320 / 4)),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    REG_W(XR_ADDR, XR_COLOR_ADDR),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),        // 16x16 logo to 0xF000
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),        // RG 8-bpp + 4-bpp B

// 16-color 320x200 color tut
#endif
    REG_WAITVTOP(),
    REG_WAITVSYNC(),
    XREG_SETW(PA_HV_FSCALE, 0x0005),        // 400 line scale

    XREG_SETW(PA_GFX_CTRL, 0x0055),           // bitmap, 8-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),          // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),          // display start address
    XREG_SETW(PA_LINE_LEN, (320 / 4)),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    XREG_SETW(PB_GFX_CTRL, 0x0080),        // disable

    REG_W(XR_ADDR, XR_COLOR_ADDR),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),        // tut
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),        // RG 8-bpp + 4-bpp B

    REG_WAITVTOP(),
    REG_WAITVSYNC(),

    REG_WAITVTOP(),
    REG_WAITVSYNC(),


    REG_END(),
    // end test data
};

#if 0
uint16_t     BusInterface::test_stfont[1024] = {REG_W(WR_ADDR, 0x3),
                                          REG_W(WR_INC, 0x1),
                                          REG_W(DATA, 0x0200 | 'H'),
                                          REG_B(DATA, 'e'),
                                          REG_B(DATA, 'l'),
                                          REG_B(DATA, 'l'),
                                          REG_B(DATA, 'o'),
                                          REG_B(DATA, '!'),
                                          REG_W(WR_ADDR, 0x0),
                                          REG_W(DATA, 0x0e00 | '\x0e'),
                                          REG_W(DATA, 0x0e00 | '\x0f'),
                                          REG_W(WR_ADDR, X_COLS * 5),
                                          REG_W(DATA, 0x0200 | 'A'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, 'a'),
                                          REG_B(DATA, 'r'),
                                          REG_B(DATA, 'i'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'S'),
                                          REG_B(DATA, 'T'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, '8'),
                                          REG_B(DATA, 'x'),
                                          REG_B(DATA, '1'),
                                          REG_B(DATA, '6'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'F'),
                                          REG_B(DATA, 'o'),
                                          REG_B(DATA, 'n'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'T'),
                                          REG_B(DATA, 'e'),
                                          REG_B(DATA, 's'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, '\x1c'),
                                          REG_B(WR_INC, X_COLS - 1),
                                          REG_B(DATA, '\x1d'),
                                          REG_B(WR_INC, 1),
                                          REG_B(DATA, '\x1e'),
                                          REG_B(DATA, '\x1f'),
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
    bool vga_hsync_previous = !H_SYNC_POLARITY;
    bool vga_vsync_previous = !V_SYNC_POLARITY;
    int  frame_num          = 0;
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

        top->clk = 1;        // clock rising
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        top->clk = 0;        // clock falling
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        if (top->reconfig_o)
        {
            log_printf("FPGA RECONFIG: config #0x%x\n", top->boot_select_o);
            done = true;
        }

        if (top->bus_intr_o)
        {
            logonly_printf("[@t=%lu FPGA INTERRUPT]\n", main_time);
        }

        if (frame_num > 1)
        {
#if 0
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

        vtop_detect = top->xosera_main->dv_de_o;

        // end of hsync
        if (!hsync && vga_hsync_previous)
        {
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
                    "[@t=%lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
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
                        sprintf(
                            save_name, LOGDIR "xosera_vsim_%dx%d_f%02d.png", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
                        IMG_SavePNG(screen_shot, save_name);
                        SDL_FreeSurface(screen_shot);
                        float fnum = ((1.0 / PIXEL_CLOCK_MHZ) * ((main_time - first_frame_start) / 2)) / 1000.0;
                        log_printf("[@t=%lu] %8.03f ms frame #%3u saved as \"%s\" (%dx%d)\n",
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

#if SDL_RENDER
        if (sim_render)
        {
            SDL_Event e;
            SDL_PollEvent(&e);

            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            {
                log_printf("Window closed\n");
                break;
            }
        }
#endif
    }

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
