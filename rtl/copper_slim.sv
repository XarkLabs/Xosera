// copper.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Ross Bamford - https://github.com/roscopeco
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`define EN_COPP         // TEMP: remove
`define EN_COPP_SLIM    // TEMP: remove

`ifdef EN_COPP
`ifdef EN_COPP_SLIM

`define SUB_ONLY        // only SUB, no load
//`define BLT_ONLY        // only BLT, no BGE
`define SET_NO_STALL    // eliminate bubble in pipeline after SET/SETM

`include "xosera_pkg.sv"

module slim_copper(
    output       logic          xr_wr_en_o,             // for all XR writes
    input   wire logic          xr_wr_ack_i,            // for all XR writes
    output       addr_t         xr_wr_addr_o,           // for all XR writes
    output       word_t         xr_wr_data_o,           // for all XR writes
    output       copp_addr_t    copmem_rd_addr_o,
    output       logic          copmem_rd_en_o,
    input   wire logic [15:0]   copmem_rd_data_i,
    input   wire logic          cop_xreg_wr_i,          // strobe to write internal config register
    input   wire word_t         cop_xreg_data_i,        // data for internal config register
    input   wire hres_t         h_count_i,
    input   wire vres_t         v_count_i,
    input   wire logic          reset_i,
    input   wire logic          clk
    );

//  Slim Copper opcodes:
//
// | XR Op Immediate     | Assembly              | Cyc | Description                      |
// |---------------------|-----------------------|-----|----------------------------------|
// | rr00 oooo oooo oooo | SET   xadr14,#im16    |  4+ | [xadr14] <= im16                 |
// | iiii iiii iiii iiii |                       |     |   (2 word op)                    |
// | --01 r-cc cccc cccc | SETM  xadr14,xcadr10  |  4+ | [xadr14] <= xcadr;               |
// | rroo oooo oooo oooo |                       |     |   (2 word op)                    |
// | --10 0-ii iiii iiii | HPOS  #im10           |  2+ | wait until video HPOS >= im10    |
// | --10 1-ii iiii iiii | VPOS  #im10           |  2+ | wait until video VPOS >= im10    |
// | --11 0-cc cccc cccc | BGE   cadr10          | 2/4 | if (B==0) PC <= cadr10           |
// | --11 1-cc cccc cccc | BLT   cadr10          | 2/4 | if (B==1) PC <= cadr10           |
// |---------------------|-----------------------|-----|----------------------------------|
//
// im16     =   16-bit immediate word (2nd word in SET xxx,#im16 instructions)
// xadr14   =   XR region + 12-bit offset:          xx00 oooo oooo oooo (XADDR with 12-bit offset)
// im10     =   10-bit immediate value:             0000 00ii iiii iiii
// cadr10   =   10-bit copper address:              0000 00nn nnnn nnnn
// B        =   borrow (less-than) flag from SUB/CMP (set if RA < value [unsigned])
//
// Special internal pseudo XR registers (XR reg with bit 8 set), only available to copper:
//
// | Pseudo Xreg Name | XR addr | Operation             | Description                               |
// |------------------|---------|-----------------------|-------------------------------------------|
// | COP_RA_SUB       | 0x0801  | B, RA <= RA - val16   | set RA to RA - val16, B set if RA < val16 |
// |------------------|---------|-----------------------|-------------------------------------------|
//
// Notable pseudo instructions:
//  LD   #val16                     RA = val16;         (2 words)
//  > SET     COP_RA,#val16
//
//  ADDI #val16                     RA = RA + val16;    (2 words, B = inverted carry)
//  > SET     COP_RA_SUB,#-val16
//
//  SUBI #val16                     RA = RA - val16;    (2 words, B = RA < val16 [unsigned])
//  > SET     COP_RA_SUB,#val16
//
//  CMPI #val16                     RA < val16;         (2 words, B = RA < val16 [unsigned])
//  > SET     COP_RA_CMP,#val16
//
//  STPI (RA),#val16               [RA] = val16; RA++; (3 words, self-mod)
//  > STR     *+1,RA
//  > SET     dummy,#val16
//
// Example copper code: (TODO)
//
//  REPEAT: =       10-1
//  endframe:
//          SET     COP_VPOS,#0                 ; wait until vertical line 0 (starts at line > 479)
//          SET     XR_PA_GFX_CTRL,#0x0065      ; set initial PA_GFX_MODE
//          SET     COP_VPOS,#100               ; wait until vertical line 100
//          SET     XR_PA_GFX_CTRL,#0x0055      ; change PA_GFX_MODE
//          LDI     RA,#120                     ; load RA with starting scan line
//          ST      RA,vline                    ; store RA to copper mem variable
//          LDI     RA,#REPEAT                  ; load color line repeat count
//          ST      RA,rep_count                ; store RA to copper mem counter variable
//          LDI     RS,#color_tbl               ; load source reg RS (address of table in COPPER mem)
//          SET     COP_RD,#XR_COLOR_ADDR       ; set dest reg RD (colormap addr - can't use LDI)
//  vloop:
//          LD      RA,vline                    ; store RA to copper mem variable
//          STX     RA,COP_VPOS                 ; wait for line RA
//          ADDI    RA,#1                       ; increment scan line (SUBI alias)
//          ST      RA,vline                    ; store RA to copper mem variable
//          LRSA    RA                          ; load RA with a color (and inc RS)
//          SRD     RA                          ; store color in RA (no inc RD)
//          LRSA    RA                          ; load RA with a color (and inc RS)
//          SET     COP_HPOS,#80                ; wait for 1/4 way across screen
//          SRD     RA                          ; store color in RA (no inc RD)
//          LRSA    RA                          ; load RA with next color (and inc RS)
//          SET     COP_HPOS,#80*2              ; wait for 1/2 way across screen
//          SRD     RA                          ; store color in RA (no inc RD)
//          LRSA    RA                          ; load RA with next color (and inc RS)
//          SET     COP_HPOS,#80*3              ; wait for 3/4 way across screen
//          SRD     RA                          ; store color in RA (no inc RD)
//          LD      RA,rep_count                ; load repeat counter variable
//          SUBI    RA,#1                       ; decrement count
//          ST      RA,rep_count                ; store repeat counter variable
//          BLT     nextcolor                   ; if count went negative, don't reload RS to repeat colors
//          SUBI    RS,#4                       ; subtract 4 from RS
//          LDI     RA,#REPEAT                  ; load repeat value
//          ST      RA,rep_count                ; reset repeat counter
// nextcolor:
//          STX     RS,COP_RA                   ; move RS to RA
//          SUB     RA,#color_end               ; subtract color_end from RS to check for end of table
//          BLT     vloop                       ; loop if RS < color_end
//          SET     COP_VPOS,#-1                ; halt until vsync
//          .data                               ; pushes data to end of copper RAM (for LDI 9-bit copper address)
// color_tbl:
//          .word   0x0400, 0x0040, 0x0004, 0x0444,
//          .word   0x0600, 0x0060, 0x0006, 0x0666,
//          .word   0x0800, 0x0080, 0x0008, 0x0888,
//          .word   0x0C00, 0x00C0, 0x000C, 0x0CCC
// color_end:
// vline:
//          .word   0
// rep_count:
//          .word   0
//

// opcode type {slightly scrambled [13:12],[15:14]}
typedef enum logic [1:0] {
    OP_SET          = 2'b00,        // SET  xaddr,#im16
    OP_SETM         = 2'b01,        // SETM xaddr,xcadr10
    OP_HVPOS        = 2'b10,        // HPOS/VPOS
    OP_Bcc          = 2'b11         // BGE/BLT
} copp_opcode_t;

// opcode decode bits
typedef enum logic [3:0] {
    B_OPCODE        = 4'd12,        // 2-bits
    B_ARG           = 4'd11,        // 12-bit operand/HVPOS or Bcc bit
    B_ARG10         = 4'd9          // im10/cadr10
} copp_bits_t;

// XR bits for psuedo registers
localparam  COP_XREG        = 11;    // pseudo XR register flag (in xreg #)
`ifndef SUB_ONLY
localparam  COP_XREG_SUB    = 0;    // pseudo XR register LOAD/SUB select
`endif

// execution state
typedef enum logic [1:0] {
    ST_FETCH        = 2'b00,        // stalled (waiting for HPOS/VPOS or XR bus write)
    ST_DECODE       = 2'b01,        // decode opcode in cop_IR
    ST_SETR_RD      = 2'b10,        // wait for memory read
    ST_SETR_WR      = 2'b11         // write to XR bus
} copp_ex_state_t;

// copper registers
word_t          cop_RA;             // accumulator/GPR
copp_addr_t     cop_PC;             // current program counter (r/o copper mem)
word_t          cop_IR;             // instruction register (holds executing opcode)
hres_t          cop_wait_val;       // value to wait for HPOS/VPOS

// execution flags
//logic           cop_B_flag;         // last CMPS/SUBS did a borrow (for BGE/BLT)
logic           wait_hv_flag;       // waiting for HPOS/VPOS
logic           wait_for_v;         // false if waiting for >= HPOS else waiting for == VPOS

// bus signals
logic           ram_rd_en;          // copper memory read enable
copp_addr_t     ram_rd_addr;        // copper memory address
word_t          ram_read_data;      // copper memory data in

logic           reg_wr_en;          // pseudo XR register write enable
logic           xr_wr_en;           // XR bus write enable
addr_t          write_addr;         // XR bus address/pseudo XR register number
word_t          write_data;         // XR bus data out/pseudo XR register data out

// control signals
logic           cop_en;             // copper enable/reset (set via COPP_CTRL)
logic           cop_reset;          // copper reset
logic           rd_pipeline;        // read flags for memory pipeline history (higher bit is older)
logic [1:0]     cop_ex_state;       // current execution state
logic           sub_B;
word_t          sub_res;
                // compute subtract borrow and result
assign          { sub_B, sub_res }  = { 1'b0, 16'(cop_RA) } - { 1'b0, write_data };

copp_addr_t     cop_next_PC;        // incremented PC value
assign          cop_next_PC = cop_PC + 1'b1;

// forward bus signals to/from external ports
assign          xr_wr_en_o          = xr_wr_en;
assign          xr_wr_addr_o        = write_addr;
assign          xr_wr_data_o        = write_data;
assign          copmem_rd_en_o      = ram_rd_en;
assign          copmem_rd_addr_o    = ram_rd_addr;
assign          ram_read_data       = copmem_rd_data_i;

// ignore intentionally unused bits (to avoid warnings)
logic unused_bits = &{1'b0, cop_xreg_data_i[14:0]};

// copper xreg (enable/disable)
always_ff @(posedge clk) begin
    if (reset_i) begin
        cop_en          <= 1'b0;
        cop_reset       <= 1'b0;
    end else begin
        cop_reset       <= 1'b0;
        // keep reset if not enabled and reset before SOF
        if (!cop_en || (h_count_i == xv::TOTAL_WIDTH - 4) && (v_count_i == xv::TOTAL_HEIGHT - 1)) begin
            cop_reset       <= 1'b1;
        end

        // COPP_CTRL xreg register write
        if (cop_xreg_wr_i) begin
            cop_en           <= cop_xreg_data_i[15];
        end
    end
end

// register write (and pseudo XR register aliases)
always_ff @(posedge clk) begin
    if (cop_reset) begin
        cop_RA          <= '0;
//        cop_B_flag      <= 1'b0;
    end else begin
        if (reg_wr_en) begin
`ifdef SUB_ONLY
//            cop_B_flag  <= sub_B;           // set B flag
            cop_RA      <= sub_res;         // load RA with subtract result
`else
            // check for CMP/SUB vs load operations
           if (!write_addr[COP_XREG_SUB]) begin
                cop_RA      <= write_data;  // load RA
           end else begin
//                cop_B_flag  <= sub_B;       // set B flag
                cop_RA      <= sub_res;     // load RA with subtract result
           end
`endif
        end
    end
end

// main FSM for copper
always_ff @(posedge clk) begin
    if (cop_reset) begin
        ram_rd_en       <= 1'b0;
        ram_rd_addr     <= '0;

        xr_wr_en        <= 1'b0;
        write_addr      <= '0;
        write_data      <= '0;

        reg_wr_en       <= 1'b0;

        cop_PC          <= '0;
        cop_IR          <= '0;

        wait_hv_flag    <= 1'b0;
        wait_for_v      <= 1'b0;
        cop_wait_val    <= '0;

        rd_pipeline     <= 1'b0;
        cop_ex_state    <= ST_FETCH;
    end else begin
        // reset strobes
        ram_rd_en       <= 1'b0;
        reg_wr_en       <= 1'b0;
        ram_rd_addr     <= cop_PC;                      // read PC

        // remember if read was done last cycle
        rd_pipeline     <= ram_rd_en;

        // only clear XR write enable when ack'd
        if (xr_wr_ack_i) begin
            xr_wr_en        <= 1'b0;
        end


        if (wait_for_v) begin
            if (v_count_i >= $bits(v_count_i)'(cop_wait_val)) begin
                wait_hv_flag    <= 1'b0;
            end
        end else begin
            if (h_count_i >= $bits(h_count_i)'(cop_wait_val)) begin
                wait_hv_flag    <= 1'b0;
            end
        end

        case (cop_ex_state)
            // fetch opcode, store to cop_IR when available
            ST_FETCH: begin
                // if no instruction ready, or new instruction is 2 words, pre-read
                if (!wait_hv_flag && rd_pipeline == 1'b0 && ram_rd_en == 1'b0) begin
                    ram_rd_en       <= 1'b1;                            // read copper memory
//                    ram_rd_addr     <= cop_PC;                          // read PC
                    cop_PC          <= cop_next_PC;                     // increment PC
                end
                // if instruction ready, save in cop_IR it and proceed to DECODE
                if (rd_pipeline) begin
                    cop_IR          <= ram_read_data;                   // store instruction in cop_IR

                    // if 2 word opcode, start 2nd read
                    if (ram_read_data[B_OPCODE+1] == 1'b0) begin
                        ram_rd_en       <= 1'b1;                            // read copper memory
//                        ram_rd_addr     <= cop_PC;                          // read PC
                        cop_PC          <= cop_next_PC;                     // increment PC
                    end

                    cop_ex_state    <= ST_DECODE;                       // decode instruction
                end
            end
            // decode instruction in cop_IR
            ST_DECODE: begin
                case (cop_IR[B_OPCODE+:2])
                    OP_SET: begin
                        write_addr      <= cop_IR;                          // use opcode as XR address
                        write_data      <= ram_read_data;                   // instruction word as data
                        if (rd_pipeline) begin
                            // write to copper regs instead of XR registers if COP_XREG bit set
                            if ((cop_IR[15:14] == xv::XR_CONFIG_REGS[15:14] && cop_IR[COP_XREG])) begin
                                reg_wr_en   <= 1'b1;                        // write cop reg
                            end else begin
                                xr_wr_en    <= 1'b1;                        // write XR bus
                            end

`ifdef SET_NO_STALL
                            ram_rd_en       <= 1'b1;                        // read copper memory
//                            ram_rd_addr     <= cop_PC;                      // read PC
                            cop_PC          <= cop_next_PC;                 // increment PC
`endif
                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                    end
                    OP_SETM: begin
                        ram_rd_en       <= 1'b1;                            // read copper memory (may be ignored)
                        ram_rd_addr     <= xv::COPP_W'(cop_IR);             // use opcode as source address

                        // ram_rd_en       <= 1'b1;                            // read copper memory
                        // ram_rd_addr     <= cop_PC;                          // read PC
                        // cop_PC          <= cop_next_PC;                     // increment PC

                        cop_ex_state    <= ST_SETR_RD;                      // wait for read data
                    end
                    OP_HVPOS: begin
                        wait_hv_flag    <= 1'b1;
                        wait_for_v      <= cop_IR[B_ARG];
                        cop_wait_val    <= $bits(cop_wait_val)'(cop_IR[B_ARG10:0]);

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                    OP_Bcc: begin
                        // branch taken if B clear/set for BGE/BLT
`ifdef BLT_ONLY
                        if (sub_B /*cop_B_flag */ == cop_IR[B_ARG]) begin
`else
                        if (sub_B /* cop_B_flag */) begin
`endif
                            cop_PC          <= xv::COPP_W'(cop_IR[B_ARG10:0]); // set new PC
                        end

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                endcase
            end
            // wait a cycle waiting for data memory read
            ST_SETR_RD: begin
                write_addr          <= ram_read_data;                       // read dest XR address

                cop_ex_state        <= ST_SETR_WR;
            end
            // store word read from memory to XR bus
            ST_SETR_WR: begin
                // write to copper regs instead of XR registers if COP_XREG bit set
                if (write_addr[15:14] == xv::XR_CONFIG_REGS[15:14] && write_addr[COP_XREG]) begin
                    reg_wr_en       <= 1'b1;                                // write cop reg
                end else begin
                    xr_wr_en        <= 1'b1;                                // write XR bus
                end
                write_data      <= cop_IR[COP_XREG] ? cop_RA : ram_read_data; // RA or mem

`ifdef SET_NO_STALL
                ram_rd_en       <= 1'b1;                                    // read copper memory
//                ram_rd_addr     <= cop_PC;                                  // read PC
                cop_PC          <= cop_next_PC;                             // increment PC
`endif
                cop_ex_state    <= ST_FETCH;
            end
            default: ; // Should never happen
        endcase // Execution state
    end
end

endmodule

`endif
`endif
`default_nettype wire               // restore default
