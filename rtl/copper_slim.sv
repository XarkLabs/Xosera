// copper_slim.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Ross Bamford  - https://github.com/roscopeco
// Copyright (c) 2022 Xark          - https://github.com/XarkLabs
//
// See top-level LICENSE file for license information. (Hint: MIT) foo
//
`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`ifdef EN_COPP

`include "xosera_pkg.sv"

module copper_slim(
    output       logic          xr_wr_en_o,             // XR bus write enable
    input   wire logic          xr_wr_ack_i,            // XR bus ack
    output       addr_t         xr_wr_addr_o,           // XR bus address
    output       word_t         xr_wr_data_o,           // XR bus write data
    output       copp_addr_t    copmem_rd_addr_o,       // copper memory
    output       logic          copmem_rd_en_o,         // copper memory read enable
    input   wire logic [15:0]   copmem_rd_data_i,       // copper memory data
    input   wire logic          cop_xreg_wr_i,          // COPP_CTRL register write strobe
    input   wire logic          cop_xreg_enable_i,      // COPP_CTRL register enable write data
    input   wire hres_t         h_count_i,              // horizontal video position
    input   wire vres_t         v_count_i,              // vertical video position
    input   wire logic          end_of_line_i,          // end of line signal\
`ifdef EN_COPP_VBLITWAIT
    input   wire logic          blit_busy_i,            // blitter busy signal
`endif
    input   wire logic          reset_i,
    input   wire logic          clk
);

// `define AVOID_RD_RW_HAZARD          // delay read to next cycle if also writing (iCE40UP5K does not need this)

`define SETM_4CYCLE                    // SETM also 4 cycles at slight LC cost (~27 LCs, worth it)

//  Slim Copper opcodes:
//
// | XR Op Immediate     | Assembly             |Flag | Cyc | Description                      |
// |---------------------|----------------------|-----|-----|----------------------------------|
// | rr00 oooo oooo oooo | SETI   xadr14,#val16 |  B  |  4  | dest [xadr14] <= source #val16   |
// | iiii iiii iiii iiii |    <im16 value>      |     |     |   (2 word op)                    |
// | --01 rccc cccc cccc | SETM  xadr16,cadr11  |  B  |  4* | dest [xadr16] <= source [cadr11] |
// | rroo oooo oooo oooo |    <xadr16 address>  |     |     |   (2 word op)                    |
// | --10 0iii iiii iiii | HPOS   #im11         |     |  4+ | wait until video HPOS >= im11    |
// | --10 1iii iiii iiii | VPOS   #im11         |     |  4+ | wait until video VPOS >= im11    |
// | --11 0ccc cccc cccc | BRGE   cadd11        |     |  4  | if (B==0) PC <= cadd11           |
// | --11 1ccc cccc cccc | BRLT   cadd11        |     |  4  | if (B==1) PC <= cadd11           |
// |---------------------|----------------------|-----|-----|----------------------------------|
//
// xadr14   =   XR region + 12-bit offset           xx00 oooo oooo oooo (1st word SETI, dest)
// im16     =   16-bit immediate word               iiii iiii iiii iiii (2nd word SETI, source)
// cadr11   =   11-bit copper address + register    ---- rnnn nnnn nnnn (1st word SETM, source)
// xadr16   =   XR region + 14-bit offset           rroo oooo oooo oooo (2nd word SETM, dest)
// im11     =   11-bit immediate value              ---- -iii iiii iiii (HPOS, VPOS)
// cadd11   =   11-bit copper address/register      ---- -nnn nnnn nnnn (BRGE, BRLT)
// B        =   borrow flag set when RA < val16 written [unsigned subtract])
//
// NOTE: cadr10 bits[15:11] are ignored reading copper memory, however by setting
//       bits[15:14] to 110a a cadr10 address can be used as either the source or dest
//       for SETM (when opcode bit a=1) or as destination XADDR with SETI (with opcode bit=0).
//
// Internal pseudo register (accessed as XR reg or copper address when COP_XREG bit set)
//
// | Pseudo reg     | Addr   | Operation               | Description                               |
// |----------------|--------|-------------------------|-------------------------------------------|
// | RA     (read)  | 0x0800 | RA                      | return current value in RA register       |
// | RA     (write) | 0x0800 | RA = val16, B = 0       | set RA to val16, clear B flag             |
// | RA_SUB (write) | 0x0801 | RA = RA - val16, B=LT   | set RA to RA - val16, update B flag       |
// | RA_CMP (write) | 0x07FF | B flag update           | update B flag only (updated on any write) |
// |----------------|--------|-------------------------|-------------------------------------------|
// NOTE: The B flag is updated after any write, RA_CMP is just a convenient xreg with no effect
//
// Example copper code: (not using any pseudo instructions)
//
// ; copy table of values to contiguous registers
// ; (using only simple 1:1 pseudo ops)
// set_tbl
//                 SETI    set_reg+0,#SETM+val_tbl     ; set start set_reg source (plus SETM bit)
//                 SETI    set_reg+1,#XR_COLOR_ADDR    ; set start set_reg dest
// set_reg         SETM    $FFFF,$C7FF                 ; set color reg (self-modified)
//                 LDM     set_reg+1                   ; load RA with set_reg dest
//                 ADDI    #1                          ; increment RA
//                 STM     set_reg+1                   ; store RA to set_reg dest
//                 LDM     set_reg                     ; set RA with set_reg source
//                 ADDI    #1                          ; increment RA
//                 CMPI    #SETM+end_tbl               ; compare RA with table end (plus SETM bit)
//                 BRGE    set_done                    ; branch if done
//                 STM     set_reg+0                   ; store RA to set_reg source
//                 BRGE    set_reg                     ; branch always (ST clears B)
// set_done        VPOS    #-1                         ; halt until SOF
//
// val_tbl         .word   0x000, 0x111, 0x222, 0x333
//                 .word   0x444, 0x555, 0x666, 0x777
//                 .word   0x888, 0x999, 0xaaa, 0xbbb
//                 .word   0xccc, 0xddd, 0xeee, 0xfff
// end_tbl

// opcode type {slightly scrambled [13:12],[15:14]}
typedef enum logic [1:0] {
    OP_SETI         = 2'b00,        // SETI xaddr,#im16
    OP_SETM         = 2'b01,        // SETM xaddr,xcadr10
    OP_HVPOS        = 2'b10,        // HPOS/VPOS
    OP_BRcc         = 2'b11         // BRGE/BRLT
} copp_opcode_t;

// opcode decode bits
localparam  B_OPCODE        = 12;   // 2-bits
localparam  B_HV_SEL        = 11;   // HPOS/VPOS select bit
localparam  B_HV_POS        = 10;   // 11-bit operand
localparam  B_V_BLIT        = 10;   // VPOS wait blit busy
localparam  B_BR_SEL        = 11;   // BRGE/BRLT select bit
localparam  B_BR_ADR        = 10;   // 11-bit copper address
localparam  B_COP_REG       = 11;   // cop RA register bit
localparam  B_COP_SUB       = 0;    // cop_RA LOAD/SUB select

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

// execution flags
logic           wait_hv_flag;       // waiting for HPOS/VPOS
logic           wait_for_v;         // false if waiting for >= HPOS else waiting for == VPOS

// copper memory bus signals
logic           ram_rd_en;          // copper memory read enable
logic           rd_reg_save;        // read RA contents vs memory on SETM
copp_addr_t     ram_rd_addr;        // copper memory address
word_t          ram_read_data;      // copper memory data in

// XR memory/register bus signals
logic           reg_wr_en;          // XR pseudo register write enable
logic           xr_wr_en;           // XR bus write enable
addr_t          write_addr;         // XR bus address/pseudo XR register number
word_t          write_data;         // XR bus data out/pseudo XR register data out

// control signals
logic           cop_en;             // copper enable/reset (set via COPP_CTRL)
logic           cop_reset;          // copper reset (set if not enabled, or line 0, pixel 0)
logic           cop_run;            // copper running
copp_ex_state_t cop_ex_state;       // current execution state
logic           rd_pipeline;        // flag if memory read on last cycle

// ALU :)
logic           B_flag;             // B flag (borrow flag)
word_t          RA_sub;             // current RA - written data subtract result
assign          { B_flag, RA_sub }  = 17'(cop_RA) - 17'(write_data);

copp_addr_t     cop_next_PC;        // incremented PC value
assign          cop_next_PC = cop_PC + 1'b1;

// forward bus signals to/from external ports
assign          xr_wr_en_o          = xr_wr_en;
assign          xr_wr_addr_o        = write_addr;
assign          xr_wr_data_o        = write_data;
assign          copmem_rd_en_o      = ram_rd_en;
assign          copmem_rd_addr_o    = ram_rd_addr;
assign          ram_read_data       = copmem_rd_data_i;

`ifndef SYNTHESIS
/* verilator lint_off UNUSEDSIGNAL */
logic           op_valid;
word_t          opcode;
word_t          op_imm;
word_t          op_src;
word_t          op_dest;

logic [8*4-1:0] op_name;
always_comb begin
    if (op_valid) begin
        case (opcode[13:11])
        3'b000: op_name = "SETI";
        3'b001: op_name = "SETI";
        3'b010: op_name = "SETM";
        3'b011: op_name = "SETM";
        3'b100: op_name = "HPOS";
        3'b101: op_name = "VPOS";
        3'b110: op_name = "BRGE";
        3'b111: op_name = "BRLT";
        endcase
    end else begin
        op_name = "----";
    end
end
/* verilator lint_on  UNUSEDSIGNAL */
`endif

// copper control xreg (enable/disable), also does start of frame reset
always_ff @(posedge clk) begin
    if (reset_i) begin
`ifdef EN_COPPER_INIT   // start with copper running so it can init Xosera
        cop_reset       <= 1'b1;
        cop_en          <= 1'b1;
        cop_run         <= 1'b1;
`else
        cop_reset       <= 1'b0;
        cop_en          <= 1'b0;
        cop_run         <= 1'b0;
`endif
    end else begin
        // keep in reset if not enabled and reset at SOF
        if (end_of_line_i && (v_count_i == 0)) begin
            cop_reset       <= 1'b1;
            cop_run         <= cop_en;
        end else begin
            cop_reset       <= !cop_run;
        end

        // COPP_CTRL xreg register write to set cop_en
        if (cop_xreg_wr_i) begin
            cop_en          <= cop_xreg_enable_i;
            if (!cop_xreg_enable_i) begin
                cop_run         <= 1'b0;
            end
        end
    end
end

// register write (and pseudo XR register aliases)
always_ff @(posedge clk) begin
    if (cop_reset) begin
        cop_RA          <= '0;
    end else begin
        if (reg_wr_en) begin
            // check for load vs subtract operation
           if (!write_addr[B_COP_SUB]) begin
                cop_RA      <= write_data;  // load RA with data
           end else begin
                cop_RA      <= RA_sub;      // load RA with RA - data result
           end
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

        rd_reg_save     <= 1'b0;
        wait_hv_flag    <= 1'b0;
        wait_for_v      <= 1'b0;

        rd_pipeline     <= 1'b0;
        cop_ex_state    <= ST_FETCH;
    end else begin
        // reset strobes
        ram_rd_en       <= 1'b0;
        reg_wr_en       <= 1'b0;
        ram_rd_addr     <= cop_PC;  // assume reading PC

        // remember if read was done last cycle
        rd_pipeline     <= ram_rd_en;

        // only clear XR write enable when ack'd
        if (xr_wr_ack_i) begin
            xr_wr_en        <= 1'b0;
        end

        case (cop_ex_state)
            // fetch opcode, store to cop_IR when available
            ST_FETCH: begin
                 // if read data not ready
                if (!rd_pipeline) begin
`ifndef SYNTHESIS
                    op_valid        <= 1'b0;
`endif
                    // if not waiting and read not already started then read PC
                    if (!wait_hv_flag && !ram_rd_en) begin
                        ram_rd_en       <= 1'b1;                            // read copper memory
                        cop_PC          <= cop_next_PC;                     // increment PC
                    end
                end else begin
                    cop_IR          <= ram_read_data;                       // store instruction in cop_IR
`ifndef SYNTHESIS
                    op_valid        <= 1'b1;
                    opcode          <= ram_read_data;
                    op_imm          <= 'X;
                    op_src          <= 'X;
                    op_dest         <= 'X;
`endif

                    // if this is a 2 word opcode, start 2nd word read
                    if (ram_read_data[B_OPCODE+1] == 1'b0) begin
                        ram_rd_en       <= 1'b1;                            // read copper memory
                        cop_PC          <= cop_next_PC;                     // increment PC
                    end

                    cop_ex_state    <= ST_DECODE;                           // decode instruction
                end
            end
            // decode instruction in cop_IR
            ST_DECODE: begin
                case (cop_IR[B_OPCODE+:2])
                    OP_SETI: begin
                        write_addr      <= cop_IR;                          // use opcode as XR address
                        write_data      <= ram_read_data;                   // instruction word as data
`ifndef SYNTHESIS
                        op_imm          <= ram_read_data;
                        op_dest         <= cop_IR & 16'hCFFF;
`endif
                        if (rd_pipeline) begin
                            // write to internal register instead of xreg if COP_XREG bit set in dest
                            if ((cop_IR[15:14] == xv::XR_CONFIG_REGS[15:14] && cop_IR[B_COP_REG])) begin
                                reg_wr_en   <= 1'b1;                        // write cop reg
                            end else begin
                                xr_wr_en    <= 1'b1;                        // write XR bus
                            end

                            // NOTE: expecting iCE40UP5K read & write at same address and cycle returns new write data
                            ram_rd_en       <= 1'b1;                        // read copper memory
                            cop_PC          <= cop_next_PC;                 // increment PC

                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                    end
                    OP_SETM: begin
                        ram_rd_en       <= 1'b1;                            // read copper memory (may be unused)
                        ram_rd_addr     <= xv::COPP_W'(cop_IR);             // use opcode as source address
                        rd_reg_save     <= cop_IR[B_COP_REG];
`ifndef SYNTHESIS
                        op_src          <= cop_IR & 16'hCFFF;
`endif

                        cop_ex_state    <= ST_SETR_RD;                      // wait for read data
                    end
                    OP_HVPOS: begin
                        wait_hv_flag    <= 1'b1;
                        wait_for_v      <= cop_IR[B_HV_SEL];

`ifndef SYNTHESIS
                        op_imm          <= 16'(cop_IR[B_HV_POS:0]);
`endif

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                    OP_BRcc: begin
                        // branch taken if B clear/set for BRGE/BRLT
                        if (B_flag == cop_IR[B_BR_SEL]) begin
                            cop_PC          <= xv::COPP_W'(cop_IR[B_BR_ADR:0]); // set new PC
                        end

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                endcase
            end
            // wait a cycle waiting for data memory read
            ST_SETR_RD: begin
                cop_IR              <= ram_read_data;                       // save dest address word
`ifndef SYNTHESIS
                op_dest             <= ram_read_data & 16'hCFFF;
`endif
`ifdef  SETM_4CYCLE
                ram_rd_en       <= 1'b1;                                    // read copper memory
`endif

                cop_ex_state        <= ST_SETR_WR;                          // write out word
            end
            // store word read from memory to XR bus
            ST_SETR_WR: begin
                // write to copper reg instead of xreg if COP_XREG bit set in dest
                if (cop_IR[15:14] == xv::XR_CONFIG_REGS[15:14] && cop_IR[B_COP_REG]) begin
                    reg_wr_en       <= 1'b1;                                // write cop reg
                end else begin
                    xr_wr_en        <= 1'b1;                                // write XR bus
                end

                // write data from copper reg instead of memory read if COP_XREG bit set in source
                write_addr      <= cop_IR;
                write_data      <= rd_reg_save ? cop_RA : ram_read_data;

`ifndef  SETM_4CYCLE
                ram_rd_en       <= 1'b1;                                    // read copper memory
`endif
                cop_PC          <= cop_next_PC;                             // increment PC

                cop_ex_state    <= ST_FETCH;
            end
        endcase // Execution state

        if (wait_for_v) begin
            if (v_count_i >= $bits(v_count_i)'(cop_IR)) begin
                wait_hv_flag    <= 1'b0;
            end
        end else begin
            if (h_count_i >= $bits(h_count_i)'(cop_IR)) begin
                wait_hv_flag    <= 1'b0;
            end
        end

`ifdef EN_COPP_HWAITEOL
        // Feature: stop waiting for HPOS at the start of a new line
        if (wait_hv_flag & end_of_line_i & !wait_for_v) begin
            wait_hv_flag    <= 1'b0;
        end
`endif

`ifdef EN_COPP_VBLITWAIT
        // Feature: stop waiting for VPOS #$7FF [B_V_BLIT] when blit not busy
        if (wait_hv_flag & cop_IR[B_V_BLIT] & wait_for_v & !blit_busy_i) begin
            wait_hv_flag    <= 1'b0;
        end
`endif

    end
end

endmodule

`endif
`default_nettype wire               // restore default
