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
#include "Vxosera_main_vram.h"
#include "Vxosera_main_xosera_main.h"

#include "verilated_vcd_c.h"        // for VM_TRACE
#include <SDL.h>                    // for SDL_RENDER
#include <SDL_image.h>

#define MAX_TRACE_FRAMES 4        // video frames to dump to VCD file (and then screen-shot and exit)

// Current simulation time (64-bit unsigned)
vluint64_t main_time        = 0;
vluint64_t frame_start_time = 0;

volatile bool done;
bool          sim_render = SDL_RENDER;
bool          sim_bus    = BUS_INTERFACE;
bool          wait_close = false;

class BusInterface
{
    const int   BUS_START_TIME = 3324934;        // 1685002;    // 640x480 2nd frame
    const float BUS_CLOCK_DIV  = 7.7;

    enum
    {
        // register 16-bit read/write (no side effects)
        XVID_AUX_ADDR,        // reg 0: TODO video data (as set by VID_CTRL)
        XVID_CONST,           // reg 1: TODO CPU data (instead of read from VRAM)
        XVID_RD_ADDR,         // reg 2: address to read from VRAM
        XVID_WR_ADDR,         // reg 3: address to write from VRAM

        // special, odd byte write triggers
        XVID_DATA,            // reg 4: read/write word from/to VRAM RD/WR
        XVID_DATA_2,          // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
        XVID_AUX_DATA,        // reg 6: aux data (font/audio)
        XVID_COUNT,           // reg 7: TODO blitter "repeat" count/trigger

        // write only, 16-bit
        XVID_RD_INC,           // reg 9: read addr increment value
        XVID_WR_INC,           // reg A: write addr increment value
        XVID_WR_MOD,           // reg C: TODO write modulo width for 2D blit
        XVID_RD_MOD,           // reg B: TODO read modulo width for 2D blit
        XVID_WIDTH,            // reg 8: TODO width for 2D blit
        XVID_BLIT_CTRL,        // reg D: TODO
        XVID_UNUSED_1,         // reg E: TODO
        XVID_UNUSED_2          // reg F: TODO
    };

    static const char * reg_name[];
    enum
    {
        BUS_PREP,
        BUS_STROBE,
        BUS_HOLD,
        BUS_STROBEOFF,
        BUS_END
    };

    bool    enable;
    int64_t last_time;
    int     state;
    int     index;

    static int      test_data_len;
    static uint16_t test_data[1024];

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
        enable          = _enable;
        index           = 0;
        state           = BUS_PREP;
        top->bus_cs_n_i = 1;
    }

    void process(Vxosera_main * top)
    {
        char tempstr[256];
        // bus test
        if (enable && main_time >= BUS_START_TIME)
        {
            int64_t bus_time = (main_time - BUS_START_TIME) / BUS_CLOCK_DIV;
            if (test_data[index] == 0xffff)
            {
                enable    = false;
                last_time = bus_time - 1;
            }

            if (bus_time >= last_time)
            {
                last_time = bus_time + 1;

                int bytesel = (test_data[index] & 0x1000) ? 1 : 0;
                int reg_num = (test_data[index] >> 8) & 0xf;
                int data    = test_data[index] & 0xff;

                switch (state)
                {
                    case BUS_PREP:
                        printf("[@t=%lu] ", main_time);

                        top->bus_cs_n_i    = 0;
                        top->bus_bytesel_i = bytesel;
                        top->bus_rd_nwr_i  = 0;
                        top->bus_reg_num_i = reg_num;
                        top->bus_data_i    = data;
                        sprintf(tempstr, "r[0x%x] %s.%3s", reg_num, reg_name[reg_num], bytesel ? "lsb*" : "msb");
                        printf("  %-25.25s <= 0x%02x\n", tempstr, data);
                        break;
                    case BUS_STROBE:
                        top->bus_cs_n_i = 1;
                        last_time       = bus_time + 2;
                        break;
                    case BUS_HOLD:
                        break;
                    case BUS_STROBEOFF:
                        top->bus_cs_n_i = 0;
                        break;
                    case BUS_END:
                        top->bus_cs_n_i    = 0;
                        top->bus_bytesel_i = 0;
                        top->bus_rd_nwr_i  = 0;
                        top->bus_reg_num_i = 0;
                        top->bus_data_i    = 0;
                        last_time          = bus_time + 9;
                        if (++index > test_data_len)
                        {
                            enable = false;
                        }
                        break;
                    default:
                        assert(false);
                }
                state = state + 1;
                if (state > BUS_END)
                {
                    state = BUS_PREP;
                }
            }
        }
    }
};

const char * BusInterface::reg_name[] = {
    // register 16-bit read/write (no side effects)
    "XVID_AUX_ADDR",        // reg 0: TODO video data (as set by VID_CTRL)
    "XVID_CONST",           // reg 1: TODO CPU data (instead of read from VRAM)
    "XVID_RD_ADDR",         // reg 2: address to read from VRAM
    "XVID_WR_ADDR",         // reg 3: address to write from VRAM

    // special, odd byte write triggers
    "XVID_DATA",            // reg 4: read/write word from/to VRAM RD/WR
    "XVID_DATA_2",          // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    "XVID_AUX_DATA",        // reg 6: aux data (font/audio)
    "XVID_COUNT",           // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    "XVID_RD_INC",           // reg 9: read addr increment value
    "XVID_WR_INC",           // reg A: write addr increment value
    "XVID_WR_MOD",           // reg C: TODO write modulo width for 2D blit
    "XVID_RD_MOD",           // reg B: TODO read modulo width for 2D blit
    "XVID_WIDTH",            // reg 8: TODO width for 2D blit
    "XVID_BLIT_CTRL",        // reg D: TODO
    "XVID_UNUSED_1",         // reg E: TODO
    "XVID_UNUSED_2"          // reg F: TODO
};

#define REG_B(r, v) (((BusInterface::XVID_##r) | 0x10) << 8) | ((v)&0xff)
#define REG_W(r, v)                                                                                                    \
    ((BusInterface::XVID_##r) << 8) | (((v) >> 8) & 0xff), (((BusInterface::XVID_##r) | 0x10) << 8) | ((v)&0xff)

BusInterface bus;
int          BusInterface::test_data_len   = 999;
uint16_t     BusInterface::test_data[1024] = {REG_W(WR_ADDR, 0x3),
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
                                          REG_W(WR_ADDR, 106 * 5),
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
                                          REG_B(WR_INC, 105),
                                          REG_B(DATA, '\x1d'),
                                          REG_B(WR_INC, 1),
                                          REG_B(DATA, '\x1e'),
                                          REG_B(DATA, '\x1f'),
                                          0xffff};

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

    double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
    printf("\nXosera simulation. Video Mode: %dx%d @%0.02fHz clock %0.03fMhz\n",
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
        nextarg += 1;
    }

#if BUS_INTERFACE
    // bus test data init
    bus.set_cmdline_data(argc, argv, nextarg);
#endif

    if (sim_render)
        printf("Press SPACE for screen-shot, ESC or ^C to exit.\n\n");
    else
        printf("Press ^C to exit.\n\n");

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
    int  current_y          = 24;
    bool vga_hsync_previous = !H_SYNC_POLARITY;
    bool vga_vsync_previous = !V_SYNC_POLARITY;
    int  frame_num          = 0;
    int  x_max              = 0;
    int  y_max              = 0;
    int  hsync_count = 0, hsync_min = 0, hsync_max = 0;
    int  vsync_count = 0;

#if VM_TRACE
    const auto trace_path = "logs/xosera_vsim.vcd";
    printf("Started writing VCD waveform file to \"%s\"...\n", trace_path);

    VerilatedVcdC * tfp = new VerilatedVcdC;

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
        if (frame_num <= MAX_TRACE_FRAMES) tfp->dump(main_time);
#endif
        main_time++;

        top->clk = 0;        // clock falling
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES) tfp->dump(main_time);
#endif
        main_time++;

        if (frame_num > 1 && top->xosera_main->vram_sel && top->xosera_main->vram_wr)
        {
            printf(" => write VRAM[0x%04x]=0x%04x\n", top->xosera_main->vram_addr, top->xosera_main->blit_data_out);
        }

        bool hsync = H_SYNC_POLARITY ? top->hsync_o : !top->hsync_o;
        bool vsync = V_SYNC_POLARITY ? top->vsync_o : !top->vsync_o;

#if SDL_RENDER
        if (sim_render)
        {
            if (top->xosera_main->dv_de_o)
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
                    printf("Frame %3u pixel %d, %d RGB is 0x%02x 0x%02x 0x%02x when NOT visible\n",
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
                    SDL_SetRenderDrawColor(renderer, (top->red_o << 3), (top->green_o << 3), (top->blue_o << 3), 255);
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

        if (hsync) hsync_count++;

        // end of hsync
        if (!hsync && vga_hsync_previous)
        {
            if (hsync_count > hsync_max) hsync_max = hsync_count;
            if (hsync_count < hsync_min || !hsync_min) hsync_min = hsync_count;
            hsync_count = 0;

            if (current_x > x_max) x_max = current_x;

            current_x = 0;
            current_y++;

            if (vsync) vsync_count++;
        }

        vga_hsync_previous = hsync;

        if (!vsync && vga_vsync_previous)
        {
            if (current_y - 1 > y_max) y_max = current_y - 1;

            if (frame_num > 0)
            {
                vluint64_t frame_time = (main_time - frame_start_time) / 2;
                printf("[@t=%lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
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
#if 0
                        sprintf(
                            save_name, "logs/xosera_vsim_%dx%d_f%02d.bmp", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
                        SDL_SaveBMP(screen_shot, save_name);
#else
                        sprintf(
                            save_name, "logs/xosera_vsim_%dx%d_f%02d.png", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
                        IMG_SavePNG(screen_shot, save_name);
#endif
                        SDL_FreeSurface(screen_shot);
                        printf("Frame %3u saved as \"%s\" (%dx%d)\n", frame_num, save_name, w, h);
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
#if VM_TRACE
                printf("Finished writing VCD waveform file \"%s\"\n", trace_path);
#endif
                printf("Exiting simulation.\n");
                break;
            }
            frame_num += 1;
        }

        vga_vsync_previous = vsync;

#if SDL_RENDER
        if (sim_render)
        {
            SDL_Event e;
            SDL_PollEvent(&e);
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE)
            {
                take_shot = true;
            }

            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            {
                printf("Window closed\n");
                break;
            }
        }
#endif
    }

    FILE * mfp = fopen("logs/xosera_vsim_text.txt", "w");
    if (mfp != nullptr)
    {
        auto       vmem = top->xosera_main->vram->memory;
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

    FILE * bfp = fopen("logs/xosera_vsim_vram.bin", "w");
    if (bfp != nullptr)
    {
        auto       vmem = top->xosera_main->vram->memory;
        uint16_t * mem  = &vmem[0];
        fwrite(mem, 128 * 1024, 1, bfp);
        fclose(bfp);
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
            fprintf(stderr, "Press a RETURN:\n");
            fgetc(stdin);
        }

        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
#endif

    printf("Simulated %d frames, %lu pixel clock ticks (% 0.04f milliseconds)\n",
           frame_num,
           (main_time / 2),
           ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);

    return EXIT_SUCCESS;
}
