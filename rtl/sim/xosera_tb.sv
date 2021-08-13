// xosera_tb.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
//`default_nettype none    // mandatory for Verilog sanity
//`timescale 1ns/1ps

`include "xosera_pkg.sv"

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`define MEMDUMP                     // dump VRAM contents to file
`define BUSTEST
`define MAX_FRAMES      4
`define LOAD_MONOBM

module xosera_tb();

import xv::*;

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

logic reconfig;
logic [1:0] boot_select;

// bus interface
logic vblank;
logic bus_cs_n;
logic bus_rd_nwr;
logic bus_bytesel;
logic [3: 0] bus_reg_num;
logic [7: 0] bus_data_in;
logic [7: 0] bus_data_out;

integer i, j, f;
integer frame;
logic [15:0] test_addr;
logic [15:0] test_inc;
logic [15:0] test_addr2;
logic [15:0] test_data0;
logic [15:0] test_data1;
logic [15:0] test_data2;
logic [15:0] test_data3;
logic [15:0] readword;

xosera_main xosera(
                .clk(clk),
                .red_o(red),                    // pixel clock
                .green_o(green),                // pixel clock
                .blue_o(blue),                  // pixel clock
                .vblank_o(vblank),              // Vertical blank indicator
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
                .reconfig_o(reconfig),          // reconfigure FPGA
                .boot_select_o(boot_select),    // reconfigure selection
                .reset_i(reset)                 // reset signal
            );

parameter CLK_PERIOD    = (1000000000.0 / PIXEL_FREQ);
parameter M68K_PERIOD   = 80;

initial begin
    $timeformat(-9, 0, " ns", 20);
    $dumpfile("sim/logs/xosera_tb_isim.fst");
    $dumpvars(0, xosera);

    frame = 0;
    test_addr = 'hABCD;
    test_inc = 'h0001;
    test_addr2 = 'h1234;
    test_data0 = 'hD070;
    test_data1 = 'hD171;
    test_data2 = 'hD272;
    test_data3 = 'hD373;
    clk = 1'b0;

    bus_cs_n = 1'b1;
    bus_rd_nwr = 1'b1;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'hX;
    bus_data_in = 8'hXX;

    // reset for 5 clocks
    reset = 1'b1;
    #(CLK_PERIOD * 2) reset = 1'b0;
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

    # 10ns bus_cs_n = 1'b0;    // CS strobe
    #(M68K_PERIOD * 2) bus_cs_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'bX;
    bus_data_in = 8'bX;
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

    # 10ns bus_cs_n = 1'b0;    // strobe
    #(M68K_PERIOD) data = xosera.bus_data_o;
    #(M68K_PERIOD) bus_cs_n = 1'b1;
    bus_rd_nwr = 1'bX;
    bus_bytesel = 1'bX;
    bus_reg_num = 4'bX;
    bus_data_in = 8'bX;
endtask

task xvid_setw(
    input  logic [3:0]   r_num,
    input  logic [15:0]   data
    );

    write_reg(1'b0, r_num, data[15:8]);
    #(M68K_PERIOD * 2);
    write_reg(1'b1, r_num, data[7:0]);
    #(M68K_PERIOD * 2);

endtask

// function to continuously select read value to put on bus
task inject_file(
    string filename,
    logic [3:0] r_num
    );
    integer fd;
    integer r;
    logic [7:0] tempbyte;

    fd = $fopen(filename, "rb");

    while (!$feof(fd)) begin
        r = $fread(tempbyte, fd);
        #(M68K_PERIOD * 2)  write_reg(1'b0, r_num, tempbyte);
        r = $fread(tempbyte, fd);
        #(M68K_PERIOD * 2)  write_reg(1'b1, r_num, tempbyte);
    end

    $fclose(fd);
endtask

function logic [63:0] regname(
        input logic [3:0] num
    );
    begin
        case (num)
            4'h0: regname = "AUX_ADDR";
            4'h1: regname = "CONST   ";
            4'h2: regname = "RD_ADDR ";
            4'h3: regname = "WR_ADDR ";
            4'h4: regname = "DATA    ";
            4'h5: regname = "DATA_2  ";
            4'h6: regname = "AUX_DATA";
            4'h7: regname = "COUNT   ";
            4'h8: regname = "RD_INC  ";
            4'h9: regname = "WR_INC  ";
            4'hA: regname = "WR_MOD  ";
            4'hB: regname = "RD_MOD  ";
            4'hC: regname = "WIDTH   ";
            4'hD: regname = "BLITCTRL";
            4'hE: regname = "UNUSED_E";
            4'hF: regname = "UNUSED_F";
            default: regname = "????????";
        endcase
    end
endfunction

always @* begin
    if (reconfig) begin
        $display("XOSERA REBOOT: To flash config #0x%x", boot_select);
        $finish;
    end
end

`ifdef BUSTEST
/* verilator lint_off LATCH */
always begin
    bus_cs_n = 1'b1;
    bus_rd_nwr = 1'b0;
    bus_bytesel = 1'b0;
    bus_reg_num = 4'b0;
    bus_data_in = 8'b0;

    if (xosera.blitter.blit_state == xosera.blitter.IDLE) begin

`ifdef LOAD_MONOBM
        while (xosera.video_gen.v_last_frame_pixel != 1'b1) begin
            # 1ns;
        end

        #(M68K_PERIOD * 2)  xvid_setw(XVID_WR_INC, test_inc);
        #(M68K_PERIOD * 2)  xvid_setw(XVID_WR_ADDR, 16'h0000);
        #(M68K_PERIOD * 2)  xvid_setw(XVID_AUX_ADDR, AUX_GFXCTRL);
        #(M68K_PERIOD * 2)  xvid_setw(XVID_AUX_DATA, 16'h8000);

        inject_file("sim/mountains_mono_640x480w.raw", XVID_DATA);  // pump binary file into DATA

        # 1000ms;

`endif
`ifdef ZZZUNDEF // read test
        # 10ms;
        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_WR_ADDR, test_addr[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_WR_ADDR, test_addr[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_WR_INC, test_inc[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_WR_INC, test_inc[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_DATA, test_data0[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_DATA, test_data0[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_DATA, test_data1[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_DATA, test_data1[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_DATA, test_data2[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_DATA, test_data2[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_RD_INC, test_inc[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_RD_INC, test_inc[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_RD_ADDR, test_addr[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_RD_ADDR, test_addr[7:0]);

        #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_DATA, readword[15:8]);
        #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_DATA, readword[7:0]);
        $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

        #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_DATA, readword[15:8]);
        #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_DATA, readword[7:0]);
        $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

        #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_DATA, readword[15:8]);
        #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_DATA, readword[7:0]);
        $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_WR_ADDR, test_addr2[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_WR_ADDR, test_addr2[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_DATA, test_data2[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_DATA, test_data2[7:0]);

        #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_RD_ADDR, test_addr2[15:8]);
        #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_RD_ADDR, test_addr2[7:0]);

        #(M68K_PERIOD * 4);

        #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_DATA, readword[15:8]);
        #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_DATA, readword[7:0]);
        $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

`endif

`ifdef ZZZUNDEF // some other test...

    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h00);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h01);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h02);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

    #(1ms) ;
    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

    #(1500us) ;
    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_AUX_ADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_AUX_ADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XVID_AUX_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XVID_AUX_DATA, readword[7:0]);
    $display("%0t REG READ R[%x] => %04x", $realtime, xosera.blitter.bus_reg_num, readword);

`endif

    end
    else begin
        #(M68K_PERIOD * 4);
    end
end
/* verilator lint_on LATCH */
`endif

integer flag = 0;
logic [15:0] last_rd_addr = 0;
always @(negedge clk) begin
    if (xosera.blitter.blit_state == xosera.blitter.IDLE) begin
        if (xosera.blitter.blit_vram_sel_o) begin
            if (xosera.blitter.blit_wr_o) begin
                $display("%0t Write VRAM[%04x] <= %04x", $realtime, xosera.vram.address_in, xosera.vram.data_in);
            end
            else begin
                flag <= 1;
                last_rd_addr <= xosera.vram.address_in;
            end
        end
        else if (flag == 1) begin
            $display("%0t Read VRAM[%04x] => %04x", $realtime, last_rd_addr, xosera.vram.data_out);
            flag <= 0;
        end
    end
end

// toggle clock source at pixel clock frequency+
always begin
    #(CLK_PERIOD/2) clk = ~clk;
end

always @(posedge clk) begin
    if (xosera.video_gen.v_last_frame_pixel == 1'b1) begin
        frame <= frame + 1;
        $display("Finished rendering frame #%1d", frame);

        if (frame == `MAX_FRAMES) begin
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
end

// NOTE: Horrible hacky Verilog string array to print register name (fixed 8 characters, and in reverse order).
always @(posedge clk) begin
    if (xosera.blitter.bus_write_strobe) begin
        if (xosera.blitter.bus_bytesel) begin
            $display("%0t BUS WRITE:  R[%1x:%s] <= __%02x", $realtime, xosera.blitter.bus_reg_num, regname(xosera.blitter.bus_reg_num), xosera.blitter.bus_data_byte);
        end
        else begin
            $display("%0t BUS WRITE:  R[%1x:%s] <= %02x__", $realtime, xosera.blitter.bus_reg_num, regname(xosera.blitter.bus_reg_num),xosera.blitter.bus_data_byte);
        end
    end
    if (xosera.blitter.bus_read_strobe) begin
        if (xosera.bus_bytesel_i) begin
            $display("%0t BUS READ:  R[%1x:%s] => __%02x", $realtime, xosera.blitter.bus_reg_num, regname(xosera.blitter.bus_reg_num),xosera.blitter.bus_data_o);
        end
        else begin
            $display("%0t BUS READ:  R[%1x:%s] => %02x__", $realtime, xosera.blitter.bus_reg_num, regname(xosera.blitter.bus_reg_num), xosera.blitter.bus_data_o);
        end
    end
end

endmodule

`default_nettype wire               // restore default
