# Xosera - Xark's Open Source Embedded Retro Adapter

## Xosera Reference Information

Xosera has 16 16-bit bus accessable registers that are used to control its operation.  The even
and odd bytes in the word can be accessed independently [TODO].

Xosera's 128KB of VRAM is organized as 64K x 16-bit words, so a full VRAM address is 16-bits (and an
individual byte is not directly accessible).

### Xosera video modes

Xosera always outputs a fixed video resolution (either 640x480 or 848x480 widescreen at 60 Hz, fixed at boot),
but it uses several different video modes to control how the display is generated.

| Mode  | Tile size | 640x480 (4:3)                | 848x480 (16:9)                 | Colors                                                |
--------|-----------| -----------------------------|--------------------------------|------------------------------------------------------ |
| Text  | 8x16      | 80x30 tiles<br /> 2400 words |  106x30 tiles<br /> 3180 words | 2 from 16 color palette per tile using attribute byte |

Note 1: Tile size can be 8x16 or 8x8

### Xosera 16-bit registers

| Reg # | Name                 | Description                                                      |
--------| ---------------------| ---------------------------------------------------------------- |
| 0x0     | `XVID_RD_INC`          | `RD_ADDR` increment per word read
| 0x1     | `XVID_WR_INC`          | `WR_ADDR` increment per word write
| 0x2     | `XVID_RD_MOD`          | `WR_ADDR` increment every `WIDTH` words read
| 0x3     | `XVID_WR_MOD`          | `RD_ADDR` increment every `WIDTH` words write
| 0x4     | `XVID_WIDTH`           | width in words for 2-D rectangular blit
| 0x5     | `XVID_RD_ADDR`         | set read address for reading from VRAM [Note 1]
| 0x6     | `XVID_WR_ADDR`         | set write address for writing to VRAM
| 0x7     | `XVID_DATA`            | read/write VRAM word at `RD`/`WR_ADDR` then add `RD`/`WR_INC`[Note 2]
| 0x8     | `XVID_DATA_2`          | 2nd`XVID_DATA`(to allow for 32-bit read/write) [Note 2, 3]
| 0xA     | `XVID_COUNT`           | count of words in async operation
| 0x9     | `XVID_VID_CTRL`        | various video control settings, see below
| 0xB     | `XVID_VID_DATA`        | video control data (depending on `VID_CTRL` see below)
| 0xC     | `XVID_AUX_RD_ADDR`     | TODO aux read address (font audio etc.?)
| 0xD     | `XVID_AUX_WR_ADDR`     | TODO aux write address (font audio etc.?)
| 0xE     | `XVID_AUX_CTRL`        | TODO audio and other control settings
| 0xF     | `XVID_AUX_DATA`        | TODO aux data (depending on `AUX_CTRL`)

Only the upper two bits are used when reading registers, so effectively there are 4 of the 16-bit registers
that can be read from (each "repeated" four times):

1. The word at `VRAM[RD_ADDR]` will be read only when odd byte LSB is written (even byte MSB is saved until then). The word read can be accessed via `XVID_DATA`/`2`.
2. The word Xosera VRAM access is in 16-bit words only, so the even byte (MSB)
3. When XVID_DATA_2 is written to individually (i.e., the preceding register write was not XVID_DATA), it will store
the value written internally, but not write data to VRAM nor apply the WR_INC value.  This is useful to load a constant
value into the blitter (e.g. for a VRAM fill).

### Xosera VID_CTRL regiters

To access these registers, write the register number to `XVID_VID_CTRL`, then write the register data to `XVID_VID_DATA` (these registers are write-only).
(TODO same for plane B)

| Reg # | Name              | Description                                                                 |
--------| ------------------| ----------------------------------------------------------------------------|
| 0x0   | A_start_addr      | [15:0] starting VRAM address for display (wraps at 0xffff)                  |
| 0x1   | A_words_per_line  | [15:0] words per line (TODO a bit funky...)                                 |
| 0x2   | A_fine_scroll     | [10:8] horizontal (0-7) pixel scroll, [3:0] vertical (0-15) pixel scroll    |
| 0x3   | A_font_ctrl       | [8] font bank (0/1), [3:0] font Y height-1 (0-15) (truncated, TODO 8x8 etc.)|
