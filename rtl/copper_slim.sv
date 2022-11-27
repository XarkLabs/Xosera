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

//`define EN_COPP_SLIM    // TEMP: remove
//`define EN_COPP         // TEMP: remove
`ifdef EN_COPP
`ifdef EN_COPP_SLIM

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
    input   wire                restart_i,              // restart at end of visible (right after last visible pixel)
    input   wire logic          reset_i,
    input   wire logic          clk
    );

// | XR Op Immediate      | Assembly              |Flags| Cyc | Description                      |
// |----------------------|-----------------------|-----|-----|----------------------------------|
// | xx 00 nnnn nnnn nnnn | SET     #im16,xadr14  |     |  2+ | [xadr14] <= #im16 (2 word op)    |
// | xx 01 00-- ---- ---- | MOVA    (RS),xadr12   |     |  3+ | [xadr12] <= [RS]                 |
// | -- 01 01-- ---- ---- | MOVD    (RS),(RD)+    |     |  3+ | [RD] <= [RS] ; RS++              |
// | xx 01 10-- ---- ---- | MOVSA   (RS)+,xadr12  |     |  3+ | [xadr12] <= [RS] ; RS++          |
// | -- 01 11-- ---- ---- | MOVSD   (RS)+,(RD)+   |     |  3+ | [RD] <= [RS] ; RS++ ; RD++       |
// | s0 10 nnnn nnnn nnnn | CMPS    #sim13        |  B  |  2  |       RS - #sim13 (B flag only)  |
// | s1 10 nnnn nnnn nnnn | SUBS    #sim13        |  B  |  2  | RS <= RS - #sim13                |
// | -0 11 --nn nnnn nnnn | BGE     cad10         | B=0 | 2/4 | if (B==0) PC <= cad10 ; B <= 0   |
// | -1 11 --nn nnnn nnnn | BLT     cad10         | B=0 | 2/4 | if (B==1) PC <= cad10 ; B <= 0   |
// |----------------------|-----------------------|-----|-----|----------------------------------|
//
// B flag = result of last SUBS/CMPS caused a borrow (e.g., RS < #sim13)
//
// im16     =   16-bit immediate word (word after SET, PC will skip over)
// xadr14   =   14-bit XR region + offset:  xx00 nnnn nnnn nnnn
// xadr12   =   12-bit XR region + offset:  xx00 00nn nnnn nnnn
// sim13    =   13-bit sign-extended word:  ssss nnnn nnnn nnnn ([15:12] must all be 0 or 1)
// cad10    =   10-bit copper address:      1100 00nn nnnn nnnn
//
// Special internal pseudo XR registers (XR reg with bit 8 set):
//
// Register aliases (0x100, 0x101):    COP_RS, COP_RD (used to load register value)
// Pseudo registers (0x102, 0x103):    COP_HPOS, COP_VPOS (used to wait for beam position)
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
    OP_SET  = 2'b00,  //
    OP_MOV  = 2'b01,  //
    OP_SUB  = 2'b10,  //
    OP_BLT  = 2'b11   //
} copp_opcode_t;

// opcode decode bits
typedef enum logic [3:0] {
    B_SIGN         =   4'd15,   // sign for 13-bit sim13
    B_OPCODE       =   4'd12,   // 2-bits
    B_ARG          =   4'd11,   // 12-bit argument or MOV/MOVS (source [RS] or [RS++])
    B_MOV_A_D      =   4'd10,   // MOVA/MOVD (dest xaddr or [RD++])
    B_CMP_SUB      =   4'd9     // CMPS/SUBS
} copp_bits_t;

// copper registers
typedef enum logic [1:0] {
    REG_RS          = 2'b00,    // accumulator/source ptr
    REG_RD          = 2'b01,    // dest source ptr
    REG_HPOS        = 2'b10,    // write will wait until HPOS >= value written
    REG_VPOS        = 2'b11     // write will wait until VPOS == value written (NOTE: equals, possibly in next frame)
} copp_reg_num_t;

// execution state
typedef enum logic [2:0] {
    ST_FETCH      = 3'b000,        // fetch opcode, store to IR reg
    ST_WAIT       = 3'b001,        // stalled (waiting for HPOS/VPOS or XR bus write)
    ST_DECODE     = 3'b010,        // decode IR reg
    ST_MOVE_READ  = 3'b011,        // wait for RS memory read
    ST_MOVE_WR    = 3'b100,        // write RS read word to XR bus
    ST_SUB        = 3'b101,        // SUBS/CMPS from RS, set B if RS < value (and optionally store result)
    ST_BRANCH     = 3'b110         // BGE/BLT based on B flag
} copp_ex_state_t;

// external port outputs
assign xr_wr_en_o           = xr_wr_en;
assign xr_wr_addr_o         = xr_wr_addr;
assign xr_wr_data_o         = xr_wr_data;
assign copmem_rd_en_o       = ram_rd_en;
assign copmem_rd_addr_o     = ram_rd_addr;

// internal registers
logic           ram_rd_en;      // copper memory read enable
copp_addr_t     ram_rd_addr;    // copper memory address

logic           xr_wr_en;       // XR bus write enable
addr_t          xr_wr_addr;     // XR bus address/copper register number
word_t          xr_wr_data;     // XR bus data/copper register data

logic           cop_reg_wr_en;  // copper register write enable (also uses xr_wr_addr and xr_wr_data)

// copper registers
word_t          cop_RS;         // source pointer register (r/o copper mem)
word_t          cop_RD;         // desitination pointer register (w/o XADDR)
copp_addr_t     cop_PC;         // current program counter (r/o copper mem)
word_t          cop_IR;         // instruction register (executing opcode)
hres_t          cop_wait_val;   // value to wait for HPOS/VPOS (if waiting)

// execution flags
logic           cop_B_flag;     // last CMPS/SUBS did a borrow (for BGE/BLT)
logic           wait_hv_flag;   // waiting for HPOS/VPOS
logic           wait_for_v;     // false if waiting for >= HPOS else waiting for == VPOS

// control signals
logic           cop_en;         // copper enable/reset (set via COPP_CTRL)
//copp_addr_t     cop_init_PC;    // initial PC (set via COPP_CTRL)
logic           inc_RS;         // strobe to increment RS
logic           inc_RD;         // strobe to increment RD
logic [1:0]     rd_pipeline;    // read flags for memory pipeline history (higher bit is older)
logic [2:0]     cop_ex_state;   // current execution state
copp_addr_t     cop_PC_plus1;

always_comb     cop_PC_plus1    = cop_PC + 1'b1;

logic unused_signals = &{1'b0, cop_xreg_data_i[14:0]};

// register write (and pseudo XR register aliases)
always_ff @(posedge clk) begin
    if (reset_i) begin
        cop_RS          <= '0;
        cop_RD          <= '0;
        cop_wait_val    <= '0;
        wait_hv_flag    <= 1'b0;
        wait_for_v      <= 1'b0;

    end else begin
        if (inc_RS) begin
            cop_RS          <= cop_RS + 1'b1;
        end

        if (inc_RD) begin
            cop_RD          <= cop_RD + 1'b1;
        end

        if (cop_reg_wr_en) begin
            case (xr_wr_addr[1:0])
                REG_RS: cop_RS      <= xr_wr_data;
                REG_RD: cop_RD      <= xr_wr_data;
                REG_HPOS: begin
                    wait_hv_flag    <= 1'b1;
                    wait_for_v      <= 1'b0;
                    cop_wait_val    <= $bits(cop_wait_val)'(xr_wr_data);
                end
                REG_VPOS: begin
                    wait_hv_flag    <= 1'b1;
                    wait_for_v      <= 1'b1;
                    cop_wait_val    <= $bits(cop_wait_val)'(xr_wr_data);
                end
            endcase
        end

        if ((wait_hv_flag && !wait_for_v) && h_count_i >= cop_wait_val) begin
            wait_hv_flag    <= 1'b0;
        end

        if ((wait_hv_flag && wait_for_v) && v_count_i == $bits(v_count_i)'(cop_wait_val)) begin
            wait_hv_flag    <= 1'b0;
        end
    end
end

// main FSM for copper
always_ff @(posedge clk) begin
    if (reset_i) begin
        ram_rd_en       <= 1'b0;
        ram_rd_addr     <= '0;

        xr_wr_en        <= 1'b0;
        xr_wr_addr      <= '0;
        xr_wr_data      <= '0;

        cop_reg_wr_en   <= 1'b0;

        cop_PC          <= '0;
        cop_IR          <= '0;
        cop_B_flag      <= 1'b0;

        cop_en          <= 1'b0;
//        cop_init_PC     <= '0;

        inc_RS          <= 1'b0;
        inc_RD          <= 1'b0;

        rd_pipeline     <= '0;
        cop_ex_state    <= ST_FETCH;
    end else begin
        // reset strobes
        ram_rd_en       <= 1'b0;
        inc_RS          <= 1'b0;
        inc_RD          <= 1'b0;
        cop_reg_wr_en   <= 1'b0;

        // only clear XR write enable when ack'd
        if (xr_wr_ack_i) begin
            xr_wr_en        <= 1'b0;
        end

        // xreg register write
        if (cop_xreg_wr_i) begin
            cop_en           <= cop_xreg_data_i[15];
//            cop_init_PC      <= xv::COPP_W'(cop_xreg_data_i);
        end

        // shift IR pipeline
        rd_pipeline     <= rd_pipeline << 1;

        // disabled or resart
        if (!cop_en || restart_i) begin
            rd_pipeline     <= '0;                                      // clear pipeline
//            cop_PC          <= cop_init_PC;                             // set initial PC
            cop_PC          <= '0;

            cop_ex_state    <= ST_FETCH;
        end else begin
            case (cop_ex_state)
                // fetch opcode, store to IR_reg
                ST_FETCH: begin
                    rd_pipeline[0]  <= 1'b1;                                // pipeline next PC read
                    ram_rd_en       <= 1'b1;                                // read copper memory
                    ram_rd_addr     <= cop_PC;                              // read PC
                    cop_PC          <= cop_PC_plus1;                        // increment PC

                    // if instruction memory ready
                    if (rd_pipeline[1]) begin
                        cop_IR          <= copmem_rd_data_i;                // store instruction in IR

                        if (xr_wr_en || wait_hv_flag) begin
                            cop_ex_state    <= ST_WAIT;                     // stall waiting XR bus or HPOS/VPOS
                        end else begin
                            cop_ex_state    <= ST_DECODE;                   // decode instruction
                        end
                    end
                end
                // stall waiting for xr_wr_en ack or HPOS/VPOS position
                ST_WAIT: begin
                    if (!xr_wr_en && !wait_hv_flag) begin
                        cop_ex_state    <= ST_DECODE;                       // decode instruction
                    end
                end
                // decode instuction in IR_reg
                ST_DECODE: begin
                    case (cop_IR[B_OPCODE+:2])
                        OP_SET: begin
                            // write to copper regs instead of XR bus if xreg >= 0x100
                            if (cop_IR[15:14] == 2'b00 && cop_IR[8]) begin
                                cop_reg_wr_en   <= 1'b1;                    // write cop reg
                            end else begin
                                xr_wr_en        <= 1'b1;                    // write XR bus
                            end
                            xr_wr_addr      <= cop_IR;                      // use opcode as XR address
                            xr_wr_data      <= copmem_rd_data_i;            // instruction word as data

                            rd_pipeline[0]  <= 1'b1;                        // pipeline next PC read
                            ram_rd_en       <= 1'b1;                        // read copmem
                            ram_rd_addr     <= cop_PC;                      // read next PC
                            cop_PC          <= cop_PC_plus1;                // increment PC

                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                        OP_MOV: begin
                            ram_rd_en       <= 1'b1;                        // read copper memory
                            ram_rd_addr     <= xv::COPP_W'(cop_RS);         // read RS
                            inc_RS          <= cop_IR[B_ARG];               // optionally increment RS

                            cop_ex_state    <= ST_MOVE_READ;                // wait for read data
                        end
                        OP_SUB: begin
                            cop_reg_wr_en   <= cop_IR[B_CMP_SUB];           // write result or not (CMPS/SUBS)
                            xr_wr_addr      <= 16'(REG_RS);                 // write to RS reg
                            // subtract RS - #sim13, saving borrow result
                            { cop_B_flag, xr_wr_data }  <= { 1'b0, cop_RS } - { 1'b0, {4{cop_IR[B_SIGN]}}, cop_IR[B_ARG:0]};

                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                        OP_BLT: begin
                            // branch taken if B clear/set for BGE/BLT
                            if (cop_B_flag == cop_IR[B_CMP_SUB]) begin
                                rd_pipeline     <= '0;                      // flush pipeline
                                cop_PC          <= xv::COPP_W'(cop_IR[B_ARG:0]); // set new PC
                            end

                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                    endcase
                end
                // delay a cycle waiting for data read
                ST_MOVE_READ: begin
                    cop_ex_state        <= ST_MOVE_WR;
                end
                // store word read from RS to XR bus
                ST_MOVE_WR: begin
                    // write to copper regs instead of XR bus if xreg >= 0x100
                    if (cop_IR[15:14] == 2'b00 && cop_IR[8]) begin
                        cop_reg_wr_en   <= 1'b1;                            // write cop reg
                    end else begin
                        xr_wr_en        <= 1'b1;                            // write XR bus
                    end

                    if (cop_IR[B_MOV_A_D]) begin
                        xr_wr_addr      <= cop_IR & 16'hC3FF;               // use opcode xadr12 as XR address
                    end else begin
                        xr_wr_addr      <= cop_RD;                          // use RD as XR address
                        inc_RD          <= 1'b1;                            // increment RD
                    end
                    xr_wr_data      <= copmem_rd_data_i;                    // memory word as data

                    rd_pipeline[0]  <= 1'b1;                                // pipeline next PC read
                    ram_rd_en       <= 1'b1;                                // read copmem
                    ram_rd_addr     <= cop_PC;                              // read next PC
                    cop_PC          <= cop_PC_plus1;                        // increment PC

                    cop_ex_state    <= ST_FETCH;
                end
                default: ; // Should never happen
            endcase // Execution state
        end
    end
end

endmodule

`endif
`endif
`default_nettype wire               // restore default
