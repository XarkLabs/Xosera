# Building Xosera and Configuration

## ​Tools Needed

Xosera uses the [Yosys/NextPNR​](https://github.com/YosysHQ) open-source FPGA toolchain and is currently targeting the [Lattice iCE40UltraPlus](https://www.latticesemi.com/en/Products/FPGAandCPLD/iCE40UltraPlus) 5K FPGA.  While these tools can all be built from source, I have been using the convenient binary releases from the [open-tool-forge/fpga-toolchain​] project (available for Linux, MacOS and Windows).  These can be easily used without privileges (just unpack and set your path) and have pretty much all the tools required to synthesize designs for the Lattice iCE40 and ECP5 FPGAs.

I have tested Xosera FPGA bitstream generation successfully on all three OSes.  Under Windows I used [Cygwin64](https://cygwin.com/) shell for GNU "make" and a few other Unix utilities ([MSYS2](https://www.msys2.org/) may also work).

However, note that the simulation targets require either [Verilator](https://www.veripool.org/wiki/verilator)​ or [Icarus Verilog](http://iverilog.icarus.com/)​ to be installed (which are not currently included in the fpga-toolchain releases).  These are primarily used for development, debugging and testing of the design (and allows inspection of all signals over time - hard to do inside a real FPGA).

There is also a simple C++ FTDI utility included "host_spi", that can be used to send SPI target commands from the PC via USB FTDI to the Xosera design (for easier testing and development).  Currently this utility has only been tested on Linux (it uses the open-source [libftdi](https://www.intra2net.com/en/developer/libftdi/) library​ and should be fairly portable).

## Top-level Makefile Targets

In the top directory of Xosera, there is a "driver" Makefile that has the following targets:

* **all** (default target)
  * builds all targets (Xosera bitstream for iCEBreaker and Upduino, all simulation targets and host_spi FTDI utility)
* **sim** - builds simulation targets
  * Uses sim/Makefile
  * requires Verilator and/or Icarus Verilog tools installed
* [**icebreaker**](#icebreaker-target) - builds iCEBreaker FPGA Xosera bitstream
  * Uses icebreaker/Makefile
* **upduino** - builds Upduino FPGA Xosera bitstream
  * Uses upduino/Makefile
* **host_spi** - builds the FTDI SPI target utility
  * Uses host_spi/Makefile
* **clean** - deletes all output files that can be rebuilt
  * cleans icebreaker, upduino, host_spi and sim directories

<a name="icebreaker-target"></a>

## Icebreaker FPGA Target

​The iCEBreaker FPGA​ board is a great little open-source FPGA iCE40UP5K board.  It has three 2x6 PMOD connectors as well as FTDI 2232H JTAG and 16MB of flash.  It is very convenient to use with the PMOD connectors, so typically is my main Xosera development target.

In the "icebreaker" directory is the Makefile to build Xosera for the iCEBreaker FPGA board.  There are several "​​VIDEO_OUTPUT" options for different PMOD display devices, as well as several possible "VIDEO_MODE" settings (see below for VIDEO_MODE settings).

​iCEBreaker VIDEO_OUPUT options include the following for selecting video output device:

* **PMOD_1B2_DVI12** - 12-bit [DVI via HDMI PMOD from 1BitSquared​](https://1bitsquared.com/products/pmod-digital-video-interface) (as shown [here​](https://hackaday.io/project/173731/gallery#0a6102557e8b8c3aa18dedca5f91d63a))
* **​​PMOD_DIGILENT_VGA** - 12-bit [​VGA PMOD from Digilent](https://store.digilentinc.com/pmod-vga-video-graphics-array/)​
* **PMOD_XESS_VGA** - 12-bit [StickIt!-​VGA PMOD from Xess](http://www.xess.com/shop/product/stickit-vga/)​ (out of production - but [design is open](https://github.com/xesscorp/StickIt/tree/master/modules/Vga))​
* **PMOD_XESS_VGA_SINGLE** - ​6-bit using a single PMOD from [StickIt!-​VGA PMOD from Xess](http://www.xess.com/shop/product/stickit-vga/)​ (out of production - but [design is open](https://github.com/xesscorp/StickIt/tree/master/modules/Vga))

<a name="upduino-target"></a>

## Upduino FPGA Target

​The [Upduino 3.0​](https://github.com/tinyvision-ai-inc/UPduino-v3.0) is a new low-cost open-source FPGA iCE40UP5K ​board suitable for breadboards or embedding.  It is based on the older [Upduino 2.x​](https://github.com/tinyvision-ai-inc/UPduino-v2.1) design, but has some [significant issues fixed](https://hackaday.io/page/8864-upduino-30-third-time-appears-to-be-the-charm) (as well as some nice improvements).  Upduino boards require a VGA (or HDMI/DVI) breakout board or breadboard hookup. I have been using a modified Xess StickIt!-VGA (without all pins soldered, as shown [here](https://hackaday.io/project/173731/gallery#8e9ad0d7c922e14d922da6ecdfc4d165)​), but any 3.3V VGA breakout-board should work (up to 4-bits red, green and blue for 4096 colors, but less bits also works).  You can also just wire a VGA connector and a few resistors to make an [R2R DAC](https://en.wikipedia.org/wiki/Resistor_ladder#R%E2%80%932R_resistor_ladder_network_(digital_to_analog_conversion))​ as shown [here](https://papilio.cc/index.php?n=Papilio.ArcadeMegaWing)​ (or [here](https://fraserinnovations.com/fpga-tutor/fpga-beginner-tutorial-vga-experiment-fpga-board-for-beginner-experiment-13/)​ or an 8-color version [here](https://www.fpga4fun.com/PongGame.html)).

It is likely that you will also need to change the pin mappings used in [upduino/xosera_upd.v](upduino/xosera_upd.v) (look for the comment "Change video output pin mappings here").

If you are using an Upduino 3.0, then you will need to short the "OSC" jumper so gpio_20 is 12MHz input clock.  If you choose not to do this (or you are using an Upduino 2.x with working PLL), you can also run a wire from 12Mhz output pin to gpio_35.

* Add **-DCLKGPIO35** to DEFINES in [upduino/Makefile](upduino/Makefile) when using gpio_35 as clock input

Note that the PLL on Upduino 2.x boards [tends to be unreliable](https://tinyvision.ai/blogs/processing-at-the-edge/ground-trampolines-and-phase-locked-loops).  I have gotten around this problem by directly using the target VGA pixel clock frequency input (e.g., 25MHz for 640x480) and avoiding use of the PLL (as shown [here](https://hackaday.io/project/173731/gallery#4b2ea717d510a35635f8f468754db238)).

* Add **-DNOPLL** to DEFINES in [upduino/Makefile](upduino/Makefile) when directly inputting the pixel clock frequency

## All FPGA Targets

* **all** (default target)
  * builds FPGA bitstream
* **prog**
  * build FPGA bitstream and program board via USB
* **timing**
  * run icetime for timing report (.rpt)
* **count**
  * produce count log showing resources used per module
* **lint**
  * run lint only pass with Verilator
* **show**
  * produce DOT diagrams showing each module graphically
* **clean**
  * deletes all output files that can be rebuilt

[WIP]
