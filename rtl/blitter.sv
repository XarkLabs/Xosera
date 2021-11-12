// blitter.sv
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

module blitter(
/* verilator lint_off UNUSED */
    // video registers and control
    input  wire logic            blit_reg_wr_en_i,   // strobe to write internal config register number
    input  wire logic  [3:0]     blit_reg_num_i,     // internal config register number (for reads)
    input  wire logic [15:0]     blit_reg_data_i,    // data for internal config register
    output      logic [15:0]     blit_reg_data_o,    // register/status data reads
    // video memories
    output      logic            blit_vram_sel_o,    // vram select
    output      logic            blit_xr_sel_o,      // XR select
    output      logic            blit_wr_o,          // blit write
    output      logic [15:0]     blit_addr_o,        // address out (vram/XR)
    input  wire logic [15:0]     blit_data_i,        // data word data in
    output      logic [15:0]     blit_data_o,        // data word data out
    output      logic            busy_o,
    // standard signals
    input  wire logic            reset_i,            // system reset in
    input  wire logic            clk                 // clock (video pixel clock)
/* verilator lint_on UNUSED */
);

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin


    end else begin
        // video register write
        if (blit_reg_wr_en_i) begin
            case ({ xv::XR_BLIT_REGS[6:5], blit_reg_num_i})
                xv::XR_BLIT_MODE: begin
                end
                xv::XR_BLIT_RD_MOD: begin
                end
                xv::XR_BLIT_WR_MOD: begin
                end
                xv::XR_BLIT_WR_MASK: begin
                end
                xv::XR_BLIT_WIDTH: begin
                end
                xv::XR_BLIT_RD_ADDR: begin
                end
                xv::XR_BLIT_WR_ADDR: begin
                end
                xv::XR_PA_GFX_CTRL: begin
                end
                xv::XR_BLIT_COUNT: begin
                end
                default: begin
                end
            endcase
        end
    end
end

// video registers read
always_ff @(posedge clk) begin
    case ({ xv::XR_BLIT_REGS[6:5], blit_reg_num_i })
        xv::XR_BLIT_MODE:           blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_RD_MOD:         blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_WR_MOD:         blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_WR_MASK:        blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_WIDTH:          blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_RD_ADDR:        blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_WR_ADDR:        blit_reg_data_o <= 16'h0000;
        xv::XR_PA_GFX_CTRL:         blit_reg_data_o <= 16'h0000;
        xv::XR_BLIT_COUNT:          blit_reg_data_o <= 16'h0000;
        default:                    blit_reg_data_o <= 16'h0000;
    endcase
end

endmodule
`default_nettype wire               // restore default
