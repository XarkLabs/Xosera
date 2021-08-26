; *************************************************************
; Copyright (c) 2021 roscopeco <AT> gmail <DOT> com
; *************************************************************
;
        section .text                     ; This is normal code

        include "../xosera_m68k_api/xosera_equates.asm"

install_intr::
        movem.l D0-D7/A0-A6,-(A7)

        move.l  #Xosera_intr,$68
        and.w   #$F0FF,SR
        clr.l   D1
        move.w  SR,D1
        move.l  #15,D0
        move.l  #16,D2
        trap    #15

        movem.l (A7)+,D0-D7/A0-A6
        rts

Xosera_intr:
        movem.l D0-D2/A0,-(A7)

        move.l  #XVR_regbase,A0
        movep.w XVR_aux_addr(A0),D2

        moveq.l #XVA_scanline,D1
        movep.w D1,XVR_aux_addr(A0)
        movep.w XVR_aux_data(A0),D0

;         bra     .Xosera_Vsync

;         moveq.l #XVA_linestart,D1
;         movep.w D1,XVR_aux_addr(A0)
;         move.w  #$F000,D0
;         movep.w XVR_aux_data(A0),D0

;         moveq.l #XVA_gfxctrl,D1
;         movep.w D1,XVR_aux_addr(A0)
;         move.w  #$0000,D0
;         movep.w XVR_aux_data(A0),D0

;         move.b  (A0),D0                 ; clear intr
;         movep.w D2,XVR_aux_addr(A0)
;         movem.l (A7)+,D0-D2/A0
;         rte

; .Xosera_Vsync
        move.w  NUCLEAR,D0
        addq.l  #1,D0
        move.w  NUCLEAR,D0

        move.w  #XVA_COLORMEM+2,D1
        movep.w D1,XVR_aux_addr(A0)
        movep.w D0,XVR_aux_data(A0)

        move.b  (A0),D0                 ; clear intr
        movep.w D2,XVR_aux_addr(A0)
        movem.l (A7)+,D0-D2/A0
        rte

NUCLEAR dc.w  $0000


