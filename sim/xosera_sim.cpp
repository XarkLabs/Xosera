// C++ "driver" for Xosera Verilator simulation
//
// vim: set noet ts=4 sw=4
//
// Thanks to Dan "drr" Rodrigues for the amazing icestation-32 project which
// has a nice example of how to use Verilator with Yosys and SDL.  This code
// was created starting with that (so drr gets most of the credit).

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xosera_defs.h"

#include "verilated.h"

#include "Vxosera_main.h"
#include "Vxosera_main_xosera_main.h"
#include "Vxosera_main_vram.h"

#if VM_TRACE
#include "verilated_vcd_c.h"
#endif

#if SDL_RENDER
#include <SDL.h>
#endif

#define MAX_TRACE_FRAMES 3 // video frames to dump to VCD file (and then screen-shot and exit)

#if SPI_INTERFACE

const int SPI_START_TIME = 1680002; // 640x480 2nd frame
const int SPI_CLOCK_DIV = 16;
enum class SPI
{
	SS_LOW,
	SCK_HIGH,
	SCK_HIGH2,
	SCK_LOW,
	SCK_LOW2,
	BYTE_DONE,
	SS_HIGH,
	IDLE
};

const char *SPI_name[] = {
	"SS_LOW",
	"SCK_HIGH",
	"SCK_HIGH2",
	"SCK_LOW",
	"SCK_LOW2",
	"BYTE_DONE",
	"SS_HIGH",
	"IDLE"};

SPI spi_test_state;
SPI last_spi_test_state;
int spi_last_time;
int spi_test_bit;
bool spi_recv_strobe;
uint8_t spi_send_byte;
uint8_t spi_recv_byte;

uint8_t spi_pc_data_index = 0;
uint8_t spi_pc_data_len = 16;
uint8_t spi_pc_data[1024] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x55, 0xaa, 0x55, 0xaa, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
#endif

volatile bool done;
bool sim_render = SDL_RENDER;
bool sim_spi = SPI_INTERFACE;
bool wait_close = false;

void ctrl_c(int s)
{
	(void)s;
	done = true;
}

// Current simulation time (64-bit unsigned)
vluint64_t main_time = 0;
vluint64_t frame_start_time = 0;

// Called by $time in Verilog
double sc_time_stamp()
{
	return main_time;
}

int main(int argc, char **argv)
{
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = ctrl_c;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
	printf("\nXosera simulation. Video Mode: %dx%d @%0.02fHz clock %0.03fMhz\n", VISIBLE_WIDTH, VISIBLE_HEIGHT, Hz, PIXEL_CLOCK_MHZ);

	int nextarg = 1;

	while (nextarg < argc && (argv[nextarg][0] == '-' || argv[nextarg][0] == '/'))
	{
		if (strcmp(argv[nextarg] + 1, "n") == 0)
		{
			sim_render = false;
		}
		else if (strcmp(argv[nextarg] + 1, "s") == 0)
		{
			sim_spi = true;
		}
		else if (strcmp(argv[nextarg] + 1, "w") == 0)
		{
			wait_close = true;
		}
		nextarg += 1;
	}

#if SPI_INTERFACE
	size_t len = 0;
	for (int i = nextarg; i < argc && len < sizeof(spi_pc_data); i++)
	{
		char *endptr = nullptr;
		int value = static_cast<int>(strtoul(argv[i], &endptr, 0) & 0xffUL);
		if (endptr != nullptr && *endptr == '\0')
		{
			spi_pc_data[len] = value;
			len++;
		}
		else
		{
			break;
		}
	}

	if (len != 0)
	{
		spi_pc_data_len = len;
	}
#endif

	if (sim_render)
		printf("Press SPACE for screen-shot, ESC or ^C to exit.\n\n");
	else
		printf("Press ^C to exit.\n\n");

	Verilated::commandArgs(argc, argv);

#if VM_TRACE
	Verilated::traceEverOn(true);
#endif

	Vxosera_main *top = new Vxosera_main;

#if SDL_RENDER
	SDL_Renderer *renderer = nullptr;
	SDL_Window *window = nullptr;
	if (sim_render)
	{
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
			return EXIT_FAILURE;
		}

		window = SDL_CreateWindow(
			"Xosera-sim",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			TOTAL_WIDTH,
			TOTAL_HEIGHT,
			SDL_WINDOW_SHOWN);

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
		SDL_RenderSetScale(renderer, 1, 1);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
	}

	bool shot_all = true; // screenshot all frames
	bool take_shot = false;

#endif

	int current_x = 0;
	int current_y = 24;
	bool vga_hsync_previous = !H_SYNC_POLARITY;
	bool vga_vsync_previous = !V_SYNC_POLARITY;
	int frame_num = 0;
	int x_max = 0, y_max = 0;
	int hsync_count = 0, hsync_min = 0, hsync_max = 0;
	int vsync_count = 0;

#if VM_TRACE
	const auto trace_path = "logs/xosera_vsim.vcd";
	printf("Started writing VCD waveform file to \"%s\"...\n", trace_path);

	VerilatedVcdC *tfp = new VerilatedVcdC;

	top->trace(tfp, 99); // trace to heirarchal depth of 99
	tfp->open(trace_path);
#endif

	top->reset = 1; // start in reset

#if SPI_INTERFACE
	top->spi_cs_i = 1;
	top->spi_sck_i = 0;
	top->spi_copi_i = 0;
#endif

	while (!done && !Verilated::gotFinish())
	{
		if (main_time == 4)
		{
			top->reset = 0; // tale out of reset after 2 cycles
		}

#if SPI_INTERFACE
		// SPI test
		if (sim_spi && main_time >= SPI_START_TIME && last_spi_test_state != SPI::IDLE)
		{
			int spi_time = (main_time - SPI_START_TIME) / SPI_CLOCK_DIV;
			if (spi_time > spi_last_time)
			{
				spi_last_time = spi_time;
				spi_recv_strobe = false;
				switch (spi_test_state)
				{
				case SPI::SS_LOW:
					top->spi_cs_i = 0;
					spi_send_byte = spi_pc_data[spi_pc_data_index];
					printf(" <= 0x%02x sending to FPGA\n", spi_send_byte);
					top->spi_copi_i = (spi_send_byte & 0x80) ? 1 : 0;
					spi_test_bit = 0;
					spi_test_state = SPI::SCK_HIGH;
					break;
				case SPI::SCK_HIGH:
					top->spi_sck_i = 1;
					top->spi_copi_i = (spi_send_byte & 0x80) ? 1 : 0;
					spi_send_byte = spi_send_byte << 1;
					spi_test_bit = spi_test_bit + 1;
					spi_recv_byte <<= 1;
					spi_recv_byte |= (top->spi_cipo_o) ? 1 : 0;
					spi_test_state = SPI::SCK_HIGH2;
					break;
				case SPI::SCK_HIGH2:
					spi_test_state = SPI::SCK_LOW;
					break;
				case SPI::SCK_LOW:
					top->spi_sck_i = 0;
					spi_test_state = SPI::SCK_LOW2;
					break;
				case SPI::SCK_LOW2:
					if (spi_test_bit == 0x8)
					{
						spi_test_bit = 0;
						spi_test_state = SPI::BYTE_DONE;
						spi_recv_strobe = true;
					}
					else
					{
						spi_test_state = SPI::SCK_HIGH;
					}
					break;
				case SPI::BYTE_DONE:
					spi_pc_data_index += 1;
					if (spi_pc_data_index < spi_pc_data_len)
					{
						spi_test_bit = 0;
						spi_send_byte = spi_pc_data[spi_pc_data_index];
						printf(" <= 0x%02x sending to FPGA\n", spi_send_byte);
						top->spi_copi_i = (spi_send_byte & 0x80) ? 1 : 0;
						spi_test_state = SPI::SCK_HIGH;
					}
					else
					{
						spi_test_state = SPI::SS_HIGH;
					}
					break;
				case SPI::SS_HIGH:
					top->spi_cs_i = 1;
					spi_test_state = SPI::IDLE;
					break;
				case SPI::IDLE:
					break;
				}
				if (false && spi_test_state != SPI::SCK_HIGH && spi_test_state != SPI::SCK_LOW)
				{
					printf("%10s -> %10s RECV=0x%02x SENDOUT[%d]=%02x BIT:%d SS:%d SCK:%d COPI:%d CIPO:%d\n", SPI_name[static_cast<int>(last_spi_test_state)], SPI_name[static_cast<int>(spi_test_state)], spi_recv_byte, spi_pc_data_index, spi_send_byte, spi_test_bit,
						   top->spi_cs_i, top->spi_sck_i, top->spi_copi_i, top->spi_cipo_o);
				}
				last_spi_test_state = spi_test_state;

				if (spi_recv_strobe)
				{
					printf(" => 0x%02x received from FPGA\n", spi_recv_byte);
					spi_recv_byte = 0x00;
				}
			}
		}
#endif

		top->clk = 1; // tick
		top->eval();

#if VM_TRACE
		if (frame_num <= MAX_TRACE_FRAMES)
			tfp->dump(main_time);
#endif
		main_time++;

		top->clk = 0; // tock
		top->eval();

#if VM_TRACE
		if (frame_num <= MAX_TRACE_FRAMES)
			tfp->dump(main_time);
#endif
		main_time++;

		bool hsync = H_SYNC_POLARITY ? top->vga_hs : !top->vga_hs;
		bool vsync = V_SYNC_POLARITY ? top->vga_vs : !top->vga_vs;

#if SDL_RENDER
		if (sim_render)
		{
			if (top->xosera_main->visible)
			{
				// sim_render current VGA output pixel (4 bits per gun)
				SDL_SetRenderDrawColor(renderer, (top->vga_r << 4) | top->vga_r,
									   (top->vga_g << 4) | top->vga_g,
									   (top->vga_b << 4) | top->vga_b,
									   255);
			}
			else
			{
				if (top->vga_r != 0 || top->vga_g != 0 || top->vga_b != 0)
				{
					printf("Frame %3u pixel %d, %d RGB is 0x%02x 0x%02x 0x%02x\n", frame_num, current_x, current_y, top->vga_r, top->vga_g, top->vga_b);
				}

				// sim_render dithered border area
				if (((current_x ^ current_y) & 1) == 1) // non-visible
				{
					SDL_SetRenderDrawColor(renderer, (top->vga_r << 3),
										   (top->vga_g << 3),
										   (top->vga_b << 3),
										   255);
				}
				else
				{
					SDL_SetRenderDrawColor(renderer, 0x21,
										   vsync ? 0x41 : 0x21,
										   hsync ? 0x41 : 0x21,
										   0xff);
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

		if (!vsync && vga_vsync_previous)
		{
			if (current_y - 1 > y_max)
				y_max = current_y - 1;

			if (frame_num > 0)
			{
				vluint64_t frame_time = (main_time - frame_start_time) / 2;
				printf("Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n", frame_num, frame_time, ((1.0 / PIXEL_CLOCK_MHZ) * frame_time) / 1000.0,
					   x_max, y_max + 1, hsync_max, vsync_count);
#if SDL_RENDER

				if (sim_render)
				{
					if (shot_all || take_shot || frame_num == MAX_TRACE_FRAMES)
					{
						int w = 0, h = 0;
						char save_name[256] = {0};
						SDL_GetRendererOutputSize(renderer, &w, &h);
						SDL_Surface *screen_shot = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
						SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screen_shot->pixels, screen_shot->pitch);
						sprintf(save_name, "logs/xosera_vsim_%dx%d_f%02d.bmp", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
						SDL_SaveBMP(screen_shot, save_name);
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
			hsync_min = 0;
			hsync_max = 0;
			vsync_count = 0;
			current_y = 0;

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

	FILE *mfp = fopen("logs/xosera_vsim_text.txt", "w");
	if (mfp != nullptr)
	{
		auto *mem = top->xosera_main->vram->memory;

		for (int y = 0; y < VISIBLE_HEIGHT / 16; y++)
		{
			fprintf(mfp, "%04x: ", y * (VISIBLE_WIDTH / 8));
			for (int x = 0; x < VISIBLE_WIDTH / 8; x++)
			{
				auto m = mem[y * (VISIBLE_WIDTH / 8) + x];
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

	FILE *bfp = fopen("logs/xosera_vsim_vram.bin", "w");
	if (bfp != nullptr)
	{
		auto *mem = top->xosera_main->vram->memory;
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
		SDL_Quit();
	}
#endif

	printf("Simulated %d frames, %lu pixel clock ticks (% 0.04f milliseconds)\n", frame_num, (main_time / 2), ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);

	return EXIT_SUCCESS;
}
