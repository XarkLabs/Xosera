// xosera_tb.sv
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

`define MEMDUMP                     // dump VRAM contents to file
`define COPMEMDUMP                  // dump copper program memory contents to file
`define AUDIOMEMDUMP                // dump audio parameter memory contents to file
`define BUSTEST
`define MAX_FRAMES      1
//`define LOAD_MONOBM

module xosera_tb();

import xv::*;

/* verilator lint_off UNUSED */

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

logic rxd, txd;
assign rxd = 1'b1;

logic reconfig;
logic [1:0] boot_select;

// bus interface
logic           bus_intr;
logic           bus_cs_n;
logic           bus_rd_nwr;
logic           bus_bytesel;
logic [3: 0]    bus_reg_num;
logic [7: 0]    bus_data_in;
logic [7: 0]    bus_data_out;

// tb vars
integer     i, j, f;
integer     frame;

addr_t      test_addr;
word_t      test_inc;
addr_t      test_addr2;
word_t      test_data0;
word_t      test_data1;
word_t      test_data2;
word_t      test_data3;

/* verilator lint_on UNUSED */

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
                .bus_intr_o(bus_intr),          // interrupt signal
                .audio_l_o(audio_l),            // left audio PWM channel
                .audio_r_o(audio_r),            // right audio PWM channel
                .serial_txd_o(txd),             // UART transmit
                .serial_rxd_i(rxd),             // UART receive
                .reconfig_o(reconfig),          // reconfigure FPGA
                .boot_select_o(boot_select),    // reconfigure selection
                .reset_i(reset)                 // reset signal
            );

parameter CLK_PERIOD    = (1000000000.0 / PIXEL_FREQ);
parameter M68K_PERIOD   = 80;

/* verilator lint_off UNUSED */
word_t       readword;
/* verilator lint_on UNUSED */

integer logfile;

initial begin
    $timeformat(-9, 0, " ns", 20);
    logfile = $fopen("sim/logs/xosera_tb_isim.log");
    $dumpfile("sim/logs/xosera_tb_isim.fst");
    $dumpvars(0, xosera);

    $display("Xosera - Verilog testbench started");
    // $monitor ("time=%09t clk=%x v =%4d == v_m =%4d v_st :%d EOL :%x EOF :%x\ntime=%09t clk=%x vn=%4d == v_mn=%4d v_stn:%d EOLn:%x EOFn:%x\n",
    //     $realtime, clk,
    //     xosera.video_gen.v_count, xosera.video_gen.v_count_next_value, xosera.video_gen.v_state, xosera.video_gen.end_of_line, xosera.video_gen.end_of_frame,
    //     $realtime, clk,
    //     xosera.video_gen.v_count_next, xosera.video_gen.v_count_next_value, xosera.video_gen.v_state_next, xosera.video_gen.end_of_line_next, xosera.video_gen.end_of_frame_next);

    frame = 1;
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

    bus_cs_n    <= 1'b1;
    bus_rd_nwr  <= 1'b0;
    bus_bytesel <= b_sel;
    bus_reg_num <= r_num;
    bus_data_in <= data;

    # 10ns bus_cs_n             <= 1'b0;    // CS strobe
    #(M68K_PERIOD * 2) bus_cs_n <= 1'b1;
    bus_rd_nwr                  <= 1'bX;
    bus_bytesel                 <= 1'bX;
    bus_reg_num                 <= 4'bX;
    bus_data_in                 <= 8'bX;
endtask

task read_reg(
    input  logic         b_sel,
    input  logic [3:0]   r_num,
    output logic [7:0]   data
    );

    bus_cs_n <= 1'b1;
    bus_rd_nwr <= 1'b1;
    bus_bytesel <= b_sel;
    bus_reg_num <= r_num;

    # 10ns bus_cs_n <= 1'b0;    // strobe
    #(M68K_PERIOD) data <= xosera.bus_data_o;
    #(M68K_PERIOD) bus_cs_n <= 1'b1;
    bus_rd_nwr <= 1'bX;
    bus_bytesel <= 1'bX;
    bus_reg_num <= 4'bX;
    bus_data_in <= 8'bX;
endtask

task xvid_setw(
    input  logic [3:0]   r_num,
    input  word_t         data
    );

    write_reg(1'b0, r_num, data[15:8]);
    #(M68K_PERIOD * 2);
    write_reg(1'b1, r_num, data[7:0]);
    #(M68K_PERIOD * 2);

endtask

// function to continuously select read value to put on bus
/* verilator lint_off BLKSEQ */
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
        if (r == 1) begin
            #(M68K_PERIOD * 2)  write_reg(1'b0, r_num, tempbyte);
        end
        r = $fread(tempbyte, fd);
        if (r == 1) begin
            #(M68K_PERIOD * 2)  write_reg(1'b1, r_num, tempbyte);
        end
    end

    $fclose(fd);
endtask
/* verilator lint_off BLKSEQ */

function automatic logic [63:0] regname(
        input logic [3:0] num
    );
    begin
        case (num)
            4'h0: regname = "SYS_CTRL";
            4'h1: regname = "INT_CTRL";
            4'h2: regname = "TIMER   ";
            4'h3: regname = "RD_XADDR";
            4'h4: regname = "WR_XADDR";
            4'h5: regname = "XDATA   ";
            4'h6: regname = "RD_INCR ";
            4'h7: regname = "RD_ADDR ";
            4'h8: regname = "WR_INCR ";
            4'h9: regname = "WR_ADDR ";
            4'hA: regname = "DATA    ";
            4'hB: regname = "DATA_2  ";
            4'hD: regname = "PIXEL_X ";
            4'hE: regname = "PIXEL_Y ";
            4'hC: regname = "UART    ";
            4'hF: regname = "FEATURE ";
            default: regname = "????????";
        endcase
    end
endfunction

always @* begin
    if (reconfig) begin
        $fdisplay(logfile, "%0t XOSERA REBOOT: To flash config #0x%x", $realtime, boot_select);
        $finish;
    end
end

always @(negedge clk) begin
    if (bus_intr) begin
        $fdisplay(logfile, "%0t XOSERA INTERRUPT signal active", $realtime);
    end
end

`ifdef BUSTEST
/* verilator lint_off LATCH */
always begin
    bus_cs_n <= 1'b1;
    bus_rd_nwr <= 1'b0;
    bus_bytesel <= 1'b0;
    bus_reg_num <= 4'b0;
    bus_data_in <= 8'b0;

    # 8ms;

// audio test

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_INCR, 16'h0001);
    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_ADDR, 16'h0100);

    inject_file("../testdata/raw/ramptable.raw", XM_DATA);

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_AUD0_VOL));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h8080);

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_AUD0_PERIOD));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h1000);

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_AUD0_LENGTH));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h00FF);

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_AUD0_START));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h0100);

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_AUD_CTRL));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h0001);

// end audio test

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_PA_GFX_CTRL));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h0040);

`ifdef LOAD_MONOBM
    while (xosera.video_gen.end_of_frame != 1'b1) begin
        # 1ns;
    end

    # 15ms;

    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_INCR, test_inc);
    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_ADDR, 16'h0000);
    #(M68K_PERIOD * 2)  xvid_setw(XM_WR_XADDR, 16'(XR_PA_GFX_CTRL));
    #(M68K_PERIOD * 2)  xvid_setw(XM_XDATA, 16'h0040);

    inject_file("../testdata/raw/space_shuttle_color_small.raw", XM_DATA);  // pump binary file into DATA

    # 1000ms;

`endif
`ifdef ZZZUNDEF // read test
    # 10ms;
    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_WR_ADDR, test_addr[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_WR_ADDR, test_addr[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_WR_INCR, test_inc[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_WR_INCR, test_inc[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_DATA, test_data0[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_DATA, test_data0[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_DATA, test_data1[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_DATA, test_data1[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_DATA, test_data2[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_DATA, test_data2[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XVID_RD_INC, test_inc[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XVID_RD_INC, test_inc[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_ADDR, test_addr[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_ADDR, test_addr[7:0]);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_DATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_DATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_DATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_WR_ADDR, test_addr2[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_WR_ADDR, test_addr2[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_DATA, test_data2[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_DATA, test_data2[7:0]);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_ADDR, test_addr2[15:8]);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_ADDR, test_addr2[7:0]);

    #(M68K_PERIOD * 4);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_DATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_DATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

`endif

`ifdef ZZZUNDEF // some other test...

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h00);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h01);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h02);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(1ms) ;
    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

    #(1500us) ;
    #(M68K_PERIOD * 4)  write_reg(1'b0, XM_RD_XADDR, 8'h00);
    #(M68K_PERIOD * 4)  write_reg(1'b1, XM_RD_XADDR, 8'h03);

    #(M68K_PERIOD * 4)  read_reg(1'b0, XM_XDATA, readword[15:8]);
    #(M68K_PERIOD * 4)  read_reg(1'b1, XM_XDATA, readword[7:0]);
    $fdisplay(logfile, "%0t REG READ R[%x] => %04x", $realtime, xosera.reg_interface.bus_reg_num, readword);

`endif
end
/* verilator lint_on LATCH */
`endif

integer flag = 0;
addr_t       last_rd_addr = 0;
always @(negedge clk) begin
    if (xosera.reg_interface.regs_vram_sel_o) begin
        if (xosera.reg_interface.regs_wr_o) begin
            if (xosera.reg_interface.bus_bytesel) begin
                $fdisplay(logfile, "%0t Write VRAM[%04x] <= %04x", $realtime, xosera.vram_arb.vram.address_in, xosera.vram_arb.vram.data_in);
            end
        end
        else begin
            flag <= 1;
            last_rd_addr <= xosera.vram_arb.vram.address_in;
        end
    end
    else if (flag == 1) begin
        $fdisplay(logfile, "%0t Read VRAM[%04x] => %04x", $realtime, last_rd_addr, xosera.vram_arb.vram.data_out);
        flag <= 0;
    end
end

integer cop_cyc = 0;
always @(negedge clk) begin
    if (xosera.copper.cop_en && cop_cyc < 9999) begin
        $fdisplay(logfile, "%0t %04d: ST=%x %2b IR=%04x M=%04x PC=%03x RS=%03x",
            $realtime, cop_cyc, xosera.copper.cop_ex_state, xosera.copper.rd_pipeline, xosera.copper.cop_IR, xosera.copper.copmem_rd_data_i,
            xosera.copper.cop_PC, xosera.copper.cop_RA);
        $fdisplay(logfile, "%0t     : mem_rd=%x mem_addr=%03x xr_wr=%x reg_wr=%x xr_addr=%x xr_data=%x",
            $realtime, xosera.copper.ram_rd_en, xosera.copper.ram_rd_addr,
            xosera.copper.xr_wr_en, xosera.copper.reg_wr_en, xosera.copper.write_addr, xosera.copper.write_data);
        cop_cyc <= cop_cyc + 1'b1;
    end
end

// toggle clock source at pixel clock frequency+
always begin
    #(CLK_PERIOD/2) clk <= ~clk;
end

always @(posedge clk) begin
    if (xosera.video_gen.end_of_frame == 1'b1) begin
        frame <= frame + 1;
        $fdisplay(logfile, "%0t Finished rendering frame #%1d", $realtime, frame);
        $display("%0t Finished rendering frame #%1d", $realtime, frame);

        if (frame > `MAX_FRAMES || $realtime > (`MAX_FRAMES * 18_000_000)) begin
`ifdef MEMDUMP
            f = $fopen("sim/logs/xosera_tb_isim_vram.txt", "w");
            for (i = 0; i < 65536; i += 16) begin
                $fwrite(f, "%04x: ", i[15:0]);
                for (j = 0; j < 16; j++) begin
                    $fwrite(f, "%04x ", xosera.vram_arb.vram.memory[i+j][15:0]);
                end
                $fwrite(f, "  ");
                for (j = 0; j < 16; j++) begin
                    if (xosera.vram_arb.vram.memory[i+j][7:0] >= 32 && xosera.vram_arb.vram.memory[i+j][7:0] < 127) begin
                        $fwrite(f, "%c", xosera.vram_arb.vram.memory[i+j][7:0]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                end
                $fwrite(f, "\n");
            end
            $fclose(f);
`endif
`ifdef COPMEMDUMP
            f = $fopen("sim/logs/xosera_tb_isim_copp.txt", "w");
            for (i = 0; i < 'h400; i += 16) begin
                $fwrite(f, "%04x: ", i[15:0]);
                for (j = 0; j < 16; j++) begin
                    $fwrite(f, "%04x ", xosera.xrmem_arb.coppermem.bram[i+j][15:0]);
                end
                $fwrite(f, "  ");
                for (j = 0; j < 16; j++) begin
                    if (xosera.xrmem_arb.coppermem.bram[i+j][15:8] >= 32 && xosera.xrmem_arb.coppermem.bram[i+j][15:8] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem.bram[i+j][15:8]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                    if (xosera.xrmem_arb.coppermem.bram[i+j][7:0] >= 32 && xosera.xrmem_arb.coppermem.bram[i+j][7:0] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem.bram[i+j][7:0]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                end
                $fwrite(f, "\n");
            end

            for (i = 0; i < 'h200; i += 16) begin
                $fwrite(f, "%04x: ", i[15:0]);
                for (j = 0; j < 16; j++) begin
                    $fwrite(f, "%04x ", xosera.xrmem_arb.coppermem_2.bram[i+j][15:0]);
                end
                $fwrite(f, "  ");
                for (j = 0; j < 16; j++) begin
                    if (xosera.xrmem_arb.coppermem_2.bram[i+j][15:8] >= 32 && xosera.xrmem_arb.coppermem_2.bram[i+j][15:8] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem_2.bram[i+j][15:8]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                    if (xosera.xrmem_arb.coppermem_2.bram[i+j][7:0] >= 32 && xosera.xrmem_arb.coppermem_2.bram[i+j][7:0] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem_2.bram[i+j][7:0]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                end
                $fwrite(f, "\n");
            end

            $fclose(f);
`endif
`ifdef EN_AUDIO
`ifdef AUDIOMEMDUMP
            f = $fopen("sim/logs/xosera_tb_isim_audiomem.txt", "w");
            for (i = 0; i < 2**xv::AUDIO_W; i += 16) begin
                $fwrite(f, "%04x: ", i[15:0]);
                for (j = 0; j < 16; j++) begin
                    $fwrite(f, "%04x ", xosera.video_gen.audio_mixer.audio_mem.bram[i+j][15:0]);
                end
                $fwrite(f, "  ");
                for (j = 0; j < 16; j++) begin
                    if (xosera.video_gen.audio_mixer.audio_mem.bram[i+j][15:8] >= 32 && xosera.video_gen.audio_mixer.audio_mem.bram[i+j][15:8] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem.bram[i+j][15:8]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                    if (xosera.video_gen.audio_mixer.audio_mem.bram[i+j][7:0] >= 32 && xosera.video_gen.audio_mixer.audio_mem.bram[i+j][7:0] < 127) begin
                        $fwrite(f, "%c", xosera.xrmem_arb.coppermem.bram[i+j][7:0]);
                    end else
                    begin
                        $fwrite(f, ".");
                    end
                end
                $fwrite(f, "\n");
            end
            $fclose(f);
`endif
`endif
            $finish;
        end
    end
end

always @(posedge clk) begin
    if (xosera.reg_interface.bus_write_strobe) begin
        if (xosera.reg_interface.bus_bytesel) begin
            $fdisplay(logfile, "%0t BUS WRITE:  R[%1x:%s] <= __%02x", $realtime, xosera.reg_interface.bus_reg_num, regname(xosera.reg_interface.bus_reg_num), xosera.reg_interface.bus_data_byte);
        end
        else begin
            $fdisplay(logfile, "%0t BUS WRITE:  R[%1x:%s] <= %02x__", $realtime, xosera.reg_interface.bus_reg_num, regname(xosera.reg_interface.bus_reg_num),xosera.reg_interface.bus_data_byte);
        end
    end
    if (xosera.reg_interface.bus_read_strobe) begin
        if (xosera.bus_bytesel_i) begin
            $fdisplay(logfile, "%0t BUS READ:  R[%1x:%s] => __%02x", $realtime, xosera.reg_interface.bus_reg_num, regname(xosera.reg_interface.bus_reg_num),xosera.reg_interface.bus_data_o);
        end
        else begin
            $fdisplay(logfile, "%0t BUS READ:  R[%1x:%s] => %02x__", $realtime, xosera.reg_interface.bus_reg_num, regname(xosera.reg_interface.bus_reg_num), xosera.reg_interface.bus_data_o);
        end
    end
end

endmodule

`default_nettype wire               // restore default
