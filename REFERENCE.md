# Xosera - Xark's Open Source Embedded Retro Adapter

## Xosera Reference Information

Xosera has 16 16-bit bus accessable registers that are used to control its operation.  The even
and odd bytes in the word can be accessed independently [TODO].

Xosera's 128KB of VRAM is organized as 64K x 16-bit words, so a full VRAM address is 16-bits (and an
individual byte is not directly accessible).

### Xosera 16-bit write regiters

| Reg # | Name                 | Description                                                      |
--------| ---------------------| ---------------------------------------------------------------- |
| 0     | XVID_RD_ADDR         | address to read from VRAM
| 1     | XVID_WR_ADDR         | address to write from VRAM
| 2     | XVID_DATA            | write data word to address in WR_ADDR, add WR_INC to WR_ADDR
| 3     | XVID_DATA_2          | (same as XVID_DATA to allow for 32-bit DATA access)
| 4     | XVID_VID_CTRL        | video control, controls VID_DATA operation
| 5     | XVID_VID_DATA        | write VID_CTRL data (operation controlled by VID_CTRL)
| 6     | XVID_RD_INC          | RD_ADDR increment value
| 7     | XVID_WR_INC          | WR_ADDR increment value
| 8     | XVID_RD_MOD          | RD_ADDR increment per line
| A     | XVID_WR_MOD          | WR_ADDR increment per line
| 9     | XVID_WIDTH           | width for rectangular blit
| B     | XVID_BLIT_COUNT      | blitter repeat count/blit status
| C     | XVID_AUX_RD_ADDR     | aux read address (font audio etc.?)
| D     | XVID_AUX_WR_ADDR     | aux write address (font audio etc.?)
| E     | XVID_AUX_CTRL        | audio and other control, controls AUX_DATA operation
| F     | XVID_AUX_DATA        | write data word to AUX_WR_ADDR (depending on AUX_CTRL)

Only the upper two bits are used when reading registers, so effectively there are 4 of the 16-bit registers
that can be read from (each "repeated" four times):

### Xosera 16-bit read regiters

| Reg # | Name                 | Description                                                      |
--------| ---------------------| ---------------------------------------------------------------- |
| 0 - 3 | XVID_DATA            | read data word from RD_ADDR, add RD_INC to RD_ADDR
| 4 - 7 | XVID_VID_DATA        | read video controller status info
| 8 - B | XVID_BLIT_COUNT      | read blitter status info (blit done, etc.)
| C - F | XVID_AUX_DATA        | read data word from AUX_RD_ADDR (depending on AUX_CTRL)
