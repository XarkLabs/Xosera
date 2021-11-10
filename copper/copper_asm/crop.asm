#include "copper.casm"

copperlist:
    wait  0, 40, 0b000010               ; Wait for line 40, ignore X position
    mover PA_GFX_CTRL, 0x75             ; Move 0x75 to GFX control
    wait  0, 440, 0b000010              ; Wait for line 440, ignore X position
    mover PA_GFX_CTRL, 0xF5             ; Move 0xF5 to GFX control
    wait  0, 0, 0b000011                ; Wait for next frame
