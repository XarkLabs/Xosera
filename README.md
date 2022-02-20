# Xosera - Xark's Open Source Embedded Retro Adapter

##### _"Not as clumsy or random as a GPU, an embedded retro video adapter for a more civilized age."_

### Xosera is an FPGA based video adapter designed with the [rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) retro computer in mind (likely adaptable to most any computer with an 8-bit parallel bus and a few control signals)

Inspired in concept only by it's "namesake" the [Commander X16](https://www.commanderx16.com/)'s VERA, Xosera is an original open-source video adapter design, built with open-source tools, that is being tailored with features appropriate for a Motorola 68K era retro computer.

![Xosera prototype board in rosco_m68k](pics/Xosera_rosco_m68k_board.jpg)  
Picture of Xosera prototype being tested with logic-analyzer (most of the `TODO`s are now done)

Currently the design is using the [iCE40UP5K FPGA](https://www.latticesemi.com/en/Products/FPGAandCPLD/iCE40UltraPlus) which is fully supported by the open [Yosys/NextPNR tools](https://github.com/YosysHQ).  Development is now mostly on the Xosera board for the rosco_m68k system using [UPduino 3.x](https://github.com/tinyvision-ai-inc/UPduino-v3.0), but still builds for [iCEBreaker FPGA](https://github.com/icebreaker-fpga/icebreaker) board.

This is currently a work in progress, but you can follow along at [Hackaday.io](https://hackaday.io/Xark) or in the [rosco_m68k Discord](https://rosco-m68k.com/docs) (in the #xosera-developers or #xosera-users channel).  The design is largely complete, but still a few more features to hopefully squeeze in (and likely a few bugs and issues to investigate).

During development take everything here but the Verilog code with a grain of salt (i.e., the documentation may be out of date - or not written yet).  If you see an issue, feel free to make a pull request or ask on rosco_m68k Discord if you have a question (there are Xosera development and user channels).

While the FPGA design is adaptable, the current primary development focus is for rosco_m68k Xosera PCB, see [rosco_m68k Hardware Projects feature/xosera branch](https://github.com/rosco-m68k/hardware-projects/tree/feature/xosera) which is now available from [Tindie|(<https://www.tindie.com/products/rosco/xosera-fpga-video-r1/>) (thanks Ross 😃).

[See this for information on building and configuring Xosera](BUILDING.md)

Current Xosera features include:

* VGA output at 640x480@60Hz or 848x480@60Hz (16:9 widescreen 480p)
* Register based interface using 16 16-bit main registers (accessed 8-bits at a time)
* 128KB of embedded main video RAM (limited by current modest FPGA)
* 10KB of tile RAM for tilemaps or tile glyph definitions (or either can be stored in main VRAM)
* Xosera memory accessed via multiple 16-bit read/write ports with auto-increment and nibble write masking
* Dual 256 x 16-bit ARGB colormap RAM (16 "blend" values and 4096 colors), one colormap palette per video "playfield"
* Dual overlaid video "playfields" with 4-levels of "alpha blending" or additive blending (wrapping or saturating)
* 8x8 or 8x16 tile based display modes (with adjustable displayed height, e.g., for 8x11)
* 1-bit tiled mode allows 256 8x8/8x16 glyphs (8-bit) and 16 forground/background colors (similar to PC text mode)
* 4-bit and 8-bit tiled modes allow 1024 8x8 glyphs (10-bit), H and/or V mirroring and 16 colormap choices (similar to some consoles)
* 1-bit bitmap mode with 16 forground/background color attribute byte (similar to bitmapped PC text mode)
* 4 or 8-bit "chunky pixel" bitmap mode with 16 or 256 colors (128KB VRAM permitting, not enough for full bitmap at 256 colors)
* Horizontal and/or vertical pixel replication, so pixel size can be from 1x1 up to 4x4 native pixels (e.g., for 320x240 mode)
* Fractional horizontal and/or vertical scaling (e.g., to allow scaling to modes like 640x200 or 512x384 using non-uniform pixel size)
* Smooth horizontal and vertical tile scrolling (native pixel scroll offset)
* Amiga-inspired video-synchronized co-processor ("copper") to alter video display registers or colors on the fly
* Rectangular bitmap "blitter" with support for logical operations, transparency, masking and shifting (~10 million words/sec)
* C API that provides easy low-level register access (and transforms into efficient inline 68K assembly code)
* GNU Make based build using the pre-built [YosysHQ OSS CAD Suite Builds](https://github.com/YosysHQ/oss-cad-suite-build/releases/latest) tested on Linux (Ubuntu 20.04 and also Ubuntu on RPi4 and RISC-V 64), Windows 10 and MacOS.
* Fast Verilator simulation including SPI interface and using SDL2 for PNG screenshot of each video frame
* Icarus Verilog simulation

Planned Xosera features TODO:

* DVI/HDMI output at 640x480@60Hz or 848x480@60Hz (16:9 widescreen 480p) using 1BitSquared DV PMOD (currently this _mostly works_, but is not 100% solid at this point - not exactly sure the issue).
* 4 dual 8-bit (stereo) audio channels with full channel mixer for audio output similar to Amiga
* At least one "cursor" sprite above video playfields (and ideally more, probably with 16 or 256 colors)

Possible improvements for the future:

* Convert VRAM to be 32-bit from current 16-bit width to allow more bandwidth per cycle
* Treat VRAM as multiple banks to allow for more concurrent operations (probably minor win though)
* Line-draw or polygon acceleration (but very "tight" on iCE40UP5K FPGA resources currently)
* In the fullness of time, port to a larger FPGA (like Lattice ECP5) to support more VRAM and improved resolution and features

![Xosera 16-color 640x400 VGA Test](pics/Xosera_16_color_test.jpg)  
Picture of Xosera 16-color 640x400 VGA Test

![Early Xosera 848x480 DVI Font Test](pics/XoseraTest_848x480_DVI.jpg)  
Picture of early Xosera 848x480 DVI Font Test (using iCEBreaker board)
