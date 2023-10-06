#pragma once

#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "xlasm.h"

struct copper : public Ixlarch
{
    const char *      variant_names() override;
    bool              set_variant(std::string name) override;
    const std::string get_variant() override;
    void              reset(xlasm * xl) override;
    void              activate(xlasm * xl) override;
    void              deactivate(xlasm * xl) override;
    int32_t           lookup_register(const std::string & opcode) override;
    int32_t           check_opcode(const std::string & opcode) override;
    int32_t           process_opcode(xlasm *                          xl,
                                     int32_t                          idx,
                                     std::string &                    opcode,
                                     size_t                           cur_token,
                                     const std::vector<std::string> & tokens) override;
    uint32_t          check_directive(const std::string & directive) override;
    int32_t           process_directive(xlasm *                          xl,
                                        uint32_t                         idx,
                                        const std::string &              directive,
                                        const std::string &              label,
                                        size_t                           cur_token,
                                        const std::vector<std::string> & tokens) override;


    bool support_dollar_hex() override
    {
        return true;
    }
    bool column_one_labels() override
    {
        return true;
    }
    uint32_t max_bit_width() override
    {
        return 16;
    }
    bool is_big_endian() override
    {
        return true;
    }

    uint32_t code_alignment() override
    {
        assert(false);

        return 2;
    }

    uint32_t data_alignment(size_t size) override
    {
        (void)size;

        return 2;
    }


    copper() noexcept;
    ~copper() override
    {
    }

    enum
    {
        SLIMCOPPER
    };

    //  Slim Copper opcodes:
    //
    // | XR Op Immediate     | Assembly             |Flag | Cyc | Description                      |
    // |---------------------|----------------------|-----|-----|----------------------------------|
    // | rr00 oooo oooo oooo | SETI   xadr14,#val16 |  B  |  4  | dest [xadr14] <= source #val16   |
    // | iiii iiii iiii iiii |    <im16 value>      |     |     |   (2 word op)                    |
    // | --01 rccc cccc cccc | SETM  xadr16,cadr11  |  B  |  5  | dest [xadr16] <= source [cadr11] |
    // | rroo oooo oooo oooo |    <xadr16 address>  |     |     |   (2 word op)                    |
    // | --10 0iii iiii iiii | HPOS   #im11         |     |  5+ | wait until video HPOS >= im11    |
    // | --10 1iii iiii iiii | VPOS   #im11         |     |  5+ | wait until video VPOS >= im11    |
    // | --11 0ccc cccc cccc | BRGE   cadr10        |     |  4  | if (B==0) PC <= cadr10           |
    // | --11 1ccc cccc cccc | BRLT   cadr10        |     |  4  | if (B==1) PC <= cadr10           |
    // |---------------------|----------------------|-----|-----|----------------------------------|
    //
    // xadr14   =   XR region + 12-bit offset           xx00 oooo oooo oooo (1st word, SETI dest)
    // im16     =   16-bit immediate word               iiii iiii iiii iiii (2nd word, SETI source)
    // cadr11   =   10-bit copper address + register    ---- rnnn nnnn nnnn (1st word, SETM source)
    // xadr16   =   XR region + 14-bit offset           rroo oooo oooo oooo (2nd word, SETM dest)
    // im11     =   11-bit immediate value              ---- -iii iiii iiii (HPOS, VPOS)
    // cadr10   =   10-bit copper address/register      ---- -nnn nnnn nnnn (BRGE, BRLT)
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

    enum op_t
    {
        OP_SETI,
        OP_MOVI,
        OP_SETM,
        OP_MOVM,
        OP_HPOS,
        OP_VPOS,
        OP_BRGE,
        OP_BRLT,
        // pseudo-ops
        OP_LDI,
        OP_LDM,
        OP_STM,
        OP_CLRB,
        OP_SUBI,
        OP_ADDI,
        OP_SUBM,
        OP_CMPI,
        OP_CMPM,
        OP_MOVE
    };

    // copper operand types
    enum operand
    {
        N,            // none
        IM11,         // 16-bit immediate
        IM16,         // 16-bit immediate
        NIM16,        // negated 16-bit immediate
        CM,           // copper memory address
        XM14,         // XR memory address w/12-bit offset
        XM16,         // XR memory address w/14-bit offset
        MS,           // MOVE source
        MD            // MOVE dest
    };

    enum reg_addr
    {
        RA     = 0x800,
        RA_SUB = 0x801,
        RA_CMP = 0x7FF
    };

    struct op_tbl
    {
        op_t         op_idx;
        uint16_t     bits;
        uint16_t     mask;
        const char * name;
        operand      a[2];
        int32_t      len;        // negative for "worse case" (might be less from SQUEEZE pass)
        uint32_t     flags;
        uint32_t     cyc;
    };

    struct reg_tbl
    {
        const char * name;
        uint32_t     val;
    };

    typedef std::unordered_map<std::string, uint32_t> opcode_map_t;
    typedef std::unordered_map<std::string, uint32_t> directive_map_t;

    static directive_map_t directives;
    static opcode_map_t    opcodes;

    static constexpr xlasm::directive_t directives_list[] = {{"WORD", xlasm::DIR_DEF_16}, {"DW", xlasm::DIR_DEF_16}};

    static constexpr op_tbl ops[] = {{OP_SETI, 0x0000, 0x3000, "SETI", {XM14, IM16}, 2, 0, 4},
                                     {OP_MOVI, 0x0000, 0x3000, "MOVI", {IM16, XM14}, 2, 0, 4},
                                     {OP_SETM, 0x1000, 0x3000, "SETM", {XM16, CM}, 2, 0, 4},
                                     {OP_MOVM, 0x1000, 0x3000, "MOVM", {CM, XM16}, 2, 0, 4},
                                     {OP_HPOS, 0x2000, 0x3800, "HPOS", {IM11}, 1, 0, 5},
                                     {OP_VPOS, 0x2800, 0x3800, "VPOS", {IM11}, 1, 0, 5},
                                     {OP_BRGE, 0x3000, 0x3800, "BRGE", {CM}, 1, 0, 4},
                                     {OP_BRLT, 0x3800, 0x3800, "BRLT", {CM}, 1, 0, 4},
                                     {OP_LDI, 0x0800, 0x3FFF, "LDI", {IM16}, 2, 0, 4},
                                     {OP_LDM, 0x1000, 0x3000, "LDM", {CM}, 2, 0, 4},
                                     {OP_STM, 0x1000, 0x3000, "STM", {XM16}, 2, 0, 4},
                                     {OP_CLRB, 0x1800, 0x3800, "CLRB", {}, 2, 0, 4},
                                     {OP_SUBI, 0x0801, 0x3FFF, "SUBI", {IM16}, 2, 0, 4},
                                     {OP_ADDI, 0x0801, 0x3FFF, "ADDI", {NIM16}, 2, 0, 4},
                                     {OP_SUBM, 0x1000, 0x3000, "SUBM", {CM}, 2, 0, 4},
                                     {OP_CMPI, 0x07FF, 0x3FFF, "CMPI", {IM16}, 2, 0, 4},
                                     {OP_CMPM, 0x1000, 0x3000, "CMPM", {CM}, 2, 0, 4},
                                     {OP_MOVE, 0x0000, 0x0000, "MOVE", {MS, MD}, 2, 0, 4}};
};
