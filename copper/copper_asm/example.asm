; A basic example of using the copper assembler. This list splits
; the screen into three sections (red, green and blue) by 
; adjusting palette entry zero.
;
; Copyright (c) 2021 Ross Bamford - https://github.com/roscopeco
;
; See top-level LICENSE file for license information. (Hint: MIT)

#include "copper.casm"

copperlist:
    movep 0x0F00, 0                   ; Make background red
    movep 0x0400, 0xA                 ; Make foreground dark red
    wait  0, 160, 0b000010            ; Wait for line 160, ignore X position
    movep 0x00F0, 0                   ; Make background green
    movep 0x0040, 0xA                 ; Make foreground dark green
    wait  0, 320, 0b000010            ; Wait for line 320, ignore X position
    movep 0x000F, 0                   ; Make background blue
    movep 0x0004, 0xA                 ; Make foreground dark blue
    wait  0, 0, 0b000011              ; Wait for next frame

