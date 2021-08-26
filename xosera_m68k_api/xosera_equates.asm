;------------------------------------------------------------
;                                  ___ ___ _   
;  ___ ___ ___ ___ ___       _____|  _| . | |_ 
; |  _| . |_ -|  _| . |     |     | . | . | '_|
; |_| |___|___|___|___|_____|_|_|_|___|___|_,_| 
;                     |_____|       software v1 
;------------------------------------------------------------
; Copyright (c)2021 Ross Bamford
; See top-level LICENSE.md for licence information.
;
; Equates for Xosera (XVID) by Xark
;------------------------------------------------------------

; XVR direct registers (directly accessable in m68K memory space, 8-bits, every other byte)
XVR_regbase     equ     $f80060     ; Xosera register base address
XVR_aux_addr    equ     $0          ; (RW)  XVA read/write address      
XVR_const_val   equ     $4          ; (RW)  XVA constant data           
XVR_rd_addr     equ     $8          ; (RW)  VRAM read address           
XVR_wr_addr     equ     $C          ; (RW)  VRAM write address          
XVR_data        equ     $10         ; (RW*) VRAM read/write data        
XVR_data_2      equ     $14         ; (RW*) VRAM read/write data (2nd)  
XVR_aux_data    equ     $18         ; (RW*) XVA read/write data         
XVR_count       equ     $1C         ; (RW*) Blitter "repeat" count      
XVR_rd_inc      equ     $20         ; (RW)  Read address increment      
XVR_wr_inc      equ     $24         ; (RW)  Write address increment     
XVR_wr_mod      equ     $28         ; (RW)  Read modulo width           
XVR_rd_mod      equ     $2C         ; (RW)  Write modulo width          
XVR_width       equ     $30         ; (RW)  Width for 2D blit           
XVR_blit_ctrl   equ     $34         ; (RW*) Blitter control             
XVR_unused_e    equ     $38         ; Unused at present           
XVR_unused_f    equ     $3C         ; Unused at present

; XVA aux registers (store addr to XVR_aux_addr, read/write XVR_aux_data)
XVA_dispstart   equ     $0000       ; (RW)  display VRAM start address
XVA_dispwidth   equ     $0001       ; (RW)  display address increment for each display line
XVA_scrollxy    equ     $0002       ; (RW)  H & V scroll offset
XVA_fontctrl    equ     $0003       ; (RW)  font base address, in fontRAM/VRAM, font height-1
XVA_gfxctrl     equ     $0004       ; (RW)  display graphics mode, pixel repeat and colorbase
XVA_linestart   equ     $0005       ; (RW)  address of next display line (for altering mid-display)
XVA_lineintr    equ     $0006       ; (RW)  scan line to generate interrupt [15] enable
XVA_unused_7    equ     $0007
XVA_vidwidth    equ     $0008       ; (RO)  native display width in pixels
XVA_vidheight   equ     $0009       ; (RO)  native display height in pixels
XVA_features    equ     $000A       ; (RO)  Xosera feature bits [TODO]
XVA_scanline    equ     $000B       ; (RO)  Read current scanline and blanking status
XVA_githash_h   equ     $000C       ; (RO)  upper 16-bits of 32-bit Git hash ID
XVA_githash_l   equ     $000D       ; (RO)  lower 16-btis of 32-bit Git hash ID
XVA_unused_e    equ     $000E
XVA_unused_f    equ     $000F

XVA_FONTMEM     equ     $4000    ; 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
XVA_COLORMEM    equ     $8000    ; 0x8000-0x80FF 256 word color lookup table (0xXRGB)
XVA_AUDIOMEM    equ     $C000    ; 0xC000-0xFFFF TODO (audio registers)
