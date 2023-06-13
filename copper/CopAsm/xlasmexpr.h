// xlasexpr.h - modified "shunt" expression parser
// Supports most C operators and precedence.

/* The authors of this work have released all rights to it and placed it
in the public domain under the Creative Commons CC0 1.0 waiver
(http://creativecommons.org/publicdomain/zero/1.0/).

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Retrieved from: http://en.literateprograms.org/Shunting_yard_algorithm_(C)?oldid=18970
*/

// Hacked significantly by Xark - so blame him for any problems. :-)

#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "xlasm.h"

#if 0
#define exp_dprintf(x, ...) printf(x, ##__VA_ARGS__)
#else
#define exp_dprintf(...) (void)0
#endif

class expression
{
    enum
    {
        MAXOPSTACK  = 64,
        MAXNUMSTACK = 64
    };

    enum
    {
        ASSOC_NONE = 0,
        ASSOC_LEFT,
        ASSOC_RIGHT
    };

    enum op_t
    {
        OP_UMINUS,
        OP_UPLUS,
        OP_UNOT,
        OP_LOG_UNOT,
        OP_UHIGHW,
        OP_ULOWW,
        OP_EXPONENT,
        OP_MULTIPLY,
        OP_DIVIDE,
        OP_MODULO,
        OP_ADD,
        OP_SUB,
        OP_SHL,
        OP_SHR,
        OP_AND,
        OP_OR,
        OP_XOR,
        OP_LOG_AND,
        OP_LOG_OR,
        OP_LOG_EQ,
        OP_LOG_NEQ,
        OP_LOG_LT,
        OP_LOG_LTE,
        OP_LOG_GT,
        OP_LOG_GTE,
        OP_TERNARY,
        OP_LPAREN,
        OP_RPAREN,
        NUM_OPS,
        OP_DUMMY
    };

    struct op_s
    {
        const char * op_str;
        uint32_t     str_len;
        op_t         op;
        int64_t      prec;
        int64_t      assoc;
        int64_t      unary;
        int64_t (*eval)(expression * exp, int64_t a1, int64_t a2);
    };

    xlasm *             xl;
    const struct op_s * opstack[MAXOPSTACK];
    int64_t             numstack[MAXNUMSTACK];
    int32_t             nopstack;
    int32_t             nnumstack;
    int32_t             brace_balance;
    int32_t             errorcode;

public:
    void eval_error(int error, const char * fmt, ...) ATTRIBUTE((format(printf, 3, 4)))
    {
        char experr[4096] = {0};

        errorcode = error;

        if (xl->ctxt.pass != xlasm::context_t::PASS_2 || !xl->ctxt.file)
            return;

        va_list args;
        va_start(args, fmt);
        snprintf(experr, sizeof(experr), "E%03X: ", errorcode);
        vsnprintf(experr + strlen(experr), sizeof(experr) - strlen(experr), fmt, args);
        va_end(args);

        xl->error("%s", experr);
    }

    void eval_error2(int error, const char * fmt, ...) ATTRIBUTE((format(printf, 3, 4)))
    {
        char experr[4096] = {0};

        errorcode = error;

        if (!xl->ctxt.file)
            return;

        va_list args;
        va_start(args, fmt);
        snprintf(experr, sizeof(experr), "E%03X: ", errorcode);
        vsnprintf(experr + strlen(experr), sizeof(experr) - strlen(experr), fmt, args);
        va_end(args);

        xl->error("%s", experr);
    }

private:
    static int64_t eval_uminus(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("- %lld = %lld\n", a1, -a1);
        return -a1;
    }
    static int64_t eval_uplus(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("+ %lld = %lld\n", a1, +a1);
        return +a1;
    }
    static int64_t eval_unot(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("~ %lld = %lld\n", a1, ~a1);
        return ~a1;
    }
    static int64_t eval_ulognot(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("! %lld = %lld\n", a1, (int64_t)(!a1));
        return !a1;
    }
    static int64_t eval_uhighw(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("highw %lld = %lld\n", a1, (a1 >> 16) & 0xffff);
        return (a1 >> 16) & 0xffff;
    }
    static int64_t eval_uloww(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("loww %lld = %lld\n", a1, a1 & 0xffff);
        return a1 & 0xffff;
    }

    static int64_t eval_uhi(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("hi %lld = %lld\n", a1, ((a1 + 0x800) >> 12) & 0xfffff);
        return ((a1 + 0x800) >> 12) & 0xfffff;
    }
    static int64_t eval_ulo(expression *, int64_t a1, int64_t)
    {
        exp_dprintf("lo %lld = %lld\n", a1, (a1 - (((a1 + 0x800) >> 12) << 12)) & 0xfff);
        return (a1 - (((a1 + 0x800) >> 12) << 12)) & 0xfff;
    }

    static int64_t eval_exp(expression * exp, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld ** %lld = %lld\n", a1, a2, a2 < 0 ? 0 : (a2 == 0 ? 1 : a1 * eval_exp(exp, a1, a2 - 1)));
        return a2 < 0 ? 0 : (a2 == 0 ? 1 : a1 * eval_exp(exp, a1, a2 - 1));
    }
    static int64_t eval_mul(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld * %lld = %lld\n", a1, a2, a1 * a2);
        return a1 * a2;
    }

    static int64_t eval_add(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld = %lld\n", a1, a2, a1 + a2);
        return a1 + a2;
    }
    static int64_t eval_sub(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld = %lld\n", a1, a2, a1 - a2);
        return a1 - a2;
    }

    static int64_t eval_shl(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld << %lld = %lld\n", a1, a2, a1 << a2);
        return a1 << a2;
    }
    static int64_t eval_shr(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld >> %lld = %lld\n", a1, a2, a1 >> a2);
        return a1 >> a2;
    }

    static int64_t eval_eq(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld == %lld = %lld\n", a1, a2, (int64_t)(a1 == a2));
        return a1 == a2;
    }
    static int64_t eval_neq(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld != %lld\n", a1, a2, (int64_t)(a1 != a2));
        return a1 != a2;
    }

    static int64_t eval_lt(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld < %lld = %lld\n", a1, a2, (int64_t)(a1 < a2));
        return a1 < a2;
    }
    static int64_t eval_lte(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld <= %lld\n", a1, a2, (int64_t)(a1 <= a2));
        return a1 <= a2;
    }

    static int64_t eval_gt(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld > %lld = %lld\n", a1, a2, (int64_t)(a1 > a2));
        return a1 > a2;
    }
    static int64_t eval_gte(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld >= %lld\n", a1, a2, (int64_t)(a1 >= a2));
        return a1 >= a2;
    }

    static int64_t eval_and(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld & %lld\n", a1, a2, a1 & a2);
        return a1 & a2;
    }
    static int64_t eval_or(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld | %lld\n", a1, a2, a1 | a2);
        return a1 | a2;
    }
    static int64_t eval_xor(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld ^ %lld\n", a1, a2, a1 ^ a2);
        return a1 ^ a2;
    }

    static int64_t eval_logand(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld && %lld\n", a1, a2, (int64_t)(a1 && a2));
        return a1 && a2;
    }
    static int64_t eval_logor(expression *, int64_t a1, int64_t a2)
    {
        exp_dprintf("%lld + %lld || %lld\n", a1, a2, (int64_t)(a1 || a2));
        return a1 || a2;
    }

    static int64_t eval_upcrelhi(expression * exp, int64_t a1, int64_t)
    {
        int64_t  pc   = exp->xl->symbol_value(exp->xl, ".");
        uint32_t hi20 = static_cast<uint32_t>(((a1 - pc + 0x800) >> 12) & 0x000fffff);
        exp_dprintf("parsed sym '%s' v = 0x%llx\n", ".", pc);
        exp_dprintf("hi20 = 0x%08x\n", hi20);
        return hi20;
    }

    static int64_t eval_upcrello(expression * exp, int64_t a1, int64_t)
    {
        int64_t  pc   = exp->xl->symbol_value(exp->xl, ".");
        uint32_t hi20 = static_cast<uint32_t>(((a1 - pc + 0x800) >> 12) & 0x000fffff);
        uint32_t lo12 = static_cast<uint32_t>((a1 - pc - hi20) & 0xfff);
        exp_dprintf("parsed sym '%s' v = 0x%llx\n", ".", pc);
        exp_dprintf("hi20 = 0x%08x\n", hi20);
        exp_dprintf("lo12 = 0x%08x\n", lo12);
        return lo12;
    }

    static int64_t eval_cond(expression *, int64_t a1, int64_t a2, int64_t a3)
    {
        exp_dprintf("%lld ? %lld : %lld = %lld\n", a1, a2, a3, a1 ? a2 : a3);
        return a1 ? a2 : a3;
    }

    static int64_t eval_div(expression * exp, int64_t a1, int64_t a2)
    {
        if (!a2)
        {
            exp->eval_error(0x100, "Division by zero");
            return 0;
        }
        exp_dprintf("%lld / %lld = %lld\n", a1, a2, a1 / a2);
        return a1 / a2;
    }

    static int64_t eval_mod(expression * exp, int64_t a1, int64_t a2)
    {
        if (!a2)
        {
            exp->eval_error(0x101, "Modulo by zero");
            return 0;
        }
        exp_dprintf("%lld %% %lld = %lld\n", a1, a2, a1 % a2);
        return a1 % a2;
    }


    static constexpr op_s ops[] = {
        {"u-", 2, OP_UMINUS, 100, ASSOC_RIGHT, 1, eval_uminus},
        {"u+", 2, OP_UPLUS, 100, ASSOC_RIGHT, 1, eval_uplus},

        {"!", 1, OP_LOG_UNOT, 99, ASSOC_RIGHT, 1, eval_ulognot},
        {"~", 1, OP_UNOT, 99, ASSOC_RIGHT, 1, eval_unot},

        {".highw", 6, OP_UHIGHW, 98, ASSOC_RIGHT, 1, eval_uhighw},
        {".loww", 5, OP_ULOWW, 98, ASSOC_RIGHT, 1, eval_uloww},

        {"%hi", 3, OP_UHIGHW, 98, ASSOC_RIGHT, 1, eval_uhi},
        {"%lo", 3, OP_ULOWW, 98, ASSOC_RIGHT, 1, eval_ulo},

        {"%pcrel_hi", 9, OP_UHIGHW, 98, ASSOC_RIGHT, 1, eval_upcrelhi},
        {"%pcrel_lo", 9, OP_ULOWW, 98, ASSOC_RIGHT, 1, eval_upcrello},

        {"**", 2, OP_EXPONENT, 90, ASSOC_RIGHT, 2, eval_exp},

        {"*", 1, OP_MULTIPLY, 80, ASSOC_LEFT, 2, eval_mul},
        {"/", 1, OP_DIVIDE, 80, ASSOC_LEFT, 2, eval_div},
        {"%", 1, OP_MODULO, 80, ASSOC_LEFT, 2, eval_mod},

        {"+", 1, OP_ADD, 50, ASSOC_LEFT, 2, eval_add},
        {"-", 1, OP_SUB, 50, ASSOC_LEFT, 2, eval_sub},

        {"<<", 2, OP_SHL, 49, ASSOC_LEFT, 2, eval_shl},
        {">>", 2, OP_SHR, 49, ASSOC_LEFT, 2, eval_shr},

        {"<=", 2, OP_LOG_LTE, 49, ASSOC_LEFT, 2, eval_lte},
        {"<", 1, OP_LOG_LT, 49, ASSOC_LEFT, 2, eval_lt},

        {">=", 2, OP_LOG_GTE, 49, ASSOC_LEFT, 2, eval_gte},
        {">", 1, OP_LOG_GT, 49, ASSOC_LEFT, 2, eval_gt},

        {"==", 2, OP_LOG_EQ, 48, ASSOC_LEFT, 2, eval_eq},
        {"!=", 2, OP_LOG_NEQ, 48, ASSOC_LEFT, 2, eval_neq},

        {"&&", 2, OP_LOG_AND, 44, ASSOC_LEFT, 2, eval_logand},

        {"&", 1, OP_AND, 47, ASSOC_LEFT, 2, eval_and},

        {"^", 1, OP_XOR, 46, ASSOC_LEFT, 2, eval_xor},

        {"||", 2, OP_LOG_OR, 43, ASSOC_LEFT, 2, eval_logor},

        {"|", 1, OP_OR, 45, ASSOC_LEFT, 2, eval_or},

        {"?", 1, OP_TERNARY, 40, ASSOC_RIGHT, 3, nullptr},        // special case for ternary (HACK: also adds LPAREN)

        {"(", 1, OP_LPAREN, 0, ASSOC_NONE, 0, nullptr},
        {")", 1, OP_RPAREN, 0, ASSOC_NONE, 0, nullptr},
    };


    const op_s * getop(const char *& chptr)
    {
        // HACK: To keep ternary operator (?:) working correctly, an invisible is added LPAREN after '?'
        //       this hack makes the ':' act as the matching RPAREN.
        if (chptr[0] == ':')
        {
            const op_s * op = &ops[(sizeof(ops) / sizeof(ops[0])) - 1];        // RPAREN
            assert(op->op == OP_RPAREN);

            return op;
        }

        // don't search unary -/+
        for (uint32_t i = 2; i < (sizeof(ops) / sizeof(ops[0])); ++i)
        {
            assert(strlen(ops[i].op_str) == ops[i].str_len);
            if (strncmp(ops[i].op_str, chptr, ops[i].str_len) == 0)
            {
                if (ops[i].op != OP_LOG_UNOT || chptr[1] != '=')        // special case ! vs !=
                {
                    exp_dprintf("getopt = operator %s (from %s)\n", ops[i].op_str, chptr);
                    chptr += ops[i].str_len - 1;
                    return ops + i;
                }
            }
        }
        exp_dprintf("getopt = fail operator %s\n", chptr);

        return nullptr;
    }

    void push_opstack(const struct op_s * op)
    {
        if (nopstack > MAXOPSTACK - 1)
        {
            eval_error2(0x103, "Operator stack overflow");
            nopstack--;
        }
        opstack[nopstack++] = op;
    }

    const struct op_s * pop_opstack()
    {
        if (!nopstack)
        {
            //			eval_error(0x104, "Operator stack empty");
            nopstack++;
            opstack[0] = nullptr;
        }
        return opstack[--nopstack];
    }

    void push_numstack(int64_t num)
    {
        if (nnumstack > MAXNUMSTACK - 1)
        {
            eval_error2(0x105, "Number stack overflow");
            nnumstack--;
        }
        numstack[nnumstack++] = num;
    }

    int64_t pop_numstack()
    {
        if (!nnumstack)
        {
            eval_error2(0x106, "Syntax error, not enough arguments");
            ++nnumstack;
            numstack[0] = 0;
        }
        return numstack[--nnumstack];
    }

    void shunt_op(const struct op_s * op)
    {
        const struct op_s * pop;
        int64_t             n1, n2, n3;

        exp_dprintf("operator %s\n", op->op_str);

        if (op->op == OP_LPAREN)
        {
            brace_balance++;
            push_opstack(op);
            return;
        }
        else if (op->op == OP_RPAREN)
        {
            brace_balance--;
            while (nopstack > 0 && opstack[nopstack - 1]->op != OP_LPAREN)
            {
                pop = pop_opstack();
                if (!pop)
                    return;

                if (pop->unary == 1)
                {
                    n1 = pop_numstack();
                    push_numstack(pop->eval(this, n1, 0));
                }
                else if (pop->unary == 2)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    push_numstack(pop->eval(this, n2, n1));
                }
                else if (pop->unary == 3)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    n3 = pop_numstack();
                    push_numstack(eval_cond(this, n3, n2, n1));
                }
            }

            pop = pop_opstack();
            if (!pop || pop->op != OP_LPAREN)
            {
                eval_error2(0x107, "Closing parenthesis ')' with no opening '('");
            }
            return;
        }

        if (op->assoc == ASSOC_RIGHT)
        {
            while (nopstack && op->prec < opstack[nopstack - 1]->prec)
            {
                pop = pop_opstack();
                if (!pop)
                    return;

                if (pop->unary == 1)
                {
                    n1 = pop_numstack();
                    push_numstack(pop->eval(this, n1, 0));
                }
                else if (pop->unary == 2)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    push_numstack(pop->eval(this, n2, n1));
                }
                else if (pop->unary == 3)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    n3 = pop_numstack();
                    push_numstack(eval_cond(this, n3, n2, n1));
                }
            }
        }
        else if (op->assoc == ASSOC_LEFT)
        {
            while (nopstack && op->prec <= opstack[nopstack - 1]->prec)
            {
                pop = pop_opstack();
                if (!pop)
                    return;

                if (pop->unary == 1)
                {
                    n1 = pop_numstack();
                    push_numstack(pop->eval(this, n1, 0));
                }
                else if (pop->unary == 2)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    push_numstack(pop->eval(this, n2, n1));
                }
                else if (pop->unary == 3)
                {
                    n1 = pop_numstack();
                    n2 = pop_numstack();
                    n3 = pop_numstack();
                    push_numstack(eval_cond(this, n3, n2, n1));
                }
            }
        }

        push_opstack(op);
    }

public:
    bool evaluate(xlasm *     xl_,
                  std::string expression,
                  int64_t *   result,
                  size_t *    last_offset     = nullptr,
                  bool        allow_undefined = true)
    {
        struct op_s         startop = {"X", 1, OP_DUMMY, 0, ASSOC_NONE, 0, nullptr}; /* Dummy operator to mark TOS */
        const struct op_s * op      = nullptr;
        int64_t             n1, n2, n3;
        const struct op_s * lastop = &startop;

        xl            = xl_;
        errorcode     = 0;
        brace_balance = 0;
        nopstack      = 0;
        memset(&opstack, 0, sizeof(opstack));
        nnumstack = 0;
        memset(&numstack, 0, sizeof(numstack));
        *result = 0;        // default

        //		printf("evaluate(\"%s\")\n", expression.c_str());

        const char * expr;
        for (expr = expression.c_str(); *expr && !errorcode; ++expr)
        {
            if (expr[0] == ' ')
                continue;

            op = getop(expr);
            if (op)
            {
                if (lastop && (lastop == &startop || lastop->op != OP_RPAREN))
                {
                    // special cases to avoid ambiguity
                    if (op->op == OP_SUB)
                    {
                        op = &ops[0];        // unary minus
                        assert(op->op == OP_UMINUS);
                    }
                    if (op->op == OP_ADD)
                    {
                        op = &ops[1];        // unary plus
                        assert(op->op == OP_UPLUS);
                    }
                    else if (op->op != OP_LPAREN && op->unary > 1)
                    {
                        eval_error(0x108, "Illegal use of operator '%s' at: %.32s", op->op_str, expr);
                        return false;
                    }
                }

                // if we see open paren with a number on the stack, but no operator, assume this is a new expression.
                // E.g. for MIPS addressing mode "XXX(rX)".
                if (op->op == OP_LPAREN && lastop == nullptr && nnumstack != 0)
                {
                    break;
                }

                shunt_op(op);
                if (op->op == OP_TERNARY)        // insert invisible LPAREN after ? (and : becomes RPAREN)
                {
                    op = &ops[(sizeof(ops) / sizeof(ops[0])) - 2];        // LPAREN
                    assert(op->op == OP_LPAREN);
                    shunt_op(op);
                }

                lastop = op;

                continue;
            }

            if (isdigit(*expr) || *expr == '\'' || (xl->dollar_hex() && *expr == '$'))
            {
                // if we see open paren with a number on the stack, but no operator, assume this is a new expression.
                // E.g. for MIPS addressing mode "XXX(rX)".
                if (lastop == nullptr && nnumstack != 0)
                {
                    break;
                }

                const char * oexpr = expr;
                int64_t      v     = 0;
                if (expr[0] == '\'')
                {
                    if (expr[2] != '\'' || !isprint(expr[1]))
                    {
                        eval_error2(0x10C, "Character literal syntax error at: %.32s", expr);
                        return false;
                    }
                    v = expr[1];
                    expr += 3;
                }
                else if (expr[0] == '0' && expr[1] == 'b')        // need binary!
                {
                    expr += 2;
                    while (*expr == '0' || *expr == '1')
                    {
                        v = (v << 1) | (*expr & 1);
                        expr++;
                    }
                }
                else if (xl->dollar_hex() && expr[0] == '$')        // old school hex
                {
                    expr += 1;
                    v = static_cast<int64_t>(strtoul(const_cast<char *>(expr), const_cast<char **>(&expr), 16));
                }
                else
                {
                    v = static_cast<int64_t>(strtoul(const_cast<char *>(expr), const_cast<char **>(&expr), 0));
                }

                push_numstack(v);
                lastop = nullptr;

                // for loop will still increment, so back off one
                if (oexpr != expr)
                {
                    exp_dprintf("literal=%lld (0x%llx) [%lld chars]\n", v, v, expr - oexpr);
                    expr--;
                }
                else
                {
                    eval_error2(0x10C, "Literal syntax error at: %.32s", expr);
                    return false;
                }

                continue;
            }

            if (isalpha(*expr) || *expr == '.' || *expr == '_')
            {
                char         symname[64] = {0};
                uint32_t     symlen      = 1;
                const char * sptr        = expr + 1;
                while (symlen < sizeof(symname) && *sptr &&
                       (isalpha(*sptr) || isdigit(*sptr) || *sptr == '.' || *sptr == '_'))
                {
                    symlen++;
                    sptr++;
                }
                strncpy(symname, expr, symlen);

                bool    undefined = false;
                int64_t v         = xl->symbol_value(xl, symname, &undefined);
                exp_dprintf("parsed sym '%s' v = 0x%llx\n", symname, v);

                if (!allow_undefined && undefined)
                {
                    eval_error(0x10C, "Use of undefined symbol: %.32s", expr);
                    return false;
                }

                push_numstack(v);
                lastop = nullptr;

                expr += symlen - 1;
                continue;
            }

            if (!isspace(*expr) && (!xl->dollar_hex() && *expr != '$'))
            {
                eval_error(0x10A, "Expression syntax error at: %.32s", expr);
                return false;
            }
        }

        if (brace_balance > 0)
        {
            eval_error(0x10C, "Open parenthesis '(' with no closing ')'");
            return false;
        }

        while (!errorcode && nopstack)
        {
            op = pop_opstack();
            if (!op)
                break;

            if (op->unary == 1)
            {
                n1 = pop_numstack();
                push_numstack(op->eval(this, n1, 0));
            }
            else if (op->unary == 2)
            {
                n1 = pop_numstack();
                n2 = pop_numstack();
                push_numstack(op->eval(this, n2, n1));
            }
            else if (op->unary == 3)
            {
                assert(op->op == OP_TERNARY);
                n1 = pop_numstack();
                n2 = pop_numstack();
                n3 = pop_numstack();
                push_numstack(eval_cond(this, n3, n2, n1));        // special case for ?: ternary op
            }
        }
        if (!errorcode && nnumstack != 1)
        {
            eval_error2(0x10B, "Multiple values (%d) after evaluation, should be only one.", nnumstack);
            return false;
        }

        if (last_offset != nullptr)
            *last_offset = static_cast<size_t>((expr - expression.c_str()));
        *result = numstack[0];

        return errorcode ? false : true;
    }
};
