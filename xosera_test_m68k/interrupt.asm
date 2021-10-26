; *************************************************************
; Copyright (c) 2021 roscopeco <AT> gmail <DOT> com
; *************************************************************
;
        section .text                     ; This is normal code

        include "../xosera_m68k_api/xosera_m68k_defs.asm"

install_intr::
                movem.l D0-D7/A0-A6,-(A7)

                or.w    #$0200,SR               ; disable interrupts

                move.l  #XM_BASEADDR,A0         ; get Xosera base addr
                move.b  #$0F,D0                 ; all interrupt source bits
                move.b  D0,XM_TIMER+2(A0)       ; clear out any prior pending interrupts
                move.w  #$080F,D0               ; vsync int, all nibbles write, 
                movep.w D0,XM_SYS_CTRL(A0)      ; enable VSYNC interrupt

                move.l  #Xosera_intr,$68        ; set interrupt vector
                and.w   #$F0FF,SR               ; enable interrupts

                movem.l (A7)+,D0-D7/A0-A6
                rts

remove_intr::
                movem.l D0-D7/A0-A6,-(A7)

                move.l  $60,D0                  ; copy spurious int handler
                move.l  D0,$68

                movem.l (A7)+,D0-D7/A0-A6
                rts

; interrupt routine
Xosera_intr:
                movem.l D0-D1/A0,-(A7)          ; save minimal regs

                move.l  #XM_BASEADDR,A0         ; get Xosera base addr
                movep.w XM_XR_ADDR(A0),D1       ; save aux_addr value

                moveq.l #XR_VID_CTRL,D0         ; XR reg VID_CTRL
                movep.w D0,XM_XR_ADDR(A0)
                move.b  XM_XR_DATA+2(A0),D0     ; read intr status [3:0] (low byte)
                move.b  D0,XM_TIMER+2(A0)       ; clear any interrupts in status

                ; NOTE: could check D0 bits [3:0] for
                ;       interrupt sources, but for now
                ;       just assume it is vsync [3]

                move.w  #XR_COLOR_MEM+2,D0      ; set color entry #2
                movep.w D0,XM_XR_ADDR(A0)

                move.w  NukeColor,D0            ; increment NukeColor
                addq.l  #1,D0
                move.w  D0,NukeColor

                movep.w D0,XM_XR_DATA(A0)       ; to NukeColor

                move.w  #XR_CURSOR_X,D0         ; move cursor
                movep.w D0,XM_XR_ADDR(A0)
                movep.w XM_XR_DATA(A0),D0
                addq.l  #3,D0
                and.w   #$7ff,D0
                movep.w D0,XM_XR_DATA(A0)
                move.w  #XR_CURSOR_Y,D0         ; move cursor
                movep.w D0,XM_XR_ADDR(A0)
                movep.w XM_XR_DATA(A0),D0
                addq.l  #1,D0
                and.w   #$1ff,D0
                movep.w D0,XM_XR_DATA(A0)

                add.l   #1,XFrameCount          ; increment frame counter

                movep.w D1,XM_XR_ADDR(A0)       ; restore aux_addr
                movem.l (A7)+,D0-D1/A0          ; restore regs
                rte

NukeColor       dc.w    $0000
XFrameCount::   dc.l    $00000000


