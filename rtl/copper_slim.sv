// copper.sv
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

`define EN_COPP         // TEMP: for editing, remove
`define EN_COPP_SLIM    // TEMP: for editing, remove

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
    input   wire logic          reset_i,
    input   wire logic          clk
);

//  Slim Copper opcodes:
//
// | XR Op Immediate     | Assembly             |Flag | Cyc | Description                      |
// |---------------------|----------------------|-----|-----|----------------------------------|
// | rr00 oooo oooo oooo | SETI   xadr14,#val16 |  B  |  4+ | dest [xadr14] <= source #val16   |
// | iiii iiii iiii iiii |    <im16 value>      |     |     |   (2 word op)                    |
// | --01 -rcc cccc cccc | SETM  xadr16,cadr10  |  B  |  5+ | dest [xadr16] <= source [cadr10] |
// | rroo oooo oooo oooo |    <xadr16 address>  |     |     |   (2 word op)                    |
// | --10 0-ii iiii iiii | HPOS   #im10         |     |  5+ | wait until video HPOS >= im10    |
// | --10 1-ii iiii iiii | VPOS   #im10         |     |  5+ | wait until video VPOS >= im10    |
// | --11 0rcc cccc cccc | BRGE   cadr10        |     |  4  | if (B==0) PC <= cadr10           |
// | --11 1rcc cccc cccc | BRLT   cadr10        |     |  4  | if (B==1) PC <= cadr10           |
// |---------------------|----------------------|-----|-----|----------------------------------|
//
// xadr14   =   XR region + 12-bit offset:      xx00 oooo oooo oooo (1st word in SETI)
// im16     =   16-bit immediate word           iiii iiii iiii iiii (2nd word in SETI)
// cadr10   =   10-bit copper address/register: ---- -rnn nnnn nnnn (r=1 for RA read)
// im10     =   10-bit immediate value:         ---- --ii iiii iiii (HPOS, VPOS)
// xadr16   =   XR region + 14-bit offset:      rroo oooo oooo oooo (2nd word in SETM)
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
// Notable pseudo instructions:
//  ->  LDI  #val16             ; load RA with immediate val16 (2 words, B = 0)
//  +     SETI  RA,#val16           ; RA = val16
//
//  ->  LD   caddr              ; load RA with copper mem contents at address caddr (2 words, B = 0)
//  +     SETM  RA,caddr            ; RA -= value
//
//  ->  ADDI #val16             ; add val16 to RA, RA = RA + val16 (2 words, B = inverted carry)
//  +     SETI  RA_SUB,#-val16      ; RA -= -val16
//
//  ->  SUBI #val16             ; subtract val16 from RA, RA = RA - val16 (2 words, B = RA < val16 [unsigned])
//  +     SETI  RA_SUB,#val16       ; RA -= value
//
//  ->  SETP (caddr),#val16     ; store val16 to ptr addr in cadr10, *caddr = val16; (4 words, self-mod)
//  +     SETM *+2,caddr            ; self-modify SETI dest
//  +     SETI  dummy,#val16        ; store to [RA]
//
//  ->  SETPA (dcaddr),scaddr   ; store memory to ptr addr in dcadr10, *dcadr10 = scadr10; (4 words, self-mod)
//  +     SETM *+2,dcadr10          ; self-modify SETI dest
//  +     SETM dummy,scadr10        ; store scadr10 to (dcadr10)
//
//  ->  BRA  (caddr)            ; branch to ptr, PC = [cadr10] (3 words)
//  +     SETM RA,caddr             ; RA = [cadr10]
//  +     BRGE  RA                  ; branch to [cadr0]
//
// Example copper code: (not using any pseudo instructions)
//
// REPEAT   =       10
// start    SETI     XR_PA_GFX_CTRL,#0x0055     ; set PA_GFX_MODE
//          SETI     rep_count,#REPEAT-1        ; set line repeat counter
//          SETI     color_ptr,#SETM+color_tbl  ; set source ptr with SETM opcode + color table
//          SETI     vpos_lp,#VPOS+120          ; modify vpos_wait with VPOS opcode + starting line
//
// vpos_lp  VPOS    #0                          ; wait for next scan line (self-modified)
//          HPOS    #LEFT+0                     ; wait for left edge of screen
//          SETM    *+2,color_ptr               ; modify next SETM source with color_ptr
//          SETM    XR_COLOR_ADDR,0             ; set color from color_ptr (self-modified)
//          SETM    RA,color_ptr                ; load color_ptr
//          SETM    RA_SUB,#-1                  ; increment
//          SETM    color_ptr,RA                ; store color_ptr
//
//          HPOS    #LEFT+(640/2)               ; wait for 1/2 way across screen
//          SETM    *+2,color_ptr               ; modify next SETM source with color_ptr
//          SETM    XR_COLOR_ADDR,0             ; set color from color_ptr (self-modified)
//          SETM    RA,color_ptr                ; load color_ptr
//          SETM    RA_SUB,#-1                  ; increment
//          SETM    color_ptr,RA                ; store color_ptr
//
//          SETM    RA,vpos_lp                  ; load vpos
//          SETI     RA_SUB,#-1                 ; increment scan line
//          SETM    vpos_lp,RA                  ; load vpos
//
//          SETI     RA,rep_count               ; load repeat counter variable
//          SETI     RA_SUB,#1                  ; decrement count
//          SETM    rep_count,RA                ; store repeat counter variable
//          SETM    RA,rep_count                ; load repeat counter variable
//          SETI     RA_SUB,#1                  ; decrement count
//          SETM    rep_count,RA                ; store repeat counter variable
//          BRLT     nx_color                   ; if count went negative, next color
//
//          SETM    RA,color_ptr                ; load color_ptr
//          SETM    RA_SUB,#-2                  ; rewind to repeat colors again
//          SETM    color_ptr,RA                ; store color_ptr
//
//          SETM    RA,vpos_w                   ; load vpos
//          SETI     RA_SUB,#-1                 ; increment scan line
//          SETM    vpos_w,RA                   ; load vpos
//          BRGE    vpos_lp                     ; always taken (since SETx xxx,RA sets B with RA-RA)
//
//          SETI     rep_count,#REPEAT-1        ; reset line repeat counter
//
// nx_color SETM    RA,color_ptr                ; load color_ptr
//          SETI     RA_CMP,#SETM+color_end     ; compare color_ptr to SETM opcode + color_end
//          BRLT    vpos_lp                     ; loop if color_ptr < color_end
//          VPOS    #-1                         ; halt until next frame
//
// colortbl .word   0x0400, 0x0444,
//          .word   0x0600, 0x0666,
//          .word   0x0800, 0x0888,
//          .word   0x0C00, 0x0CCC
// color_end
//
// rep_count
//          .word   0
// color_ptr
//          .word   0
//
// ; copy table of values to contiguous registers
// ; (using only simple 1:1 pseudo ops)
// set_tbl
//          SETI     set_reg+0,#SETM+val_tbl    ; set start set_reg source (plus SETM bit)
//          SETI     set_reg+1,#XR_COLOR_ADDR   ; set start set_reg dest
// set_reg  SETM    -1,-1                       ; set color reg (self-modified)
//          LD      set_reg+1                   ; load RA with set_reg dest
//          ADDI    #1                          ; increment RA
//          ST      set_reg+1                   ; store RA to set_reg dest
//          LD      set_reg                     ; set RA with set_reg source
//          ADDI    #1                          ; increment RA
//          CMPI    #SETM+end_tbl               ; compare RA with table end (plus SETM bit)
//          BRGE     set_done                   ; branch if done
//          ST      set_reg+0                   ; store RA to set_reg source
//          BRGE     set_reg                    ; branch always (ST clears B)
// set_done VPOS    #-1                         ; halt until SOF
//
// val_tbl  .word   0x000, 0x111, 0x222, 0x333
//          .word   0x444, 0x555, 0x666, 0x777
//          .word   0x888, 0x999, 0xaaa, 0xbbb
//          .word   0xccc, 0xddd, 0xeee, 0xfff
// end_tbl

// opcode type {slightly scrambled [13:12],[15:14]}
typedef enum logic [1:0] {
    OP_SETI         = 2'b00,        // SETI xaddr,#im16
    OP_MOVE         = 2'b01,        // SETM xaddr,xcadr10
    OP_HVPOS        = 2'b10,        // HPOS/VPOS
    OP_BRcc         = 2'b11         // BRGE/BRLT
} copp_opcode_t;

// opcode decode bits
typedef enum logic [3:0] {
    B_OPCODE        = 4'd12,        // 2-bits
    B_ARG           = 4'd11,        // 12-bit operand/HVPOS or Bcc bit
    B_ARG10         = 4'd9          // im10/cadr10
} copp_bits_t;

// XR bits for pseudo registers
localparam  COP_XREG        = 10;   // cop register read/write (cadr/xreg)
localparam  COP_XREG_SUB    = 0;    // cop_RA LOAD/SUB select

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
logic           wait_hv_flag;       // waiting for HPOS/VPOS
logic           wait_for_v;         // false if waiting for >= HPOS else waiting for == VPOS

// copper memory bus signals
logic           ram_rd_en;          // copper memory read enable
copp_addr_t     ram_rd_addr;        // copper memory address
word_t          ram_read_data;      // copper memory data in

// XR memory/register bus signals
logic           reg_wr_en;          // XR pseudo register write enable
logic           xr_wr_en;           // XR bus write enable
addr_t          write_addr;         // XR bus address/pseudo XR register number
word_t          write_data;         // XR bus data out/pseudo XR register data out

// control signals
logic           cop_en;             // copper enable/reset (set via COPP_CTRL)
logic           cop_reset;          // copper reset
logic [1:0]     cop_ex_state;       // current execution state
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

// ignore intentionally unused bits (to avoid warnings)
logic unused_bits = &{1'b0, cop_xreg_data_i[14:0]};

// copper control xreg (enable/disable), also does start of frame reset
always_ff @(posedge clk) begin
    if (reset_i) begin
        cop_en          <= 1'b0;
        cop_reset       <= 1'b0;
    end else begin
        cop_reset       <= 1'b0;

        // keep in reset if not enabled and reset just before SOF
        if (!cop_en || (h_count_i == xv::TOTAL_WIDTH - 4) && (v_count_i == xv::TOTAL_HEIGHT - 1)) begin
            cop_reset       <= 1'b1;
        end

        // COPP_CTRL xreg register write to set cop_en
        if (cop_xreg_wr_i) begin
            cop_en           <= cop_xreg_data_i[15];
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
           if (!write_addr[COP_XREG_SUB]) begin
                cop_RA      <= write_data;  // load RA with data
           end else begin
                cop_RA      <= RA_sub;      // load RA with subtract result
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

        wait_hv_flag    <= 1'b0;
        wait_for_v      <= 1'b0;
        cop_wait_val    <= '0;

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
                 // if read data not ready
                if (!rd_pipeline) begin
                    // if not waiting and read not already started then read PC
                    if (!wait_hv_flag && !ram_rd_en) begin
                        ram_rd_en       <= 1'b1;                            // read copper memory
                        cop_PC          <= cop_next_PC;                     // increment PC
                    end
                end else begin
                    cop_IR          <= ram_read_data;                       // store instruction in cop_IR

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
                        if (rd_pipeline) begin
                            // write to internal register instead of xreg if COP_XREG bit set in dest
                            if ((cop_IR[15:14] == xv::XR_CONFIG_REGS[15:14] && cop_IR[COP_XREG])) begin
                                reg_wr_en   <= 1'b1;                        // write cop reg
                            end else begin
                                xr_wr_en    <= 1'b1;                        // write XR bus
                            end
`ifdef NO_SET_HAZARD
                            if (cop_IR[15:14] != xv::XR_COPPER_ADDR[15:14])
`endif
                            begin
                                ram_rd_en       <= 1'b1;                    // read copper memory
                                cop_PC          <= cop_next_PC;             // increment PC
                            end

                            cop_ex_state    <= ST_FETCH;                    // fetch next instruction
                        end
                    end
                    OP_MOVE: begin
                        ram_rd_en       <= 1'b1;                            // read copper memory (may be unused)
                        ram_rd_addr     <= xv::COPP_W'(cop_IR);             // use opcode as source address

                        cop_ex_state    <= ST_SETR_RD;                      // wait for read data
                    end
                    OP_HVPOS: begin
                        wait_hv_flag    <= 1'b1;
                        wait_for_v      <= cop_IR[B_ARG];
                        cop_wait_val    <= $bits(cop_wait_val)'(cop_IR[B_ARG10:0]);

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                    OP_BRcc: begin
                        // branch taken if B clear/set for BRGE/BRLT
                        if (B_flag == cop_IR[B_ARG]) begin
                            cop_PC          <= xv::COPP_W'(cop_IR[B_ARG10:0]); // set new PC
                        end

                        cop_ex_state    <= ST_FETCH;                        // fetch next instruction
                    end
                endcase
            end
            // wait a cycle waiting for data memory read
            ST_SETR_RD: begin
                write_addr          <= ram_read_data;                       // read dest address word

                cop_ex_state        <= ST_SETR_WR;                          // write out word
            end
            // store word read from memory to XR bus
            ST_SETR_WR: begin
                // write to copper reg instead of xreg if COP_XREG bit set in dest
                if (write_addr[15:14] == xv::XR_CONFIG_REGS[15:14] && write_addr[COP_XREG]) begin
                    reg_wr_en       <= 1'b1;                                // write cop reg
                end else begin
                    xr_wr_en        <= 1'b1;                                // write XR bus
                end

                // write data from copper reg instead of memory read if COP_XREG bit set in source
                write_data      <= cop_IR[COP_XREG] ? cop_RA : ram_read_data;

                // pre-read unless write copper address (self-mod hazard)
                if (cop_IR[15:14] != xv::XR_COPPER_ADDR[15:14]) begin
                    ram_rd_en       <= 1'b1;                                // read copper memory
                    cop_PC          <= cop_next_PC;                         // increment PC
                end

                cop_ex_state    <= ST_FETCH;
            end
        endcase // Execution state
    end
end

endmodule

`endif
`endif
`default_nettype wire               // restore default
