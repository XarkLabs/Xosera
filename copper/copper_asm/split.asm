; A basic example of using the copper assembler. This list splits
; the screen into two "viewports" of equal size. The top viewport
; uses low-resolution 320x240 @ 4bpp, while the bottom one uses 
; high-resolution 640x480 @ 1bpp.
;
; Copyright (c) 2021 Ross Bamford - https://github.com/roscopeco
;
; See top-level LICENSE file for license information. (Hint: MIT)

#include "copper.casm"

copperlist:
    mover PA_GFX_CTRL, 0x0065     ; First half of screen in 4-bpp + Hx2 + Vx2
    movep 0xf, 0x0ec6             ; Palette entry 0xf from tut bitmap

    wait  0, 240, 0b00010         ; Wait for line 240, H position ignored

    mover PA_LINE_ADDR, 0x3e80    ; Line start now at 16000
    mover PA_GFX_CTRL, 0x0040     ; 1-bpp + Hx1 + Vx1
    movep 0xf, 0x0fff             ; Palette entry 0xf to white for 1bpp bitmap
    nextf

