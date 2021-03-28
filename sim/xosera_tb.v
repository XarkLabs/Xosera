// xosera_tb.v
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
`default_nettype none    // mandatory for Verilog sanity
`timescale 1ns/1ns

module xosera_tb();

//`include "xosera_defs.vh"  // Xosera global Verilog definitions

logic clk;
logic reset;
// video
logic [3: 0] red, green, blue;
logic vsync, hsync;
logic visible;
// audio
logic audio_l;
logic audio_r;

// bus interface
logic bus_sel_n;
logic bus_rd_nwr;
logic bus_bytesel;
logic [3: 0] bus_reg_num;
logic [7: 0] bus_data_in;
logic [7: 0] bus_data_out;

integer i, f;
integer frame;
integer addrval;
integer dataval;
logic last_vs;

xosera_main xosera(
                .clk(clk),
                .red_o(red),                    // pixel clock
                .green_o(green),                // pixel clock
                .blue_o(blue),                  // pixel clock
                .vsync_o(vsync),                // vertical sync
                .hsync_o(hsync),                // horizontal sync
                .visible_o(visible),            // visible (aka display enable)
                .bus_sel_n_i(bus_sel_n),        // register select strobe
                .bus_rd_nwr_i(bus_rd_nwr),      // 0 = write, 1 = read
                .bus_reg_num_i(bus_reg_num),    // register number (0-15)
                .bus_bytesel_i(bus_bytesel),    // 0 = high-byte, 1 = low-byte
                .bus_data_i(bus_data_in),       // 8-bit data bus input
                .bus_data_o(bus_data_out),      // 8-bit data bus output
                .audio_l_o(audio_l),            // left audio PWM channel
                .audio_r_o(audio_r),            // right audio PWM channel
                .reset_i(reset)                 // reset signal
            );

`include "xosera_clk_defs.vh"       // Xosera global Verilog definitions
`include "xosera_defs.vh"          // Xosera global Verilog definitions

parameter CLK_PERIOD    = (1000000000.0 / PIXEL_FREQ);
parameter M68K_PERIOD   = 83.333;

initial begin
    $timeformat(-9, 0, " ns", 20);
    $dumpfile("logs/xosera_isim.vcd");
    $dumpvars(0, xosera);

    frame = 0;
    dataval = 'hABCD;
    addrval = 'hBEEF;
    clk = 1'b0;

    bus_sel_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    // reset for 5 clocks
    reset = 1'b1;
    #(CLK_PERIOD * 5) reset = 1'b0;
end

always begin

    #(16ms) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b0;
    bus_reg_num = xosera.blitter.R_XVID_WR_ADDR;
    bus_data_in = addrval[15:8];

    #(M68K_PERIOD * 2) bus_sel_n = 1'b0;    // strobe
    #(M68K_PERIOD * 4) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    #(M68K_PERIOD * 4) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b1;
    bus_reg_num = xosera.blitter.R_XVID_WR_ADDR;
    bus_data_in = addrval[7:0];

    #(M68K_PERIOD * 2) bus_sel_n = 1'b0;    // strobe
    #(M68K_PERIOD * 4) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    addrval = addrval + 1;

    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b0;
    bus_reg_num = xosera.blitter.R_XVID_DATA;
    bus_data_in = dataval[15:8];

    #(M68K_PERIOD * 2) bus_sel_n = 1'b0;    // strobe
    #(M68K_PERIOD * 4) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    #(M68K_PERIOD * 4) bus_sel_n = 1'b1;
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b1;
    bus_reg_num = xosera.blitter.R_XVID_DATA;
    bus_data_in = dataval[7:0];

    #(M68K_PERIOD * 2) bus_sel_n = 1'b0;    // strobe
    #(M68K_PERIOD * 4) bus_sel_n = 1'b0;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    dataval = dataval + 1;

end

// toggle clock source at pixel clock frequency
always begin
    #(CLK_PERIOD/2) clk = ~clk;
end

always @(posedge clk) begin
    if (last_vs != vsync && vsync != V_SYNC_POLARITY) begin
        frame <= frame + 1;
        $display("Finished rendering frame #%1d", frame);

        if (frame == 3) begin
            f = $fopen("logs/xosera_isim_vram.txt", "w");
            for (i = 0; i < 4096; i++) begin
                $fwrite(f, "%04x: %04x\n", i, xosera.vram.memory[i][15:0]);
            end
            $fclose(f);
            $finish;
        end
    end

    last_vs <= vsync;
end

always @(posedge clk) begin
    if (xosera.bus_write_strobe) begin
        $display("%0t BUS WRITE: %1x = %04x", $realtime, xosera.bus_reg_num, xosera.bus_data_write);
    end
end

endmodule
