; *************************************************************
; Copyright (c) 2021 roscopeco <AT> gmail <DOT> com
; *************************************************************
;
        section .text                     ; This is normal code

        include "xosera_m68k_defs.inc"

SPURIOUS_VEC    =       $60                     ; spurious handler (ignores interrupt)
XOSERA_VEC      =       $68                     ; xosera interrupt vector

install_intr::
                movem.l D0-D7/A0-A6,-(A7)

                or.w    #$0200,SR               ; disable interrupts

                lea.l   XM_BASEADDR,A0          ; get Xosera base addr
                move.w  #$007F,D0               ; clear any pending
                movep.w D0,XM_INT_CTRL(A0) 
                move.w  #$2F00,D0               ; enable timer & audio interrupts
                movep.w D0,XM_INT_CTRL(A0)

                lea.l   (Xosera_intr,PC),A0
                move.l  A0,XOSERA_VEC.w         ; set interrupt vector
                and.w   #$F0FF,SR               ; enable interrupts

                movem.l (A7)+,D0-D7/A0-A6
                rts


remove_intr::
                movem.l D0-D7/A0-A6,-(A7)

                lea.l   XM_BASEADDR,A0          ; get Xosera base addr
                moveq.l #$000F,D0               ; disable interrupts, and clear pending
                movep.w D0,XM_INT_CTRL(A0)      ; enable VSYNC interrupt
                move.l  SPURIOUS_VEC.w,D0       ; copy spurious int handler
                move.l  D0,XOSERA_VEC.w         ; to xosera int handler

                movem.l (A7)+,D0-D7/A0-A6
                rts


; interrupt routine
Xosera_intr:
                movem.l D0-D2/A0-A2,-(A7)       ; save minimal regs

                lea.l   XM_BASEADDR,A2          ; get Xosera base addr
                move.b  XM_INT_CTRL+2(A2),D0    ; read pending interrupts                
;                move.b  D0,XM_INT_CTRL+2(A2)    ; acknowledge and clear all interrupts

                btst    #INT_CTRL_TIMER_INTR_B,D0 ; Check timer bit
                beq.s   .AfterTimer             ; Skip timer if zero
                move.b  #INT_CTRL_TIMER_INTR_F,XM_INT_CTRL+2(A2)    ; acknowledge and clear timer
               
                ; Here, it's time to step the mod and reset the counter...
                jsr     ptmodTimeStep

.AfterTimer
                move.b  XM_INT_CTRL+2(A2),D0    ; read pending interrupts
                andi.b  #INT_CTRL_AUD_ALL_F,D0  ; Check audio interrupt bits
                beq.s   .Done
                
                ; Here, it's time to service sample chunks                
                move.l  D0,-(A7)
                jsr     ptmodServiceSamples
                add.l   #4,A7

                move.b  D0,XM_INT_CTRL+2(A2)    ; acknowledge the interrupts we serviced

                bra.s   .AfterTimer

.Done           movem.l (A7)+,D0-D2/A0-A2       ; restore regs
                rte

        section .data
                dc.w    0

        ifd DEBUG_INTERRUPT
DOT             dc.b    '.',0
        endif
