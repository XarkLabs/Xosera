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
                movem.l D0-D1/A0,-(A7)          ; save minimal regs

                move.l  #XVR_regbase,A0         ; get Xosera base addr
                movep.w XVR_aux_addr(A0),D1     ; save aux_addr value

                moveq.l #XVA_scanline,D0        ; current scanline reg
                movep.w D0,XVR_aux_addr(A0)
                movep.w XVR_aux_data(A0),D0     ; read scanline

                tst.w   D0                      ; must tst, movep does not set flags
                bmi     .Xosera_Vsync           ; if in blank, assume vsync

                moveq.l #XVA_linestart,D0       ; set line_start reg
                movep.w D0,XVR_aux_addr(A0)
                moveq.l #$0000,D0               ; to VRAM addr $0000
                movep.w D0,XVR_aux_data(A0)

                movep.w D1,XVR_aux_addr(A0)     ; restore aux_addr
                movem.l (A7)+,D0-D1/A0          ; restore regs
                rte                             ; return from interrupt

.Xosera_Vsync
                move.w  #XVA_COLORMEM+2,D0      ; set color entry #2
                movep.w D0,XVR_aux_addr(A0)

                move.w  NukeColor,D0            ; increment NukeColor
                addq.l  #1,D0
                move.w  D0,NukeColor

                movep.w D0,XVR_aux_data(A0)     ; to NukeColor

                add.l   #1,XFrameCount          ; increment frame counter

                movep.w D2,XVR_aux_addr(A0)     ; restore aux_addr
                movem.l (A7)+,D0-D1/A0          ; restore regs
                rte

NukeColor       dc.w    $0000
XFrameCount::   dc.l    $00000000


