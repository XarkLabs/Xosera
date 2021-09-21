#include "pr_api.h"

#include <xosera_m68k_api.h>

#define MAX_WIDTH   320
#define MAX_HEIGHT  200

extern volatile uint32_t XFrameCount;

static uint8_t g_disp_buffer;
static uint16_t g_start_line;
static uint16_t g_height;
static uint16_t g_first_disp_buffer_addr, g_second_disp_buffer_addr;

static void wait_pr_done()
{
    uint8_t busy;
    do {
        busy = xm_getbh(WR_PR_CMD);
    } while(busy & 0x80);
}

void wait_frame()
{
    uint32_t f = XFrameCount;
    while (XFrameCount == f);
}

static void swapi(int *a, int *b)
{
    int c = *a;
    *a = *b;
    *b = c;
}

void pr_init(int start_line, int height)
{
    g_start_line = start_line;
    g_height = height;
    wait_pr_done();
    xm_setw(WR_PR_CMD, PR_DEST_HEIGHT | height);
}

void pr_init_swap()
{
    g_disp_buffer = 0;

    g_first_disp_buffer_addr = g_start_line * MAX_WIDTH / 2;
    g_second_disp_buffer_addr = g_first_disp_buffer_addr + g_height * MAX_WIDTH / 2;

    xreg_setw(PA_DISP_ADDR, g_first_disp_buffer_addr);
    //xreg_setw(PA_LINE_ADDR, 0x0000);
    wait_pr_done();
    xm_setw(WR_PR_CMD, PR_DEST_ADDR | (g_second_disp_buffer_addr >> 4));
}

void pr_swap(bool is_vsync_enabled)
{
    wait_pr_done();
    if (is_vsync_enabled)
        wait_frame();

    if (g_disp_buffer) {
        g_disp_buffer = 0;
        xreg_setw(PA_DISP_ADDR, g_first_disp_buffer_addr);
        xm_setw(WR_PR_CMD, PR_DEST_ADDR | (g_second_disp_buffer_addr >> 4));
    } else {
        g_disp_buffer = 1;
        xreg_setw(PA_DISP_ADDR, g_second_disp_buffer_addr);
        xm_setw(WR_PR_CMD, PR_DEST_ADDR | (g_first_disp_buffer_addr >> 4));
    }
}

void pr_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    if (y0 > y2) {
        swapi(&x0, &x2);
        swapi(&y0, &y2);
    }

    if (y0 > y1) {
        swapi(&x0, &x1);
        swapi(&y0, &y1);
    }

    if (y1 > y2) {
        swapi(&x1, &x2);
        swapi(&y1, &y2);
    }

    wait_pr_done();

    xm_setw(WR_PR_CMD, PR_COORDX0 | (x0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY0 | (y0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX1 | (x1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY1 | (y1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX2 | (x2 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY2 | (y2 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COLOR | color);
    xm_setw(WR_PR_CMD, PR_EXECUTE);
}

void pr_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color)
{
    if (y0 > y1) {
        swapi(&x0, &x1);
        swapi(&y0, &y1);
    }
    
    wait_pr_done();
    xm_setw(WR_PR_CMD, PR_COORDX0 | (x0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY0 | (y0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX1 | (x1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY1 | (y0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX2 | (x0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY2 | (y1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COLOR | color);
    xm_setw(WR_PR_CMD, PR_EXECUTE);

    wait_pr_done();
    xm_setw(WR_PR_CMD, PR_COORDX0 | (x1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY0 | (y0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX1 | (x0 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY1 | (y1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDX2 | (x1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COORDY2 | (y1 & 0x0FFF));
    xm_setw(WR_PR_CMD, PR_COLOR | color);
    xm_setw(WR_PR_CMD, PR_EXECUTE);    
}

void pr_clear()
{
    pr_draw_filled_rectangle(0, 0, MAX_WIDTH, g_height - 1, 1);
}
