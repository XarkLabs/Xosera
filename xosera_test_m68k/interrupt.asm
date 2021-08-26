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
                movem.l D0-D2/A0,-(A7)

                move.l  #XVR_regbase,A0         ; get Xosera addr
                movep.w XVR_aux_addr(A0),D2     ; save aux_addr value

                moveq.l #XVA_scanline,D1        ; current scanline reg
                movep.w D1,XVR_aux_addr(A0)
                movep.w XVR_aux_data(A0),D0     ; read scanline

                tst.w   D0
                bmi     .Xosera_Vsync           ; if in blank, assume vsync

                moveq.l #XVA_linestart,D1
                movep.w D1,XVR_aux_addr(A0)
                move.w  #$0000,D0
                movep.w D0,XVR_aux_data(A0)

                movep.w D2,XVR_aux_addr(A0)
                move.b  (A0),D0
                movem.l (A7)+,D0-D2/A0
                rte

.Xosera_Vsync
                move.w  NUCLEAR,D0
                add.w   #1,D0
                and.w   #$0FFF,D0
                move.w  D0,NUCLEAR
                add.l   #1,XFrameCount

                move.w  #XVA_COLORMEM+2,D1
                movep.w D1,XVR_aux_addr(A0)
                movep.w D0,XVR_aux_data(A0)

                movep.w D2,XVR_aux_addr(A0)
                move.b  (A0),D0
                movem.l (A7)+,D0-D2/A0
                rte

NUCLEAR         dc.w    $0000
XFrameCount::   dc.l    $00000000


