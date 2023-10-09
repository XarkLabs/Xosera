// xlasm.cpp

#include <algorithm>
#include <assert.h>
#include <ctime>
#include <ctype.h>
#include <errno.h>
#include <locale>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <codecvt>
#endif

#include <sys/stat.h>

#include "xlasm.h"
#include "xlasmexpr.h"

#include "xlasmcopper.h"

// utility classes and helper functions

#if 0        // not used
template<typename ToType, typename FromType>
static inline ToType union_cast(const FromType & from)
{
    static_assert(sizeof(ToType) == sizeof(FromType),
                  "must be same size");        // the size of both union_cast types must be the same

    union
    F
        ToType   to;
        FromType from;
    } u;

    u.from = from;
    return u.to;
}
#endif

#if 0
static bool hasEnding(const std::string & fullString, const std::string & ending)
{
    if (fullString.length() >= ending.length())
    {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else
    {
        return false;
    }
}
#endif

static void rtrim(std::string & str, const std::string & ws)
{
    size_t found;
    found = str.find_last_not_of(ws);
    if (found != std::string::npos)
        str.erase(found + 1);
    else
        str.clear();        // str is all whitespace
}

// using these to avoid some "strict" type conversion warnings with system version returning int
char uppercase(char v)
{
    return static_cast<char>(::toupper(v));
}

char lowercase(char v)
{
    return static_cast<char>(::tolower(v));
}

// output error and exit immediately
void fatal_error(const char * msg, ...)
{
    va_list ap;
    va_start(ap, msg);

    printf(TERM_ERROR "FATAL ERROR: ");
    vprintf(msg, ap);
    printf(TERM_CLEAR "\n");

    va_end(ap);

    exit(10);
}

Ixlarch::~Ixlarch()
{
}

std::vector<Ixlarch *> Ixlarch::architectures;

void Ixlarch::register_arch(Ixlarch * arch)
{
    auto it = std::upper_bound(architectures.begin(), architectures.end(), arch, [](Ixlarch * a, Ixlarch * b) {
        return strcmp(a->variant_names(), b->variant_names()) < 0;
    });
    architectures.insert(it, arch);
}

Ixlarch * Ixlarch::find_arch(const std::string & architecture)
{
    for (auto & a : Ixlarch::architectures)
    {
        if (a->set_variant(architecture))
        {
            return a;
        }
    }

    return nullptr;
}

// xlasm main class
xlasm::xlasm(const std::string & architecture)
        : initial_variant(architecture)
        , arch(nullptr)
        , total_size_generated(0)
        , last_size_generated(0)
        , bytes_optimized(0)
        , undefined_sym_count(0)
        , line_sec_addr(0)
        , listing_file(nullptr)
        , last_diag_file(nullptr)
        , undefined_section(nullptr)
        , sym_defined(nullptr)
        , line_last_file(nullptr)
        , func_section(nullptr)
        , endfunc_section(nullptr)
        , previous_section(nullptr)
        , line_sec_start(nullptr)
        , undefined_begin_size(0)
        , line_sec_size(0)
        , applied_hints(0)
        , pending_hints(0)
        , crc_value(0)
        , next_section_index(0)
        , error_count(0)
        , warning_count(0)
        , virtual_line_num(0)
        , prev_virtual_line_num(0)
        , pass_count(0)
        , last_diag_line(0)
        , line_sec_org(false)
        , suppress_line_list(false)
        , suppress_line_listsource(false)
        , force_end_file(false)
        , force_exit_assembly(false)
{
    //	std::random_device rd;		// non-deterministic generator for seed
    random_seed = 42;        // rd();
}

bool xlasm::dollar_hex()
{
    return arch->support_dollar_hex();
}

constexpr xlasm::directive_t xlasm::directives_list[];

int32_t xlasm::assemble(const std::vector<std::string> & in_files, const std::string & out_file, const opts_t & opts)
{
    if (!in_files.size())
    {
        dprintf("No input files.\n");
        return 0;
    }

    // copy option flags
    opt = opts;

    // init initial architecture
    arch = Ixlarch::find_arch(initial_variant);
    arch->activate(this);
    arch->set_variant(initial_variant);

    // copy source file names
    for (auto it = in_files.begin(); it != in_files.end(); ++it)
        input_names.push_back(*it);

    // copy output file name
    object_filename = out_file;

    // listing file name
    if (opt.listing)
    {
        if (object_filename.size())
            listing_filename = removeExtension(object_filename) + ".lst";
        else
            listing_filename = removeExtension(in_files[0]) + ".lst";
    }

    if (directives.size() == 0)
    {
        for (size_t i = 0; i < NUM_ELEMENTS(directives_list) && directives_list[i].name; i++)
        {
            std::string n(directives_list[i].name);
            uint32_t &  d = directives[n];
            assert(d == DIR_UNKNOWN);
            d = directives_list[i].index;
        }
    }

    for (auto it = opt.define_sym.begin(); it != opt.define_sym.end(); ++it)
    {
        std::string label  = *it;
        int64_t     result = 1;

        if (!isalpha(label[0]) && !isdigit(label[0]) && label[0] != '_')
        {
            fatal_error("invalid define symbol \"%s\"", label.c_str());
        }

        auto epos = it->find('=');
        if (epos != std::string::npos && it->size() > epos + 1)
        {
            label           = it->substr(0, epos);
            std::string arg = it->substr(epos + 1);
            expression  expr;

            if (arg.size() && !expr.evaluate(this, arg.c_str(), &result))
            {
                fatal_error("error evaluating define symbol expression \"%s\"", it->c_str());
            }
        }
        notice(2, "Defined \"%s\" = 0x" PR_X64 "/" PR_D64 "\n", label.c_str(), result, result);

        add_sym(label.c_str(), symbol_t::LABEL, result);
    }

    if (opt.include_path.size())
    {
        notice(2, "Include search paths:");
        for (auto it = opt.include_path.begin(); it != opt.include_path.end(); ++it)
        {
            notice(2, "    \"%s\"", it->c_str());
        }
    }

    dprintf("Assembling " PR_DSIZET " %s file%s into output \"%s\"",
            in_files.size(),
            arch->get_variant().c_str(),
            in_files.size() == 1 ? "" : "s",
            object_filename.c_str());
    if (opt.listing)
        dprintf(" with listing \"%s\"", listing_filename.c_str());
    dprintf("\n");

    // Read source files
    int32_t index = 1;
    for (auto it = input_names.begin(); it != input_names.end(); ++it, index++)
    {
        source_t & f = source_files[*it];
        int        e = f.read_file(this, *it, *it);
        if (e)
            fatal_error("reading file \"%s\" error: %s", it->c_str(), strerror(e));

        dprintf("File \"%s\" read into memory (" PR_DSIZET " lines, " PR_U64 " bytes).\n",
                it->c_str(),
                source_files[*it].orig_line.size(),
                source_files[*it].file_size);
    }

    do_passes();

    printf("%scopasm %s%s with %d warning%s and %d error%s%s\n",
           error_count ? "\n*** " : "",
           ((error_count && !opt.no_error_kill) || force_exit_assembly) ? "FAILED" : "completed",
           (error_count == 0 && !force_exit_assembly) ? " successfully" : "",
           warning_count,
           warning_count == 1 ? "" : "s",
           error_count,
           error_count == 1 ? "" : "s",
           error_count ? " ***\n" : "");

    return error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// iterate over all lines of files processing input
int32_t xlasm::do_passes()
{
    next_section_index = 1;

    if (opt.listing)
    {
        listing_file = fopen(listing_filename.c_str(), "wt");

        if (!listing_file)
            fatal_error("Opening listing file \"%s\" error: %s\n", listing_filename.c_str(), strerror(errno));
    }

    arch = Ixlarch::find_arch(initial_variant);
    arch->set_variant(initial_variant);

    ctxt.pass = context_t::PASS_1;

    // create default sections
    sections["text"].name  = "text";
    sections["text"].arch  = arch;
    sections["text"].index = 0;

    source_t no_file;

    ctxt.section     = &sections["text"];
    ctxt.file        = &no_file;
    previous_section = ctxt.section;

    arch->reset(this);

    // add special symbols
    add_sym(".", symbol_t::INTERNAL, 0x00);
    add_sym(".rand16", symbol_t::INTERNAL, 0x00);
    add_sym(".RAND16", symbol_t::INTERNAL, 0x00);

    do
    {
        pass_reset();

        ctxt.section     = &sections["text"];
        previous_section = ctxt.section;

        // iterate over all files
        for (auto fit = input_names.begin(); fit != input_names.end(); ++fit)
        {
            source_t & f = source_files[*fit];

            process_file(f);

            if (force_exit_assembly)
                break;
        }

        ctxt.file = nullptr;
        diag_flush();

        if (ctxt.pass == context_t::PASS_2)
            check_undefined();

        if (force_exit_assembly)
            break;

        if (opt.no_error_kill)
        {
            if (error_count)
            {
                printf("Continuing despite errors (-k option).\n");
            }
            continue;
        }
        if (error_count)
            break;

    } while (ctxt.pass != context_t::PASS_2);

    if (opt.listing && opt.xref)
    {
        uint32_t oldpass = ctxt.pass;
        ctxt.pass        = context_t::UNKNOWN;
        process_xref();
        ctxt.pass = oldpass;
    }

    if (ctxt.pass == context_t::PASS_2)
    {
        process_output();
    }
    else
    {
        printf("No output generated.\n");
    }

    return 0;
}

int32_t xlasm::pass_reset()
{
    error_count = 0;        // ??

    if (prev_virtual_line_num)
    {
        if (prev_virtual_line_num != virtual_line_num)
        {
            fatal_error("Number of processed lines (%d) differs from previous pass (%d).",
                        virtual_line_num,
                        prev_virtual_line_num);
        }
    }
    prev_virtual_line_num = virtual_line_num;

    std::vector<section_t *> secs;
    for (auto it = sections.begin(); it != sections.end(); ++it)
    {
        auto & sec = it->second;
        if (!sec.data.size())
            continue;

        secs.push_back(&sec);
    }
    std::sort(std::begin(secs), std::end(secs), [](const section_t * lhs, const section_t * rhs) {
        return lhs->index < rhs->index;
    });

    total_size_generated = 0;
    for (auto it = secs.begin(); it != secs.end(); ++it)
    {
        total_size_generated += static_cast<int64_t>((*it)->data.size());
    }

    uint32_t pending_secs = 0;
    int64_t  addr         = 0;
    for (auto it = secs.begin(); it != secs.end(); ++it)
    {
        section_t * sec = *it;

        if (addr >= sec->load_addr)
            sec->load_addr = addr;
        else
            addr = sec->load_addr;

        addr += sec->data.size();

        sec->addr = sec->load_addr;
        //		dprintf("Cleared section #%d \"%s\" 0x" PR_X64 "-0x" PR_X64 " (0x" PR_X64 "/" PR_D64 " bytes)\n",
        // sec->index, sec->name.c_str(), sec->load_addr, sec->load_addr+sec->data.size() - (sec->data.size() ? 1 : 0),
        //			sec->data.size(), sec->data.size());
        sec->data.clear();
        sec->last_defined_sym = nullptr;

        if (sec->flags & section_t::REFERENCED_FLAG)
        {
            if (sec->flags & section_t::FUNCTION_FLAG && sec->flags & section_t::REMOVED_FLAG)
            {
                pending_secs++;
                sec->flags &= ~static_cast<uint32_t>(section_t::REMOVED_FLAG);
            }
        }
        else if (sec->flags & section_t::FUNCTION_FLAG && !(sec->flags & section_t::REMOVED_FLAG))
        {
            pending_secs++;
            sec->flags |= static_cast<uint32_t>(section_t::REMOVED_FLAG);
        }
    }

    auto it = symbols.begin();
    while (it != symbols.end())
    {
        auto & sym = it->second;
        if (sym.type == symbol_t::UNDEFINED /*  || sym.type == symbol_t::VARIABLE */)
        {
            //			dprintf("Erasing symbol \"%s\" (value: 0x" PR_X64 "/" PR_D64 " \"%s\")\n", sym.name.c_str(),
            // sym.value, sym.value, sym.str.c_str());
            it = symbols.erase(it);
        }
        else
            ++it;
    }

    arch->deactivate(this);
    arch = Ixlarch::find_arch(initial_variant);
    arch->activate(this);
    arch->set_variant(initial_variant);

    //	dprintf("Erasing " PR_D64 " macros\n", macros.size());
    macros.clear();
    // BUG: should not clear this between passes:    expanded_macros.clear();

    line_last_file           = nullptr;
    line_sec_start           = nullptr;
    line_sec_size            = 0;
    line_sec_addr            = 0;
    ctxt.conditional         = condition_t();
    ctxt.conditional_nesting = 0;
    ctxt.macroexp_ptr        = nullptr;
    ctxt.macrodef_ptr        = nullptr;

    rng.seed(random_seed);

    virtual_line_num = 0;

    if (ctxt.pass == context_t::PASS_1 && prev_virtual_line_num)
        ctxt.pass = context_t::PASS_OPT;

    if (ctxt.pass == context_t::PASS_OPT && last_size_generated == total_size_generated)
        ctxt.pass = context_t::PASS_2;

    if (pass_count >= MAX_PASSES)
    {
        ctxt.pass = context_t::PASS_2;
        warning(
            "Maximum passes of %d exceeded, skipping optimization of final %d instructions and %d function sections",
            MAX_PASSES,
            pending_hints,
            pending_secs);
    }

    if (ctxt.pass != context_t::PASS_2)
    {
        bytes_optimized = 0;
    }
    uint32_t nonopt  = pending_hints;
    uint32_t appopts = applied_hints;
    (void)nonopt;
    (void)appopts;
    pending_hints = 0;
    applied_hints = 0;

    pass_count++;

    switch (ctxt.pass)
    {
        case context_t::PASS_1: {
#if 0
            dprintf("Pass %2d (initial)\n", pass_count);
#endif
            break;
        }
        case context_t::PASS_OPT: {
#if 0
            dprintf("Pass %2d (" PR_D64 " bytes, %d instructions optimized, %d pending, %d sections)\n",
                    pass_count,
                    total_size_generated,
                    appopts,
                    nonopt,
                    pending_secs);
#else
#if 0
            dprintf("Pass %2d (" PR_D64 " words)\n", pass_count, total_size_generated >> 1);
#endif
#endif
            last_size_generated = total_size_generated;
            break;
        }
        case context_t::PASS_2: {
#if 0
            std::string saved;
            strprintf(saved, ", optimization saved 0x" PR_X64 "/" PR_D64 " bytes", bytes_optimized, bytes_optimized);
            dprintf("Pass %2d (final%s)\n", pass_count, (bytes_optimized) ? saved.c_str() : "");
#endif
            last_size_generated = total_size_generated;
            break;
        }
    }

    return 0;
}

int32_t xlasm::check_undefined()
{
    for (auto it = symbols.begin(); it != symbols.end(); ++it)
    {
        auto & sym = it->second;
        if (sym.type == symbol_t::UNDEFINED)
        {
            ctxt.file = sym.file_first_referenced;
            ctxt.line = sym.line_first_referenced;
            error("Undefined symbol \"%s\" first referenced here", sym.name.c_str());
        }
    }

    return 0;
}

void xlasm::update_crc16(uint8_t x)
{
    uint16_t crc = static_cast<uint16_t>(crc_value);
    x ^= crc >> 8;
    x ^= x >> 4;
    crc = static_cast<uint16_t>(crc << 8);
    crc ^= x;
    crc ^= x << 5;
    crc ^= x << 12;
    crc_value = static_cast<uint16_t>(crc & 0xffff);
}

void xlasm::update_crc32(uint8_t x)
{
    uint32_t crc, mask;

    crc = crc_value;

    crc = crc ^ x;
    for (int32_t j = 7; j >= 0; j--)
    {
        mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1)));
        crc  = (crc >> 1) ^ (0xEDB88320 & mask);
    }

    crc_value = ~crc;
}

static void C_dump(FILE * out, const uint8_t * mem, size_t num)
{
    fprintf(out, "    ");
    for (size_t i = 0; i < num; i += 2)
    {
        fprintf(out, "0x%02x%02x", mem[i], mem[i + 1]);
        if (i != num - 2)
        {
            fprintf(out, ", ");
            if (((i >> 1) & 0x7) == 0x7)
            {
                fprintf(out, "\n    ");
            }
        }
    }
    fprintf(out, "\n");
}

static void vsim_dump(FILE * out, const uint8_t * mem, size_t num)
{
    for (size_t i = 0; i < num; i += 2)
    {
        fprintf(out, "    REG_W(XDATA, 0x%02x%02x),", mem[i], mem[i + 1]);

        if ((i & 0xf) == 0)
        {
            fprintf(out, "        // @ 0x%04zx", i >> 1);
        }
        fprintf(out, "\n");
    }
}

static void mem_dump(FILE * out, const uint8_t * mem, size_t num)
{
    for (size_t i = 0; i < num; i += 2)
    {
        fprintf(out, "%02x%02x", mem[i], mem[i + 1]);

        if ((i & 0xf) == 0)
        {
            fprintf(out, "        // @ 0x%04zx", i >> 1);
        }
        fprintf(out, "\n");
    }
}

int32_t xlasm::process_output()
{
    std::vector<section_t *> secs;

    // collect output
    for (auto it = sections.begin(); it != sections.end(); ++it)
    {
        auto & sec = it->second;
        if (!sec.data.size())
            continue;

        secs.push_back(&sec);
    }

    std::sort(std::begin(secs), std::end(secs), [](const section_t * lhs, const section_t * rhs) {
        return (lhs->load_addr == rhs->load_addr) ? lhs->name < rhs->name : lhs->load_addr < rhs->load_addr;
    });

    if (!secs.size())
    {
        dprintf("No output generated.\n");
        return 0;
    }

//    if (listing_file)
//        fprintf(listing_file, "\nOutput sections:\n");

    int64_t pad           = 0;
    int64_t total_size    = 0;
    int64_t cur_load_addr = secs[0]->load_addr;
    int32_t i             = 0;
    for (auto it : secs)
    {
        if (!(it->flags & section_t::NOLOAD_FLAG))
        {
            if (it->load_addr > cur_load_addr)
                pad += it->load_addr - cur_load_addr;
#if 0
            if (!object_filename.size())
            {
                std::string secline;

                strprintf(secline,
                          "Section #%d \"%s\" 0x" PR_X64 "-0x" PR_X64 " (0x" PR_XSIZET "/" PR_DSIZET " bytes)%s%s%s\n",
                          i,
                          it->name.c_str(),
                          static_cast<uint64_t>(it->load_addr),
                          (static_cast<uint64_t>(it->load_addr) + it->data.size() - (it->data.size() ? 1 : 0)),
                          it->data.size(),
                          it->data.size(),
                          it->flags & section_t::NOLOAD_FLAG ? " NOLOAD" : "",
                          it->flags & section_t::FUNCTION_FLAG ? " FUNC" : "",
                          it->flags & section_t::REFERENCED_FLAG ? " REFERENCED" : "");

                dprintf("%s", secline.c_str());
                if (listing_file)
                    fprintf(listing_file, "%s", secline.c_str());
            }
#endif
            cur_load_addr = it->load_addr + static_cast<int64_t>(it->data.size());

            total_size = pad + static_cast<int64_t>(it->data.size());
        }
        i += 1;
    }

    enum class output_format
    {
        NONE,
        C_FILE,
        VSIM_FILE,
        MEM_FILE,
        BIN_FILE
    } out_fmt = output_format::NONE;

    std::string basename  = object_filename;
    std::string extension = object_filename;

    size_t dir_found = basename.find('/');
    if (dir_found == std::string::npos)
    {
        dir_found = basename.find('\\');
    }
    if (dir_found != std::string::npos)
    {
        basename = basename.substr(dir_found + 1, std::string::npos);
    }
    size_t ext_found = basename.find('.');
    if (ext_found != std::string::npos)
    {
        extension = basename.substr(ext_found, std::string::npos);
        basename.resize(ext_found);
    }

    bool header_file = false;
    crc_value        = 0xffffffff;

    if (pad != 0)
    {
        fatal_error("Generated " PR_D64 " unexpected pad bytes.", pad);
    }

    if (total_size & 1)
    {
        fatal_error("Generated unexpected odd file size of " PR_D64 " bytes", total_size);
    }

    if (secs.size() > 1)
    {
        fatal_error("Expected a single segment (" PR_DSIZET " were generated)", secs.size());
    }

    int64_t load_addr = secs[0]->load_addr;

    if (!object_filename.size())
    {
        out_fmt = output_format::NONE;
        dprintf("Dry run - no output file: " PR_D64 " 16-bit words were generated.\n", total_size >> 1);
    }
    else if (extension == ".c" || extension == ".cpp" || extension == ".h")
    {
        header_file = extension == ".h";
        out_fmt     = output_format::C_FILE;
        dprintf("Writing C file \"%s\": uint16_t %s[" PR_D64 "];\n",
                object_filename.c_str(),
                basename.c_str(),
                total_size >> 1);
    }
    else if (extension == ".vsim.h")
    {
        out_fmt = output_format::VSIM_FILE;
        dprintf("Writing vsim C fragment \"%s\" (with " PR_D64 " 16-bit words).\n",
                object_filename.c_str(),
                total_size >> 1);
    }
    else if (extension == ".memh" || extension == ".mem")
    {
        out_fmt = output_format::MEM_FILE;
        dprintf(
            "Writing Verilog file \"%s\" (with " PR_D64 " 16-bit words).\n", object_filename.c_str(), total_size >> 1);
    }
    else        // otherwise, assume binary output
    {
        out_fmt = output_format::BIN_FILE;
        dprintf("Writing binary file \"%s\": " PR_D64 " 16-bit words.\n", object_filename.c_str(), total_size >> 1);
    }

    FILE *      out       = nullptr;
    std::string baseupper = basename;
    std::transform(baseupper.begin(), baseupper.end(), baseupper.begin(), uppercase);

    // prepare output start
    switch (out_fmt)
    {
        case output_format::NONE:
            break;
        case output_format::C_FILE: {
            out = fopen(object_filename.c_str(), "w");
            if (!out)
                fatal_error("opening output file \"%s\", error: %s", object_filename.c_str(), strerror(errno));
            fprintf(out, "// Xosera copper binary \"%s\"\n", basename.c_str());

            fprintf(out, "#if !defined(INC_%s_%c)\n", baseupper.c_str(), header_file ? 'H' : 'C');
            fprintf(out, "#define INC_%s_%c\n", baseupper.c_str(), header_file ? 'H' : 'C');
            fprintf(out, "#include <stdint.h>\n");
            fprintf(out, "\n");
            fprintf(out,
                    "static const uint16_t %s_start __attribute__ ((unused)) = 0x" PR_X64_04
                    ";    // copper program XR start addr\n",
                    basename.c_str(),
                    load_addr);
            fprintf(out,
                    "static const uint16_t %s_size  __attribute__ ((unused)) = %6" PRId64
                    ";    // copper program size in words\n",
                    basename.c_str(),
                    total_size >> 1);
            fprintf(out,
                    "static uint16_t %s_bin[" PR_D64 "] __attribute__ ((unused)) =\n",
                    basename.c_str(),
                    total_size >> 1);
            fprintf(out, "{\n");
        }
        break;
        case output_format::VSIM_FILE: {
            out = fopen(object_filename.c_str(), "w");
            if (!out)
                fatal_error("opening output file \"%s\", error: %s", object_filename.c_str(), strerror(errno));
            fprintf(out, "// Xosera copper binary \"%s\"\n", basename.c_str());
            fprintf(out, "// vsim C fragment with " PR_D64 " 16-bit words\n", total_size >> 1);
            fprintf(out, "    REG_W(WR_XADDR, 0x" PR_X64_04 "),\n", load_addr);
        }
        break;
        case output_format::MEM_FILE: {
            out = fopen(object_filename.c_str(), "w");
            if (!out)
                fatal_error("opening output file \"%s\", error: %s", object_filename.c_str(), strerror(errno));
            fprintf(out, "// Xosera copper binary \"%s\"\n", basename.c_str());
            fprintf(out, "// " PR_D64 " 16-bit words\n", total_size >> 1);
        }
        break;
        case output_format::BIN_FILE: {
            out = fopen(object_filename.c_str(), "wb");
            if (!out)
                fatal_error("opening output file \"%s\", error: %s", object_filename.c_str(), strerror(errno));
        }
        break;
        default:
            assert(false);
    }

    cur_load_addr = secs[0]->load_addr;
    i             = 0;
    for (auto it : secs)
    {
        if (it->flags & section_t::NOLOAD_FLAG)
        {
            dprintf("Skipping section #%d \"%s\" 0x" PR_X64 "-0x" PR_X64 " (0x" PR_XSIZET "/" PR_DSIZET " bytes)%s\n",
                    i,
                    it->name.c_str(),
                    it->load_addr,
                    it->load_addr + static_cast<int64_t>(it->data.size()) - (it->data.size() == 0 ? 0 : 1),
                    it->data.size(),
                    it->data.size(),
                    it->flags & section_t::NOLOAD_FLAG ? " NOLOAD" : "");
        }
        else
        {

            if (it->load_addr - cur_load_addr)
                pad = it->load_addr - cur_load_addr;
            else
                pad = 0;

            if (pad)
            {
                assert(false);        // no pad expected here
#if 0
                dprintf("(writing " PR_D64 " pad bytes)\n", pad);
                while (pad--)
                {
                    update_crc16(0);
                    if (out)
                    {
                        fputc(0, out);
                        if (ferror(out))
                            fatal_error("writing output file \"%s\", error: %s", object_filename.c_str(), strerror(errno));
                    }
                }
#endif
            }

            dprintf("Writing section #%d \"%s\" 0x" PR_X64 "-0x" PR_X64 " (0x" PR_XSIZET "/" PR_DSIZET " words)%s\n",
                    i,
                    it->name.c_str(),
                    it->load_addr,
                    it->load_addr + static_cast<int64_t>(it->data.size() >> 1) - (it->data.size() == 0 ? 0 : 1),
                    it->data.size() >> 1,
                    it->data.size() >> 1,
                    it->flags & section_t::NOLOAD_FLAG ? " NOLOAD" : "");

            // write output body
            if (out)
            {
                switch (out_fmt)
                {
                    case output_format::NONE:
                        break;
                    case output_format::C_FILE: {
                        C_dump(out, it->data.data(), it->data.size());
                    }
                    break;
                    case output_format::VSIM_FILE: {
                        vsim_dump(out, it->data.data(), it->data.size());
                    }
                    break;
                    case output_format::MEM_FILE: {
                        mem_dump(out, it->data.data(), it->data.size());
                    }
                    break;
                    case output_format::BIN_FILE: {
                        if (fwrite(it->data.data(), it->data.size(), 1, out) != 1)
                            fatal_error("writing binary output file \"%s\", error: %s",
                                        object_filename.c_str(),
                                        strerror(errno));
                    }
                    break;
                    default:
                        assert(false);
                        break;
                }
            }

            for (auto cit = it->data.begin(); cit != it->data.end(); ++cit)
                update_crc32(*cit);

            cur_load_addr = it->load_addr + static_cast<int64_t>(it->data.size());
        }

        // close output file
        if (out)
        {
            switch (out_fmt)
            {
                case output_format::NONE:
                    break;
                case output_format::C_FILE: {
                    fprintf(out, "};\n");
                    if (exports.size())
                    {
                        for (auto expsym : exports)
                        {
                            symbol_t sym = symbols[expsym];
                            if (sym.type != symbol_t::UNDEFINED)
                                fprintf(out,
                                        "static const uint16_t %s__%s  __attribute__ ((unused)) = %6" PRId64
                                        "; // 0x%04" PRIx64 "\n",
                                        basename.c_str(),
                                        sym.name.c_str(),
                                        sym.value - load_addr,
                                        sym.value);
                        }

                        fprintf(out,
                                "static const uint16_t %s_export_size  __attribute__ ((unused)) = " PR_DSIZET ";\n",
                                basename.c_str(),
                                exports.size());
                        fprintf(out,
                                "static const uint16_t %s_export[" PR_DSIZET "]  __attribute__ ((unused)) = {\n",
                                basename.c_str(),
                                exports.size());

                        size_t last_count = exports.size();
                        for (auto expsym : exports)
                        {
                            --last_count;
                            fprintf(
                                out, "    %s__%s%s\n", basename.c_str(), expsym.c_str(), last_count == 0 ? "" : ",");
                        }
                        fprintf(out, "};\n");
                    }
                    fprintf(out, "#endif // INC_%s_%c\n", baseupper.c_str(), header_file ? 'H' : 'C');
                }
                break;
                case output_format::VSIM_FILE:        // nothing more to do here
                    break;
                case output_format::MEM_FILE:        // nothing more to do here
                    break;
                case output_format::BIN_FILE:        // nothing more to do here
                    break;
                default:
                    assert(false);
            }
        }
        fclose(out);
        out = nullptr;
    }

    dprintf("Total output size " PR_D64 " bytes, CRC-32: 0x%08x, effective lines %d.\n",
            total_size,
            crc_value,
            virtual_line_num);

    return 0;
}


int32_t xlasm::process_file(source_t & f)
{
    int32_t rc = 0;

    // reset context per file (except section and state)
    ctxt.conditional.state   = 1;
    ctxt.conditional.wastrue = 1;
    ctxt.conditional_nesting = 0;
    ctxt.line                = 0;
    ctxt.file                = &f;

    // iterate over all lines in file
    for (ctxt.line = 0; ctxt.line < f.src_line.size(); ctxt.line++)
    {
        rc = process_line();
        if (rc)
            break;

        if (error_count >= MAXERROR_COUNT)
        {
            force_exit_assembly = true;
            force_end_file      = true;
        }

        if (force_end_file || force_exit_assembly)
            break;
    }
    force_end_file = false;

    if (error_count >= MAXERROR_COUNT)
    {
        error("Exiting due to maximum error count (%d)", error_count);
        exit(10);
    }

    if (func_section != nullptr)
        error("Ending file inside FUNC");

    if (ctxt.conditional_nesting != 0)
        warning("Ending file inside conditional IF block");

    return rc;
}

int32_t xlasm::process_line()
{
    int32_t rc = 0;

    const std::vector<std::string> & tokens = ctxt.file->src_line[ctxt.line];

    if (opt.verbose > 3 && tokens.size())
    {
        std::string tokdbg;
        for (auto it = tokens.begin(); it != tokens.end(); ++it)
        {
            tokdbg += "|";
            tokdbg += *it;
            tokdbg += "|";
            if (it + 1 != tokens.end())
                tokdbg += " ";
        }
#if 0
		notice(0, "LINE: %s", tokdbg.c_str());
#endif
#if 1
        dprintf("LINE: %s\n", tokdbg.c_str());
#endif
    }

    std::string label;
    std::string command;
    size_t      cur_token = 0;

    undefined_sym_count  = 0;
    undefined_section    = ctxt.section;
    undefined_begin_size = undefined_section->data.size();

    if (!suppress_line_list && (ctxt.macroexp_ptr == nullptr || !opt.suppress_macro_expansion))
    {
        line_sec_org   = false;
        line_sec_start = ctxt.section;
        line_sec_addr  = line_sec_start->addr;
        line_sec_size  = line_sec_start->data.size();
    }

    // if more tokens, look for directive/mnemonic
    for (; cur_token < tokens.size(); cur_token++)
    {
        auto & tok = tokens[cur_token];

        // parse a label if this line defines one
        if (cur_token == 0 && tok.size() && tok.back() == ':')
        {
            label.assign(tok.begin(), tok.end() - 1);        // remove colon
            continue;
        }

        if (tok[0] == '.')
            command.assign(tok.begin() + 1, tok.end());
        else
            command.assign(tok.begin(), tok.end());

        // make keyword uppercase for comparison and to stand out in error messages
        std::transform(command.begin(), command.end(), command.begin(), uppercase);

        // check architecture directives
        uint32_t directive_idx = arch->check_directive(command);

        // if arch known directive, let arch handle it
        if (directive_idx >= NUM_DIRECTIVES)
        {
            cur_token++;
            rc = arch->process_directive(this, directive_idx, command, label, cur_token, tokens);
            label.clear();
            break;
        }

        if (directive_idx == DIR_UNKNOWN)
        {
            // check standard directives
            auto it = directives.find(command);
            if (it != directives.end())
            {
                directive_idx = it->second;
            }
        }

        // if known directive, or we are defining a macro (in which case DIR_UNKNOWN is okay)
        if (directive_idx != DIR_UNKNOWN || ctxt.macrodef_ptr != nullptr)
        {
            cur_token++;
            rc = process_directive(directive_idx, command, label, cur_token, tokens);
            label.clear();
            break;
        }

        // if current conditional state false, skip this line
        if (!ctxt.conditional.state)
            break;

        // check for macro invocation
        if (macros.count(command) == 1)
        {
            if (label.size())
            {
                process_labeldef(label);
                label.clear();
            }

            if (context_stack.size() > MAXMACRO_STACK)
            {
                fatal_error("%s:%d: Exceeded MACRO nesting depth of %d levels",
                            ctxt.file->name.c_str(),
                            ctxt.line + ctxt.file->line_start,
                            MAXMACRO_STACK);
            }

            if (!opt.suppress_macro_expansion && listing_file)
                process_line_listing();
            context_stack.push(ctxt);

            cur_token++;
            source_t & m = expand_macro(command, cur_token, tokens);

            process_file(m);
            ctxt = context_stack.top();
            context_stack.pop();

            if (!opt.suppress_macro_expansion)
                suppress_line_list = true;

            notice(3, "Resuming after MACRO \"%s\"", command.c_str());
            break;
        }

        // check if it is an architecture opcode
        int32_t opcode_idx = arch->check_opcode(command);
        if (opcode_idx != -1)
        {
            if (label.size())
            {
                process_labeldef(label);
                label.clear();
            }

            cur_token++;
            rc = arch->process_opcode(this, opcode_idx, command, cur_token, tokens);

            break;
        }

        // check if a label definition (without colon)
        if (arch->column_one_labels() && cur_token == 0)
        {
            label.assign(tok.begin(), tok.end());
        }
        else
        {
            error("Unrecognized directive or %s instruction \"%s\"", arch->get_variant().c_str(), tok.c_str());
            break;
        }
    }

    if (label.size() && ctxt.conditional.state)        // if only a label was present & conditional true
    {
        process_labeldef(label);
    }

    if (listing_file)
        process_line_listing();

    if (error_count >= MAXERROR_COUNT)
    {
        force_exit_assembly = true;
        force_end_file      = true;
    }
    virtual_line_num++;

    return rc;
}

int32_t xlasm::process_line_listing()
{
    assert(listing_file);

    // listing
    if (suppress_line_list || (ctxt.macroexp_ptr != nullptr && opt.suppress_macro_expansion) ||
        (!ctxt.conditional.state && opt.suppress_false_conditionals))
    {
        suppress_line_list = false;
        std::string outline;
        for (auto it = pre_messages.begin(); it != pre_messages.end(); ++it)
        {
            strprintf(outline, "       ");
            strprintf(outline, "      ");

            strprintf(outline, "%s\n", it->c_str());
        }
        pre_messages.clear();
        for (auto it = post_messages.begin(); it != post_messages.end(); ++it)
        {
            strprintf(outline, "       ");
            strprintf(outline, "      ");

            strprintf(outline, "%s\n", it->c_str());
        }
        post_messages.clear();
        if (outline.size())
            fputs(outline.c_str(), listing_file);
        return 0;
    }

    if (ctxt.pass == context_t::PASS_2 && opt.listing && ctxt.file)
    {
        std::string outline;
        bool        show_value        = false;
        bool        show_section_name = false;

        if (!line_last_file || line_last_file->name != ctxt.file->name)
        {
            strprintf(outline, "                    // File: %s\n", ctxt.file->name.c_str());
            line_last_file = ctxt.file;
        }

        for (auto it = pre_messages.begin(); it != pre_messages.end(); ++it)
        {
            strprintf(outline, "       ");
            strprintf(outline, "      ");

            strprintf(outline, "%s\n", it->c_str());
        }
        pre_messages.clear();

        if (line_sec_start != ctxt.section)
            show_section_name = true;
#if 0   // mem friendly list
        if (!opt.suppress_line_numbers)
        {
            if (suppress_line_listsource)
                strprintf(outline, "       ");
            else
                strprintf(outline, "%6d ", ctxt.line + ctxt.file->line_start);
        }

        int64_t v = 0;
        if (sym_defined != nullptr && sym_defined->type != symbol_t::UNDEFINED && sym_defined->type != symbol_t::STRING)
        {
            v          = sym_defined->value;
            show_value = true;
        }
        else if (line_sec_start != ctxt.section || line_sec_addr != ctxt.section->addr || line_sec_org)
        {
            v          = ctxt.section->addr + static_cast<int64_t>(ctxt.section->data.size() >> 1);
            show_value = true;
        }

        if (line_sec_start == ctxt.section && line_sec_addr == ctxt.section->addr &&
            line_sec_size != ctxt.section->data.size())
        {
            strprintf(outline, PR_X64_04 ": ", line_sec_addr + static_cast<int64_t>(line_sec_size >> 1));
        }
        else if (show_value || line_sec_start != ctxt.section || line_sec_addr != ctxt.section->addr)
        {
            strprintf(outline, PR_X64_04 "= ", static_cast<uint64_t>(v));
        }
        else
        {
            strprintf(outline, "      ");
        }
#endif
        if (show_section_name)
        {
            std::string secname;
            strprintf(secname, "[%.22s]", ctxt.section->name.c_str());
            strprintf(outline, "%s", secname.c_str());
        }
        else if (!ctxt.conditional.state)
        {
            strprintf(outline, "%-18.18s", "<false>");        // 22?
        }
        else
        {
#if 0
			for (size_t i = 0; i < 8; i++)
			{
				if (line_sec_start == ctxt.section && line_sec_size+i < ctxt.section->data.size())
				{
					if (ctxt.section->flags & section_t::NOLOAD_FLAG)
						strprintf(outline, "..");
					else
						strprintf(outline, "%02X", ctxt.section->data[line_sec_size+i]);
				}
				else
					strprintf(outline, "  ");
			}
#else
            for (size_t i = 0; i < 8;)
            {
                if (line_sec_start == ctxt.section && line_sec_size + i + 4 <= ctxt.section->data.size())
                {
                    if (ctxt.section->flags & section_t::NOLOAD_FLAG)
                        strprintf(outline, "........");
                    else
                    {
                        strprintf(outline,
                                  "%02X%02X %02X%02X ",
                                  ctxt.section->data[line_sec_size + i],
                                  ctxt.section->data[line_sec_size + i + 1],
                                  ctxt.section->data[line_sec_size + i + 2],
                                  ctxt.section->data[line_sec_size + i + 3]);
                    }

                    i += 4;
                }
                else if (line_sec_start == ctxt.section && line_sec_size + i + 2 <= ctxt.section->data.size())
                {
                    if (ctxt.section->flags & section_t::NOLOAD_FLAG)
                        strprintf(outline, "....");
                    else
                    {
                        strprintf(outline,
                                  "%02X%02X ",
                                  ctxt.section->data[line_sec_size + i],
                                  ctxt.section->data[line_sec_size + i + 1]);
                    }
                    i += 2;
                }
                else if (line_sec_start == ctxt.section && line_sec_size + i < ctxt.section->data.size())
                {
                    assert(false);
                    if (ctxt.section->flags & section_t::NOLOAD_FLAG)
                        strprintf(outline, "..");
                    else
                        strprintf(outline, "%02X", ctxt.section->data[line_sec_size + i]);

                    i += 1;
                }
                else
                {
                    strprintf(outline, "  %s", (i & 1) ? " " : "");
                    i += 1;
                }
            }
#endif
        }

#if 1   // mem friendly list
        if (suppress_line_listsource)
            strprintf(outline, "//       ");
        else
            strprintf(outline, "// %6d ", ctxt.line + ctxt.file->line_start);

        int64_t v = 0;
        if (sym_defined != nullptr && sym_defined->type != symbol_t::UNDEFINED && sym_defined->type != symbol_t::STRING)
        {
            v          = sym_defined->value;
            show_value = true;
        }
        else if (line_sec_start != ctxt.section || line_sec_addr != ctxt.section->addr || line_sec_org)
        {
            v          = ctxt.section->addr + static_cast<int64_t>(ctxt.section->data.size() >> 1);
            show_value = true;
        }

        if (line_sec_start == ctxt.section && line_sec_addr == ctxt.section->addr &&
            line_sec_size != ctxt.section->data.size())
        {
            strprintf(outline, PR_X64_04 ": ", line_sec_addr + static_cast<int64_t>(line_sec_size >> 1));
        }
        else if (show_value || line_sec_start != ctxt.section || line_sec_addr != ctxt.section->addr)
        {
            strprintf(outline, PR_X64_04 "= ", static_cast<uint64_t>(v));
        }
        else
        {
            strprintf(outline, "      ");
        }
#endif

        if (!suppress_line_listsource)
        {
            strprintf(outline, "\t%s", ctxt.file->orig_line[ctxt.line].c_str());
        }
        else
            strprintf(outline, "\t<alignment pad>");

        if (opt.listing_bytes > 8 && line_sec_start == ctxt.section && line_sec_size + 8 < ctxt.section->data.size())
        {
            uint64_t line_beg_addr = 0;
            for (uint32_t i = 8; i < opt.listing_bytes; i++)
            {
                if (((i - 8) & 0x7) == 0 && line_sec_size + i < ctxt.section->data.size())
                {
                    strprintf(outline, "\n");
                    line_beg_addr = line_sec_addr + (static_cast<int64_t>(line_sec_size + i) >> 1);
                }
                if (line_sec_size + i < ctxt.section->data.size())
                {
                    if (ctxt.section->flags & section_t::NOLOAD_FLAG)
                        strprintf(outline, "..");
                    else
                    {
                        strprintf(outline, "%02X%s", ctxt.section->data[line_sec_size + i], (i & 1) ? " " : "");
                    }
                }
                if (((i - 8) & 0x7) == 7 && line_sec_size + i < ctxt.section->data.size())
                {
                    strprintf(outline, "//        " PR_X64_04 ": ", line_beg_addr);
                }
            }

            if (line_sec_size + opt.listing_bytes < ctxt.section->data.size())
                strprintf(outline, "+");
        }

        outline += '\n';

        for (auto it = post_messages.begin(); it != post_messages.end(); ++it)
        {
            strprintf(outline, "       ");
            strprintf(outline, "      ");

            strprintf(outline, "%s\n", it->c_str());
        }
        post_messages.clear();

        fputs(outline.c_str(), listing_file);
    }

    sym_defined = nullptr;

    return 0;
}

static bool comp_xref_name(const xlasm::symbol_t * lhs, const xlasm::symbol_t * rhs)
{
    return lhs->name < rhs->name;
}

static bool comp_xref_value(const xlasm::symbol_t * lhs, const xlasm::symbol_t * rhs)
{
    return lhs->value < rhs->value;
}

int32_t xlasm::process_xref()
{
    if (!listing_file)
        return 0;

    std::vector<const symbol_t *> sym_xref;

    for (auto it = symbols.begin(); it != symbols.end(); ++it)
    {
        auto & sym = it->second;

        if (sym.type == symbol_t::INTERNAL)
            continue;

        if (sym.type == symbol_t::STRING)
        {
            expression expr;
            int64_t    result = -1;
            expr.evaluate(this, sym.str.c_str(), &result);
            sym.value = result;
        }
        sym_xref.push_back(&sym);
    }

    std::sort(sym_xref.begin(), sym_xref.end(), comp_xref_name);

    fprintf(listing_file, "\n\nSymbols (sorted by name):\n\n");

    for (auto it = std::begin(sym_xref); it != std::end(sym_xref); ++it)
    {
        std::string outline;
        std::string valstr;

        const symbol_t * sym = *it;

        if (sym->type == symbol_t::REGISTER)
        {
            valstr = sym->str;
        }
        else
        {
            strprintf(valstr, "0x" PR_X64 " / " PR_D64 "", sym->value, sym->value);
        }
        strprintf(outline, "%s %-32.32s = %-32.32s", sym->type_abbrev(), sym->name.c_str(), valstr.c_str());
        if (sym->type == symbol_t::STRING)
            strprintf(outline, "\"%.64s\"", sym->str.c_str());
        strprintf(outline, "\n");

        fputs(outline.c_str(), listing_file);
    }

    std::sort(std::begin(sym_xref), std::end(sym_xref), comp_xref_value);

    fprintf(listing_file, "\n\nSymbols (sorted by value):\n\n");

    for (auto it = std::begin(sym_xref); it != std::end(sym_xref); ++it)
    {
        std::string outline;
        std::string valstr;

        const symbol_t * sym = *it;

        if (sym->type == symbol_t::REGISTER)
        {
            valstr = sym->str;
        }
        else
        {
            strprintf(valstr, "0x" PR_X64 " / " PR_D64 "", sym->value, sym->value);
        }
        strprintf(outline, "%s %-32.32s = %-32.32s", sym->type_abbrev(), sym->name.c_str(), valstr.c_str());
        if (sym->type == symbol_t::STRING)
            strprintf(outline, "\"%.64s\"", sym->str.c_str());
        strprintf(outline, "\n");

        fputs(outline.c_str(), listing_file);
    }

    return 0;
}

int32_t xlasm::process_directive(uint32_t                         idx,
                                 const std::string &              directive,
                                 const std::string &              label,
                                 size_t                           cur_token,
                                 const std::vector<std::string> & tokens)
{
    // macro directives first (only processed if current conditional true)
    if (ctxt.conditional.state)
    {
        switch (idx)
        {
            // MACRO ===============================
            case DIR_MACRO: {
                return define_macro_begin(directive, label, cur_token, tokens);
            }

            // ENDMACRO ===============================
            case DIR_ENDMACRO: {
                return define_macro_end(directive, label, cur_token, tokens);
            }

            default:
                break;
        }
    }

    // if currently defining a macro, only save other directives/opcodes for processing when macro is invoked
    if (ctxt.macrodef_ptr != nullptr)
    {
        ctxt.macrodef_ptr->body.src_line.push_back(tokens);
        ctxt.macrodef_ptr->body.file_size += ctxt.file->orig_line[ctxt.line].size();

        return 0;
    }

    // directives processed even if current conditional false
    switch (idx)
    {
        // IF ===============================
        case DIR_IF: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            condition_stack.push(ctxt.conditional);

            ctxt.conditional.state   = (result != 0) ? 1U : 0U;
            ctxt.conditional.wastrue = ctxt.conditional.state;
            ctxt.conditional_nesting++;

            notice(2,
                   "conditional %s (%s) is %s",
                   directive.c_str(),
                   exprstr.c_str(),
                   ctxt.conditional.state ? "true" : "false");

            return 0;
        }

        // IFSTREQ/IFSTRNE/IFSTREQI/IFSTRNEI ===============================
        case DIR_IFSTR:
        case DIR_IFSTRI: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            if ((tokens.size() - cur_token) != 3)
            {
                error("Directive %s requires two string arguments separated by string operator", directive.c_str());
                break;
            }

            static const char * str_ops[] = {"==", "!=", "<", "<=", ">", ">=", "contains"};

            int32_t str_op = -1;
            for (size_t i = 0; i < (sizeof(str_ops) / sizeof(str_ops[0])); i++)
            {
                if (tokens[cur_token + 1] == str_ops[i])
                {
                    str_op = static_cast<int32_t>(i);
                    break;
                }
            }

            if (str_op < 0)
            {
                error("Directive %s requires operator ==, !=, <, <=, >, >= or \"contains\"", directive.c_str());
                break;
            }

            std::string exprstr1;
            if (tokens[cur_token][0] == '\"' || tokens[cur_token][0] == '\'')
            {
                exprstr1 = removeQuotes(tokens[cur_token]);
            }
            else
            {
                symbol_t & sym = symbols[tokens[cur_token]];
                if (sym.type == symbol_t::UNDEFINED)
                {
                    if (!sym.name.size())
                        sym.name = tokens[cur_token];
                    //					if (ctxt.pass == context_t::PASS_2)
                    //						warning("Evaluating undefined string symbol \"%s\" as \"\"",
                    // sym.name.c_str());

                    if (!sym.file_first_referenced)
                    {
                        sym.file_first_referenced = ctxt.file;
                        sym.line_first_referenced = ctxt.line;
                    }
                    undefined_sym_count++;
                }
                else if (sym.type == symbol_t::STRING)
                    exprstr1 = sym.str;
                else if (ctxt.pass == context_t::PASS_2)
                    warning("Evaluating non-string symbol \"%s\" as \"\"", sym.name.c_str());
            }

            std::string exprstr2;
            if (tokens[cur_token + 2][0] == '\"' || tokens[cur_token + 2][0] == '\'')
            {
                exprstr2 = removeQuotes(tokens[cur_token + 2]);
            }
            else
            {
                symbol_t & sym = symbols[tokens[cur_token + 2]];
                if (sym.type == symbol_t::UNDEFINED)
                {
                    if (!sym.name.size())
                        sym.name = tokens[cur_token + 2];
                    //					if (ctxt.pass == context_t::PASS_2)
                    //						warning("Evaluating undefined string symbol \"%s\" as \"\"",
                    // sym.name.c_str());

                    if (!sym.file_first_referenced)
                    {
                        sym.file_first_referenced = ctxt.file;
                        sym.line_first_referenced = ctxt.line;
                    }
                    undefined_sym_count++;
                }
                else if (sym.type == symbol_t::STRING)
                    exprstr2 = sym.str;
                else if (ctxt.pass == context_t::PASS_2)
                    warning("Evaluating non-string symbol \"%s\" as \"\"", sym.name.c_str());
            }

            if (idx == DIR_IFSTRI)
            {
                std::transform(exprstr1.begin(), exprstr1.end(), exprstr1.begin(), uppercase);
                std::transform(exprstr2.begin(), exprstr2.end(), exprstr2.begin(), uppercase);
            }

            bool result = false;

            switch (str_op)
            {
                case 0:        // ==
                    result = (exprstr1 == exprstr2);
                    break;
                case 1:        // !=
                    result = (exprstr1 != exprstr2);
                    break;
                case 2:        // <
                    result = (exprstr1 < exprstr2);
                    break;
                case 3:        // <=
                    result = (exprstr1 <= exprstr2);
                    break;
                case 4:        // >
                    result = (exprstr1 > exprstr2);
                    break;
                case 5:        // >=
                    result = (exprstr1 >= exprstr2);
                    break;
                case 6:        // ~=
                case 7:        // contains
                    result = (exprstr1.find(exprstr2) != std::string::npos);
                    break;
                default:
                    assert(false);
                    break;
            }

            condition_stack.push(ctxt.conditional);

            ctxt.conditional.state   = (result != 0) ? 1U : 0U;
            ctxt.conditional.wastrue = ctxt.conditional.state;
            ctxt.conditional_nesting++;

            notice(2,
                   "conditional %s (%s %s %s) is %s",
                   directive.c_str(),
                   exprstr1.c_str(),
                   str_ops[str_op],
                   exprstr2.c_str(),
                   ctxt.conditional.state ? "true" : "false");

            return 0;
        }

        // ELSEIF ===============================
        case DIR_ELSEIF: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            if (condition_stack.empty())
            {
                error("%s encountered outside IF/ENDIF block", directive.c_str());
                break;
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            ctxt.conditional.state = !(ctxt.conditional.wastrue) && (result != 0) ? 1U : 0U;
            ctxt.conditional.wastrue |= ctxt.conditional.state;

            notice(2,
                   "conditional %s (%s) is %s",
                   directive.c_str(),
                   exprstr.c_str(),
                   ctxt.conditional.state ? "true" : "false");

            return 0;
        }

        // ELSE ===============================
        case DIR_ELSE: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            if (tokens.size() - cur_token != 0)
            {
                error("" PR_DSIZET " extra token%s after %s",
                      (tokens.size() - cur_token),
                      (tokens.size() - cur_token) == 1 ? "" : "s",
                      directive.c_str());
            }

            if (condition_stack.empty())
            {
                error("%s encountered outside IF/ENDIF block", directive.c_str());
                break;
            }

            ctxt.conditional.state = !(ctxt.conditional.wastrue) ? 1U : 0U;
            ctxt.conditional.wastrue |= ctxt.conditional.state;

            notice(2, "conditional %s is %s", directive.c_str(), ctxt.conditional.state ? "true" : "false");

            return 0;
        }

        // ENDIF ===============================
        case DIR_ENDIF: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            if (tokens.size() - cur_token != 0)
            {
                error("" PR_DSIZET " extra token%s after %s",
                      (tokens.size() - cur_token),
                      (tokens.size() - cur_token) == 1 ? "" : "s",
                      directive.c_str());
            }

            if (condition_stack.empty())
            {
                error("ENDIF encountered without matching IF");
                return 0;
            }

            bool prev_cond = ctxt.conditional.state;

            condition_stack.pop();

            if (condition_stack.empty())
            {
                ctxt.conditional.state   = 1;
                ctxt.conditional.wastrue = 1;
            }
            else
                ctxt.conditional = condition_stack.top();

            ctxt.conditional_nesting--;

            if (!prev_cond && opt.suppress_false_conditionals)
                suppress_line_list = true;

            notice(2, "conditional %s resumes %s", directive.c_str(), ctxt.conditional.state ? "true" : "false");

            return 0;
        }

        default:
            break;
    }

    // ignore any of the following directives if the conditional assembly condition if false
    if (!ctxt.conditional.state)
    {
        return 0;
    }

    switch (idx)
    {
        // LIST ===============================
        case DIR_LIST: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            bool enable = (result != 0) ? true : false;

            opt.listing = enable;

            return 0;
        }

        // LISTMAC ===============================
        case DIR_LISTMAC: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            bool disable = (result != 0) ? false : true;

            opt.suppress_macro_expansion = disable;

            return 0;
        }

        // MACNAME ===============================
        case DIR_MACNAME: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            bool disable = (result != 0) ? false : true;

            opt.suppress_macro_name = disable;

            return 0;
        }

        // LISTCOND ===============================
        case DIR_LISTCOND: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            bool disable = (result != 0) ? false : true;

            opt.suppress_false_conditionals = disable;

            return 0;
        }

        // INCLUDE ===============================
        case DIR_INCLUDE: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            if (ctxt.macrodef_ptr != nullptr)
            {
                error("%s not permitted in MACRO definition", directive.c_str());

                return 0;
            }

            if (tokens.size() - cur_token > 1)
            {
                error("" PR_DSIZET " extra token%s after %s filename",
                      (tokens.size() - cur_token - 1),
                      (tokens.size() - cur_token - 1) == 1 ? "" : "s",
                      directive.c_str());
            }
            else if (tokens.size() - cur_token < 1)
            {
                error("missing %s filename", directive.c_str());

                return 0;
            }

            if (context_stack.size() > MAXINCLUDE_STACK)
            {
                fatal_error("%s:%d: Exceeded %s file nesting depth of %d files",
                            ctxt.file->name.c_str(),
                            ctxt.line + ctxt.file->line_start,
                            directive.c_str(),
                            MAXINCLUDE_STACK);
            }

            std::string basename = removeQuotes(tokens[cur_token]);
            std::string filename = basename;

            source_t & f = source_files[basename];

            int e = f.read_file(this, basename, filename);
            if (e)
            {
                for (int i = 0; i < (int)opt.include_path.size(); i++)
                {
                    filename = opt.include_path[i] + std::string("/") + basename;
                    int ie   = f.read_file(this, basename, filename);

                    if (!ie)
                    {
                        e = ie;
                        break;
                    }
                }
            }

            if (e)
                fatal_error("%s:%d: Error reading %s file \"%s\": %s",
                            ctxt.file->name.c_str(),
                            ctxt.line + ctxt.file->line_start,
                            directive.c_str(),
                            filename.c_str(),
                            strerror(e));

            if (listing_file)
                process_line_listing();

            if (ctxt.pass == context_t::PASS_1 || opt.verbose > 2)
            {
                notice(2,
                       "Including file \"%s\" (" PR_DSIZET " lines, " PR_D64 " bytes)",
                       filename.c_str(),
                       f.orig_line.size(),
                       f.file_size);
            }
            context_stack.push(ctxt);
            process_file(f);
            ctxt = context_stack.top();
            context_stack.pop();
            if (ctxt.pass == context_t::PASS_1 || opt.verbose > 2)
            {
                notice(2, "Resuming after %s of file \"%s\"", directive.c_str(), filename.c_str());
            }

            suppress_line_list = true;

            return 0;
        }

        // EQU ===============================
        case DIR_EQU: {
            if (!label.size())
            {
                error("Expected symbol definition before %s", directive.c_str());

                return 0;
            }

            std::string exprstr;
            int64_t     result = 0;

            if (tokens[cur_token][0] != '\"' && tokens[cur_token][0] != '\'')
                result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);
            else
                exprstr = removeQuotes(tokens[cur_token]);

            if (arch->lookup_register(label) >= 0)
            {
                warning(
                    "Symbol definition: \"%s\" is also a register for %s", label.c_str(), arch->get_variant().c_str());
            }

            symbol_t & sym = symbols[label];

            if (sym.type == symbol_t::UNDEFINED)
            {
                sym.type         = symbol_t::STRING;
                sym.name         = label;
                sym.str          = exprstr;
                sym.line_defined = ctxt.line;
                sym.file_defined = ctxt.file;
                sym.section      = ctxt.section;
                sym.value        = result;

                notice(3,
                       "Defined symbol \"%s\" %s 0x" PR_X64 "/" PR_D64 "",
                       label.c_str(),
                       directive.c_str(),
                       sym.value,
                       sym.value);
            }
            else
            {
                if (sym.type != symbol_t::STRING || sym.line_defined != ctxt.line || sym.file_defined != ctxt.file)
                {
                    error("Duplicate symbol definition: \"%s\" first at %s(%d)",
                          label.c_str(),
                          sym.file_defined->name.c_str(),
                          sym.line_defined);

                    return 0;
                }

                sym.str     = exprstr;
                sym.section = ctxt.section;
                sym.value   = result;
            }
            sym_defined = &sym;

            return 0;
        }

        // ASSIGN (=) ===============================
        case DIR_ASSIGN: {
            if (!label.size())
            {
                error("Expected variable definition before %s", directive.c_str());

                return 0;
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            symbol_t & sym = symbols[label];

            if (sym.type == symbol_t::UNDEFINED)
            {
                sym.type         = symbol_t::VARIABLE;
                sym.name         = label;
                sym.line_defined = ctxt.line;
                sym.file_defined = ctxt.file;
                sym.section      = ctxt.section;
                sym.value        = result;
            }
            else
            {
                if (sym.type != symbol_t::VARIABLE)
                {
                    error("Cannot assign to non-variable: \"%s\" defined at %s(%d)",
                          label.c_str(),
                          sym.file_defined->name.c_str(),
                          sym.line_defined);

                    return 0;
                }

                assert(sym.name == label);
                sym.line_defined = ctxt.line;
                sym.file_defined = ctxt.file;
                sym.section      = ctxt.section;
                sym.value        = result;
            }

            notice(3, "Assigned variable \"%s\" = 0x" PR_X64 "/" PR_D64 "", label.c_str(), sym.value, sym.value);
            sym_defined = &sym;

            return 0;
        }

        // UNDEFINE (UNDEF, UNSET) ===============================
        case DIR_UNDEFINE: {
            if (!label.size())
            {
                error("Expected variable definition before %s", directive.c_str());
                return 0;
            }

            if (tokens.size() - cur_token != 0)
                error("" PR_DSIZET " extra token%s after %s",
                      (tokens.size() - cur_token),
                      (tokens.size() - cur_token) == 1 ? "" : "s",
                      directive.c_str());

            symbol_t::sym_t otype = symbols[label].type;
            std::string     ntype;

            switch (otype)
            {
                case symbol_t::INTERNAL:
                    error("%s used on special symbol \"%s\"", directive.c_str(), label.c_str());
                    return 0;
                case symbol_t::REGISTER:
                    ntype = "user register";
                    return 0;
                case symbol_t::UNDEFINED:
                    ntype = "undefined";
                    break;
                case symbol_t::LABEL:
                    ntype = "label";
                    break;
                case symbol_t::VARIABLE:
                    ntype = "variable";
                    break;
                case symbol_t::COMM:
                    ntype = "common";
                    break;
                case symbol_t::STRING:
                    ntype = "string";
                    break;
                default:
                    assert(false);
            }

            symbols.erase(label);
            notice(3, "%s %s symbol \"%s\"", directive.c_str(), ntype.c_str(), label.c_str());

            return 0;
        }

        // EXPORT ===============================
        case DIR_EXPORT: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
                return 0;
            }

            if (ctxt.pass == context_t::PASS_2)
            {
                do
                {
                    std::string export_label = tokens[cur_token];

                    if (std::find(exports.begin(), exports.end(), export_label) == exports.end())
                    {
                        symbol_t & sym = symbols[export_label];

                        if (sym.type != symbol_t::VARIABLE && sym.type != symbol_t::LABEL)
                        {
                            error("Cannot export symbol not a label or variable: \"%s\"", export_label.c_str());

                            return 0;
                        }

                        notice(3,
                               "Exported variable \"%s\" = 0x" PR_X64 "/" PR_D64 "",
                               export_label.c_str(),
                               sym.value,
                               sym.value);
                        exports.push_back(export_label);
                    }

                    cur_token++;

                    if (cur_token < tokens.size())
                    {
                        assert(tokens[cur_token] == ",");

                        if (cur_token + 1 >= tokens.size())
                            error("%s missing argument after \",\"", directive.c_str());
                    }
                } while (++cur_token < tokens.size());
            }


            return 0;
        }

        // ASSERT ===============================
        case DIR_ASSERT: {
            if (ctxt.pass != context_t::PASS_2)
                return 0;

            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     result = eval_tokens(directive, exprstr, cur_token, tokens, 2, 0);

            if (result == 0)
            {
                std::string msg = token_message(cur_token, tokens);
                error("%s failed (%s)%s%s", directive.c_str(), exprstr.c_str(), msg.size() ? ": " : "", msg.c_str());
            }

            return 0;
        }

        // MSG ===============================
        case DIR_MSG: {
            if (ctxt.pass != context_t::PASS_2)
                return 0;

            std::string msg = token_message(cur_token, tokens);
            notice(1, "%s %s", directive.c_str(), msg.c_str());
            return 0;
        }

        // WARN ===============================
        case DIR_WARN: {
            if (ctxt.pass != context_t::PASS_2)
                return 0;

            std::string msg = token_message(cur_token, tokens);
            warning("%s %s", directive.c_str(), msg.c_str());
            return 0;
        }

        // ERROR ===============================
        case DIR_ERROR: {
            if (ctxt.pass != context_t::PASS_2)
                return 0;

            std::string msg = token_message(cur_token, tokens);
            error("%s %s", directive.c_str(), msg.c_str());
            return 0;
        }

        // EXIT ===============================
        case DIR_EXIT: {
            if (ctxt.pass != context_t::PASS_2)
                return 0;

            std::string msg = token_message(cur_token, tokens);
            error("%s %s", directive.c_str(), msg.c_str());

            force_end_file      = true;
            force_exit_assembly = true;

            return 0;
        }

        case DIR_ORG: {
            if (label.size())
            {
                error("Label definition not permitted on %s", directive.c_str());
            }

            std::string exprstr;
            int64_t     origin = eval_tokens(directive, exprstr, cur_token, tokens, 1, ctxt.section->addr);

            if (!ctxt.section->data.size())
            {
                ctxt.section->load_addr = origin;
            }

            ctxt.section->addr = origin - static_cast<int64_t>(ctxt.section->data.size());
            line_sec_org       = true;

            return 0;
        }


        default:
            break;
    }

    // all directives past here support a "normal" label, so process it here
    if (label.size())
    {
        process_labeldef(label);
    }

    switch (idx)
    {
        // END ===============================
        case DIR_END: {

            if (tokens.size() - cur_token != 0)
                error("" PR_DSIZET " extra token%s after %s",
                      (tokens.size() - cur_token),
                      (tokens.size() - cur_token) == 1 ? "" : "s",
                      directive.c_str());

            bool moretokens = false;
            for (auto it = ctxt.file->src_line.begin() + static_cast<ssize_t>(ctxt.line) + 1;
                 it != ctxt.file->src_line.end();
                 ++it)
            {
                if (it->size() != 0)
                {
                    moretokens = true;
                    break;
                }
            }

            if (moretokens)
            {
                notice(1, "%s encountered with remaining non-comment lines (skipping)", directive.c_str());
            }

            force_end_file = true;
        }
        break;

        // VOID ===============================
        case DIR_VOID: {
            // ignore all arguments and be happy
        }
        break;

        // ALIGN ===============================
        case DIR_ALIGN: {
            std::string exprstr;
            size_t      boundary = static_cast<size_t>((eval_tokens(directive, exprstr, cur_token, tokens, 1, 0)));

            if ((boundary == 0) || (boundary & (boundary - 1)))
                error("%s requires a power of two byte boundary (" PR_DSIZET " fails)", directive.c_str(), boundary);
            else
                align_output(boundary);
        }
        break;

        // space type (reserve space) =====
        case DIR_SPACE_16: {
            std::string exprstr;
            int64_t     count = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            size_t pot = 1;
            switch (idx)
            {
                case DIR_SPACE_16:
                    pot = 2;
                    break;
                default:
                    assert(false);
                    break;
            }

            if (count >= 0)
            {
                if ((static_cast<uint64_t>(count) * pot) > MAXFILL_BYTES)
                {
                    error("%s of 0x" PR_X64 "/" PR_D64 " exceeded output size safety check of 0x%x/%d bytes",
                          directive.c_str(),
                          static_cast<uint64_t>(count) * pot,
                          static_cast<uint64_t>(count) * pot,
                          MAXFILL_BYTES,
                          MAXFILL_BYTES);
                    break;
                }

                align_output(arch->data_alignment(pot));
                for (int32_t pad = 0; pad < count; pad++)
                {
                    switch (pot)
                    {
                        case 1:
                            emit(int8_t{0});
                            break;
                        case 2:
                            emit(int16_t{0});
                            break;
                        case 4:
                            emit(int32_t{0});
                            break;
                        case 8:
                            emit(int64_t{0});
                            break;
                        default:
                            assert(false);
                            break;
                    }
                }

                notice(3,
                       "%s reserved total of " PR_D64 "*" PR_DSIZET " = 0x" PR_X64 "/" PR_D64 " bytes",
                       directive.c_str(),
                       count,
                       pot,
                       static_cast<uint64_t>(count) * pot,
                       static_cast<uint64_t>(count) * pot);
            }
            else if (count < 0)
            {
                error("Illegal negative %s value " PR_D64 "\n", directive.c_str(), count);
            }
        }
        break;

        // fill type (fill swith value) =====
        case DIR_FILL_16: {
            std::string exprstr;

            int64_t     v64 = eval_tokens(directive, exprstr, cur_token, tokens, 2, 0);

            exprstr.clear();
            int64_t count = eval_tokens(directive, exprstr, cur_token, tokens, 1, 0);

            uint16_t v16 = static_cast<uint16_t>(v64);
            size_t   pot = 1;

            switch (idx)
            {
                case DIR_FILL_16:
                    pot = 2;
                    break;
                default:
                    assert(false);
                    break;
            }

            if (ctxt.pass != context_t::PASS_1)
                check_truncation(directive, v64, static_cast<uint32_t>(pot << 3), 1);

            if (count >= 0)
            {
                if ((static_cast<uint64_t>(count) * pot) > MAXFILL_BYTES)
                {
                    error("%s of " PR_X64 "/" PR_D64 " exceeded output size safety check of 0x%x/%d bytes",
                          directive.c_str(),
                          static_cast<uint64_t>(count) * pot,
                          static_cast<int64_t>(count) * pot,
                          MAXFILL_BYTES,
                          MAXFILL_BYTES);
                    break;
                }

                if (count)
                {
                    align_output(arch->data_alignment(pot));
                    for (int64_t i = 0; i < count; i++)
                    {
                        switch (idx)
                        {
                            case DIR_FILL_16:
                                emit(v16);
                                break;
                            default:
                                assert(false);
                                break;
                        }
                    }
                }
                notice(3,
                       "%s filled a total of " PR_D64 "*" PR_DSIZET " = 0x" PR_X64 "/" PR_D64 " bytes",
                       directive.c_str(),
                       count,
                       pot,
                       static_cast<uint64_t>(count) * pot,
                       static_cast<uint64_t>(count) * pot);
            }
            else if (count < 0)
            {
                error("Illegal negative %s value " PR_D64 "\n", directive.c_str(), count);
            }
        }
        break;

        // define type (define values) =====
        case DIR_DEF_16: {
            size_t pot = 1;
            switch (idx)
            {
                case DIR_DEF_16:
                    pot = 2;
                    break;
                default:
                    assert(false);
                    break;
            }

            align_output(arch->data_alignment(pot));

            if ((tokens.size() - cur_token) == 0)
            {
                error("%s missing expected argument", directive.c_str());
                break;
            }

            uint64_t    count = 0;
            std::string exprstr;
            do
            {
                int64_t v64       = 0;
                bool    is_string = false;

                exprstr.clear();

                v64 = eval_tokens(directive, exprstr, cur_token, tokens, 0, 0);

                if (!is_string)
                {
                    uint16_t v16 = static_cast<uint16_t>(v64);

                    if (ctxt.pass != context_t::PASS_1)
                        check_truncation(directive, v64, static_cast<uint32_t>(pot << 3), 1);
                    switch (idx)
                    {
                        case DIR_DEF_16:
                            emit(v16);
                            break;
                        default:
                            assert(false);
                            break;
                    }

                    count++;
                }

                if (cur_token < tokens.size())
                {
                    assert(tokens[cur_token] == ",");

                    if (cur_token + 1 >= tokens.size())
                        error("%s missing argument after \",\"", directive.c_str());
                }
            } while (++cur_token < tokens.size());

            notice(3,
                   "%s defined a total of " PR_D64 "*" PR_DSIZET " = 0x" PR_X64 "/0x" PR_D64 " bytes",
                   directive.c_str(),
                   count,
                   pot,
                   static_cast<uint64_t>(count) * pot,
                   static_cast<uint64_t>(count) * pot);
        }
        break;

        // define hex =======================
        case DIR_DEF_HEX: {
            if ((tokens.size() - cur_token) == 0)
            {
                error("%s missing expected argument", directive.c_str());
                break;
            }

            uint64_t count = 0;
            for (; cur_token < tokens.size(); cur_token++)
            {
                std::string exprstr = removeQuotes(tokens[cur_token]);
                if (exprstr.size() & 1)
                {
                    error("%s requires an even number of contiguous hex digits", directive.c_str());
                    break;
                }

                for (auto hit = exprstr.begin(); hit != exprstr.end(); hit += 2)
                {
                    uint8_t d1 = static_cast<uint8_t>(toupper(hit[0]));
                    uint8_t d2 = static_cast<uint8_t>(toupper(hit[1]));

                    if (isdigit(d1))
                        d1 &= 0x0f;
                    else
                        d1 -= 'A' - 0xa;

                    if (d1 > 0xf)
                    {
                        error("%s encountered non-hex digit '%c'", directive.c_str(), hit[0]);
                        return 0;
                    }

                    if (isdigit(d2))
                        d2 &= 0x0f;
                    else
                        d2 -= 'A' - 0xa;

                    if (d2 > 0xf)
                    {
                        error("%s encountered non-hex digit '%c'", directive.c_str(), hit[1]);
                        return 0;
                    }

                    uint8_t v = static_cast<uint8_t>((d1 << 4) | d2);

                    emit(v);
                    count++;
                }
            }
            notice(3, "%s defined a total of 0x" PR_X64 "/0x" PR_D64 " bytes", directive.c_str(), count, count);
        }
        break;

        // INCBIN =================================
        case DIR_INCBIN: {
            if (tokens.size() - cur_token > 1)
            {
                error("" PR_DSIZET " extra token%s after %s filename",
                      (tokens.size() - cur_token - 1),
                      (tokens.size() - cur_token - 1) == 1 ? "" : "s",
                      directive.c_str());
            }
            else if (tokens.size() - cur_token < 1)
            {
                error("missing %s filename", directive.c_str());

                return 0;
            }

            std::string name = removeQuotes(tokens[cur_token]);

            struct stat binstat;

            if (stat(name.c_str(), &binstat) < 0)
            {
                error("%s getting stat for file \"%s\" error: %s", directive.c_str(), name.c_str(), strerror(errno));
                break;
            }

            size_t sz = static_cast<size_t>(binstat.st_size);

            if (sz & 1)
                error("%s opening file \"%s\" error: odd size not allowed", directive.c_str(), name.c_str());

            FILE * fp = fopen(name.c_str(), "r");

            if (fp == nullptr)
            {
                error("%s opening file \"%s\" error: %s", directive.c_str(), name.c_str(), strerror(errno));
                break;
            }

            if (ferror(fp))
            {
                error("%s:%d: %s reading file \"%s\" error: %s",
                      ctxt.file->name.c_str(),
                      ctxt.line + ctxt.file->line_start,
                      directive.c_str(),
                      name.c_str(),
                      strerror(errno));
                fclose(fp);
                break;
            }
            size_t off = ctxt.section->data.size();
            ctxt.section->data.insert(ctxt.section->data.end(), sz, uint8_t{0});
            if (fread(&ctxt.section->data[off], sz, 1, fp) != 1)
            {
                error("%s:%d: %s reading file \"%s\" error: %s",
                      ctxt.file->name.c_str(),
                      ctxt.line + ctxt.file->line_start,
                      directive.c_str(),
                      name.c_str(),
                      strerror(errno));
            }
            fclose(fp);
        }
        break;

        default: {
            notice(
                0, "[TODO Directive \"%s\"=%d args:" PR_DSIZET "]", directive.c_str(), idx, tokens.size() - cur_token);
        }
        break;
    }

    return 0;
}

int32_t xlasm::process_section(const std::string &              directive,
                               const std::string &              label,
                               size_t                           cur_token,
                               const std::vector<std::string> & tokens)
{
    if (label.size())
    {
        error("Label definition not permitted on %s", directive.c_str());
    }

    if (tokens.size() - cur_token < 1)
    {
        error("%s missing required name", directive.c_str());
        return 0;
    }

    std::string segname = removeQuotes(tokens[cur_token++]);
    uint32_t    flags   = 0;

    bool    addr_given = false;
    int64_t addr       = 0;
    if (cur_token < tokens.size() && tokens[cur_token] == ",")
    {
        cur_token++;
        if (cur_token >= tokens.size() || tokens[cur_token] != ",")
        {
            std::string exprstr;
            addr       = eval_tokens(directive, exprstr, cur_token, tokens, 0, 0);
            addr_given = true;
        }
    }

    if (cur_token < tokens.size() && tokens[cur_token] == ",")
    {
        cur_token++;
        if (cur_token < tokens.size())
        {
            std::string flag_name = tokens[cur_token++];
            std::transform(flag_name.begin(), flag_name.end(), flag_name.begin(), lowercase);

            if (flag_name == "noload")
                flags |= section_t::NOLOAD_FLAG;
        }
        else
            error("%s missing flags after \",\"", directive.c_str());
    }

    if (cur_token != tokens.size())
        error("Unexpected additional arguments for %s", directive.c_str());

    section_t & seg = sections[segname];

    if ((addr_given || flags != 0) && seg.load_addr != addr && seg.data.size() != 0)
    {
        error("%s can't redefine non-empty section \"%s\"", directive.c_str(), segname.c_str());
        return 0;
    }

    if (!seg.name.size())
    {
        seg.name   = segname;
        uint32_t i = next_section_index++;
        if (segname.find("data.") != std::string::npos)
            i += 10000;
        else if ((flags & section_t::NOLOAD_FLAG) || segname.find("bss.") != std::string::npos)
            i += 20000;
        seg.index = i;
        seg.flags = flags;
    }

    if (addr_given)
    {
        seg.load_addr = addr;
        seg.addr      = addr;
    }

    notice(2,
           "%s \"%s\" (size " PR_DSIZET " / 0x" PR_XSIZET ")",
           directive.c_str(),
           seg.name.c_str(),
           seg.data.size(),
           seg.data.size());

    previous_section = ctxt.section;
    ctxt.section     = &seg;
    if (seg.data.size() == 0)
    {
        seg.arch = arch;
    }

    return 0;
}

int32_t xlasm::process_labeldef(std::string label)
{
    if (arch->lookup_register(label) >= 0)
    {
        warning("Symbol definition: \"%s\" is also a register for %s", label.c_str(), arch->get_variant().c_str());
    }

    if (ctxt.macrodef_ptr != nullptr)
    {
        notice(3, "Deferring label def in macro def\"%s\"", label.c_str());
        return 0;
    }

    symbol_t & sym = symbols[label];

    if (sym.type == symbol_t::UNDEFINED)
    {
        notice(3,
               "Creating UNDEFINED label \"%s\" at %s(%d)%s%s",
               label.c_str(),
               ctxt.file->name.c_str(),
               ctxt.line,
               ctxt.macroexp_ptr ? " macro-exp" : "",
               ctxt.macrodef_ptr ? " macro-def" : "");
        sym.type         = symbol_t::LABEL;
        sym.name         = label;
        sym.line_defined = ctxt.line;
        sym.file_defined = ctxt.file;
        sym.section      = ctxt.section;
    }
    else
    {
        if (sym.line_defined != ctxt.line || sym.file_defined != ctxt.file)
        {
            error("Duplicate label definition: \"%s\" first at %s(%d) vs now %s(%d)%s%s%s",
                  label.c_str(),
                  sym.file_defined->name.c_str(),
                  sym.line_defined,
                  ctxt.file->name.c_str(),
                  ctxt.line,
                  ctxt.macroexp_ptr ? " macro-exp" : "",
                  ctxt.macrodef_ptr ? " macro-def" : "",
                  sym.type_name());
        }
    }

    sym.value                      = ctxt.section->addr + (static_cast<int64_t>(ctxt.section->data.size() >> 1));
    ctxt.section->last_defined_sym = &sym;
    sym_defined                    = &sym;
    notice(3, "Defined label \"%s\" = 0x" PR_X64 "/" PR_D64 "", label.c_str(), sym.value, sym.value);

    return 0;
}

int32_t xlasm::align_output(size_t pot)
{
    assert((pot > 0) && !(pot & (pot - 1)));

    size_t off    = static_cast<size_t>(ctxt.section->addr) + ctxt.section->data.size();
    size_t newoff = (off + pot - 1) & ~(pot - 1);
    size_t delta  = newoff - off;

    if (delta)
    {
        size_t pad = delta;

        while (pad)
        {
            if (pad & (1 << 0))
            {
                emit(int8_t{0});
                pad -= sizeof(int8_t);
            }
            else if (pad & (1 << 1))
            {
                emit(int16_t{0});
                pad -= sizeof(int16_t);
            }
            else if (pad & (1 << 2))
            {
                emit(int32_t{0});
                pad -= sizeof(int32_t);
            }
            else
            {
                emit(int64_t{0});
                pad -= sizeof(int64_t);
            }
        }

        notice(3, "" PR_DSIZET " byte%s alignment padding inserted", delta, delta != 1 ? "s" : "");

        if (ctxt.section->last_defined_sym)
        {
            symbol_t & lsym = *ctxt.section->last_defined_sym;

            if ((lsym.type != symbol_t::UNDEFINED && lsym.type != symbol_t::STRING) && lsym.section == ctxt.section &&
                lsym.value == static_cast<int64_t>(off))
            {
                warning("" PR_DSIZET " byte%s alignment padding inserted after label \"%s\" definition",
                        delta,
                        delta != 1 ? "s" : "",
                        lsym.name.c_str());
            }
        }

        suppress_line_listsource = true;
        if (listing_file)
            process_line_listing();
        suppress_line_listsource = false;
        line_sec_size            = line_sec_start->data.size();
    }

    return 0;
}

int64_t xlasm::eval_tokens(const std::string &              cmd,
                           std::string &                    exprstr,
                           size_t &                         cur_token,
                           const std::vector<std::string> & tokens,
                           int32_t                          expected_args,
                           int64_t                          defval)
{
    int64_t result = defval;

    exprstr.clear();
    if ((tokens.size() - cur_token) == 0)
    {
        error("Missing expected argument%s after %s", expected_args == 1 ? "" : "s", cmd.c_str());

        return result;
    }
    else
    {
        for (auto it = tokens.begin() + static_cast<int32_t>(cur_token); it != tokens.end(); ++it, cur_token++)
        {
            if (*it == ",")
                break;

            //			if (exprstr.size())
            //				exprstr += " ";

            exprstr += *it;
        }

        if (expected_args > 1 && cur_token < tokens.size() && tokens[cur_token] == ",")
            cur_token++;
    }

    size_t     last_offset = 0;
    expression expr;

    if (!exprstr.size() || !expr.evaluate(this, exprstr.c_str(), &result, &last_offset))
    {
        warning("%s failed evaluating expression \"%.64s\", using default value " PR_D64,
                cmd.c_str(),
                exprstr.size() ? exprstr.c_str() : "<none>",
                defval);
        result = defval;
    }

    if (ctxt.pass == context_t::PASS_2)
    {
        if (last_offset < exprstr.size())
        {
            error("%s extra character(s) \"%.64s\" following expression", cmd.c_str(), exprstr.c_str() + last_offset);
        }

        if (expected_args == 1 && tokens.size() != cur_token)
        {
            error("%s unexpected extra argument(s)", cmd.c_str());
        }
    }

    return result;
}

bool xlasm::define_macro_begin(const std::string &              directive,
                               const std::string &              label,
                               size_t                           cur_token,
                               const std::vector<std::string> & tokens)
{
    if (ctxt.macrodef_ptr != nullptr)
    {
        error("Nested %s definitions not permitted", directive.c_str());
        return 0;
    }

    if (!label.size() && tokens.size() - cur_token < 1)
    {
        error("Missing %s name", directive.c_str());
        return 0;
    }

    const std::string & name     = label.size() ? label : tokens[cur_token++];
    std::string         upr_name = name;

    std::transform(upr_name.begin(), upr_name.end(), upr_name.begin(), uppercase);

    if (!isalpha(upr_name[0]) && upr_name[0] != '_')
    {
        error("Illegal %s name \"%s\"", directive.c_str(), name.c_str());
        return 0;
    }

    macro_t & m = macros[upr_name];

    if (m.name.size() != 0)
    {
        error("%s redefinition of \"%s\" not permitted", directive.c_str(), m.name.c_str());
        return 0;
    }

    m.name = name;

    notice(3, "Defining %s \"%s\"", directive.c_str(), name.c_str());

    for (auto it = tokens.begin() + static_cast<int32_t>(cur_token); it != tokens.end(); ++it)
    {
        if (!isalpha((*it)[0]) && !isdigit((*it)[0]) && (*it)[0] != '_')
        {
            error("%s \"%s\" illegal parameter name \"%s\"", directive.c_str(), name.c_str(), it->c_str());
            return 0;
        }

        bool dupe = false;
        for (auto mit = m.args.begin(); mit != m.args.end(); ++mit)
        {
            if (*it == *mit)
            {
                dupe = true;
                break;
            }
        }

        if (dupe)
        {
            error("%s \"%s\" duplicated parameter name \"%s\"", directive.c_str(), name.c_str(), it->c_str());
            continue;
        }

        m.args.push_back(*it);
        ++it;

        std::string def;

        if (it != tokens.end() && *it == "=")
        {
            ++it;        // consume "="

            for (; it != tokens.end() && *it != ","; ++it)
            {
                if (def.size())
                    def += " ";
                def += *it;
            }
        }

        m.def.push_back(removeQuotes(def));

        if (it == tokens.end())
        {
            break;
        }
        if (*it != ",")
        {
            error("%s \"%s\" unexpected \"%s\" after parameter", directive.c_str(), name.c_str(), it->c_str());
            break;
        }
    }

    std::string pstr;
    for (size_t i = 0; i < m.args.size(); i++)
    {
        if (pstr.size())
            pstr += ", ";
        pstr += "\\";
        pstr += m.args[i];
        if (m.def[i].size())
        {
            pstr += "=\"";
            pstr += m.def[i];
            pstr += "\"";
        }
    }

    notice(3, "%s \"%s\" parameters: %s", directive.c_str(), name.c_str(), pstr.c_str());

    m.name            = name;
    m.body.line_start = ctxt.line + 2;
    m.body.name       = ctxt.file->name;

    ctxt.macrodef_ptr = &m;

    return 0;
}

bool xlasm::define_macro_end(const std::string &              directive,
                             const std::string &              label,
                             size_t                           cur_token,
                             const std::vector<std::string> & tokens)
{
    if (ctxt.macrodef_ptr == nullptr)
    {
        error("%s encountered without matching MACRO", directive.c_str());
        return 0;
    }

    if (label.size())
    {
        error("Label definition not permitted on %s", directive.c_str());
    }

    if (tokens.size() - cur_token != 0)
        error("" PR_DSIZET " extra token%s after %s",
              (tokens.size() - cur_token),
              (tokens.size() - cur_token) == 1 ? "" : "s",
              directive.c_str());

    notice(3,
           "%s for MACRO \"%s\" (" PR_DSIZET " lines)",
           directive.c_str(),
           ctxt.macrodef_ptr->name.c_str(),
           ctxt.macrodef_ptr->body.src_line.size());

#if 1
    int32_t mlinenum = 1;
    for (auto md = ctxt.macrodef_ptr->body.src_line.begin(); md != ctxt.macrodef_ptr->body.src_line.end();
         ++md, ++mlinenum)
    {
        std::string mline;
        for (auto mt = (*md).begin(); mt != (*md).end(); ++mt)
        {
            if (mline.size())
                mline += " ";
            mline += "|";
            mline += *mt;
            mline += "|";
        }
        notice(3, "%6d: %s", mlinenum, mline.c_str());
    }
#endif

    ctxt.macrodef_ptr = nullptr;

    return 0;
}

xlasm::source_t & xlasm::expand_macro(std::string & name, size_t cur_token, const std::vector<std::string> & tokens)
{
    macro_t & m = macros[name];

    name = m.name;        // use name defined with (not uppercase)
    std::vector<std::string> parms;

    // TODO: Fixup token parsing, e.g., {"A "B C,x} drops C
    {
        std::string rawparm;
        std::string parm;
        size_t      parm_idx = 0;
        auto        it       = tokens.begin() + static_cast<int32_t>(cur_token);
        while (it != tokens.end())
        {
            rawparm.clear();
            parm.clear();

            while (it != tokens.end() && *it != ",")
            {
                if ((*it)[0] == '\"')
                {
                    if (rawparm.size())
                    {
                        break;
                    }

                    rawparm = *it;
                    ++it;
                    break;
                }
                rawparm += *it;
                ++it;
            }

            if (!rawparm.size())
            {
                if (m.def.size() > parm_idx && m.def[parm_idx].size() != 0)
                {
                    rawparm = m.def[parm_idx];
                }
                else
                {
                    error("MACRO \"%s\" parameter \"%s\" unset with no default value",
                          name.c_str(),
                          m.args[parm_idx].c_str());
                }
            }

            if (rawparm.size() && rawparm[0] == '\"')
            {
                parm = removeQuotes(rawparm);
            }
            else
            {
                parm = rawparm;
            }

            parms.push_back(parm);

            parm_idx++;

            if (it == tokens.end())
            {
                continue;
            }
            else if (*it != ",")
            {
                error("MACRO \"%s\" expected \",\" after: %s", name.c_str(), rawparm.c_str());
                break;
            }
            else
            {
                ++it;
            }
        }

        // set remaining default arguments
        while (m.def.size() && parm_idx < m.def.size())
        {
            parms.push_back(m.def[parm_idx]);
            parm_idx++;
        }
    }

    std::string key = m.name;        // key used to identify identically expanded macros (same arguments)

    if (parms.size() > 0)
    {
        key += "[";
        key += std::to_string(parms.size());
        key += "]";
        uint32_t key_idx = 0;
        for (auto it = parms.begin(); it != parms.end(); ++it, key_idx++)
        {
            key += "|";

#if 0
			if (key_idx < m.args.size() && m.args[key_idx].size())
				key += m.args[key_idx];
			else
			{
				key += std::to_string(key_idx);
			}
			key += "=";
#endif
            key += *it;
        }
        key += "|";
    }

    source_t & s = expanded_macros[key];

    m.invoke_count++;

    // has this particular macro/parameter combination been expanded already?
    if (!s.name.size())
    {
        s.name       = m.body.name;
        s.file_size  = m.body.file_size;
        s.line_start = m.body.line_start;

        s.src_line = m.body.src_line;

        std::string unique_str;
        strprintf(unique_str, "_%s_%d", m.name.c_str(), m.invoke_count);
        notice(3, "Invoked MACRO \"%s\" with key <%s> and unique ID %s", name.c_str(), key.c_str(), unique_str.c_str());

        bool        spammed = false;
        std::string sn;
        std::string mn;
        for (auto lit = s.src_line.begin(); lit != s.src_line.end(); ++lit)
        {
            // {
            // 	std::string fake_line;

            // 	if (!opt.suppress_macro_name)
            // 		fake_line = "<" + name + ">\t";

            // 	size_t idx = 0;
            // 	for (auto tit = lit->begin(); tit != lit->end(); ++tit, idx++)
            // 	{
            // 		if (idx == 0 && tit->back() != ':')
            // 			fake_line += "\t\t";
            // 		fake_line += *tit;
            // 		if (tit + 1 != lit->end() && idx == 0)
            // 			fake_line += "\t\t";
            // 	}
            // 	s.orig_line.push_back(fake_line);
            // 	dprintf("BEFORE: " PR_DSIZET ": %s\n", lit - s.src_line.begin(), fake_line.c_str());
            // }
            for (auto tit = lit->begin(); tit != lit->end(); ++tit)
            {
                size_t search_start = 0;
                bool   hasquotes    = (tit->size() && ((*tit)[0] == '\"' || (*tit)[0] == '\''));

                uint32_t reps;
                for (reps = 0; reps < MAXMACROREPS_WARNING; reps++)
                {
                    size_t parameter_idx  = ~0UL;
                    size_t replace_pos    = 0;
                    size_t replace_length = 0;

                    if (search_start >= tit->size())
                        break;

                    search_start = tit->find("\\", search_start);

                    // if no backslash or backslash at end, we are done
                    if (search_start == std::string::npos || search_start + 1 >= tit->size())
                        break;

                    if (reps == 0)
                        notice(3,
                               "MACRO %s<%s>:" PR_DSIZET ": replacing arguments in: %s",
                               name.c_str(),
                               key.c_str(),
                               lit - s.src_line.begin(),
                               tit->c_str());

                    // if two backslashes, search for next backslash
                    if ((*tit)[search_start + 1] == '\\')
                    {
                        search_start += 2;
                        continue;
                    }

                    if ((*tit)[search_start + 1] == '@')        // '\@' unique-ifier?
                    {
                        tit->erase(search_start, 2);
                        tit->insert(search_start, unique_str);
                        continue;
                    }

                    // is this a numeric parameter after backslash?
                    if (isdigit((*tit)[search_start + 1]))
                    {
                        const char * startptr = &(*tit)[search_start + 1];
                        char *       endptr   = nullptr;
                        parameter_idx         = strtoul(startptr, &endptr, 10);
                        replace_length        = static_cast<size_t>(endptr - startptr);
                        replace_pos           = search_start;
                    }
                    else
                    {
                        // see if it matches any argument name (and longest length match)
                        for (std::vector<std::string>::const_iterator ait = m.args.begin(); ait != m.args.end(); ++ait)
                        {
                            if (replace_length < ait->size())
                            {
                                sn = "\\";
                                sn += *ait;
                                //								dprintf("Check for '%s' in '%s'...\n", sn.c_str(),
                                //(*tit).c_str());
                                size_t mp = tit->find(sn, search_start);
                                if (mp != std::string::npos && (mp == 0 || (*tit)[mp - 1] != '\\'))
                                {
                                    //									dprintf("Found '%s' in '%s' at pos "
                                    // PR_DSIZET
                                    //"\n",  sn.c_str(), tit->c_str(), mp);
                                    mn             = sn;
                                    parameter_idx  = static_cast<size_t>((ait - m.args.begin())) + 1;
                                    replace_length = ait->size();
                                    replace_pos    = search_start;
                                }
                            }
                        }
                    }

                    if (parameter_idx == ~0UL)
                        break;

                    assert(parameter_idx < parms.size() + 1);

                    std::string reptxt =
                        parameter_idx == 0 ? std::to_string(parms.size()) : parms[parameter_idx - 1].c_str();

                    ///					dprintf("Replacing '%s' with '%s' pos " PR_DSIZET ", length " PR_DSIZET
                    ///"...\n",
                    /// mn.c_str(), reptxt.c_str(), replace_pos, replace_length);

                    tit->erase(replace_pos, replace_length + 1);
                    if (hasquotes)
                        tit->insert(replace_pos, reQuote(reptxt));
                    else
                        tit->insert(replace_pos, reptxt);

                    //					dprintf("Result '%s'...\n", tit->c_str());
                }

                if (reps >= MAXMACROREPS_WARNING && !spammed)
                {
                    error("MACRO \"%s\" > %d parameter substitution iterations (likely recursive)",
                          name.c_str(),
                          MAXMACROREPS_WARNING);
                    spammed = true;
                }
            }

            {
                std::string fake_line;

                if (!opt.suppress_macro_name)
                    fake_line = "<" + name + ">\t";

                // TODO: Not happy with this macro fake listing
                size_t idx = 0;
                for (auto tit = lit->begin(); tit != lit->end(); ++tit, idx++)
                {
                    if (idx == 0 && tit->back() != ':')
                        fake_line += " ";
                    fake_line += *tit;
                    if (tit + 1 != lit->end() && idx < 2)
                        fake_line += " ";
                }
                s.orig_line.push_back(fake_line);
                //				dprintf("AFTER : " PR_DSIZET ": %s\n", lit - s.src_line.begin(), fake_line.c_str());
            }
        }
    }
    else
    {
        notice(3, "MACRO \"%s\" with key <%s> already generated", name.c_str(), key.c_str());
    }

    ctxt.macroexp_ptr = &m;
    notice(3, "Expanding MACRO <%s>", key.c_str());

    return s;
}

int64_t xlasm::symbol_value(xlasm * xl, const char * name, bool * undefined)
{
    int64_t     result = 0;
    std::string sym_name(name);

    symbol_t & sym = xl->symbols[sym_name];

    if (!sym.file_first_referenced)
    {
        sym.file_first_referenced = xl->ctxt.file;
        sym.line_first_referenced = xl->ctxt.line;
    }
    if (undefined)
        *undefined = false;

    if (sym.type == symbol_t::UNDEFINED)
    {
        if (!sym.name.size())
            sym.name = name;

        if (undefined)
            *undefined = true;

        xl->undefined_sym_count++;
    }
    else if (sym.type == symbol_t::INTERNAL)
    {
        result = xl->lookup_special_symbol(sym_name);
    }
    else if (sym.type == symbol_t::STRING)
    {
        expression expr;

        if (sym.str.size())
        {
            if (!expr.evaluate(xl, sym.str.c_str(), &result))
            {
                if (undefined)
                    *undefined = true;
            }
        }
        else
        {
            if (xl->ctxt.pass == xlasm::context_t::PASS_2)
                xl->warning(
                    "Evaluating empty string in symbol \"%s\" as 0x" PR_X64 "/" PR_D64 "", name, result, result);
        }
    }
    else if (sym.type == symbol_t::REGISTER)
    {
        xl->error("Cannot use register \"%s\" as a value", name);
        if (undefined)
            *undefined = true;
    }
    else
    {
        if (sym.section)
        {
            sym.section->flags |= section_t::REFERENCED_FLAG;
        }
        result = sym.value;
    }

    return result;
}


int32_t xlasm::lookup_register_symbol(const std::string & sym_name)
{
    int32_t result = -1;

    if (sym_name.size() == 0)
        return -1;

    result = arch->lookup_register(sym_name);

    if (result >= 0)
        return result;

    auto sit = symbols.find(sym_name);
    if (sit == symbols.end())
        return -1;

    symbol_t & sym = sit->second;

    if (!sym.file_first_referenced)
    {
        sym.file_first_referenced = ctxt.file;
        sym.line_first_referenced = ctxt.line;
    }

    if (sym.type == symbol_t::REGISTER)
    {
        assert(sym.section);
        if (arch != sym.section->arch)
        {
            error("Cannot use register \"%s\" from different architecture", sym_name.c_str());
        }
        else
        {
            result = static_cast<int32_t>(sym.value);
        }
    }
    else
    {
        error("Cannot use symbol \"%s\" as register value", sym_name.c_str());
    }

    return result;
}

int64_t xlasm::lookup_special_symbol(const std::string & sym_name)
{
    if (sym_name == ".")
    {
        return ctxt.section->addr + static_cast<int64_t>(ctxt.section->data.size() >> 1);
    }
    else if (sym_name.compare(0, 5, ".rand") == 0 || sym_name.compare(0, 5, ".RAND") == 0)
    {
        uint32_t rngbits = 16;

        if (sym_name.compare(5, std::string::npos, "16") == 0)
        {
            rngbits = 16;
        }
        else
        {
            error("Unrecognized .RAND size \"%s\" (must be 16)?", sym_name.c_str());

            return 0;
        }

        // mix all 64-bits together as needed

        uint64_t v    = 0;
        uint64_t r    = static_cast<uint64_t>(rng());
        uint64_t mask = rngbits > 64 ? (uint64_t{1} << rngbits) - 1 : uint64_t{~0U};

        for (uint32_t i = 0; i < 64; i += rngbits)
        {
            v += (r & mask);
            v += (v >> rngbits);
            v &= mask;
            r >>= rngbits;
        }

        return static_cast<int64_t>(v);
    }

    error("Unrecognized special symbol \"%s\"?", sym_name.c_str());

    return 0;
}

std::string xlasm::token_message(size_t cur_token, const std::vector<std::string> & tokens)
{
    std::string msg;

    for (auto it = tokens.begin() + static_cast<int32_t>(cur_token); it != tokens.end(); ++it)
    {
        if ((*it)[0] == ',')
            continue;

        if ((*it)[0] == '\"' || (*it)[0] == '\'')
        {
            msg += removeQuotes(*it);
        }
        else
        {
            int64_t     result = 0;
            expression  expr;
            std::string rstr;

            if (it->size() && expr.evaluate(this, it->c_str(), &result))
                strprintf(msg, "0x" PR_X64 "/" PR_D64 "", result, result);
            else
                strprintf(msg, "<expr error>");
        }
    }

    return msg;
}

std::string xlasm::quotedToRaw(const std::string cmd, const std::string & str, bool null_terminate)
{
    std::string rawstr;

    bool escape = false;
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        if (it[0] == '\\' && !escape)
        {
            escape = true;
            continue;
        }

        if (escape)
        {
            switch (it[0])
            {
                case '\'':
                    rawstr += '\'';
                    break;        // 0x27
                case '\"':
                    rawstr += '\"';
                    break;        // 0x22
                case '?':
                    rawstr += '\?';
                    break;        // 0x3f
                case '\\':
                    rawstr += '\\';
                    break;        // 0x5c
                case 'a':
                    rawstr += '\a';
                    break;        // 0x07
                case 'b':
                    rawstr += '\b';
                    break;        // 0x08
                case 'f':
                    rawstr += '\f';
                    break;        // 0x0c
                case 'n':
                    rawstr += '\n';
                    break;        // 0x0a
                case 'r':
                    rawstr += '\r';
                    break;        // 0x0d
                case 't':
                    rawstr += '\t';
                    break;        // 0x09
                case 'v':
                    rawstr += '\v';
                    break;        // 0x0b
                case '0':
                    rawstr += '\0';
                    break;        // 0x00
                case 'x': {
                    char v = '\0';
                    for (int32_t d = 0; d < 2; d++)
                    {
                        if (++it >= str.end())
                        {
                            error("%s hex literal incomplete (requires two hex digits after \"\\x\").", cmd.c_str());
                            v = '?';
                            break;
                        }

                        char    c = static_cast<char>(toupper(*it));
                        uint8_t n = static_cast<uint8_t>(c & 0xf);

                        if (!isdigit(c))
                            n = static_cast<uint8_t>(c - ('A' - 0xa));

                        if (n > 0xf)
                        {
                            error("%s encountered non-hex digit in hex literal '%c'", cmd.c_str(), c);
                            v = '?';
                            break;
                        }

                        v = static_cast<char>((v << 4) | n);
                    }

                    rawstr += v;
                }
                break;
                default: {
                    if (ctxt.pass == context_t::PASS_2)
                        warning("%s unrecognized character escape code '%c'", cmd.c_str(), it[0]);
                    rawstr += *it;
                    break;
                }
            }
        }
        else
        {
            rawstr += *it;
        }

        escape = false;
    }

    if (null_terminate)
        rawstr += '\0';

    return rawstr;
}

std::string xlasm::removeExtension(const std::string & filename)
{
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos)
        return filename;
    return filename.substr(0, lastdot);
}

std::string xlasm::removeQuotes(const std::string & quotedstr)
{
    std::string newstr;
    int32_t     trim = 0;
    if (quotedstr[0] == '\'' || quotedstr[0] == '\"')
        trim = 1;
    newstr.assign(quotedstr.begin() + trim, quotedstr.end() - trim);
    return newstr;
}

std::string xlasm::reQuote(const std::string & str)
{
    std::string newstr;
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        if (*it == '\'')
            newstr += "\\\'";
        else if (*it == '\"')
            newstr += "\\\"";
        else
            newstr += *it;
    }
    return newstr;
}

int32_t xlasm::source_t::read_file(xlasm * xa, const std::string & n, const std::string & fn)
{
    if (file_size)
    {
        //		dprintf("File '%s' has already been loaded\n", name.c_str());
        assert(name == n);
        return 0;
    }

    name = n;

    FILE * fp = fopen(fn.c_str(), "r");
    if (!fp)
    {
        return errno;
    }

    char        line_buff[MAX_LINE_LENGTH] = {0};
    std::string nline;

    while (!ferror(fp) && fgets(line_buff, sizeof(line_buff) - 1, fp) != nullptr)
    {
        nline = line_buff;
        file_size += nline.size();
        rtrim(nline, " \r\n");
        if (nline.size() < 3 || nline[0] != '#' || nline[1] != ' ' || (nline[2] < '0' && nline[2] > '9'))
        {
            orig_line.push_back(nline);
        }
    }

    if (!feof(fp))
    {
        int e = errno;
        fclose(fp);
        return e;
    }
    fclose(fp);

    // do preliminary processing on input file to make it more regular WRT whitespace and removing comments
    std::vector<std::string> cooked_tokens;
    std::string              token;
    uint32_t                 ln = 0;
    for (std::vector<std::string>::iterator it = orig_line.begin(); it != orig_line.end(); ++it, ln++)
    {
        char inquotes   = 0;
        bool escape     = false;
        bool whitespace = false;

        cooked_tokens.clear();
        token.clear();

        if (it->c_str()[0] != '#')
        {
            char c = 0, prev_c = 0;
            for (std::string::iterator sit = it->begin(); sit != it->end(); ++sit)
            {
                c = *sit;

                if (!inquotes)
                {
                    // end at comment start
                    if (c == ';')
                        break;

                    // C++ style comment start
                    if (c == '/' && (sit + 1 != it->end() && sit[1] == '/'))
                        break;

                    bool ws = (isspace(c) || c < ' ');

                    // if not in quotes and this is not the first whitespace character, discard it
                    if (ws && !whitespace)
                    {
                        whitespace = true;
                        continue;
                    }
                    else if (whitespace && ws)
                    {
                        // eat extra whitespace
                        continue;
                    }
                    else if (whitespace && !ws)
                    {
                        // replace whitespace run with a single space between label and opcode, but remove from
                        // operand
                        whitespace = false;
                        if (c != ':')
                        {
                            if (token.size())
                                cooked_tokens.push_back(token);
                            token.clear();
                        }
                    }

                    // special handling for two character tokens
                    if (strchr("!=<>&|*", c) != nullptr)
                    {
                        // must be two char token
                        if (prev_c && strchr("!=<>&|*", prev_c) != nullptr)
                        {
                            char s[3] = {prev_c, c, '\0'};
                            cooked_tokens.push_back(s);
                            prev_c = 0;

                            continue;
                        }

                        if (token.size())
                        {
                            cooked_tokens.push_back(token);
                            token.clear();
                        }

                        char next_c = ((sit + 1) != it->end()) ? sit[1] : '\0';

                        bool two_char = (c == '!' && next_c == '=') ||        // !=
                                        (c == '=' && next_c == '=') ||        // ==
                                        (c == '<' && next_c == '=') ||        // <=
                                        (c == '>' && next_c == '=') ||        // >=
                                        (c == '>' && next_c == '>') ||        // >>
                                        (c == '<' && next_c == '<') ||        // <<
                                        (c == '&' && next_c == '&') ||        // &&
                                        (c == '|' && next_c == '|') ||        // ||
                                        (c == '*' && next_c == '*');          // **

                        // single char token
                        if (!two_char)
                        {
                            char s[2] = {c, '\0'};
                            cooked_tokens.push_back(s);
                            prev_c = 0;
                        }
                        else
                        {
                            prev_c = c;
                        }

                        continue;
                    }

                    // break up operator characters into separate tokens
                    if (strchr(",()[]{}#+-/^~%$", c) != nullptr)        // NOTE: removed @ for unique-ifier
                    {
                        char s[2] = {c, '\0'};
                        if (token.size())
                            cooked_tokens.push_back(token);
                        cooked_tokens.push_back(s);
                        token.clear();

                        continue;
                    }
                }

                // if this character wasn't escaped (by backslash)
                if (!escape)
                {
                    // if it was a quote then toggle inquotes flag as needed (inside string or character literals)
                    if (c == '\"' || c == '\'')
                    {
                        if (inquotes && inquotes == c)        // is this a matching closing quote?
                        {
                            inquotes = 0;
                            token += c;
                            cooked_tokens.push_back(token);
                            token.clear();

                            continue;
                        }
                        else if (!inquotes)        // is this a new opening quote?
                        {
                            inquotes = c;
                        }
                    }
                    else if (c == '\\')        // Flag when previous character was an escape (backslash)
                    {
                        escape = inquotes != 0;        // only valid in quotes
                    }
                }
                else
                {
                    escape = false;
                }

                token += c;        // add character to "cooked" source line tokens
            }
        }

        if (inquotes)
        {
            token += inquotes;
            source_t * old_file = xa->ctxt.file;
            uint32_t   old_line = xa->ctxt.line;
            xa->ctxt.file       = this;
            xa->ctxt.line       = ln;
            xa->warning("Missing ending quote added.\n");
            xa->ctxt.file = old_file;
            xa->ctxt.line = old_line;
        }

        if (token.size())
            cooked_tokens.push_back(token);

#if 0
		dprintf("" PR_D64 "=", cooked_tokens.size());
		for (auto dit = cooked_tokens.begin(); dit != cooked_tokens.end(); ++dit)
		{
			dprintf("[%s] ", dit->c_str());
		}
		dprintf("\n");
#endif

        src_line.push_back(cooked_tokens);
    }

    return 0;
}

void xlasm::diag_showline()
{
    if (!last_diag_file)
        return;
    printf("%s:%d: %s\n",
           last_diag_file->name.c_str(),
           last_diag_line + last_diag_file->line_start,
           last_diag_file->orig_line[last_diag_line].c_str());
    fflush(stdout);
    last_diag_file = nullptr;
}

void xlasm::diag_flush()
{
    if (last_diag_file && (last_diag_file != ctxt.file || last_diag_line != ctxt.line))
    {
        diag_showline();
    }

    fflush(stdout);

    last_diag_file = ctxt.file;
    last_diag_line = ctxt.line;
}

void xlasm::error(const char * msg, ...)
{
    va_list ap;
    va_start(ap, msg);

    diag_flush();

    printf("%s:%d: ", ctxt.file->name.c_str(), ctxt.line + ctxt.file->line_start);
    printf(TERM_ERROR "ERROR: ");
    if (ctxt.macroexp_ptr)
        printf("[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
    vprintf(msg, ap);
    printf(TERM_CLEAR "\n");

    va_end(ap);

    if (ctxt.pass == context_t::PASS_2)
    {
        std::string outmsg;
        va_start(ap, msg);

        strprintf(outmsg, TERM_ERROR "ERROR: ");
        if (ctxt.macroexp_ptr)
            strprintf(outmsg, "[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
        strprintf(outmsg, TERM_CLEAR);
        vstrprintf(outmsg, msg, ap);

        pre_messages.push_back(outmsg);
        va_end(ap);
    }

    fflush(stdout);

    error_count++;
}

void xlasm::warning(const char * msg, ...)
{
    if (ctxt.pass != context_t::PASS_2)
        return;

    va_list ap;
    va_start(ap, msg);

    diag_flush();

    if (ctxt.file)
        printf("%s:%d: ", ctxt.file->name.c_str(), ctxt.line + ctxt.file->line_start);
    printf(TERM_WARN "WARNING: ");
    if (ctxt.macroexp_ptr)
        printf("[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
    vprintf(msg, ap);
    printf(TERM_CLEAR "\n");

    va_end(ap);

    if (ctxt.pass == context_t::PASS_2)
    {
        std::string outmsg;
        va_start(ap, msg);

        strprintf(outmsg, TERM_WARN "WARNING: ");
        if (ctxt.macroexp_ptr)
            strprintf(outmsg, "[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
        strprintf(outmsg, TERM_CLEAR);
        vstrprintf(outmsg, msg, ap);

        pre_messages.push_back(outmsg);
        va_end(ap);
    }

    fflush(stdout);

    warning_count++;
}

void xlasm::notice(int32_t level, const char * msg, ...)
{
    //	if (ctxt.pass != context_t::PASS_2)
    //		return;

    if (level > opt.verbose)
        return;

    va_list ap;
    va_start(ap, msg);

    diag_flush();

    if (ctxt.file)
        printf("%s:%d: ", ctxt.file->name.c_str(), ctxt.line + ctxt.file->line_start);
    printf("NOTE: ");
    if (ctxt.macroexp_ptr)
        printf("[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
    vprintf(msg, ap);
    printf("\n");

    va_end(ap);

    if (ctxt.pass == context_t::PASS_2)
    {
        std::string outmsg;
        va_start(ap, msg);

        strprintf(outmsg, "NOTE: ");
        if (ctxt.macroexp_ptr)
            strprintf(outmsg, "[in MACRO \"%s\"] ", ctxt.macroexp_ptr->name.c_str());
        vstrprintf(outmsg, msg, ap);

        post_messages.push_back(outmsg);

        va_end(ap);
    }

    fflush(stdout);

    last_diag_file = nullptr;
}

uint32_t xlasm::bits_needed_signed(int64_t v)
{
    bool s = (static_cast<uint64_t>(v) & (uint64_t{1} << 63)) != 0;

    for (uint32_t b = 62; b > 0; b--)
        if (((v & (int64_t{1} << b)) != 0) != s)
            return (b + 2);

    return 1U;
}

uint32_t xlasm::bits_needed_unsigned(int64_t v)
{
    for (uint32_t b = 63; b > 0; b--)
        if ((v & (int64_t{1} << b)) != 0)
            return (b + 1);

    return 1U;
}

bool xlasm::check_truncation(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag)
{
    assert(b >= 1 && b <= 64);

    if (b == 0)
        return true;
    if (b >= 64)
        return false;

    int64_t minv = -(int64_t{1} << (b - 1));
    int64_t maxv = (int64_t{1} << (b)) - 1;

    if (v < minv || v > maxv)
    {
        if (errwarnflag == 1)
            warning("%s out of range for %d-bit value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        else if (errwarnflag == 2)
            error("%s out of range for %d-bit value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        return true;
    }

    return false;
}

bool xlasm::check_truncation_signed(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag)
{
    assert(b >= 1 && b <= 64);

    if (b == 0)
        return true;
    if (b >= 64)
        return false;

    int64_t minv = -(int64_t{1} << (b - 1));
    int64_t maxv = (int64_t{1} << (b - 1)) - 1;

    if (v < minv || v > maxv)
    {
        if (errwarnflag == 1)
            warning("%s out of range for %d-bit signed value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        else if (errwarnflag == 2)
            error("%s out of range for %d-bit signed value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        return true;
    }

    return false;
}

bool xlasm::check_truncation_unsigned(const std::string & cmd, int64_t v, uint32_t b, int32_t errwarnflag)
{
    assert(b >= 1 && b <= 64);

    if (b == 64)
        return false;

    uint64_t maxv = (uint64_t{1} << (b)) - 1;
    uint64_t tv   = static_cast<uint64_t>(v);

    if (tv > maxv)
    {
        if (errwarnflag == 1)
            warning("%s out of range for %d-bit unsigned value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        else if (errwarnflag == 2)
            error("%s out of range for %d-bit unsigned value (0x" PR_X64 " / " PR_D64 ")", cmd.c_str(), b, v, v);
        return true;
    }

    return false;
}

void xlasm::add_sym(const char * name, symbol_t::sym_t type, int64_t value)
{
    std::string n(name);
    symbol_t &  sym = symbols[n];
    assert(sym.name.size() == 0);

    sym.name    = n;
    sym.type    = type;
    sym.value   = value;
    sym.section = ctxt.section;
}

void xlasm::remove_sym(const char * name)
{
    std::string n(name);
    symbols.erase(name);
}

void vstrprintf(std::string & str, const char * fmt, va_list va)
{
    size_t start = str.size();
    str.insert(str.end(), 1024, 0);

    vsnprintf(&str[start], 1023, fmt, va);

    size_t end = str.find('\0', start);
    assert(end != std::string::npos);
    str.erase(end);
}

void strprintf(std::string & str, const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vstrprintf(str, fmt, ap);

    va_end(ap);
}

static void show_help()
{
    printf("copasm - XarkLabs Xosera \"Slim Copper\" Assembler\n");
    printf("         Copyright 2022 Xark - MIT Licensed\n");
    printf("\n");
    printf("Usage:  copasm [options] <input files ...> [-o output.fmt]\n");
    printf("\n");
    printf("-b      maximum bytes hex per listing line (8-64, default 8)\n");
    printf("-c      suppress listing inside false conditional (.LISTCOND false)\n");
    printf("-d sym  define <sym>[=expression]\n");
    printf("-i      add default include search path (tried if include fails)\n");
    printf("-k      no error-kill, continue assembly despite errors\n");
    printf("-l      request listing file (uses output name with .lst)\n");
    printf("-m      suppress macro expansion listing (.LISTMAC false)\n");
    printf("-n      suppress macro name in listing (.MACNAME false)\n");
    printf("-o      output file name (using extension format .c/.h or binary)\n");
    printf("-q      quiet operation\n");
    printf("-v      verbose operation (repeat up to three times)\n");
    printf("-x      add symbol cross-reference to end of listing file\n");
    printf("\n");
}

int main(int argc, char ** argv)
{
    std::string              archname;
    std::vector<std::string> source_files;
    std::string              object_file;
    xlasm::opts_t            opts;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
                case 'a':
                    if (argv[i][2] != 0)
                        archname = &argv[i][2];
                    else if (i + 1 < argc)
                        archname = argv[++i];
                    else
                        fatal_error("Expected architecture name after -a option");
                    break;

                case 'b':
                    if (argv[i][2] != 0)
                    {
                        if (sscanf(&argv[i][2], "%u", &opts.listing_bytes) != 1)
                            fatal_error("Expected number after -b listing bytes option (8 per line)");
                    }
                    else if (i + 1 < argc)
                    {
                        if (sscanf(argv[++i], "%u", &opts.listing_bytes) != 1)
                            fatal_error("Expected number after -b listing bytes option (8 per line)");
                    }
                    else
                    {
                        fatal_error("Expected number after -b listing bytes option (8 per line)");
                    }


                    opts.listing_bytes = (opts.listing_bytes + 7) & ~7U;
                    if (opts.listing_bytes < 8)
                        opts.listing_bytes = 8;
                    break;

                case 'c':
                    opts.suppress_false_conditionals = true;
                    break;

                case 'd':
                    if (argv[i][2] != 0)
                    {
                        opts.define_sym.push_back(&argv[i][2]);
                    }
                    else if (i + 1 < argc)
                    {
                        opts.define_sym.push_back(argv[++i]);
                    }
                    else
                    {
                        fatal_error("Expected symbol after -d define sym option");
                    }
                    break;

                case 'i':
                    if (argv[i][2] != 0)
                    {
                        opts.include_path.push_back(&argv[i][2]);
                    }
                    else if (i + 1 < argc)
                    {
                        opts.include_path.push_back(argv[++i]);
                    }
                    else
                    {
                        fatal_error("Expected path after -i include path option");
                    }
                    break;

                case 'h':
                case '?': {
                    show_help();
                    exit(EXIT_SUCCESS);
                }
                case 'm':
                    opts.suppress_macro_expansion = true;
                    break;

                case 'n':
                    opts.suppress_macro_name = true;
                    break;

                case 'k':
                    opts.no_error_kill = true;
                    break;

                case 'l':
                    opts.listing = true;
                    break;

                case 'o':
                    if (argv[i][2] != 0)
                    {
                        object_file = &argv[i][2];
                    }
                    else if (i + 1 < argc)
                    {
                        object_file = argv[++i];
                    }
                    else
                    {
                        fatal_error("Expected filename after -o output file option");
                    }
                    break;

                case 'q':
                    opts.verbose = 0;
                    break;

                case 'v':
                    opts.verbose++;
                    break;

                case 'x':
                    opts.xref = true;
                    break;

                default:
                    show_help();
                    fatal_error("Unrecognized option -%c", argv[i][1]);
                    break;
            }

            continue;
        }
        source_files.push_back(std::string(argv[i]));
    }

    if (opts.verbose > 1)
    {
        if (opts.verbose == 2)
            printf("Verbose status messages enabled.\n");
        else if (opts.verbose > 2)
            printf("Verbose status and debugging messages enabled.\n");
    }

    copper copperarch;

    if (archname.size() == 0)
        archname = "copper";

    Ixlarch * initialarch = Ixlarch::find_arch(archname);

    if (initialarch == nullptr)
    {
        printf("Supported architectures (with variants and identifiers):\n");
        for (auto & a : Ixlarch::architectures)
        {
            printf("  %s\n", a->variant_names());
        }
        printf("\n");

        fatal_error("Unrecognized architecture \"%s\".", archname.c_str());
    }

    if (!source_files.size())
    {
        show_help();
        fatal_error("No input file(s) specified");
    }

    xlasm xl(archname);

    int rc = xl.assemble(source_files, object_file, opts);

    return rc;
}

// EOF
