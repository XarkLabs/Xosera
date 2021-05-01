#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <debug_stub.h>
#include <machine.h>

#include "xosera_regs.h"

// rosco_m68k Xosera board address (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
xosera_regs_t * xosera_ptr = (xosera_regs_t *)0xf80060;

void kmain()
{
    debug_stub();
    mcDelaymsec10(300);

    printf("Hello from rosco_m68k, now go look at the other monitor. :)\n");

    xv_setw(wr_addr, 0x0000);
    xv_setw(wr_inc, 1);
    xv_setw(data, 0x0200 | 'X');
    xv_setbl(data, 'o');
    xv_setbl(data, 's');
    xv_setbl(data, 'e');
    xv_setbl(data, 'r');
    xv_setbl(data, 'a');
    xv_setbl(data, ' ');
    xv_setw(data, 0x0400 | '6');
    xv_setbl(data, '8');
    xv_setbl(data, 'k');

    // read test
    xv_setw(rd_addr, 0x0000);
    uint16_t val = xv_getw(data);
    printf("Read back = %04x\nBye!\n", val);

    return;
}
