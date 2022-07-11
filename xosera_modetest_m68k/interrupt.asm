; vim: set et ts=8 sw=8
; ------------------------------------------------------------
; Copyright (c) 2022 Xark
; MIT License
;
; Xosera interrupt routines
; ------------------------------------------------------------

        section .text                     ; This is normal code

        include "../xosera_m68k_api/xosera_m68k_defs.inc"

SPURIOUS_VEC    equ     $60                     ; spurious handler (nop, ignores interrupt)
XOSERA_VEC      equ     $68                     ; xosera rosco_m68k interrupt vector

install_intr::
                or.w    #$0200,SR               ; disable interrupts

                lea.l   XM_BASEADDR,A0          ; get Xosera base addr

               ; enable interrupt, clear any pending
                move.w  #INT_CTRL_VIDEO_EN_F|INT_CTRL_CLEAR_ALL_F,D0
                movep.w D0,XM_INT_CTRL(A0)      ; enable & clear interrupts

                lea.l   (Xosera_intr,PC),A0     ; get xosera inerrupt routine
                move.l  A0,XOSERA_VEC           ; set interrupt vector

                and.w   #$F0FF,SR               ; enable interrupts
                rts

remove_intr::
                lea.l   XM_BASEADDR,A0          ; get Xosera base addr
                move.w  #INT_CTRL_CLEAR_ALL_F,D0 ; disable interrupts, and clear pending
                movep.w D0,XM_INT_CTRL(A0)      ; clear and mask interrupts
                move.l  SPURIOUS_VEC,D0         ; copy spurious int handler
                move.l  D0,XOSERA_VEC           ; to xosera int handler

                rts

; interrupt routine
Xosera_intr:
                movem.l D0-D1/A0,-(A7)          ; save minimal regs

                move.l  #XM_BASEADDR,A0         ; get Xosera base addr

                move.b  XM_INT_CTRL+2(A0),D0    ; read pending interrupts (low byte)
                move.b  D0,XM_INT_CTRL+2(A0)    ; acknowledge and clear interrupts

                ; NOTE: could check D0 bits for
                ;       interrupt sources, but for now
                ;       just assume it is vsync

                tst.w   NukeColor               ; test color
                bmi.s   NoNukeColor             ; if color < 0, skip color cycle

WaitXMem:       tst.b   (a0)                    ; make sure memory operation not in progress // TODO: paranoia?
                bmi.s   WaitXMem
                movep.w XM_WR_XADDR(A0),D1      ; save xaddr write address

                move.w  #XR_COLOR_ADDR+2,D0     ; set color entry #2
                movep.w D0,XM_WR_XADDR(A0)

                move.w  NukeColor,D0            ; load color
                addq.l  #1,D0                   ; cycle color
                and.w   #$0FFF,D0               ; clear non-RGB bits
                move.w  D0,NukeColor            ; save color

                movep.w D0,XM_XDATA(A0)         ; write color to xmem

                movep.w D1,XM_WR_XADDR(A0)      ; restore xmem write address

NoNukeColor:    add.l   #1,XFrameCount          ; increment frame counter

                movem.l (A7)+,D0-D1/A0          ; restore regs
                rte

        section .data

NukeColor::     dc.w    $0000
XFrameCount::   dc.l    $00000000
