// xosera_tb.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
`default_nettype none    // mandatory for Verilog sanity
`timescale 1ns/1ps

`define MEMDUMP

module xosera_tb();

//`include "xosera_defs.svh"  // Xosera global Verilog definitions

logic clk;
logic reset;
// video
logic [3: 0] red, green, blue;
logic vsync;
logic hsync;
logic dv_de;
// audio
logic audio_l;
logic audio_r;

// bus interface
logic bus_cs_n;
logic bus_rd_nwr;
logic bus_bytesel;
logic [3: 0] bus_reg_num;
logic [7: 0] bus_data_in;
logic [7: 0] bus_data_out;

integer i, j, f;
integer frame;
integer test_addr;
integer test_data;
logic [15:0] readword;
logic last_vs;

xosera_main xosera(
                .clk(clk),
                .red_o(red),                    // pixel clock
                .green_o(green),                // pixel clock
                .blue_o(blue),                  // pixel clock
                .vsync_o(vsync),                // vertical sync
                .hsync_o(hsync),                // horizontal sync
                .dv_de_o(dv_de),                // dv display enable
                .bus_cs_n_i(bus_cs_n),          // chip select strobe
                .bus_rd_nwr_i(bus_rd_nwr),      // 0 = write, 1 = read
                .bus_reg_num_i(bus_reg_num),    // register number (0-15)
                .bus_bytesel_i(bus_bytesel),    // 0 = high-byte, 1 = low-byte
                .bus_data_i(bus_data_in),       // 8-bit data bus input
                .bus_data_o(bus_data_out),      // 8-bit data bus output
                .audio_l_o(audio_l),            // left audio PWM channel
                .audio_r_o(audio_r),            // right audio PWM channel
                .reset_i(reset)                 // reset signal
            );

`include "../rtl/xosera_clk_defs.svh"       // Xosera global Verilog definitions
`include "../rtl/xosera_defs.svh"          // Xosera global Verilog definitions

parameter CLK_PERIOD    = (1000000000.0 / PIXEL_FREQ);
parameter M68K_PERIOD   = 83.333;

initial begin
    $timeformat(-9, 0, " ns", 20);
    $dumpfile("logs/xosera_tb_isim.vcd");
    $dumpvars(0, xosera);
    for (i = 0; i < 8; i = i + 1)
    $dumpvars(0,xosera.blitter.blit_reg[i]);

    frame = 0;
    test_addr = 'hABCD;
    test_data = 'hDA7A;
    clk = 1'b0;

    bus_cs_n = 1'b1;
    bus_rd_nwr = 1'b1;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    // reset for 5 clocks
    reset = 1'b1;
    #(CLK_PERIOD * 5) reset = 1'b0;
end

// function to continuously select read value to put on bus
task write_reg(
    input  logic         b_sel,
    input  logic [3:0]   r_num,
    input  logic [7:0]   data
    );

    bus_cs_n = 1'b1;
    bus_rd_nwr = 1'b0;
    bus_bytesel = b_sel;
    bus_reg_num = r_num;
    bus_data_in = data;

    #(M68K_PERIOD * 2) bus_cs_n = 1'b0;    // strobe
    #(M68K_PERIOD * 4) bus_cs_n = 1'b1;
    // verilator lint_off WIDTH
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b0;
    bus_reg_num = 4'b0;
    bus_data_in = 1'b0;
    // verilator lint_on WIDTH
endtask

task read_reg(
    input  logic         b_sel,
    input  logic [3:0]   r_num,
    output logic [7:0]   data
    );

    bus_cs_n = 1'b1;
    bus_rd_nwr = 1'b1;
    bus_bytesel = b_sel;
    bus_reg_num = r_num;

    #(M68K_PERIOD * 2) bus_cs_n = 1'b0;    // strobe
    #40 data = xosera.bus_data_o;
    #(M68K_PERIOD * 4) bus_cs_n = 1'b1;
    // verilator lint_off WIDTH
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b0;
    bus_reg_num = 4'b0;
    bus_data_in = 1'b0;
    // verilator lint_on WIDTH
endtask


always begin

    #(15ms) ;
    #(M68K_PERIOD * 4)  write_reg(1'b0, xosera.blitter.XVID_WR_ADDR, test_addr[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, xosera.blitter.XVID_WR_ADDR, test_addr[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, xosera.blitter.XVID_DATA, test_data[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, xosera.blitter.XVID_DATA, test_data[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, xosera.blitter.XVID_RD_ADDR, test_addr[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, xosera.blitter.XVID_RD_ADDR, test_addr[7:0]);

    #(M68K_PERIOD * 4)  read_reg(1'b0, xosera.blitter.XVID_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, xosera.blitter.XVID_DATA, readword[7:0]);
    $display("%0t READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);
end

always @(posedge clk) begin
    if (xosera.vram.sel && xosera.vram.wr_en) begin
        $display("%0t Write VRAM[%04x] <= %04x", $realtime, xosera.vram.address_in, xosera.vram.data_in);
    end
    else if (xosera.vram.sel && xosera.blit_vram_cycle) begin
        $display("%0t Read VRAM[%04x] => %04x", $realtime, xosera.vram.address_in, xosera.vram.data_in);
    end
end

// toggle clock source at pixel clock frequency+
always begin
    #(CLK_PERIOD/2) clk = ~clk;
end

always @(posedge clk) begin
    if (last_vs != vsync && vsync != V_SYNC_POLARITY) begin
        frame <= frame + 1;
        $display("Finished rendering frame #%1d", frame);

        if (frame == 3) begin
`ifdef MEMDUMP
            f = $fopen("logs/xosera_tb_isim_vram.txt", "w");
            for (i = 0; i < 65536; i += 16) begin
                $fwrite(f, "%04x: ", i[15:0]);
                for (j = 0; j < 16; j++) begin
                    $fwrite(f, "%04x ", xosera.vram.memory[i+j][15:0]);
                end
                $fwrite(f, "  ");
                for (j = 0; j < 16; j++) begin
                    if (xosera.vram.memory[i+j][7:0] >= 32 && xosera.vram.memory[i+j][7:0] < 127) begin
                        $fwrite(f, "%c", xosera.vram.memory[i+j][7:0]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                end
                $fwrite(f, "\n");
            end
            $fclose(f);
`endif
            $finish;
        end
    end

    last_vs <= vsync;
end

always @(posedge clk) begin
    if (xosera.blitter.bus_write_strobe) begin
        if (xosera.blitter.bus_bytesel) begin
            $display("%0t BUS WRITE:  R[%1x] <= __%02x", $realtime, xosera.blitter.bus_reg_num, xosera.blitter.bus_bytedata);
        end
        else begin
            $display("%0t BUS WRITE:  R[%1x] <= %02x__", $realtime, xosera.blitter.bus_reg_num, xosera.blitter.bus_bytedata);
        end
    end
    if (xosera.blitter.bus_read_strobe) begin
        if (xosera.bus_bytesel_i) begin
            $display("%0t BUS READ:  R[%1x] => __%02x", $realtime, xosera.blitter.bus_reg_num, xosera.blitter.bus_bytedata);
        end
        else begin
            $display("%0t BUS READ:  R[%1x] => %02x__", $realtime, xosera.blitter.bus_reg_num, xosera.blitter.bus_bytedata);
        end
    end
end

endmodule
