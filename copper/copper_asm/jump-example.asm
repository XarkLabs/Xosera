; A slightly less basic example of using copper assembler, including
; skip and jmp instructions. In this example, instead of waiting for
; specific line positions, we use conditional logic and direct jumps
; to decide what color to set.
;
; Copyright (c) 2021 Ross Bamford - https://github.com/roscopeco
;
; See top-level LICENSE file for license information. (Hint: MIT)

#include "copper.casm"

copperlist:
    skip  0, 160, 0b00010             ; Skip next if we've hit line 160
    jmp   .gored                      ; ... else, jump to set red
    skip  0, 320, 0b00010             ; Skip next if we've hit line 320
    jmp   .gogreen                    ; ... else jump to set green

    ; If here, we're above 320, so set blue

    movep 0x0, 0x000F                 ; Make background blue
    movep 0xA, 0x0004                 ; Make foreground dark blue
    nextf                             ; And we're done for this frame

.gogreen:
    movep 0x0, 0x00F0                 ; Make background green
    movep 0xA, 0x0040                 ; Make foreground dark green
    jmp   copperlist                  ; and restart

.gored:
    movep 0x0, 0x0F00                 ; Make background red
    movep 0xA, 0x0400                 ; Make foreground dark red
    jmp   copperlist                  ; and restart

