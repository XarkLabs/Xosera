// reg_interface.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

`ifdef ICE40UP5K
`define WR_ADD_MAC                  // on iCE40UP5K "waste" a DSP as 16-bit adder (saves ~20 LCs)
`endif

module reg_interface (
    // bus interface signals
    input  wire logic           bus_cs_n_i,        // register select strobe
    input  wire logic           bus_rd_nwr_i,      // 0 = write, 1 = read
    input  wire logic  [3:0]    bus_reg_num_i,     // register number
    input  wire logic           bus_bytesel_i,     // 0=even byte, 1=odd byte
    input  wire logic  [7:0]    bus_data_i,        // 8-bit data bus input
    output      logic  [7:0]    bus_data_o,        // 8-bit data bus output
    // VRAM/XR access signals
    input  wire logic           vram_ack_i,        // VRAM access ack (true when data read/written)
    input  wire logic           xr_ack_i,          // XR bus access ack (true when data read/written)
    output      logic           regs_vram_sel_o,   // VRAM select
    output      logic           regs_xr_sel_o,     // XR select
    output      logic           regs_wr_o,         // VRAM/XR read/write
    output      logic  [3:0]    regs_wrmask_o,     // VRAM nibble write masks
    output      addr_t          regs_addr_o,       // VRAM/XR address
    output      word_t          regs_data_o,       // VRAM/XR write data out
    input  wire word_t          regs_data_i,       // VRAM read data in
    input  wire word_t          xr_data_i,         // XR read data in
    // status signals
`ifdef EN_BLIT
    input  wire logic           blit_full_i,       // blit register queue full
    input  wire logic           blit_busy_i,       // blit operation in progress
`endif
    input  wire logic           h_blank_i,         // pixel outside of visible range (before left edge)
    input  wire logic           v_blank_i,         // line outside of visible range (after bottom line)
    // iCE40 reconfigure
    output      logic           reconfig_o,        // reconfigure iCE40 from flash
    // interrupt management
`ifdef EN_TIMER_INTR
    output      logic           timer_intr_o,      // timer compare interrupt
`endif
    output      intr_t          intr_mask_o,       // enabled interrupts (which signal CPU interrupt)
    output      intr_t          intr_clear_o,      // pending interrupts CPU acknowledge (clear)
    input  wire intr_t          intr_status_i,     // pending interrupts CPU status read
`ifdef EN_UART
    output      logic           uart_txd_o,        // UART receive signal
`ifndef EN_UART_TX
    input wire  logic           uart_rxd_i,        // UART transmit signal
`endif
`endif
`ifdef BUS_DEBUG_SIGNALS
    output      logic           bus_ack_o,         // ACK strobe for bus debug
`endif
    input  wire logic           reset_i,           // reset signal
    input  wire logic           clk                // pixel clock
);

// read/write storage for main interface registers
addr_t          reg_rd_xaddr;           // XR read address (RD_XADDR)
addr_t          reg_wr_xaddr;           // XR write address (WR_XADDR)
word_t          reg_xdata;              // word read from XR bus (for RD_XDATA)

word_t          reg_rd_incr;            // VRAM read increment
addr_t          reg_rd_addr;            // VRAM read address
word_t          reg_data;               // word read from VRAM (for RD_ADDR)

word_t          reg_wr_incr;            // VRAM write increment
addr_t          reg_wr_addr;            // VRAM write address

word_t          reg_timer;              // 1/10 ms timer
`ifdef EN_TIMER_INTR
byte_t          reg_timer_interval;     // 8-bit 1/10 ms timer interrupt interval
byte_t          reg_timer_countdown;    // 8-bit timer interrupt interval counter
`endif

`ifdef EN_UART
logic           uart_wr;
logic           uart_txf;
byte_t          uart_din;
`ifndef EN_UART_TX
logic           uart_rd;
logic           uart_rxf;
byte_t          uart_dout;
`endif
`endif

// read flags
logic           xr_rd;                  // flag for XR_DATA read outstanding
logic           vram_rd;                // flag for DATA read outstanding

logic           rd_incr_flag;
logic           wr_incr_flag;
logic           xrd_incr_flag;
logic           xwr_incr_flag;

logic  [3:0]    bus_reg_num;            // bus register on bus
logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic           bus_bytesel;            // msb/lsb on bus
byte_t          bus_data_byte;          // data byte from bus

byte_t          timer_latch_val;        // low byte of timer (latched on high byte read)
byte_t          reg_xdata_even;         // byte written to even byte of XR_XDATA
byte_t          reg_data_even;          // byte written to even byte of XM_DATA/XM_DATA_2

logic mem_wait;
assign mem_wait    = regs_wr_o | xr_rd | vram_rd;

// output interrupt mask
intr_t intr_mask;
assign intr_mask_o = intr_mask;

`ifdef BUS_DEBUG_SIGNALS    // debug "ack" bus strobe
assign bus_ack_o = (bus_write_strobe | bus_read_strobe);
`endif

// bus_interface handles signal synchronization, CS and register writes to Xosera
bus_interface bus(
    .bus_cs_n_i(bus_cs_n_i),              // register select strobe
    .bus_rd_nwr_i(bus_rd_nwr_i),          // 0=write, 1=read
    .bus_reg_num_i(bus_reg_num_i),        // register number
    .bus_bytesel_i(bus_bytesel_i),        // 0=even byte, 1=odd byte
    .bus_data_i(bus_data_i),              // 8-bit data bus input
    .write_strobe_o(bus_write_strobe),    // strobe for bus byte write
    .read_strobe_o(bus_read_strobe),      // strobe for bus byte read
    .reg_num_o(bus_reg_num),              // register number from bus
    .bytesel_o(bus_bytesel),              // register number from bus
    .bytedata_o(bus_data_byte),           // byte data from bus
    .reset_i(reset_i),                     // reset
    .clk(clk)                             // input clk (should be > 2x faster than bus signals)
);

`ifdef EN_UART
acia #(
    .BPS_RATE(xv::UART_BPS),
    .CLK_HZ(xv::PCLK_HZ)
) uart (
`ifndef EN_UART_TX
    .rd_i(uart_rd),
`endif
    .wr_i(uart_wr),
    .rs_i(bus_bytesel),
`ifndef EN_UART_TX
    .rx_i(uart_rxd_i),
`endif
    .tx_o(uart_txd_o),
    .din_i(uart_din),
`ifndef EN_UART_TX
    .dout_o(uart_dout),
`endif
    .txf_o(uart_txf),
`ifndef EN_UART_TX
    .rxf_o(uart_rxf),
`endif
    .rst_i(reset_i),
    .clk(clk)
);
`endif

// ~1/10th ms timer counter
//
// see https://www.excamera.com/sphinx/vhdl-clock.html
`ifdef MODE_640x480
// >>> Fraction(10000, 25125000)
// Fraction(2, 5025)
localparam  CLK_REDUCED     = 5025;
localparam  HZ_REDUCED      = 2;
`elsif MODE_848x480
// >>> Fraction(10000, 33750000)
// Fraction(1, 3375)
localparam  CLK_REDUCED     = 3375;
localparam  HZ_REDUCED      = 1;
`else
// NOTE: Assumes PCLK_HZ is divisible by 1000
localparam  CLK_REDUCED     = xv::PCLK_HZ / 1000;
localparam  HZ_REDUCED      = 10;
`endif

localparam  FRAC_BITS       = $clog2(CLK_REDUCED)+1;
logic [FRAC_BITS-1:0]       reg_timer_frac;

logic           tick;
assign          tick            = !reg_timer_frac[FRAC_BITS-1];
`ifdef EN_TIMER_INTR
logic           reg_timer_zero;
assign          reg_timer_zero  = (reg_timer_countdown == 0) ? 1'b1 : 1'b0;
byte_t          reg_timer_next;         // 8-bit timer interrupt interval counter next cycle
assign          reg_timer_next  = reg_timer_zero ? reg_timer_interval : reg_timer_countdown - 1'b1;
`endif

always_ff @(posedge clk) begin
    if (reset_i) begin
        reg_timer           <= '0;
        reg_timer_frac      <= '0;
`ifdef EN_TIMER_INTR
        timer_intr_o        <= '0;
        reg_timer_countdown <= '0;
`endif
    end else begin
`ifdef EN_TIMER_INTR
        timer_intr_o        <= '0;
`endif
        if (tick) begin
            reg_timer_frac      <= reg_timer_frac + (HZ_REDUCED - CLK_REDUCED);
            reg_timer           <= reg_timer + 1'b1;
`ifdef EN_TIMER_INTR
            timer_intr_o        <= reg_timer_zero;
            reg_timer_countdown <= reg_timer_next;
`endif
        end else begin
            reg_timer_frac      <= reg_timer_frac + HZ_REDUCED;
        end
    end
end

// continuously output byte selected for read from Xosera (to be put on bus when selected for read)
word_t      rd_temp_word;
always_comb bus_data_o  = !bus_bytesel ? rd_temp_word[15:8] : rd_temp_word[7:0];

`ifdef EN_UART
`ifndef EN_UART_TX
always_comb uart_rd    = bus_read_strobe && (bus_reg_num == xv::XM_UART);
`endif
`endif

// xm registers read
always_comb begin
    case (bus_reg_num)
        xv::XM_SYS_CTRL:
            rd_temp_word  = { mem_wait,
`ifdef EN_BLIT
                blit_full_i, blit_busy_i,
`else
                1'b0,        1'b0,
`endif
                1'b0, h_blank_i, v_blank_i,
`ifdef EN_PIXEL_ADDR
                pixel_bpp,
`else
                1'b0, 1'b0,
`endif
                4'b0, regs_wrmask_o };
        xv::XM_INT_CTRL:
            rd_temp_word  = { 1'b0, intr_mask, 1'b0, intr_status_i };
        xv::XM_TIMER:
            rd_temp_word  = { reg_timer[15:8], timer_latch_val };
        xv::XM_RD_XADDR:
            rd_temp_word  = reg_rd_xaddr;
        xv::XM_WR_XADDR:
            rd_temp_word  = reg_wr_xaddr;
        xv::XM_XDATA:
            rd_temp_word  = reg_xdata;
        xv::XM_RD_INCR:
            rd_temp_word  = reg_rd_incr;
        xv::XM_RD_ADDR:
            rd_temp_word  = reg_rd_addr;
        xv::XM_WR_INCR:
            rd_temp_word  = reg_wr_incr;
        xv::XM_WR_ADDR:
            rd_temp_word  = reg_wr_addr;
        xv::XM_DATA,
        xv::XM_DATA_2:
            rd_temp_word  = reg_data;
`ifdef EN_UART
        xv::XM_UART:
`ifndef EN_UART_TX
            rd_temp_word  = {uart_rxf, uart_txf, 6'b000000, uart_dout };
`else
            rd_temp_word  = {1'b0, uart_txf, 6'b000000, 8'h00 };
`endif
`endif
        xv::XM_FEATURE:
            rd_temp_word  =
                (16'(xv::FPGA_CONFIG_NUM)   << xv::FEATURE_CONFIG)  |
                (16'(xv::AUDIO_NCHAN)       << xv::FEATURE_AUDCHAN) |

`ifdef EN_UART
                (16'b1                      << xv::FEATURE_UART)    |
`endif
`ifdef EN_PF_B
                (16'b1                      << xv::FEATURE_PF_B)    |
`endif
`ifdef EN_BLIT
                (16'b1                      << xv::FEATURE_BLIT)    |
`endif
`ifdef EN_COPP
                (16'b1                      << xv::FEATURE_COPP)    |
`endif
                (16'(xv::VIDEO_MODE_NUM)    << xv::FEATURE_MONRES);
        default:
            rd_temp_word    = '0;
    endcase
end

// xm registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signal strobes
        reconfig_o      <= 1'b0;
        intr_clear_o    <= '0;

        // control signals
        regs_vram_sel_o <= 1'b0;
        regs_xr_sel_o   <= 1'b0;
        regs_wr_o       <= 1'b0;
        vram_rd         <= 1'b0;
        xr_rd           <= 1'b0;

        // addr/data out
        regs_addr_o     <= 16'h0000;
        regs_data_o     <= 16'h0000;

        // xosera registers
        reg_rd_xaddr    <= 16'h0000;
        reg_wr_xaddr    <= 16'h0000;
        reg_rd_addr     <= 16'h0000;
        reg_rd_incr     <= 16'h0000;
        reg_wr_addr     <= 16'h0000;
        reg_wr_incr     <= 16'h0000;
        regs_wrmask_o   <= 4'b1111;
        intr_mask       <= '0;

`ifdef EN_PIXEL_ADDR
        pixel_strobe    <= '0;
        reg_pixel_x     <= '0;
        reg_pixel_y     <= '0;
        pixel_base      <= '0;
        pixel_width     <= '0;
        pixel_bpp       <= '0;
`endif

        // temp registers
        timer_latch_val <= 8'h00;
        reg_data_even   <= 8'h00;
        reg_xdata_even  <= 8'h00;

        reg_data        <= '0;
        reg_xdata       <= '0;

`ifdef EN_TIMER_INTR
        reg_timer_interval  <= '0;
`endif

`ifdef EN_UART
        uart_wr         <= 1'b0;
        uart_din        <= '0;
`endif
        rd_incr_flag    <= 1'b0;
        wr_incr_flag    <= 1'b0;
        xrd_incr_flag   <= 1'b0;
        xwr_incr_flag   <= 1'b0;

    end else begin
        // clear strobe signals
        intr_clear_o    <= '0;

`ifdef EN_PIXEL_ADDR
        pixel_strobe    <= 1'b0;
`endif

`ifdef EN_UART
        uart_wr         <= 1'b0;
`endif
        rd_incr_flag    <= 1'b0;
        wr_incr_flag    <= 1'b0;
        xrd_incr_flag   <= 1'b0;
        xwr_incr_flag   <= 1'b0;

        // VRAM access acknowledge
        if (vram_ack_i) begin
            // if rd read then save rd data, increment rd_addr
            if (vram_rd) begin
                reg_data        <= regs_data_i;
                rd_incr_flag    <= 1'b1;
            end

            // if we did a wr write, increment wr addr
            if (regs_wr_o) begin
                wr_incr_flag    <= 1'b1;
            end

            regs_vram_sel_o <= 1'b0;
            regs_wr_o       <= 1'b0;
            vram_rd         <= 1'b0;
        end

        // XR access acknowledge
        if (xr_ack_i) begin
            if (xr_rd) begin
                reg_xdata       <= xr_data_i;
                xrd_incr_flag   <= 1'b1;
            end

            if (regs_wr_o) begin
                xwr_incr_flag   <= 1'b1;
            end

            regs_xr_sel_o   <= 1'b0;            // clear xr select
            regs_wr_o       <= 1'b0;            // clear write
            xr_rd           <= 1'b0;            // clear pending xr read
        end

        if (wr_incr_flag) begin
`ifdef WR_ADD_MAC
            reg_wr_addr     <= reg_wr_result;
`else
            reg_wr_addr     <= reg_wr_addr + reg_wr_incr;
`endif
        end

        if (xwr_incr_flag) begin
            reg_wr_xaddr    <= reg_wr_xaddr + 1'b1;
        end

        if (rd_incr_flag) begin
            reg_rd_addr     <= reg_rd_addr + reg_rd_incr;
        end

        if (xrd_incr_flag) begin
            reg_rd_xaddr    <= reg_rd_xaddr + 1'b1;
        end

        // register write
        if (bus_write_strobe) begin
            case (bus_reg_num)
                xv::XM_SYS_CTRL: begin
                    if (!bus_bytesel) begin
`ifdef EN_PIXEL_ADDR
                        pixel_bpp   <=  bus_data_byte[1:0];
                        pixel_base  <=  reg_pixel_x;
                        pixel_width <=  reg_pixel_y;
`endif
                    end else begin
                        regs_wrmask_o       <= bus_data_byte[3:0];
                    end
                end
                xv::XM_INT_CTRL: begin
                    if (!bus_bytesel) begin
                        reconfig_o          <= bus_data_byte[7];
                        intr_mask           <= bus_data_byte[6:0];
                    end else begin
                        intr_clear_o        <= bus_data_byte[6:0];
                    end
                end
                xv::XM_TIMER: begin
`ifdef EN_TIMER_INTR
                    reg_timer_interval      <= bus_data_byte;
`endif
                end
                xv::XM_RD_XADDR: begin
                    if (!bus_bytesel) begin
                        reg_rd_xaddr[15:8]  <= bus_data_byte;
                    end else begin
                        reg_rd_xaddr[7:0]   <= bus_data_byte;
                        regs_xr_sel_o       <= 1'b1;            // select XR
                        xr_rd               <= 1'b1;            // remember pending XR read request
                        regs_addr_o         <= { reg_rd_xaddr[15:8], bus_data_byte };    // output read addr (pre-read)
                    end
                end
                xv::XM_WR_XADDR: begin
                    if (!bus_bytesel) begin
                        reg_wr_xaddr[15:8]  <= bus_data_byte;
                    end else begin
                        reg_wr_xaddr[7:0]   <= bus_data_byte;
                    end
                end
                xv::XM_XDATA: begin
                    if (!bus_bytesel) begin
                        reg_xdata_even      <= bus_data_byte;   // data xr reg even byte storage
                    end else begin
                        regs_xr_sel_o       <= 1'b1;            // select XR
                        regs_wr_o           <= 1'b1;
                        regs_addr_o         <= reg_wr_xaddr;
                        regs_data_o         <= { reg_xdata_even, bus_data_byte };     // output write addr
                    end
                end
                xv::XM_RD_INCR: begin
                    if (!bus_bytesel) begin
                        reg_rd_incr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rd_incr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_RD_ADDR: begin
                    if (!bus_bytesel) begin
                        reg_rd_addr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rd_addr[7:0]    <= bus_data_byte;
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending VRAM read request
                        regs_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                    end
                end
                xv::XM_WR_INCR: begin
                    if (!bus_bytesel) begin
                        reg_wr_incr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_wr_incr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_WR_ADDR: begin
                    if (!bus_bytesel) begin
                        reg_wr_addr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_wr_addr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_DATA,
                xv::XM_DATA_2: begin
                    if (!bus_bytesel) begin
                        reg_data_even       <= bus_data_byte;   // data reg even byte storage
                    end else begin
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        regs_wr_o           <= 1'b1;            // write
                        regs_addr_o         <= reg_wr_addr;    // output write address
                        regs_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                    end
                end
`ifdef EN_PIXEL_ADDR
                xv::XM_PIXEL_X: begin
                        if (!bus_bytesel) begin
                        reg_pixel_x[15:8]   <= bus_data_byte;
                    end else begin
                        reg_pixel_x[7:0]    <= bus_data_byte;
                        pixel_strobe        <= 1'b1;
                    end
                end
                xv::XM_PIXEL_Y: begin
                    if (!bus_bytesel) begin
                        reg_pixel_y[15:8]   <= bus_data_byte;
                    end else begin
                        reg_pixel_y[7:0]    <= bus_data_byte;
                        pixel_strobe        <= 1'b1;
                    end
                end
`endif

`ifdef EN_UART
                xv::XM_UART: begin
                    if (!bus_bytesel) begin
                    end else begin
                        uart_wr     <= 1'b1;
                        uart_din    <= bus_data_byte;
                    end
                end
`endif

                default: begin
                end
            endcase
        end

        // if data read, start next pre-read
        if (bus_read_strobe && bus_bytesel) begin
            // if read from xdata then pre-read next xr rd address
            if (bus_reg_num == xv::XM_XDATA) begin
                regs_addr_o         <= reg_rd_xaddr;    // output read address
                regs_xr_sel_o       <= 1'b1;            // select XR
                xr_rd               <= 1'b1;            // remember pending vram read request
            end
            // if read from data then pre-read next vram rd address
            if (bus_reg_num == xv::XM_DATA || bus_reg_num == xv::XM_DATA_2) begin
                regs_addr_o         <= reg_rd_addr;     // output read address
                regs_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rd             <= 1'b1;            // remember pending vram read request
            end
        end

`ifdef EN_PIXEL_ADDR
        if (pixel_strobe) begin
            reg_wr_addr     <= pixel_addr;
            if (!pixel_bpp[1]) begin
                regs_wrmask_o   <= pixel_xm;
            end
        end
`endif

        // latch low byte of timer when upper byte read
        if (bus_read_strobe && !bus_bytesel) begin
            timer_latch_val <= reg_timer[7:0];
        end
    end
end

`ifdef EN_PIXEL_ADDR
logic           pixel_strobe;       // start computation
word_t          reg_pixel_x;        // x pixel coordinate
word_t          reg_pixel_y;        // y pixel coordinate
word_t          pixel_base;         // base address of bitmap
word_t          pixel_width;        // width of bitmap in words
logic [1:0]     pixel_bpp;          // bpp-4/8
word_t          pixel_xw;           // x word offset
word_t          pixel_mult;         // result of (reg_pixel_y * pixel_width) + pixel_xw
word_t          pixel_addr;         // final result (pixel_base + pixel_mult)
word_t          unused_high;        // unused high bits from multiply
word_t          unused_high2;       // unused high bits from base addition

logic [3:0]     pixel_xm;           // nibble mask within word

assign pixel_xw     = { reg_pixel_x[15], reg_pixel_x[15], reg_pixel_x[15:2] };
always_comb begin
    case (reg_pixel_x[1:0])
        2'b00:  pixel_xm    = { 1'b1, pixel_bpp[0], 1'b0, 1'b0 };
        2'b01:  pixel_xm    = { 1'b0, 1'b1, pixel_bpp[0], 1'b0 };
        2'b10:  pixel_xm    = { 1'b0, 1'b0, 1'b1, pixel_bpp[0] };
        2'b11:  pixel_xm    = { 1'b0, 1'b0, 1'b0, 1'b1 };
    endcase
end

`ifndef ICE40UP5K
// infer multiply for PIXEL_ADDR (vs SB_MAC16)
always_comb begin
    pixel_mult = pixel_xw + (reg_pixel_y * pixel_width);
    pixel_addr = pixel_base + pixel_mult;
end
`else
/* verilator lint_off PINCONNECTEMPTY */
SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext Z15
    .TOPADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext SIGNEXTIN
    .BOTADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b1),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b1)                     // 0=unsigned/1=signed input B
) pixeladdr (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A(reg_pixel_y),                    // 16-bit input A
    .B(pixel_width),                    // 16-bit input B
    .C('0),                             // 16-bit input C
    .D(pixel_xw),                       // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ unused_high, pixel_mult }),    // 32-bit result output (dual 8x8=16-bit mode with top used)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */

/* verilator lint_off PINCONNECTEMPTY */
SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b00),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext Z15
    .TOPADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b00),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext SIGNEXTIN
    .BOTADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b1),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b1)                     // 0=unsigned/1=signed input B
) pixelbase (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A('0),                             // 16-bit input A
    .B(pixel_mult),                     // 16-bit input B
    .C('0),                             // 16-bit input C
    .D(pixel_base),                     // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ unused_high2, pixel_addr }),   // 32-bit result output (dual 8x8=16-bit mode with top used)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */
`endif  // ICE40UP5K

`ifdef WR_ADD_MAC
word_t reg_wr_result;
word_t unused_high3;

/* verilator lint_off PINCONNECTEMPTY */
SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b00),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext Z15
    .TOPADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b00),       // 00=input A, 01=8x8 mult top, 10=16x16 upper 16-bit, 11=sext SIGNEXTIN
    .BOTADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) wraddrincr (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A('0),                             // 16-bit input A
    .B(reg_wr_addr),                    // 16-bit input B
    .C('0),                             // 16-bit input C
    .D(reg_wr_incr),                    // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ unused_high3, reg_wr_result }),// 32-bit result output (dual 8x8=16-bit mode with top used)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */
`endif  // WR_ADD_MAC
`endif  // PIXEL_ADDR

endmodule

`default_nettype wire               // restore default
