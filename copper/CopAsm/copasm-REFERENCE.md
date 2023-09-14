# XarkLabs copasm "Slim" Copper Assembler Reference

This assembler was a work-in-progress multi-architecture assembler that has been "bit-rotting" on my drive for some time now. I have now repurposed it into an Xosera "Slim Copper" co-processor assembler. It seems to be working well, but it has not been very well tested, so be cautious trusting it.

This version of the assembler is configured to produce output suitable for the Xosera "Slim Copper" (the 2nd revision of the Xosera copper using a very minimal 16-bit ISA).  This consists of big-endian 16-bit words and supports the XR address space of Xosera, making the default origin address $C000 (start of copper region).

## Invoking copasm

```plain text
Usage:  copasm [options] <input files ...> [-o output.fmt]

-b      maximum bytes hex per listing line (8-64, default 8)
-c      suppress listing inside false conditional (.LISTCOND false)
-d sym  define <sym>[=expression]
-i      add default include search path (tried if include fails)
-k      no error-kill, continue assembly despite errors
-l      request listing file (uses output name with .lst)
-m      suppress macro expansion listing (.LISTMAC false)
-n      suppress macro name in listing (.MACNAME false)
-o      output file name (using extension format .c/.h or binary)
-q      quiet operation
-v      verbose operation (repeat up to three times)
-x      add symbol cross-reference to end of listing file
```

Example:

```shell
copasm -l color_screen.casm -o out/color_screen.h
```

## Assembler Directives

| Directive                         | Description                                                                  |
|-----------------------------------|------------------------------------------------------------------------------|
| `INCLUDE` *"filename"*            | Include *"filename"* as input file, resume after included file               |
| `INCBIN` *"filename"*             | Include the binary contents of *"filename"* in the output                    |
| `ORG` *expression*                | Change initial output address, or logical origin (output will be contiguous) |
| `UNDEFINE` *symbol*               | Undefine *symbol*                                                            |
| *symbol* `EQU` *expression*       | Assign *expression* to *symbol* (also `=` and `ASSIGN` aliases)              |
| `ALIGN` *expression*              | Align output to power of two boundary (e.g. 2, 4, 8, 16 etc.)                |
| `SPACE` *word-count*              | Reserve *expression* words space in output                                   |
| `FILL` *word-count*,*word-value*  | Fill  *expression* words space in output                                     |
| `HEX` *packed-hex-words*          | Hex 4-digit packed constants, e.g., `DEADBEEFCOFFEE1`                        |
| *name* `MACRO`\[*arg1*, ...\]     | Define macro *name*, args with `\` will be substituted on use (e.g. `\arg1`) |
| `ENDM`                            | End macro definition                                                         |
| `WORD` *expr*\[,*exp* ...\]       | Define literal words (also `DW` and `SHORT` aliases)                         |
| `IF` *condexpr*                   | If *condexpr* zero, assembly suppressed until `ELSE`, `ELSEIF` or `ENDIF`    |
| `ELSE`                            | Else case for preceding `IF`                                                 |
| `ELSEIF` *condexpr*               | End preceding `IF` and start else case until `ELSE`, `ELSEIF` or `ENDIF`     |
| `ENDIF` *condexpr*                | End preceding `IF`                                                           |
| `MSG` *"string"* [, *expr* ...]   | Print *string* (and *expr*s) to console as NOTE during assembly              |
| `PRINT` *"string"* [, *expr* ...] | Print *string* (and *expr*s) to console as NOTE during assembly              |
| `ASSERT` *condexpr*               | Generate error if *condexpr* is zero (false)                                 |
| `WARN` *"string"* [, *expr* ...]  | Generate warning with *string* (and *expr*s)                                 |
| `ERROR` *"string"* [, *expr* ...] | Generate error with *string* (and *expr*s)                                   |
| `EXIT` *"string"* [, *expr* ...]  | Generate fatal error with *string* (and *expr*s) and end assmebly            |
| `LIST` *condexpr*                 | Enable or disable line listing                                               |
| `LISTMAC` *condexpr*              | Enable or disable macro listing                                              |
| `MACNAME` *condexpr*              | Enable or disable macro names in listing                                     |
| `LISTCOND` *condexpr*             | Enable or disable listing of lines inside false conditional                  |

Words at the start of a line are assumed to be label definitions (otherwise append a colon, `:`).  Labels can be used before they are defined (multiple pass assembler).

## Copper Instructions

| Copper Assembly             | Opcode Bits                 | B | # | ~  | Description                               |
|-----------------------------|-----------------------------|---|---|----|-------------------------------------------|
| `SETI`   *xadr14*,`#`*im16* | `rr00` `oooo` `oooo` `oooo` | B | 2 | 4  | sets [xadr14] &larr; to #val16            |
| + *im16* *value*            | `iiii` `iiii` `iiii` `iiii` | - | - | -  | *(im16, 2<sup>nd</sup> word of `SETI`)*   |
| `SETM`  *xadr16*,*cadr12*   | `--01` `rccc` `cccc` `cccc` | B | 2 | 4  | sets [xadr16] &larr; to [cadr12]          |
| + *xadr16* *address*        | `rroo` `oooo` `oooo` `oooo` | - | - | -  | *(xadr16, 2<sup>nd</sup> word of `SETM`)* |
| `HPOS`   `#`*im11*          | `--10` `0iii` `iiii` `iiii` |   | 1 | 4+ | wait until video `HPOS` >= *im11*         |
| `VPOS`   `#`*im11*          | `--10` `1iii` `iiii` `iiii` |   | 1 | 4+ | wait until video `VPOS` >= *im11*         |
| `BRGE`   *cadd11*           | `--11` `0ccc` `cccc` `cccc` |   | 1 | 4  | if (`B`==0) `PC` &larr; *cadd11*          |
| `BRLT`   *cadd11*           | `--11` `1ccc` `cccc` `cccc` |   | 1 | 4  | if (`B`==1) `PC` &larr; *cadd11*          |

| Legend   | Description                                                                                    |
|----------|------------------------------------------------------------------------------------------------|
| `B`      | borrow flag set, true when `RA` < *val16* written (borrow after unsigned subtract)             |
| `#`      | number 16-bit words for opcodes                                                                |
| `~`      | number of copper cycles (each cycle is the time for one native pixel)                          |
| *xadr14* | 2-bit XR region + 12-bit offset (1<sup>st</sup> word of `SETI`, destination address)           |
| *im16*   | 16-bit immediate word (2<sup>nd</sup> word of `SETI`, source address)                          |
| *cadr12* | 11-bit copper address or register with bit [11] (1<sup>st</sup> word of `SETM`, source adress) |
| *xadr16* | XR region + 14-bit offset (2<sup>nd</sup> word of `SETM`, destination address)                 |
| *im11*   | 11-bit immediate value (used with `HPOS`, `VPOS` wait opcodes)                                 |
| *cadd11* | 11-bit copper address (used with `BRGE`, `BRLT` branch opcodes)                                |

## Pseudo Copper Instructions

| Pseudo Instruction         | Aliased Instruction         | Description                                                        |
|----------------------------|-----------------------------|--------------------------------------------------------------------|
| `MOVE` `#`*imm16*,*xadr14* | `SETI` *xadr14*,`#`*imm16*  | m68k order `SETI`, `#`*imm16* &rarr; [*xadr14*]                    |
| `MOVE` *cadr12*,*xadr16*   | `SETM` *xadr16*,*cadr12*    | m68k order `SETM`, *xadr16* &rarr; [*cadr12*]                      |
| `MOVI` `#`*imm16*,*xadr14* | `SETI` *xadr14*,`#`*imm16*  | m68k order `SETI`, `#`*imm16* &rarr; [*xadr14*]                    |
| `MOVM` *cadr12*,*xadr16*   | `SETM` *xadr16*,*cadr12*    | m68k order `SETM`, *cadr12* &rarr; [*xadr16*]                      |
| `LDI` `#`*imm16*           | `SETI` `RA`,`#`*imm16*      | Load register `RA` &larr; `#`*imm16*, clear `B` flag               |
| `LDM` *cadr12*             | `SETM` `RA`,*cadr12*        | Load register `RA` &larr; [*cadr12*], clear `B` flag               |
| `STM` *xadr16*             | `SETM` *xadr16*,`RA`        | Store register `RA` &rarr; [*xadr16*], clear `B` flag              |
| `CLRB`                     | `SETM` `RA`,`RA`            | Store register `RA` &rarr; `RA`, clear `B` flag                    |
| `SUBI` `#`*imm16*          | `SETI` `RA_SUB`,`#`*imm16*  | `RA` &larr; `RA` - *imm16*, update `B` flag                        |
| `ADDI` `#`*imm16*          | `SETI` `RA_SUB`,`#-`*imm16* | `RA` &larr; `RA` + *imm16*, update `B` flag (subtract of -*imm16*) |
| `SUBM` *cadr12*            | `SETM` `RA_SUB`,*cadr12*    | `RA` &larr; `RA` - [*cadr12*], `B` flag updated                    |
| `CMPI` `#`*imm16*          | `SETI` `RA_CMP`,`#`*imm16*  | test if `RA` < *imm16*, update `B` flag only                       |
| `CMPM` *cadr12*            | `SETM` `RA_CMP`,*cadr12*    | test it `RA` < [*cadr12*], update `B` flag only                    |

> :mag: **Source and destination operand order:** The `SETI/SETM` assembler instructions always use *destination* &larr; *source*, and the `MOVI/MOVM` (and `MOVE`) instructions always use m68k style *source* &rarr; *destination*. However, note that in the machine code, the source and destination operand words are in *reversed* order between `SETI/MOVI` and `SETM/MOVM` opecodes.  With `SETI/MOVI`, the first opcode word is the *destination* 14-bit XR address, with opcode bits `[13:12]` set to `00` (which are usually already zero in XR addresses, except for the last 1KW of TILEMEM). With `SETM/MOVM`, the first opcode word is the copper memory *source* address with opcode bits `[13:12]` set to `01` (and bits `[15:14]` technically ignored, but usually set to `11`  copper region for consistency).  When self-modifying a `SETM/MOVM` typically you would OR or add predefined `SETM` or `MOVM` symbol to set the opcode bit `[12]`.

## Copper Predefined Symbols

| Symbol       | Value   | Description                                                         |
|--------------|---------|---------------------------------------------------------------------|
| `true`       | `1`     | True constant                                                       |
| `false`      | `0`     | False constant                                                      |
| `RA`         | `$800`  | (addr read/write) RA accumulator register                           |
| `RA_SUB`     | `$801`  | (addr write) RA = RA - value written, update B flag                 |
| `RA_CMP`     | `$7FF`  | (addr write) compute RA - value written, update B flag (RA not set) |
| `SETI`       | `$0000` | SETI opcode bits (zero, but kept for consistency)                   |
| `MOVI`       | `$0000` | MOVI alias for SETI opcode bits (zero, but kept for consistency)    |
| `SETM`       | `$1000` | SETM opcode bits (for self-modifying code)                          |
| `MOVM`       | `$1000` | MOVM alias for SETM opcode bits (for self-modifying code)           |
| `HPOS`       | `$2000` | HPOS opcode bits (for self-modifying code)                          |
| `VPOS`       | `$2800` | VPOS opcode bits (for self-modifying code)                          |
| `BRGE`       | `$3000` | BRGE opcode bits (for self-modifying code)                          |
| `BRLT`       | `$3800` | BRLT opcode bits (for self-modifying code)                          |
| `H_EOL`      | `$07FF` | HPOS End-of-Line position (waits until `HPOS #0` on next line)      |
| `V_EOF`      | `$03FF` | VPOS End-of-Frame position (waits until reset at end of frame)      |
| `V_WAITBLIT` | `$07FF` | VPOS blitter wait position (wait until blit idle or end of frame)   |

## Assembler Expressions

The assembler supports C style expressions with normal precedence rules (done internally using 64-bit integer operations). Numeric
literals can use `0x` or `$` for hexadecimal and `0b` for binary. ASCII characters are enclosed in single quotes (e.g., `'A'`).

The following C style operators are supported:

| Operator                     | Description                                         |
|------------------------------|-----------------------------------------------------|
| `-`                          | Unary minus                                         |
| `+`                          | Unary plus                                          |
| `~`                          | Unary not                                           |
| `!`                          | Logical not                                         |
| `**`                         | Exponentiation                                      |
| `*`                          | Multiplication                                      |
| `+`                          | Addition                                            |
| `-`                          | Subtraction                                         |
| `<<`                         | Shift left                                          |
| `>>`                         | Shift right                                         |
| `==`                         | Logical equality                                    |
| `!=`                         | Logical non-equality                                |
| `<`                          | Logical less-than                                   |
| `<=`                         | Logical less-than or equal                          |
| `>`                          | Logical greater-than                                |
| `>=`                         | Logical greater-than or equal                       |
| `&`                          | Bitwise AND                                         |
| `\|`                         | Bitwise OR                                          |
| `^`                          | Bitwise XOR                                         |
| `&&`                         | Logical AND                                         |
| `\|\|`                       | Logical OR                                          |
| *eval* `?` *tval* `:` *fval* | Ternary conditional expression                      |
| `\`                          | Division                                            |
| `%`                          | Modulo                                              |
| `(` *expression* `)`         | Parenthesis can be used to control evaluation order |

> :mag: **Assembler Defintion File** Adding `.include "xosera_m68k_defs.inc"` will include a file defining Xosera registers and constants for use in copper programs.

> :mag: **C Include Compatibility** When using the `C` compiler preprocessor, CopAsm is also generally compatible with the C include headers and that only define macros with expressions.  The default Xosera Makefile will invoke the C preprocessor (with `-D__COPASM__=1` defined) before assembly on `.cpasm` files (vs normal `.casm` files that are only assembled).  This can be useful to define constants shared between C/C++ and copper code (an exmaple of this is in the `xosera_boing_m68k` sample).  

See [Xosera Reference](../../REFERENCE.md) for more details about Xosera copper operation.
