// Xosera copper binary "color_bar_table"
#if !defined(INC_COLOR_BAR_TABLE_H)
#define INC_COLOR_BAR_TABLE_H
#include <stdint.h>

static const uint16_t color_bar_table_start __attribute__ ((unused)) = 0xc000;    // copper program XR start addr
static const uint16_t color_bar_table_size  __attribute__ ((unused)) =     26;    // copper program size in words
static uint16_t color_bar_table_bin[26] __attribute__ ((unused)) =
{
    0xc002, 0xd010, 0xd010, 0x8000, 0x27ff, 0xd002, 0x0800, 0x0801, 
    0xffff, 0x1800, 0xc002, 0x07ff, 0xd01a, 0xf802, 0xf000, 0x2bff, 
    0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0444, 0x0333, 0x0222, 
    0x0111, 0x0000
};
#endif // INC_COLOR_BAR_TABLE_H
