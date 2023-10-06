// XLAsm macro-assembler

#pragma once

#include <cinttypes>
#include <cstdarg>
#include <list>
#include <random>
#include <stack>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

// Miyu was here (virtually) -> :3

#define PR_D64     "%" PRId64
#define PR_U64     "%" PRIu64
#define PR_X64     "%" PRIx64
#define PR_X64_04  "%04" PRIx64
#define PR_X64_08  "%08" PRIx64
#define PR_X64_016 "%016" PRIx64

#define PR_DSIZET  "%zu"
#define PR_DSSIZET "%zd"
#define PR_XSIZET  "%zx"

#define TERM_WARN  "\033[0;35m"
#define TERM_ERROR "\033[0;33m"
#define TERM_CLEAR "\033[0m"

#if defined(__GNUC__) || defined(__clang__)
#define ATTRIBUTE(x) __attribute__(x)
#else
#define ATTRIBUTE(x)
#endif

#if 1
#define dprintf(x, ...)                                                                                                \
    if (opt.verbose)                                                                                                   \
    {                                                                                                                  \
        printf(x, ##__VA_ARGS__);                                                                                      \
        fflush(stdout);                                                                                                \
    }                                                                                                                  \
    while (0)
#else
#define dprintf(x, ...) (void)0
#endif

#define UNICODE_SUPPORT 0

[[noreturn]] void fatal_error(const char * msg, ...) ATTRIBUTE((noreturn)) ATTRIBUTE((format(printf, 1, 2)));
void              vstrprintf(std::string & str, const char * fmt, va_list va) ATTRIBUTE((format(printf, 2, 0)));
void              strprintf(std::string & str, const char * fmt, ...) ATTRIBUTE((format(printf, 2, 3)));
char              uppercase(char v);
char              lowercase(char v);

#define MAX_LINE_LENGTH 4096
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))

struct Ixlarch;

struct xlasm
{
    template<typename T>
    void emit(T v);

    template<typename T>
    T endian_swap(T v);

    struct opts_t
    {
        int32_t                  verbose;        // 0, 1, 2 or 3
        std::vector<std::string> include_path;
        std::vector<std::string> define_sym;        // unmolested original line (with no newline)
        uint32_t                 listing_bytes;
        uint64_t                 load_address;
        bool                     listing;
        bool                     xref;
        bool                     no_error_kill;
        bool                     suppress_false_conditionals;
        bool                     suppress_macro_expansion;
        bool                     suppress_macro_name;
        bool                     suppress_line_numbers;

        opts_t() noexcept
                : verbose(1)
                , listing_bytes(0x600)
                , load_address(0)
                , listing(false)
                , xref(false)
                , no_error_kill(false)
                , suppress_false_conditionals(false)
                , suppress_macro_expansion(false)
                , suppress_macro_name(false)
                , suppress_line_numbers(false)
        {
        }
    };

    struct source_t
    {
        std::string                           name;
        std::vector<std::string>              orig_line;        // unmolested original line (with no newline)
        std::vector<std::vector<std::string>> src_line;         // broken up into vector of tokens per line
        uint64_t                              file_size;
        uint32_t                              line_start;

        source_t() noexcept
                : file_size(0)
                , line_start(1)
        {
        }
        int32_t read_file(xlasm *, const std::string & n, const std::string & fn);
    };
    typedef std::unordered_map<std::string, source_t> source_map_t;

    struct symbol_t;

    struct section_t
    {
        enum
        {
            NOLOAD_FLAG     = (1 << 0),
            FUNCTION_FLAG   = (1 << 1),
            REFERENCED_FLAG = (1 << 2),
            REMOVED_FLAG    = (1 << 3)
        };

        std::string          name;
        Ixlarch *            arch;
        uint32_t             flags;
        uint32_t             index;
        int64_t              load_addr;
        int64_t              addr;
        std::vector<uint8_t> data;
        symbol_t *           last_defined_sym;

        section_t() noexcept
                : arch(nullptr)
                , flags(0)
                , index(0)
                , load_addr(0)
                , addr(0)
                , last_defined_sym(nullptr)
        {
        }
    };
    typedef std::unordered_map<std::string, section_t> section_map_t;

    struct symbol_t
    {
        enum sym_t
        {
            UNDEFINED,
            INTERNAL,
            REGISTER,
            LABEL,
            COMM,
            VARIABLE,
            STRING,
            NUM_SYM_TYPES
        };


        sym_t       type;
        uint32_t    line_defined;
        std::string name;
        std::string str;
        int64_t     value;
        uint64_t    size;
        source_t *  file_defined;
        source_t *  file_first_referenced;
        section_t * section;
        uint32_t    line_first_referenced;

        symbol_t() noexcept
                : type(UNDEFINED)
                , line_defined(0)
                , value(0)
                , size(0)
                , file_defined(nullptr)
                , file_first_referenced(nullptr)
                , section(nullptr)
                , line_first_referenced(0)
        {
        }

        const char * type_name() const
        {
            static constexpr const char * symbol_t_names[] = {
                "UNDEFINED", "INTERNAL", "REGISTER", "LABEL", "COMM", "VARIABLE", "STRING"};
            return symbol_t_names[static_cast<size_t>(type)];
        }
        const char * type_abbrev() const
        {
            static constexpr const char * symbol_t_abbrev[] = {"U", "I", "R", "L", "C", "V", "S"};
            return symbol_t_abbrev[static_cast<size_t>(type)];
        }
    };
    typedef std::unordered_map<std::string, symbol_t> symbol_map_t;
    typedef std::vector<std::string>                  export_list_t;

    struct condition_t
    {
        uint8_t state : 1;
        uint8_t wastrue : 1;
        uint8_t : 0;

        condition_t() noexcept
                : state(0U)
                , wastrue(0U)
        {
        }
    };
    typedef std::stack<condition_t> condition_stack_t;

    struct macro_t
    {
        std::string              name;
        std::vector<std::string> args;
        std::vector<std::string> def;
        source_t                 body;
        uint32_t                 invoke_count;

        macro_t() noexcept
                : invoke_count(0)
        {
        }
    };
    typedef std::unordered_map<std::string, macro_t> macro_map_t;

    struct context_t
    {
        enum pass_t
        {
            UNKNOWN,        // unset
            PASS_1,        // define labels and process all input, but undefined symbols are ignored (until end of pass)
            PASS_OPT,        // optimize code for smallest/fastest possible (done repeatedly until optimal or max passes
                             // exceeded)
            PASS_2,          // actually generate output (with all symbols defined)
            NUM_PASSES
        };
        uint32_t    pass;
        uint32_t    line;
        source_t *  file;
        section_t * section;
        macro_t *   macroexp_ptr;
        macro_t *   macrodef_ptr;
        int32_t     conditional_nesting;
        condition_t conditional;

        context_t() noexcept
                : pass(UNKNOWN)
                , line(0)
                , file(nullptr)
                , section(nullptr)
                , macroexp_ptr(nullptr)
                , macrodef_ptr(nullptr)
                , conditional_nesting(0)
        {
        }
    };

    enum
    {
        MAXERROR_COUNT       = 1000,          // aborts after this many errors
        MAXINCLUDE_STACK     = 64,            // include nest depth
        MAXMACRO_STACK       = 1024,          // nested macro depth
        MAXMACROREPS_WARNING = 255,           // max parameters replacement iterations per line
        MAXFILL_BYTES        = 0xC00L,        // max size output by space or fill directive (safety check)
        MAX_PASSES           = 10             // maximum number of assembler passes before optimization short-circuited
    };

    enum directive_index
    {
        DIR_UNKNOWN,
        DIR_INCLUDE,
        DIR_INCBIN,
        DIR_ORG,
        DIR_EQU,
        DIR_UNDEFINE,
        DIR_ASSIGN,
        DIR_ALIGN,
        DIR_SPACE_16,
        DIR_FILL_16,
        DIR_DEF_HEX,
        DIR_DEF_16,
        DIR_MACRO,
        DIR_ENDMACRO,
        DIR_VOID,
        DIR_ASSERT,
        DIR_IF,
        DIR_IFSTR,
        DIR_IFSTRI,
        DIR_ELSE,
        DIR_ELSEIF,
        DIR_ENDIF,
        DIR_END,
        DIR_MSG,
        DIR_WARN,
        DIR_EXIT,
        DIR_ERROR,
        DIR_LIST,
        DIR_LISTMAC,
        DIR_MACNAME,
        DIR_LISTCOND,
        DIR_EXPORT,
        NUM_DIRECTIVES
    };

    struct directive_t
    {
        const char * name;
        uint32_t     index;
    };

    typedef std::stack<context_t>                     context_stack_t;
    typedef std::unordered_map<std::string, uint32_t> directive_map_t;
    typedef std::unordered_map<uint32_t, uint32_t>    hint_map_t;

    static constexpr directive_t directives_list[] = {{"INCLUDE", DIR_INCLUDE},   {"INCBIN", DIR_INCBIN},
                                                      {"ORG", DIR_ORG},           {"EQU", DIR_EQU},
                                                      {"=", DIR_ASSIGN},          {"ASSIGN", DIR_ASSIGN},
                                                      {"UNDEF", DIR_UNDEFINE},    {"UNSET", DIR_UNDEFINE},
                                                      {"EXPORT", DIR_EXPORT},     {"ALIGN", DIR_ALIGN},
                                                      {"SPACE", DIR_SPACE_16},    {"FILL", DIR_FILL_16},
                                                      {"HEX", DIR_DEF_HEX},       {"HALF", DIR_DEF_16},
                                                      {"SHORT", DIR_DEF_16},      {"INT", DIR_DEF_16},
                                                      {"DD16", DIR_DEF_16},       {"MACRO", DIR_MACRO},
                                                      {"ENDMACRO", DIR_ENDMACRO}, {"ENDM", DIR_ENDMACRO},
                                                      {"VOID", DIR_VOID},         {"IF", DIR_IF},
                                                      {"IFSTR", DIR_IFSTR},       {"IFSTRI", DIR_IFSTRI},
                                                      {"ELSEIF", DIR_ELSEIF},     {"ELSE", DIR_ELSE},
                                                      {"ENDIF", DIR_ENDIF},       {"END", DIR_END},
                                                      {"MSG", DIR_MSG},           {"PRINT", DIR_MSG},
                                                      {"ASSERT", DIR_ASSERT},     {"WARN", DIR_WARN},
                                                      {"ERROR", DIR_ERROR},       {"EXIT", DIR_EXIT},
                                                      {"LIST", DIR_LIST},         {"LISTMAC", DIR_LISTMAC},
                                                      {"MACNAME", DIR_MACNAME},   {"LISTCOND", DIR_LISTCOND}};


    std::string initial_variant;        // initial architecture name to assemble for
    Ixlarch *   arch;                   // current architecture to assemble for (can be changed with ARCH directive)

    opts_t                 opt;                    // assembly options
    context_t              ctxt;                   // current assembly context
    context_stack_t        context_stack;          // context stack for include files and macros
    section_map_t          sections;               // output sections
    source_map_t           source_files;           // map of source files (tokenized at read time)
    macro_map_t            macros;                 // defined macros
    source_map_t           expanded_macros;        // source fragments from expanded macros
    symbol_map_t           symbols;                // labels and other symbols
    export_list_t          exports;
    condition_stack_t      condition_stack;         // stack for conditional assembly
    directive_map_t        directives;              // fast lookup of directives
    hint_map_t             line_hint;               // "hint" for this virtual-line (for squeeze pass)
    std::list<std::string> input_names;             // list of input filenames (assembled into one output)
    std::string            object_filename;         // output filename
    std::string            listing_filename;        // listing filename
    std::list<std::string> pre_messages;
    std::list<std::string> post_messages;
    std::mt19937_64        rng;

    int64_t     total_size_generated;
    int64_t     last_size_generated;
    int64_t     bytes_optimized;
    int64_t     undefined_sym_count;
    int64_t     line_sec_addr;
    FILE *      listing_file;
    source_t *  last_diag_file;
    section_t * undefined_section;
    symbol_t *  sym_defined;
    source_t *  line_last_file;
    section_t * func_section;
    section_t * endfunc_section;
    section_t * previous_section;
    section_t * line_sec_start;
    size_t      undefined_begin_size;
    size_t      line_sec_size;
    uint32_t    applied_hints;
    uint32_t    pending_hints;
    uint32_t    crc_value;
    uint32_t    next_section_index;
    uint32_t    error_count;
    uint32_t    warning_count;
    uint32_t    virtual_line_num;
    uint32_t    prev_virtual_line_num;
    uint32_t    pass_count;
    uint32_t    last_diag_line;

    bool line_sec_org;
    bool suppress_line_list;
    bool suppress_line_listsource;
    bool force_end_file;
    bool force_exit_assembly;

    std::random_device::result_type random_seed;

    // initialize all non-constructed members architecture
    xlasm(const std::string & architecture);

    // external interface, gathers input and options
    int32_t assemble(const std::vector<std::string> & in_files, const std::string & out_file, const opts_t & opts);

    // internal functions
    int32_t do_passes();        // read input files into memory, iterate over files for all assembler passes
    int32_t process_file(source_t & f);        // iterate over source lines in a source_t
    int32_t process_line();                    // process a single line from source_t
    int32_t process_line_listing();
    int32_t process_xref();
    int32_t process_output();
    int32_t process_labeldef(std::string label);        // define a "normal" label (i.e., set to current output address)
    int32_t process_directive(uint32_t                         idx,
                              const std::string &              directive,
                              const std::string &              label,
                              size_t                           cur_token,
                              const std::vector<std::string> & tokens);

    int32_t process_section(const std::string &              directive,
                            const std::string &              label,
                            size_t                           cur_token,
                            const std::vector<std::string> & tokens);

    // helper functions
    int32_t     pass_reset();
    int32_t     check_undefined();
    bool        define_macro_begin(const std::string &              directive,
                                   const std::string &              label,
                                   size_t                           cur_token,
                                   const std::vector<std::string> & tokens);
    bool        define_macro_end(const std::string &              directive,
                                 const std::string &              label,
                                 size_t                           cur_token,
                                 const std::vector<std::string> & tokens);
    source_t &  expand_macro(std::string & name, size_t cur_token, const std::vector<std::string> & tokens);
    int64_t     eval_tokens(const std::string &              cmd,
                            std::string &                    exprstr,
                            size_t &                         cur_token,
                            const std::vector<std::string> & tokens,
                            int32_t                          expected_args,
                            int64_t                          defval);
    bool        check_truncation(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag = 1);
    bool        check_truncation_signed(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag = 1);
    bool        check_truncation_unsigned(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag = 1);
    uint32_t    bits_needed_signed(int64_t v);
    uint32_t    bits_needed_unsigned(int64_t v);
    std::string removeExtension(const std::string & filename);
    std::string removeQuotes(const std::string & quotedstr);
    std::string reQuote(const std::string & str);
    std::string quotedToRaw(const std::string cmd, const std::string & str, bool null_terminate);
    int32_t     align_output(size_t pot);
    int64_t     lookup_special_symbol(const std::string & sym_name);
    int32_t     lookup_register_symbol(const std::string & sym_name);
    void        add_sym(const char * name, symbol_t::sym_t type, int64_t value);
    void        remove_sym(const char * name);
    void        diag_flush();
    void        diag_showline();
    void        update_crc16(uint8_t x);
    void        update_crc32(uint8_t x);

    void        error(const char * msg, ...) ATTRIBUTE((format(printf, 2, 3)));
    void        warning(const char * msg, ...) ATTRIBUTE((format(printf, 2, 3)));
    void        notice(int32_t level, const char * msg, ...) ATTRIBUTE((format(printf, 3, 4)));
    std::string token_message(size_t cur_token, const std::vector<std::string> & tokens);
    bool        dollar_hex();

    static int64_t symbol_value(xlasm *      xl,
                                const char * name,
                                bool *       allow_undefined = nullptr);        // expression evaluation symbol lookup
};

// Interface to architecture specific code
struct Ixlarch
{
    static std::vector<Ixlarch *> architectures;
    static void                   register_arch(Ixlarch *);
    static Ixlarch *              find_arch(const std::string & architecture);

    virtual ~Ixlarch();
    virtual const char *      variant_names() = 0;        // list of possible variant names for this architecture
    virtual bool              set_variant(std::string name) = 0;        // set current variant index
    virtual const std::string get_variant()                 = 0;        // name of current variant
    virtual void              reset(xlasm * xl)             = 0;        // initial reset before first pass
    virtual void              activate(xlasm * xl)          = 0;        // init before architecture
    virtual void              deactivate(
                     xlasm * xl) = 0;        // clear architecture symbols (when switching to another architecture)
    virtual uint32_t check_directive(
        const std::string & directive) = 0;        // return directive index or xlasm::DIR_UNKNOWN if not recognized
    virtual int32_t process_directive(xlasm *                          xl,
                                      uint32_t                         idx,
                                      const std::string &              directive,
                                      const std::string &              label,
                                      size_t                           cur_token,
                                      const std::vector<std::string> & tokens) = 0;
    virtual int32_t lookup_register(const std::string & name)                  = 0;
    virtual int32_t check_opcode(const std::string & opcode) = 0;        // return opcode index or -1 if not recognized
    virtual int32_t process_opcode(xlasm *                          xl,
                                   int32_t                          idx,
                                   std::string &                    opcode,
                                   size_t                           cur_token,
                                   const std::vector<std::string> & tokens) = 0;

    virtual bool is_big_endian()
    {
        return false;
    }        // true if default endian is big-endian for architecture
    virtual bool support_dollar_hex()
    {
        return false;
    }        // support '$' for hex values
    virtual bool column_one_labels()
    {
        return false;
    }        // assume column one symbol is a label definition
    virtual uint32_t max_bit_width()
    {
        return 64;
    }        // maximum bit width for address/data for architecture (<=64)
    virtual uint32_t code_alignment()
    {
        return 1;
    }        // byte boundary for opcodes
    virtual uint32_t data_alignment(size_t size)
    {
        (void)size;
        return 1;
    }        // byte boundary for data type size bytes (1-16, POT)
};


template<typename T>
void xlasm::emit(T v)
{
    union
    {
        T       v;
        uint8_t b[sizeof(T)];
    } u;

    static_assert(sizeof(T) <= 16, "bad_type");

    u.v = v;
    if (arch->is_big_endian())
    {
        for (size_t i = 0; i < sizeof(T); i++)
            ctxt.section->data.push_back(u.b[(sizeof(T) - 1) - i]);
    }
    else
    {
        for (size_t i = 0; i < sizeof(T); i++)
            ctxt.section->data.push_back(u.b[i]);
    }
}

template<typename T>
T xlasm::endian_swap(T v)
{
    union
    {
        T       v;
        uint8_t b[sizeof(T)];
    } u, o;

    static_assert(sizeof(T) <= 16, "bad_type");

    if (arch->is_big_endian() && sizeof(T) > 1)
    {
        u.v = v;
        for (size_t i = sizeof(T) - 1, j = 0; j < sizeof(T); i--, j++)
            o.b[j] = u.b[i];

        return o.v;
    }
    else
    {
        return v;
    }
}
