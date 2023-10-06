// xlasmcopper.cpp

#include <algorithm>
#include <assert.h>

#include "xlasm.h"
#include "xlasmcopper.h"
#include "xlasmexpr.h"

constexpr copper::op_tbl     copper::ops[];
constexpr xlasm::directive_t copper::directives_list[];

copper::directive_map_t copper::directives;
copper::opcode_map_t    copper::opcodes;

copper::copper() noexcept
{
    register_arch(this);

    std::string n;
    n.reserve(64);

    if (!directives.size())
    {
        for (const auto & d : directives_list)
        {
            n             = d.name;
            directives[n] = d.index;
        }
    }

    if (!opcodes.size())
    {
        for (const auto & o : ops)
        {
            if (o.name == nullptr)
                continue;
            n          = o.name;
            opcodes[n] = o.op_idx;
        }
    }
}

const char * copper::variant_names()
{
    return "Xosera Slim Copper\n    \"copper\"";
}

bool copper::set_variant(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), uppercase);
    if (name == "COPPER")
        return true;

    return false;
}

const std::string copper::get_variant()
{
    return "copper";
}

void copper::reset(xlasm * xl)
{
    xl->sections["text"].load_addr = 0xC000;
    xl->sections["text"].addr      = 0xC000;

    xl->add_sym("true", xlasm::symbol_t::LABEL, 1);
    xl->add_sym("TRUE", xlasm::symbol_t::LABEL, 1);
    xl->add_sym("false", xlasm::symbol_t::LABEL, 0);
    xl->add_sym("FALSE", xlasm::symbol_t::LABEL, 0);

    xl->add_sym("RA", xlasm::symbol_t::LABEL, 0x800);
    xl->add_sym("RA_SUB", xlasm::symbol_t::LABEL, 0x801);
    xl->add_sym("RA_CMP", xlasm::symbol_t::LABEL, 0x7FF);

    xl->add_sym("SETI", xlasm::symbol_t::LABEL, 0x0000);
    xl->add_sym("MOVI", xlasm::symbol_t::LABEL, 0x0000);
    xl->add_sym("LDI", xlasm::symbol_t::LABEL, 0x0000);
    xl->add_sym("SETM", xlasm::symbol_t::LABEL, 0x1000);
    xl->add_sym("MOVM", xlasm::symbol_t::LABEL, 0x1000);
    xl->add_sym("LDM", xlasm::symbol_t::LABEL, 0x1000);
    xl->add_sym("STM", xlasm::symbol_t::LABEL, 0x1000);
    xl->add_sym("HPOS", xlasm::symbol_t::LABEL, 0x2000);
    xl->add_sym("VPOS", xlasm::symbol_t::LABEL, 0x2800);
    xl->add_sym("BRGE", xlasm::symbol_t::LABEL, 0x3000);
    xl->add_sym("BRLT", xlasm::symbol_t::LABEL, 0x3800);

    xl->add_sym("H_EOL", xlasm::symbol_t::LABEL, 0x7FF);
    xl->add_sym("V_EOF", xlasm::symbol_t::LABEL, 0x3FF);
    xl->add_sym("V_WAITBLIT", xlasm::symbol_t::LABEL, 0x7FF);
}

void copper::activate(xlasm * xl)
{
    (void)xl;
}

void copper::deactivate(xlasm * xl)
{
    (void)xl;
}

// return directive_index or xlasm::DIR_UNKNOWN if not recognized
uint32_t copper::check_directive(const std::string & directive)
{
    uint32_t index = xlasm::DIR_UNKNOWN;
    auto     it    = directives.find(directive);

    if (it != directives.end())
    {
        index = it->second;
    }

    return index;
}

int32_t copper::process_directive(xlasm *                          xl,
                                  uint32_t                         idx,
                                  const std::string &              directive,
                                  const std::string &              label,
                                  size_t                           cur_token,
                                  const std::vector<std::string> & tokens)
{
    (void)xl;
    (void)idx;
    (void)directive;
    (void)label;
    (void)cur_token;
    (void)tokens;

    assert(false);

    return 0;
}

// return opcode index or -1 if not recognized
int32_t copper::check_opcode(const std::string & opcode)
{
    int32_t index = -1;
    auto    it    = opcodes.find(opcode);

    if (it != opcodes.end())
    {
        index = static_cast<int32_t>(it->second);
    }

    return index;
}

int32_t copper::lookup_register(const std::string & n)
{
    (void)n;
    return -1;
}

int32_t copper::process_opcode(xlasm *                          xl,
                               int32_t                          idx,
                               std::string &                    opcode,
                               size_t                           cur_token,
                               const std::vector<std::string> & tokens)
{
    // make keyword uppercase to stand out in error messages
    std::transform(opcode.begin(), opcode.end(), opcode.begin(), uppercase);

    int64_t PC = xl->ctxt.section->addr + static_cast<int64_t>(xl->ctxt.section->data.size());

    if (PC & 1)
    {
        xl->error("Copper code generated at odd address " PR_X64_04 "", PC);
    }

    std::string operstr;

    // dis-ambiguate opcode based on operands

    uint32_t opval     = ops[idx].bits;
    uint32_t opmask    = ops[idx].mask;
    uint16_t word0_val = 0;
    uint16_t word1_val = 0;
    int64_t  result    = 0;
    int      oper_num  = 0;        // operands (not including opcode)
    bool     move_imm  = false;
    operstr.clear();

    for (auto it = tokens.begin() + static_cast<int>(cur_token);
         it != tokens.end() && oper_num < 2 && ops[idx].a[oper_num] != N;
         ++it, cur_token++)
    {
        result = 0;

        if (*it != ",")
        {
            //			if (operstr.size())
            //				operstr += " ";

            operstr += *it;
        }

        if (*it == "," || it + 1 == tokens.end())
        {
            operand operand_type = ops[idx].a[oper_num];

            //            dprintf("oper[%d]=%s\n", oper_num, operstr.c_str());

            switch (operand_type)
            {
                case N:
                    break;

                // copper HPOS/VPOS position
                case IM11: {
                    expression  expr;
                    std::string exprstr;

                    if (operstr[0] != '#')
                    {
                        xl->error("Immediate operand expected for opcode %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  operstr.c_str());
                        break;
                    }
                    exprstr.assign(operstr.begin() + 1, operstr.end());
                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error("Immediate operand expected for opcode %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  operstr.c_str());
                        break;
                    }

                    if ((result >> 11) == 0 || (result >> 11) == -1)
                    {
                        result &= (1 << 11) - 1;
                    }

                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 11, 2);
                    word0_val = static_cast<uint16_t>(result & 0x7ff);
                }
                break;

                // SETI 2nd operand
                case IM16: {
                    expression  expr;
                    std::string exprstr;

                    if (operstr[0] != '#')
                    {
                        xl->error("Immediate operand expected for opcode %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  operstr.c_str());
                        break;
                    }
                    exprstr.assign(operstr.begin() + 1, operstr.end());

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error(
                            "Immediate expected for opcode %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }

                    if ((result >> 16) == 0 || (result >> 16) == -1)
                    {
                        result &= (1 << 16) - 1;
                    }

                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 16, 2);
                    word1_val = static_cast<uint16_t>(result & 0xffff);
                }
                break;

                // ADDI  operand
                case NIM16: {
                    expression  expr;
                    std::string exprstr;

                    if (operstr[0] != '#')
                    {
                        xl->error("Immediate operand expected for opcode %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  operstr.c_str());
                        break;
                    }
                    exprstr.assign(operstr.begin() + 1, operstr.end());

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error(
                            "Immediate expected for opcode %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }

                    result = -result;
                    if (result > -32768 && result < 32768)
                    {
                        result &= 0xffff;
                    }
                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 16, 2);
                    word1_val = static_cast<uint16_t>(result & 0xffff);
                }
                break;

                // copper address
                case CM: {
                    expression  expr;
                    std::string exprstr;

                    exprstr.assign(operstr.begin(), operstr.end());
                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error("copper address operand expected for opcode %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  exprstr.c_str());
                        break;
                    }

                    uint16_t xrbits = result & 0xC000;
                    uint16_t regbit = result & RA;
                    if (regbit)
                    {
                        if ((result & 0x7FE) != 0)
                        {
                            xl->warning("unknown register bits in copper address, 0x" PR_X64_04 ", will be ignored",
                                        result);
                        }
                    }
                    else if (xrbits != 0xC000)
                    {
                        xl->warning("copper XR region 0xC000 not set in 0x" PR_X64_04 ", will be assumed", result);
                    }
                    result &= 0x37FF;
                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 11, 2);
                    word0_val = static_cast<uint16_t>(result & 0x7ff);
                    word0_val |= 0xC000 | regbit;        // set copper XR region
                }
                break;

                // SETI 1st operand
                case XM14: {
                    expression  expr;
                    std::string exprstr;
                    exprstr.assign(operstr);

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error(
                            "address expected for instruction %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }
                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 16, 2);
                    word0_val = static_cast<uint16_t>(result & 0xffff);
                    if ((word0_val & 0x3000) != 0x0000)
                    {
                        xl->error("XR address offset is over 12-bits for instruction %s (evaluating \"%s\")",
                                  opcode.c_str(),
                                  exprstr.c_str());
                        break;
                    }
                }
                break;

                // SETM 2nd operand
                case XM16: {
                    expression  expr;
                    std::string exprstr;
                    exprstr.assign(operstr);

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error(
                            "address expected for instruction %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }

                    if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                        xl->check_truncation_unsigned(opcode, result, 16, 2);
                    word1_val = static_cast<uint16_t>(result & 0xffff);
                }
                break;

                // MOVE 1nd operand
                case MS: {
                    expression  expr;
                    std::string exprstr;

                    if (operstr[0] == '#')
                    {
                        move_imm = true;
                        exprstr.assign(operstr.begin() + 1, operstr.end());
                    }
                    else
                    {
                        exprstr.assign(operstr);
                    }

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error("Source expected for opcode %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }

                    if (move_imm)
                    {
                        if ((result >> 16) == 0 || (result >> 16) == -1)
                        {
                            result &= (1 << 16) - 1;
                        }

                        if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                            xl->check_truncation_unsigned(opcode, result, 16, 2);

                        word1_val = static_cast<uint16_t>(result & 0xffff);
                    }
                    else
                    {
                        uint16_t xrbits = result & 0xC000;
                        uint16_t regbit = result & RA;
                        if (regbit)
                        {
                            if ((result & 0x7FE) != 0)
                            {
                                xl->warning("unknown register bits in copper address, 0x" PR_X64_04 ", will be ignored",
                                            result);
                            }
                        }
                        else if (xrbits != 0xC000)
                        {
                            xl->warning("copper XR region 0xC000 not set in 0x" PR_X64_04 ", will be assumed", result);
                        }
                        result &= 0x37FF;

                        if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                            xl->check_truncation_unsigned(opcode, result, 11, 2);

                        word0_val = static_cast<uint16_t>(result & 0x7ff) | 0xD000 | regbit;
                    }
                }
                break;

                // MOVE 1nd operand
                case MD: {
                    expression  expr;
                    std::string exprstr;

                    exprstr.assign(operstr);

                    if (!exprstr.size() || !expr.evaluate(xl, exprstr.c_str(), &result))
                    {
                        xl->error(
                            "Immediate expected for opcode %s (evaluating \"%s\")", opcode.c_str(), operstr.c_str());
                        break;
                    }

                    if ((result >> 16) == 0 || (result >> 16) == -1)
                    {
                        result &= (1 << 16) - 1;
                    }

                    if (move_imm)
                    {
                        if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                            xl->check_truncation_unsigned(opcode, result, 16, 2);
                        word0_val = static_cast<uint16_t>(result & 0xffff);
                        if ((word0_val & 0x3000) != 0x0000)
                        {
                            xl->error("XR address offset is over 12-bits for instruction %s (evaluating \"%s\")",
                                      opcode.c_str(),
                                      exprstr.c_str());
                            break;
                        }
                    }
                    else
                    {
                        word1_val = static_cast<uint16_t>(result & 0xffff);
                    }
                }
                break;

                default:
                    assert(false);
                    break;
            }

            operstr.clear();
            oper_num++;
        }
    }

    // special case addresses
    switch (idx)
    {
        case OP_LDM:
            word1_val = RA;
            break;
        case OP_STM:
            word0_val = RA;
            break;
        case OP_CLRB:
            word0_val = RA;
            word1_val = RA;
            break;
        case OP_SUBM:
            word1_val = RA_SUB;
            break;
        case OP_CMPM:
            word1_val = RA_CMP;
            break;
        default:
            break;
    }

    if (oper_num < 2 && ops[idx].a[oper_num] != N)
    {
        xl->error("Missing required operand #%d for instruction %s", oper_num + 1, opcode.c_str());
    }
    if (tokens.size() != cur_token)
    {
        xl->error("Unexpected additional operand(s) for instruction %s", opcode.c_str());
    }

    xl->emit(static_cast<uint16_t>((opval & opmask) | (word0_val & ~opmask)));
    if (ops[idx].len == 2)
    {
        xl->emit(static_cast<uint16_t>(word1_val));
    }

    return 0;
}
