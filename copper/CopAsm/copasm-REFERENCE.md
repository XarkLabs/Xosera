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

| Instruction               | Opcodes       | Words | Description                                                       |
|---------------------------|---------------|-------|-------------------------------------------------------------------|
| `SETI` *xaddr14*,#*imm16* | `$Xooo $iiii` | 2     | Set XR dest *xaddr14* to value *imm16* (`X`=`0`/`4`/`8`/`C`)      |
| `SETM` *xaddr16*,*caddr*  | `$Dccc $xooo` | 2     | Set XR dest *xaddr16* to contents of memory *caddr*               |
| `HPOS` #*horiz-pos*       | `$2hhh`       | 1     | Halt until H >= *horiz-pos* (`hhh`=`0`-`$7FF`)                    |
| `VPOS` #*vert-pos*        | `$2vvv`       | 1     | Halt until V >= *vert-pos* (`vvv`=`$800\|0-$3FF`,`[10]=`waitblit) |
| `BRGE` *caddr*            | `$3ccc`       | 1     | Branch if `B`=`0` (`RA` - write-val >= `0`, `ccc`=`0-$7FF` )      |
| `BRLT` *caddr*            | `$3ccc`       | 1     | Branch if `B`=`1` (`RA` - write-val < `0`, `ccc`=`$800\|0-$7FF`)  |

## Pseudo Copper Instructions

| Instruction                | Alias  | Words | Description                                                              |
|----------------------------|--------|-------|--------------------------------------------------------------------------|
| `MOVE` `#`*imm16*,*xadr14* | `SETI` | 2     | m68k style `MOVE` immediate, copy `#`*imm16* &rarr; *xadr16*             |
| `MOVE` *cadr12*,*xadr16*   | `SETM` | 2     | m68k style `MOVE` memory, copy *source* &rarr; *dest*                    |
| `MOVI` `#`*imm16*,*xadr14* | `SETI` | 2     | m68k order `SETI`, copy `#`*imm16* &rarr; *cadr12*                       |
| `MOVM` *cadr12*,*xadr16*   | `SETM` | 2     | m68k order`SETM`, copy *cadr12* &rarr; *xadr16*                          |
| `LDI` `#`*imm16*           | `SETI` | 2     | Load `RA` register with value *imm16*, set `B`=`0`                       |
| `LDM` *cadr12*             | `SETM` | 2     | Load `RA` register with contents of memory *cadr12*, set `B`=`0`         |
| `STM` *xadr16*             | `SETM` | 2     | Store `RA` register contents into memory *xadr16*, set `B`=`0`           |
| `CLRB`                     | `SETM` | 2     | Store `RA` register into `RA`, set `B`=`0`                               |
| `SUBI` `#`*imm16*          | `SETI` | 2     | `RA` = `RA` - *imm16*, `B` flag updated                                  |
| `ADDI` `#`*imm16*          | `SETI` | 2     | `RA` = `RA` + *imm16*, `B` flag updated (for subtract of -*imm16*)       |
| `SUBM` *cadr12*            | `SETM` | 2     | `RA` = `RA` - contents of *cadr12*, `B` flag updated                     |
| `CMPI` `#`*imm16*          | `SETI` | 2     | test if `RA` < *imm16*, `B` flag updated (`RA` not altered)              |
| `CMPM` *cadr12*            | `SETM` | 2     | test it `RA` < contents of *cadr12*, `B` flag updated (`RA` not altered) |

## Copper Predefined Symbols

| Symbol       | Value   | Description                                                         |
|--------------|---------|---------------------------------------------------------------------|
| `true`       | `1`     | True constant                                                       |
| `false`      | `0`     | False constant                                                      |
| `RA`         | `$800`  | (addr read/write) RA accumulator register                           |
| `RA_SUB`     | `$801`  | (addr write) RA = RA - value written, update B flag                 |
| `RA_CMP`     | `$7FF`  | (addr write) compute RA - value written, update B flag (RA not set) |
| `H_EOL`      | `$7FF`  | Horizontal End-of-Line pos (waits until `HPOS #0` on next line)     |
| `V_EOF`      | `$3FF`  | Vertical End-of-Frame pos (waits until reset at end of frame)       |
| `V_WAITBLIT` | `$7FF`  | Vertical wait for blitter pos (wait until blit ready or EOF)        |
| `SETI`       | `$0000` | SETI opcode bits (for self-modifying code)                          |
| `MOVEI`      | `$0000` | MOVEI alias for SETI opcode bits (for self-modifying code)          |
| `SETM`       | `$1000` | SETM opcode bits (for self-modifying code)                          |
| `MOVE`       | `$1000` | MOVE alias for SETM opcode bits (for self-modifying code)           |
| `HPOS`       | `$2000` | HPOS opcode bits (for self-modifying code)                          |
| `VPOS`       | `$2800` | VPOS opcode bits (for self-modifying code)                          |
| `BRGE`       | `$3000` | BRGE opcode bits (for self-modifying code)                          |
| `BRLT`       | `$3800` | BRLT opcode bits (for self-modifying code)                          |

    xl->add_sym("H_EOL", xlasm::symbol_t::LABEL, 0x7FF);
    xl->add_sym("V_EOF", xlasm::symbol_t::LABEL, 0x3FF);
    xl->add_sym("V_WAITBLIT", xlasm::symbol_t::LABEL, 0x7FF);

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
