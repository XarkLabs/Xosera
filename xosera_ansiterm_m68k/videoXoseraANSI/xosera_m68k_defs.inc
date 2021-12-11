;
;  vim: set et ts=8 sw=4
; ------------------------------------------------------------
;   __ __
;  |  |  |___ ___ ___ ___ ___
;  |-   -| . |_ -| -_|  _| .'|
;  |__|__|___|___|___|_| |__,|
;
;  Xark's Open Source Enhanced Retro Adapter
;
;  - "Not as clumsy or random as a GPU, an embedded retro
;     adapter for a more civilized age."
;
;  ------------------------------------------------------------
;  Copyright (c) 2021 Xark
;  MIT License
;
;  Xosera rosco_m68k asm register definition header file
;  ------------------------------------------------------------

; See: https://github.com/XarkLabs/Xosera/blob/master/REFERENCE.md

XM_BASEADDR     equ     $f80060     ; Xosera rosco_m68k register base address

; Xosera Main Registers (XM Registers, directly CPU accessable)
; NOTE: Main register numbers are multiplied by 4 for rosco_m68k, because of even byte 6800 8-bit addressing plus
; 16-bit registers
XM_XR_ADDR      equ     $0          ; (R /W+) XR register number/address for XM_XR_DATA read/write access
XM_XR_DATA      equ     $4          ; (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
XM_RD_INCR      equ     $8          ; (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
XM_RD_ADDR      equ     $C          ; (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
XM_WR_INCR      equ     $10         ; (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
XM_WR_ADDR      equ     $14         ; (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
XM_DATA         equ     $18         ; (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR (and add XM_RD_INCR/XM_WR_INCR)
XM_DATA_2       equ     $1C         ; (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
XM_SYS_CTRL     equ     $20         ; (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
XM_TIMER        equ     $24         ; (RO   ) read 1/10th millisecond timer [TODO]
XM_UNUSED_A     equ     $28         ; (R /W ) unused direct register 0xA [TODO]
XM_UNUSED_B     equ     $2C         ; (R /W ) unused direct register 0xB [TODO]
XM_RW_INCR      equ     $30         ; (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
XM_RW_ADDR      equ     $34         ; (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
XM_RW_DATA      equ     $38         ; (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
XM_RW_DATA_2    equ     $3C         ; (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)

; XR Extended Register / Region (accessed via XM_XR_ADDR and XM_XR_DATA)

; XR Memory Regions
XR_COLOR_ADDR   equ     $8000       ; (R/W) $8000-$81FF 2 x A & B color lookup memory
XR_COLOR_SIZE   equ     $0200       ;                   2 x 256 x 16-bit words  ($ARGB)
XR_COLOR_A_ADDR equ     $8000       ; (R/W) $8000-$80FF A 256 entry color lookup memory
XR_COLOR_A_SIZE equ     $0100       ;                   256 x 16-bit words ($ARGB)
XR_COLOR_B_ADDR equ     $8100       ; (R/W) $8100-$81FF B 256 entry color lookup memory
XR_COLOR_B_SIZE equ     $0100       ;                   256 x 16-bit words ($ARGB)
XR_TILE_ADDR    equ     $A000       ; (R/W) $A000-$B3FF tile glyph/tile map memory
XR_TILE_SIZE    equ     $1400       ;                   5120 x 16-bit tile glyph/tile map memory
XR_COPPER_ADDR  equ     $C000       ; (R/W) $C000-$C7FF copper program memory (32-bit instructions)
XR_COPPER_SIZE  equ     $0800       ;                   2048 x 16-bit copper program memory addresses
XR_UNUSED_ADDR  equ     $E000       ; (-/-) $E000-$FFFF unused

; Video Config and Copper XR Registers
XR_VID_CTRL     equ     $0000       ; (R /W) display control and border color index
XR_COPP_CTRL    equ     $0001       ; (R /W) display synchronized coprocessor control
XR_UNUSED_02    equ     $0002       ; (R /W) #TODO
XR_UNUSED_03    equ     $0003       ; (R /W) #TODO
XR_UNUSED_04    equ     $0004       ; (R /W) #TODO
XR_UNUSED_05    equ     $0005       ; (R /W) #TODO
XR_VID_LEFT     equ     $0006       ; (R /W) left edge of active display window (typically 0)
XR_VID_RIGHT    equ     $0007       ; (R /W) right edge of active display window (typically 639 or 847)
XR_SCANLINE     equ     $0008       ; (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline
XR_UNUSED_09    equ     $0009       ; (RO  )
XR_VERSION      equ     $000A       ; (RO  ) Xosera optional feature bits [15:8] and version code [7:0] [TODO]
XR_GITHASH_H    equ     $000B       ; (RO  ) [15:0] high 16-bits of 32-bit Git hash build identifier
XR_GITHASH_L    equ     $000C       ; (RO  ) [15:0] low 16-bits of 32-bit Git hash build identifier
XR_VID_HSIZE    equ     $000D       ; (RO  ) native pixel width of monitor mode (e.g. 640/848)
XR_VID_VSIZE    equ     $000E       ; (RO  ) native pixel height of monitor mode (e.g. 480)
XR_VID_VFREQ    equ     $000F       ; (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)

; Playfield A Control XR Registers
XR_PA_GFX_CTRL  equ     $0010       ; (R /W) playfield A graphics control
XR_PA_TILE_CTRL equ     $0011       ; (R /W) playfield A tile control
XR_PA_DISP_ADDR equ     $0012       ; (R /W) playfield A display VRAM start address
XR_PA_LINE_LEN  equ     $0013       ; (R /W) playfield A display line width in words
XR_PA_HV_SCROLL equ     $0014       ; (R /W) playfield A horizontal and vertical fine scroll
XR_PA_LINE_ADDR equ     $0015       ; (R /W) playfield A scanline start address (loaded at start of line)
XR_PA_UNUSED_16 equ     $0016       ;
XR_PA_UNUSED_17 equ     $0017       ;

; Playfield B Control XR Registers
XR_PB_GFX_CTRL  equ     $0018       ; (R /W) playfield B graphics control
XR_PB_TILE_CTRL equ     $0019       ; (R /W) playfield B tile control
XR_PB_DISP_ADDR equ     $001A       ; (R /W) playfield B display VRAM start address
XR_PB_LINE_LEN  equ     $001B       ; (R /W) playfield B display line width in words
XR_PB_HV_SCROLL equ     $001C       ; (R /W) playfield B horizontal and vertical fine scroll
XR_PB_LINE_ADDR equ     $001D       ; (R /W) playfield B scanline start address (loaded at start of line)
XR_PB_UNUSED_1E equ     $001E       ;
XR_PB_UNUSED_1F equ     $001F       ;

; Blitter XR Registers
XR_BLIT_CTRL    equ     $0020       ; (R /W) blit control bits (transparency control, logic op and op input flags)
XR_BLIT_VAL_T   equ     $0021       ; (R /W) blit transparency constant (XOR'd with B and used for 4/8-bit zero tests)
XR_BLIT_MOD_B   equ     $0022       ; (R /W) blit modulo added to B source after each line
XR_BLIT_SRC_B   equ     $0023       ; (R /W) blit B source VRAM read address / constant value
XR_BLIT_MOD_D   equ     $0024       ; (R /W) blit modulo added to D destination after each line
XR_BLIT_VAL_C   equ     $0025       ; (R /W) blit C source XOR constant value
XR_BLIT_MOD_A   equ     $0026       ; (R /W) blit modulo added to A source after each line
XR_BLIT_SRC_A   equ     $0027       ; (R /W) blit A source VRAM read address / constant value
XR_BLIT_SHIFT   equ     $0028       ; (R /W) blit first and last word nibble masks and nibble right shift (0-3)
XR_BLIT_DST_D   equ     $0029       ; (R /W) blit D VRAM destination write address
XR_BLIT_LINES   equ     $002A       ; (R /W) blit number of lines minus 1, (repeats blit word count after modulo calc)
XR_BLIT_WORDS   equ     $002B       ; (R /W) blit word count minus 1 per line (write starts blit operation)
