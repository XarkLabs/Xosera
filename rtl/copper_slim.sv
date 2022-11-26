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
    input   wire logic          copp_reg_wr_i,          // strobe to write internal config register
    input   wire word_t         copp_reg_data_i,        // data for internal config register
    input   wire hres_t         h_count_i,
    input   wire vres_t         v_count_i,
    input   wire                restart_i,              // restart at end of visible (right after last visible pixel)
    input   wire logic          reset_i,
    input   wire logic          clk
    );


// | XR Op Immediate      | Assembly              |Flag| Cyc | Description                      |
// |----------------------|-----------------------|----|-----|----------------------------------|
// | xx 00 nnnn nnnn nnnn | SET     im14,#im16    |    |  2+ | [im14] <= im16 (2 words)         |
// | 00 01 rrsn nnnn nnnn | LDI     reg,#sim10    | Z  |  1  | Rr <= #sim10                     |
// | 01 01 rrsn nnnn nnnn | SUBI    reg,#sim10    | BZ |  1  | Rr <= Rr - #sim10 (B=reg<#sim10) |
// | 10 01 rrsn nnnn nnnn | NORI    reg,#sim10    | Z  |  1  | Rr <= ~(Rr | #sim10)             |
// | 11 01 rri- nnnn nnnn | STX     reg,xreg8     |    |  1+ | [xreg] <= Rr                     |
// | 00 10 rrnn nnnn nnnn | LD      reg,cad10     | Z  |  2  | Rr <= [cad10]                    |
// | 01 10 rrnn nnnn nnnn | SUB     reg,cad10     | BZ |  2  | Rr <= Rr - [cad10] (B=reg<cad10) |
// | 10 10 rrnn nnnn nnnn | NOR     reg,cad10     | Z  |  2  | Rr <= ~(Rr | [cad10])            |
// | 11 10 rrnn nnnn nnnn | ST      reg,cad10     |    |  2+ | [cad10] <= Rr                    |
// | 00 11 rr0- ---- ---- | LRS     reg           | Z  |  2  | Rr <= [RS]                       |
// | 00 11 rr1- ---- ---- | LRSA    reg           | Z  |  2  | Rr <= [RS]; RS++                 |
// | 01 11 rr0- ---- ---- | SRD     reg           |    |  1+ | [RD] <= Rr                       |
// | 01 11 rr1- ---- ---- | SRDA    reg           |    |  1+ | [RD] <= Rr; RD++                 |
// | 10 11 00nn nnnn nnnn | BGE     cad10         |    | 1/3 | if (B==0) PC <= cad10            |
// | 10 11 01nn nnnn nnnn | BLT     cad10         |    | 1/3 | if (B==1) PC <= cad10            |
// | 10 11 10nn nnnn nnnn | BNE     cad10         |    | 1/3 | if (Z==0) PC <= cad10            |
// | 10 11 11nn nnnn nnnn | BEQ     cad10         |    | 1/3 | if (Z==1) PC <= cad10            |
// | 11 11 ---- ---- ---- | NOP                   |    |     |                                  |
// |----------------------|-----------------------|----|-----|----------------------------------|
//
// Z flag = indicates when RA==0 (Z only changes when register RA is altered)
// B flag = result of last unsigned subtraction caused a borrow (e.g., X - Y when X < Y)
//
// im14     =   14-bit XR region + offset:  xx00 nnnn nnnn nnnn
// im16     =   16-bit immediate word (word after opcode, PC will skip over)
// reg      =   copper register: RA, RS, RD or PC
// sim10    =   10-bit sign-extended word:  ssss sssn nnnn nnnn
// xreg8    =    8-bit XR reg:              0000 00i0 nnnn nnnn (i=internal copper XR reg)
// cad10    =   10-bit copper address:      1100 00nn nnnn nnnn
//
// Special internal pseudo XR registers:
//
// Register aliases (0-3):    COP_RA, COP_RS, COP_RD, COP_PC
// Pseudo registers (4-7):    COP_HPOS, COP_VPOS, ?, ?
//
// Example copper code:
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
typedef enum logic [3:0] {
    OP_SET  = 4'b0000,  //
    OP_SET1 = 4'b0100,  //
    OP_SET2 = 4'b1000,  //
    OP_SET3 = 4'b1100,  //
    OP_LDI  = 4'b0001,  //
    OP_SUBI = 4'b0101,  //
    OP_NORI = 4'b1001,  //
    OP_STX  = 4'b1101,  //
    OP_LD   = 4'b0010,  //
    OP_SUB  = 4'b0110,  //
    OP_NOR  = 4'b1010,  //
    OP_ST   = 4'b1110,  //
    OP_LRSa = 4'b0011,  //
    OP_SRDa = 4'b0111,  //
    OP_Bcc  = 4'b1011,  //
    OP_NOP  = 4'b1111   //
} copp_opcode_t;

// opcode decode bits
typedef enum logic [3:0] {
    OPBITS_OPCODE        =   4'd12,
    OPBITS_REGNUM        =   4'd10,
    OPBITS_FLAG          =   4'd9   // sim10 sign flag, pseudo xreg flag, RS/RD inc flag
} copp_opbits_t;

// ALU operations
typedef enum logic [1:0] {
    ALU_OP_LOAD         = 2'b00,
    ALU_OP_SUB          = 2'b01,
    ALU_OP_NOR          = 2'b10,
    ALU_OP_NOP          = 2'b11
} copp_alu_ops_t;

// copper registers
typedef enum logic [2:0] {
    REG_RA          = 3'b001,       // general accumulator reg
    REG_RS          = 3'b010,       // source ptr reg (NOTE: only wide enough for copper addr)
    REG_RD          = 3'b011,       // destination ptr reg
    REG_PC          = 3'b000,       // program counter (NOTE: only wide enough for copper addr)
    REG_HPOS        = 3'b100,       // write with STX, wait until HPOS >= value written
    REG_VPOS        = 3'b101,       // write with STX, wait until VPOS == value written (NOTE: equals, possibly in next frame)
    REG_UNUSED_6    = 3'b110,
    REG_UNUSED_7    = 3'b111
} copp_reg_num_t;

// execution state
typedef enum logic [1:0] {
    ST_DECODE       = 2'b00,        // filling pipeline if empty, else decode opcode
    ST_WRITE_IMM    = 2'b01,        // write immediate 16-bit word to XR bus
    ST_LOAD_MEM     = 2'b10,        // load data from copper mem
    ST_EXEC_ALU     = 2'b11         // write ALU result to resister
} copp_ex_state_t;

logic [1:0]     cop_ex_state;       // current execution state
logic           cop_en;             // copper enable/reset
copp_addr_t     cop_init_PC;        // initial PC set via COPP_CTRL

// internal registers xregs
logic           cop_reg_wr_en;
logic [2:0]     cop_reg_wr_num;     // register number for write (includes special registers)
word_t          cop_reg_data_in;

copp_addr_t     ram_rd_addr;
logic           ram_rd_en;

logic           xr_wr_en;
word_t          xr_wr_data_out;
addr_t          xr_wr_addr_out;

assign xr_wr_en_o           = xr_wr_en;
assign xr_wr_addr_o         = xr_wr_addr_out;
assign xr_wr_data_o         = xr_wr_data_out;
assign copmem_rd_en_o       = ram_rd_en;
assign copmem_rd_addr_o     = ram_rd_addr;

logic unused_signals = &{1'b0, copp_reg_data_i[14:10]};

// copper registers
word_t          cop_RA;         // accumulator register
copp_addr_t     cop_RS;         // source pointer register (read copper mem)
word_t          cop_RD;         // desitination pointer register (write XADDR)
copp_addr_t     cop_PC;         // current program counter (copper mem)

// flags
logic           cop_h_wait;     // waiting for >= HPOS
logic           cop_v_wait;     // waiting for == VPOS
logic           cop_B_flag;     // last SUBI/SUB did a borrow (for BGE/BLT)
logic           cop_Z_flag;     // RA == 0

assign          cop_Z_flag = (cop_RA == 0); // update Z flag

// special internal xregs
hres_t          cop_wait_val;   // value to wait for HPOS/VPOS

logic           stalled;
logic           flush_pipe;     // strobe to flush pipeline (PC value altered)
logic [1:0]     IR_pipeline;    // flags for pipelined IR read status
logic           inc_RS;         // strobe to increment RS
logic           inc_RD;         // strobe to increment RD
logic           inc_PC;         // strobe to increment PC

copp_addr_t     cop_PC_plus1;
assign          cop_PC_plus1 = stalled ? cop_PC : cop_PC + 1'b1;   // next PC location

word_t          cop_IR;         // instruction register (used after 1st cycle of opcode)

// register write (and pseudo XR register aliases)
always_ff @(posedge clk) begin
    if (reset_i) begin
        cop_RA          <= '0;
        cop_RS          <= '0;
        cop_RD          <= '0;
        cop_PC          <= '0;
        cop_wait_val    <= '0;
        flush_pipe      <= 1'b0;
        cop_h_wait      <= 1'b0;
        cop_v_wait      <= 1'b0;
    end else begin
        flush_pipe      <= stalled;
        if (inc_RS) begin
            cop_RS          <= cop_RS + 1'b1;
        end
        if (inc_RD) begin
            cop_RD          <= cop_RD + 1'b1;
        end
        if (inc_PC) begin
            cop_PC          <= cop_PC_plus1;
        end
        if (cop_reg_wr_en) begin
            case (cop_reg_wr_num)
                REG_RA: cop_RA      <= cop_reg_data_in;
                REG_RS: cop_RS      <= $bits(cop_RS)'(cop_reg_data_in);
                REG_RD: cop_RD      <= cop_reg_data_in;
                REG_PC: begin
                    flush_pipe      <= 1'b1;
                    cop_PC          <= $bits(cop_PC)'(cop_reg_data_in);
                end
                REG_HPOS: begin
                    stalled         <= 1'b1;
                    cop_h_wait      <= 1'b1;
                    cop_wait_val    <= $bits(cop_wait_val)'(cop_reg_data_in);
                end
                REG_VPOS: begin
                    stalled         <= 1'b1;
                    cop_v_wait      <= 1'b1;
                    cop_wait_val    <= $bits(cop_wait_val)'(cop_reg_data_in);
                end
                REG_UNUSED_6: begin
                end
                REG_UNUSED_7: begin
                end
            endcase
        end

        if (cop_h_wait && h_count_i >= cop_wait_val) begin
            stalled <= 1'b0;
        end

        if (cop_v_wait && v_count_i == $bits(v_count_i)'(cop_wait_val)) begin
            stalled <= 1'b0;
        end
    end
end

// register read (hot, from memory output)
word_t reg_read_data;
always_comb begin
    case (copmem_rd_data_i[OPBITS_REGNUM+:2])
        2'(REG_RA): reg_read_data   = cop_RA;
        2'(REG_RS): reg_read_data   = 16'(cop_RS);
        2'(REG_RD): reg_read_data   = cop_RD;
        2'(REG_PC): reg_read_data   = 16'(cop_PC);
    endcase
end

// ALU (with NOR, so the L is not a lie)
logic [1:0]     alu_op;
word_t          alu_result_out;
logic           alu_borrow_out;
word_t          alu_lhs;
word_t          alu_rhs;

always_comb begin
    alu_borrow_out = cop_B_flag;
    case (alu_op)
        ALU_OP_LOAD:    alu_result_out = alu_rhs;
        ALU_OP_SUB:     { alu_borrow_out, alu_result_out } = { 1'b0, alu_lhs } - { 1'b0, alu_rhs };
        ALU_OP_NOR:     alu_result_out = ~(alu_lhs | alu_rhs);
        ALU_OP_NOP:     alu_result_out = alu_rhs;   // TODO: dummy or zero better?
    endcase
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        cop_en          <= 1'b0;
        cop_init_PC     <= '0;

        inc_RS          <= 1'b0;
        inc_RD          <= 1'b0;
        inc_PC          <= 1'b0;
        cop_B_flag      <= 1'b0;

        ram_rd_en       <= 1'b0;
        xr_wr_en        <= 1'b0;
        xr_wr_addr_out  <= '0;
        xr_wr_data_out  <= '0;

        cop_reg_wr_en   <= 1'b0;
        cop_reg_wr_num  <= '0;
        cop_reg_data_in <= '0;

        alu_op          <= '0;
        alu_lhs         <= '0;
        alu_rhs         <= '0;

        cop_IR          <= '0;
        IR_pipeline     <= '0;
        cop_ex_state    <= ST_DECODE;
    end
    else begin
        inc_RS          <= 1'b0;
        inc_RD          <= 1'b0;
        inc_PC          <= 1'b0;
        ram_rd_en       <= 1'b0;
        cop_reg_wr_en   <= 1'b0;

        // only clear XR write enable when ack'd
        if (xr_wr_ack_i) begin
            xr_wr_en            <= 1'b0;
        end

        // video register write
        if (copp_reg_wr_i) begin
            cop_en           <= copp_reg_data_i[15];
            cop_init_PC      <= xv::COPP_W'(copp_reg_data_i);
        end

        // IR pipeline
        if (flush_pipe) begin
            IR_pipeline <= '0;                          // clear pipeline if PC altered
        end else begin
            IR_pipeline <= { IR_pipeline[0], 1'b0 };    // else shift pipeline
        end

        // Main logic
        if (!cop_en || restart_i) begin
            cop_reg_wr_en   <= 1'b1;                    // internal copper reg write
            cop_reg_wr_num  <= REG_PC;                  // PC
            cop_reg_data_in <= 16'(cop_init_PC);        // set initial PC

            cop_ex_state    <= ST_DECODE;
        end else begin
            case (cop_ex_state)
                // decode instruction "hot" from memory output
                // if there is no instruction just read, keep pipelining PC reads until one ready
                ST_DECODE: begin
                    IR_pipeline[0]  <= 1'b1;                // pipeline next PC read
                    // if instruction ready?
                    if (!IR_pipeline[1]) begin
                        ram_rd_en       <= 1'b1;            // read copper memory
                        ram_rd_addr     <= cop_PC;          // read next PC
                        inc_PC          <= 1'b0;            // increment PC
                    end else begin
                        ram_rd_en       <= 1'b1;            // read copper memory
                        ram_rd_addr     <= cop_PC_plus1;    // read next PC
                        inc_PC          <= 1'b1;            // increment PC

                        // hot decode
                        cop_IR          <= copmem_rd_data_i;
                        cop_reg_wr_num  <= 3'(copmem_rd_data_i[OPBITS_REGNUM+:2]);  // save reg for write
                        alu_op          <= copmem_rd_data_i[15:14];                 // save alu operation
                        alu_lhs         <= reg_read_data;                           // save alu lhs (read from register)
                        alu_rhs         <= { {16-OPBITS_FLAG{copmem_rd_data_i[OPBITS_FLAG]}}, copmem_rd_data_i[OPBITS_FLAG-1:0] };

                        case (copmem_rd_data_i[15:12])
                            OP_SET, OP_SET1, OP_SET2, OP_SET3: begin
                                cop_ex_state    <= ST_WRITE_IMM;
                            end
                            OP_LDI, OP_SUBI, OP_NORI: begin
                                cop_ex_state    <= ST_EXEC_ALU;
                            end
                            OP_STX: begin
                                cop_reg_wr_en   <= copmem_rd_data_i[OPBITS_FLAG];   // internal copper reg write
                                cop_reg_wr_num  <= copmem_rd_data_i[2:0];
                                cop_reg_data_in <= reg_read_data;
                                xr_wr_en        <= !copmem_rd_data_i[OPBITS_FLAG];    // or XR bus write
                                xr_wr_addr_out  <= 16'(copmem_rd_data_i[7:0]);
                                xr_wr_data_out  <= reg_read_data;

                                cop_ex_state    <= ST_DECODE;
                            end
                            OP_LD, OP_SUB, OP_NOR: begin
                                IR_pipeline[0]  <= 1'b0;                    // not reading instruction
                                inc_PC          <= 1'b0;                    // don't increment PC
                                ram_rd_en       <= 1'b1;                    // read copper memory
                                ram_rd_addr     <= copmem_rd_data_i[9:0];   // read cad10 address

                                cop_ex_state    <= ST_LOAD_MEM;
                            end
                            OP_ST: begin
                                xr_wr_en        <= 1'b1;                    // XR bus copper write
                                xr_wr_addr_out  <= { xv::XR_COPPER_ADDR[15:14], 2'b00, copmem_rd_data_i[11:0] };
                                xr_wr_data_out  <= reg_read_data;           // data from register in instruction

                                cop_ex_state    <= ST_DECODE;
                            end
                            OP_LRSa: begin
                                IR_pipeline[0]  <= 1'b0;                    // not reading instruction
                                inc_PC          <= 1'b0;                    // don't increment PC
                                ram_rd_en       <= 1'b1;                    // read copper memory
                                ram_rd_addr     <= cop_RS;                  // read address RS
                                inc_RS          <= copmem_rd_data_i[OPBITS_FLAG];   // optionally increment RS

                                cop_ex_state    <= ST_LOAD_MEM;
                            end
                            OP_SRDa: begin
                                xr_wr_en        <= 1'b1;                    // XR bus copper write
                                xr_wr_addr_out  <= cop_RD;                  // write address RD
                                xr_wr_data_out  <= reg_read_data;           // data from register in instruction
                                inc_RD          <= copmem_rd_data_i[OPBITS_FLAG];   // optionally increment RD

                                cop_ex_state    <= ST_DECODE;
                            end
                            OP_Bcc: begin
                                cop_reg_wr_num  <= 3'b000;
                                cop_reg_data_in <= 16'(copmem_rd_data_i[9:0]);   // read cad10 address
                                if (copmem_rd_data_i[11]) begin             // Z flag for BNE/BEQ
                                    cop_reg_wr_en   <= (copmem_rd_data_i[10] == cop_Z_flag);
                                end else begin                              // B flag for BGE/BLT
                                    cop_reg_wr_en   <= (copmem_rd_data_i[10] == cop_B_flag);
                                end

                                cop_ex_state    <= ST_DECODE;
                            end
                            OP_NOP: begin
                                cop_ex_state    <= ST_DECODE;
                            end
                        endcase
                    end
                end
                // store 16-bit immediate word to XR bus
                ST_WRITE_IMM: begin
                    xr_wr_en            <= 1'b1;                    // write XR bus
                    xr_wr_addr_out      <= cop_IR;                  // use opcode as XR address
                    xr_wr_data_out      <= copmem_rd_data_i;        // pre-read instuction word as data

                    IR_pipeline[0]      <= 1'b1;                    // pipeline next PC read
                    ram_rd_en           <= 1'b1;                    // read copmem
                    ram_rd_addr         <= cop_PC_plus1;            // read next PC
                    inc_PC              <= 1'b1;                    // increment PC

                    cop_ex_state        <= ST_DECODE;
                end
                // load data word from copper memory
                ST_LOAD_MEM: begin
                    IR_pipeline[0]      <= 1'b1;                    // pipeline next PC read
                    ram_rd_en           <= 1'b1;                    // re-read PC
                    ram_rd_addr         <= cop_PC;                  // read current PC (already incremented)

                    alu_rhs             <= copmem_rd_data_i;        // use word read as ALU rhs

                    cop_ex_state        <= ST_EXEC_ALU;
                end
                // store ALU result to register
                ST_EXEC_ALU: begin
                    cop_reg_wr_en       <= 1'b1;                    // internal copper reg write
                    cop_reg_data_in     <= alu_result_out;          // write result from ALU
                    cop_B_flag          <= alu_borrow_out;          // save borrow flag from ALU

                    IR_pipeline[0]      <= 1'b1;                    // pipeline next PC read
                    ram_rd_en           <= 1'b1;                    // read copmem
                    ram_rd_addr         <= cop_PC_plus1;            // read next PC
                    inc_PC              <= 1'b1;                    // increment PC

                    cop_ex_state        <= ST_DECODE;
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
