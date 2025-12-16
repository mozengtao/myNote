# GDB Symbol Loading Mechanism: Debugging Stripped Binaries

## Table of Contents
1. [Overview](#overview)
2. [How It Works](#how-it-works)
3. [Why It Works](#why-it-works)
4. [What Gets Stripped](#what-gets-stripped)
5. [Key Requirements](#key-requirements)
6. [Complete Practical Example](#complete-practical-example)

---

## Overview

```
+------------------------------------------------------------------+
|                    GDB Symbol Loading Architecture                |
+------------------------------------------------------------------+
|                                                                   |
|   +---------------------+         +-------------------------+     |
|   |   Production Host   |         |    Development Host     |     |
|   |---------------------|         |-------------------------|     |
|   |                     |         |                         |     |
|   |  +--------------+   |         |  +------------------+   |     |
|   |  | stripped.elf |   |         |  | debug.elf        |   |     |
|   |  | (no symbols) |   |         |  | (with symbols)   |   |     |
|   |  | Size: 1.2 MB |   |         |  | Size: 45 MB      |   |     |
|   |  +--------------+   |         |  +------------------+   |     |
|   |         |           |         |          |              |     |
|   +---------|-----------+         +----------|--------------|     |
|             |                                |                    |
|             |         GDB Session            |                    |
|             |    +------------------+        |                    |
|             +--->|  Load Process    |<-------+                    |
|                  |  Memory Image    |  symbol-file                |
|                  +------------------+                             |
|                           |                                       |
|                           v                                       |
|                  +------------------+                             |
|                  | Combined View:   |                             |
|                  | Code + Symbols   |                             |
|                  | = Full Debugging |                             |
|                  +------------------+                             |
+------------------------------------------------------------------+
```

**中文说明：**
GDB 符号加载机制允许将生产环境的精简二进制文件与开发环境的调试符号文件结合使用。生产环境的文件体积小（如 1.2MB），而带符号的调试文件可能很大（如 45MB）。GDB 可以加载精简二进制的内存映像，然后从调试文件加载符号信息，实现完整的调试能力。

---

## How It Works

### Symbol Loading Process

```
+------------------------------------------------------------------------+
|                     GDB Symbol Loading Process                          |
+------------------------------------------------------------------------+
|                                                                         |
|  Step 1: Load Stripped Binary                                           |
|  +-----------------------------------------------------------------+    |
|  |  $ gdb ./stripped_binary                                         |   |
|  |                                                                   |  |
|  |  GDB reads:                                                       |  |
|  |  - ELF header (architecture, entry point)                         |  |
|  |  - .text section (executable code)                                |  |
|  |  - .data section (initialized data)                               |  |
|  |  - .bss section (uninitialized data)                              |  |
|  |  - NO symbol information available                                |  |
|  +-----------------------------------------------------------------+    |
|                              |                                          |
|                              v                                          |
|  Step 2: Load Symbol File                                               |
|  +-----------------------------------------------------------------+    |
|  |  (gdb) symbol-file ./debug_binary                                |   |
|  |                                                                   |  |
|  |  GDB reads from debug file:                                       |  |
|  |  - .symtab (symbol table)                                         |  |
|  |  - .strtab (string table)                                         |  |
|  |  - .debug_info (DWARF debug info)                                 |  |
|  |  - .debug_line (source line mapping)                              |  |
|  |  - .debug_frame (call frame info)                                 |  |
|  +-----------------------------------------------------------------+    |
|                              |                                          |
|                              v                                          |
|  Step 3: Address Mapping                                                |
|  +-----------------------------------------------------------------+    |
|  |  GDB creates mapping:                                             |  |
|  |                                                                   |  |
|  |  Memory Address    Symbol Name      Source Location               |  |
|  |  ---------------------------------------------------------------- |  |
|  |  0x0000555555555149  main()         main.c:42                     |  |
|  |  0x00005555555551a0  process_data() utils.c:128                   |  |
|  |  0x0000555555555230  cleanup()      utils.c:256                   |  |
|  +-----------------------------------------------------------------+    |
|                              |                                          |
|                              v                                          |
|  Step 4: Ready for Debugging                                            |
|  +-----------------------------------------------------------------+    |
|  |  (gdb) break main                                                 |  |
|  |  Breakpoint 1 at 0x555555555149: file main.c, line 42.            |  |
|  |                                                                   |  |
|  |  (gdb) run                                                        |  |
|  |  Starting program: ./stripped_binary                              |  |
|  |  Breakpoint 1, main() at main.c:42                                |  |
|  |  42      int result = process_data(input);                        |  |
|  +-----------------------------------------------------------------+    |
+------------------------------------------------------------------------+
```

**中文说明：**
GDB 符号加载过程分为四个步骤：
1. **加载精简二进制**：读取 ELF 头部、代码段、数据段，但没有符号信息
2. **加载符号文件**：从调试版本读取符号表、字符串表和 DWARF 调试信息
3. **地址映射**：GDB 建立内存地址到符号名和源代码位置的映射表
4. **准备调试**：现在可以使用函数名设置断点，查看源代码行号

---

## Why It Works

### Address Space Mapping

```
+------------------------------------------------------------------------+
|                       Address Space Comparison                          |
+------------------------------------------------------------------------+
|                                                                         |
|        Stripped Binary                    Debug Binary                  |
|   (same source, same flags)          (same source, same flags + -g)    |
|                                                                         |
|   +------------------------+         +------------------------+         |
|   | ELF Header             |         | ELF Header             |         |
|   | Entry: 0x1080          |   ==    | Entry: 0x1080          |         |
|   +------------------------+         +------------------------+         |
|   |                        |         |                        |         |
|   | .text (Code Section)   |         | .text (Code Section)   |         |
|   | 0x1080: main           |   ==    | 0x1080: main           |         |
|   | 0x10f0: foo            |   ==    | 0x10f0: foo            |         |
|   | 0x1160: bar            |   ==    | 0x1160: bar            |         |
|   |                        |         |                        |         |
|   | [identical bytes]      |   ==    | [identical bytes]      |         |
|   +------------------------+         +------------------------+         |
|   |                        |         |                        |         |
|   | .data (Data Section)   |         | .data (Data Section)   |         |
|   | 0x4000: global_var     |   ==    | 0x4000: global_var     |         |
|   |                        |         |                        |         |
|   +------------------------+         +------------------------+         |
|   |                        |         |                        |         |
|   | .rodata (Read-only)    |         | .rodata (Read-only)    |         |
|   | 0x2000: "Hello"        |   ==    | 0x2000: "Hello"        |         |
|   |                        |         |                        |         |
|   +------------------------+         +------------------------+         |
|   |                        |         |                        |         |
|   | (NO .symtab)           |         | .symtab (Symbol Table) |         |
|   | (NO .debug_*)          |         | .debug_info            |         |
|   |                        |         | .debug_line            |         |
|   |                        |         | .debug_frame           |         |
|   |                        |         | .debug_abbrev          |         |
|   +------------------------+         +------------------------+         |
|                                                                         |
|   Size: ~1 MB                        Size: ~50 MB                       |
|                                                                         |
|   KEY INSIGHT: Code addresses are IDENTICAL in both binaries!           |
+------------------------------------------------------------------------+
```

**中文说明：**
地址空间映射的核心原理：
- 两个二进制文件从**相同源代码**、使用**相同编译器标志**编译
- `.text` 代码段、`.data` 数据段、`.rodata` 只读数据段的**地址完全相同**
- 机器码字节**完全一致**
- 唯一区别是精简版本移除了 `.symtab` 和 `.debug_*` 段
- 这就是为什么符号文件的地址可以直接应用到精简二进制

### Symbol Table Lookup

```
+------------------------------------------------------------------------+
|                      Symbol Table Lookup Process                        |
+------------------------------------------------------------------------+
|                                                                         |
|  When you type: (gdb) break process_data                                |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  |                    Symbol Table (.symtab)                         |  |
|  +------------------------------------------------------------------+   |
|  | Index | Name          | Type     | Bind   | Address    | Size    |   |
|  |-------|---------------|----------|--------|------------|---------|   |
|  |   0   | (null)        | NOTYPE   | LOCAL  | 0x0        | 0       |   |
|  |   1   | main          | FUNC     | GLOBAL | 0x1149     | 87      |   |
|  |   2   | process_data  | FUNC     | GLOBAL | 0x11a0     | 144     |  <-- Found!
|  |   3   | cleanup       | FUNC     | GLOBAL | 0x1230     | 52      |   |
|  |   4   | global_var    | OBJECT   | GLOBAL | 0x4010     | 4       |   |
|  +------------------------------------------------------------------+   |
|                              |                                          |
|                              v                                          |
|  GDB extracts: Address = 0x11a0                                         |
|                              |                                          |
|                              v                                          |
|  +------------------------------------------------------------------+   |
|  |                 DWARF Debug Info (.debug_info)                    |  |
|  +------------------------------------------------------------------+   |
|  | DW_TAG_subprogram                                                 |  |
|  |   DW_AT_name: "process_data"                                      |  |
|  |   DW_AT_low_pc: 0x11a0                                            |  |
|  |   DW_AT_high_pc: 0x1230                                           |  |
|  |   DW_AT_decl_file: "utils.c"                                      |  |
|  |   DW_AT_decl_line: 128                                            |  |
|  +------------------------------------------------------------------+   |
|                              |                                          |
|                              v                                          |
|  +------------------------------------------------------------------+   |
|  |                 Line Number Table (.debug_line)                   |  |
|  +------------------------------------------------------------------+   |
|  | Address    | File     | Line | Column | IsStmt |                  |  |
|  |------------|----------|------|--------|--------|                  |  |
|  | 0x11a0     | utils.c  | 128  | 1      | true   |  <-- First line  |  |
|  | 0x11a8     | utils.c  | 129  | 5      | true   |                  |  |
|  | 0x11b4     | utils.c  | 130  | 5      | true   |                  |  |
|  +------------------------------------------------------------------+   |
|                              |                                          |
|                              v                                          |
|  Result: Breakpoint 1 at 0x11a0: file utils.c, line 128.                |
+------------------------------------------------------------------------+
```

**中文说明：**
符号表查找过程：
1. 当用户输入 `break process_data` 时，GDB 首先在 `.symtab` 符号表中查找
2. 找到 `process_data` 函数，获取其地址 `0x11a0`
3. 然后在 DWARF 调试信息中查找该地址对应的源文件和行号
4. 最后在行号表中建立地址到源代码行的精确映射
5. 结果：GDB 知道 `0x11a0` 对应 `utils.c` 文件第 128 行

### How GDB Uses Symbols at Runtime

```
+------------------------------------------------------------------------+
|                    GDB Runtime Symbol Usage                             |
+------------------------------------------------------------------------+
|                                                                         |
|                        Running Process                                  |
|                   +---------------------+                               |
|                   |    Process Memory   |                               |
|                   |---------------------|                               |
|                   | Stack:              |                               |
|                   |   0x7fff1234: rbp   |                               |
|                   |   0x7fff1238: ret   |                               |
|                   |---------------------|                               |
|                   | Heap:               |                               |
|                   |   0x555555559000    |                               |
|                   |---------------------|                               |
|                   | Code (.text):       |                               |
|                   |   0x555555555149    |<-- PC (current instruction)   |
|                   |---------------------|                               |
|                   | Data (.data):       |                               |
|                   |   0x555555558010    |                               |
|                   +---------------------+                               |
|                            |                                            |
|                            | GDB queries                                |
|                            v                                            |
|   +----------------------------------------------------------------+    |
|   |                    Symbol File in GDB                            |  |
|   +----------------------------------------------------------------+    |
|   |                                                                  |  |
|   |  Query 1: "What function is at PC 0x555555555149?"               |  |
|   |  --------------------------------------------------------        |  |
|   |  Lookup in .symtab:                                              |  |
|   |    0x555555555149 falls within main() [0x555555555149-0x5555...] |  |
|   |  Answer: main()                                                  |  |
|   |                                                                  |  |
|   |  Query 2: "What source line is at 0x555555555149?"               |  |
|   |  --------------------------------------------------------        |  |
|   |  Lookup in .debug_line:                                          |  |
|   |    0x555555555149 -> main.c, line 42                             |  |
|   |  Answer: main.c:42                                               |  |
|   |                                                                  |  |
|   |  Query 3: "What are the local variables in current frame?"       |  |
|   |  --------------------------------------------------------        |  |
|   |  Lookup in .debug_info:                                          |  |
|   |    main() has locals:                                            |  |
|   |      - int result at [rbp-0x4]                                   |  |
|   |      - char* input at [rbp-0x10]                                 |  |
|   |  Answer: result, input with stack offsets                        |  |
|   |                                                                  |  |
|   +----------------------------------------------------------------+    |
|                            |                                            |
|                            v                                            |
|   +----------------------------------------------------------------+    |
|   |                    GDB Display Output                            |  |
|   +----------------------------------------------------------------+    |
|   |  (gdb) where                                                     |  |
|   |  #0  main () at main.c:42                                        |  |
|   |  #1  0x00007ffff7e0a083 in __libc_start_main ()                  |  |
|   |                                                                  |  |
|   |  (gdb) info locals                                               |  |
|   |  result = 0                                                      |  |
|   |  input = 0x7fffffffe5c8 "test"                                   |  |
|   |                                                                  |  |
|   |  (gdb) list                                                      |  |
|   |  40      int main(int argc, char *argv[]) {                      |  |
|   |  41          char *input = argv[1];                              |  |
|   |  42          int result = process_data(input);   <-- current     |  |
|   |  43          printf("Result: %d\n", result);                     |  |
|   |  44          return 0;                                           |  |
|   +----------------------------------------------------------------+    |
+------------------------------------------------------------------------+
```

**中文说明：**
GDB 运行时如何使用符号：
1. **函数查找**：根据当前 PC（程序计数器）值在符号表中查找所在函数
2. **源代码行**：在行号表中查找当前地址对应的源文件和行号
3. **局部变量**：在 DWARF 调试信息中查找当前函数的局部变量及其栈偏移
4. GDB 将这些信息组合起来，显示友好的调试输出：
   - `where` 显示调用栈和源代码位置
   - `info locals` 显示局部变量的值
   - `list` 显示当前执行位置的源代码

---

## What Gets Stripped

### Comparison: Debug vs Production Image

```
+------------------------------------------------------------------------+
|              ELF Section Comparison: Debug vs Stripped                  |
+------------------------------------------------------------------------+
|                                                                         |
|  Debug Binary (compiled with -g)        Stripped Binary (strip applied) |
|  ================================       ================================|
|                                                                         |
|  $ readelf -S debug_binary              $ readelf -S stripped_binary    |
|                                                                         |
|  [Nr] Name              Type            [Nr] Name              Type     |
|  ---------------------------            ---------------------------     |
|  [ 1] .interp           PROGBITS        [ 1] .interp           PROGBITS |
|  [ 2] .note.gnu.build-id NOTE           [ 2] .note.gnu.build-id NOTE    |
|  [ 3] .gnu.hash         GNU_HASH        [ 3] .gnu.hash         GNU_HASH |
|  [ 4] .dynsym           DYNSYM          [ 4] .dynsym           DYNSYM   |
|  [ 5] .dynstr           STRTAB          [ 5] .dynstr           STRTAB   |
|  [ 6] .rela.dyn         RELA            [ 6] .rela.dyn         RELA     |
|  [ 7] .rela.plt         RELA            [ 7] .rela.plt         RELA     |
|  [ 8] .init             PROGBITS        [ 8] .init             PROGBITS |
|  [ 9] .plt              PROGBITS        [ 9] .plt              PROGBITS |
|  [10] .text             PROGBITS        [10] .text             PROGBITS |
|  [11] .fini             PROGBITS        [11] .fini             PROGBITS |
|  [12] .rodata           PROGBITS        [12] .rodata           PROGBITS |
|  [13] .data             PROGBITS        [13] .data             PROGBITS |
|  [14] .bss              NOBITS          [14] .bss              NOBITS   |
|  [15] .comment          PROGBITS        [15] .comment          PROGBITS |
|  [16] .symtab           SYMTAB          [XX] (REMOVED)                  |
|  [17] .strtab           STRTAB          [XX] (REMOVED)                  |
|  [18] .shstrtab         STRTAB          [16] .shstrtab         STRTAB   |
|  [19] .debug_aranges    PROGBITS        [XX] (REMOVED)                  |
|  [20] .debug_info       PROGBITS        [XX] (REMOVED)                  |
|  [21] .debug_abbrev     PROGBITS        [XX] (REMOVED)                  |
|  [22] .debug_line       PROGBITS        [XX] (REMOVED)                  |
|  [23] .debug_str        PROGBITS        [XX] (REMOVED)                  |
|  [24] .debug_line_str   PROGBITS        [XX] (REMOVED)                  |
|  [25] .debug_rnglists   PROGBITS        [XX] (REMOVED)                  |
|  [26] .debug_frame      PROGBITS        [XX] (REMOVED)                  |
|                                                                         |
|  Total: 26 sections                     Total: 16 sections              |
|  File size: ~45 MB                      File size: ~1.2 MB              |
+------------------------------------------------------------------------+
```

**中文说明：**
精简（strip）过程移除的内容：
- `.symtab` - 符号表（函数名、变量名）
- `.strtab` - 字符串表（符号名称字符串）
- `.debug_info` - DWARF 调试信息（类型、变量、作用域）
- `.debug_line` - 行号映射表（地址到源代码行）
- `.debug_frame` - 调用帧信息（栈展开）
- `.debug_abbrev` - DWARF 缩写表
- `.debug_str` - 调试字符串

保留的内容：
- `.text` - 可执行代码（完全不变）
- `.data` - 初始化数据
- `.rodata` - 只读数据
- `.dynsym` - 动态符号（用于动态链接，保留）

### Detailed Breakdown of Removed Sections

```
+------------------------------------------------------------------------+
|                    What Each Debug Section Contains                     |
+------------------------------------------------------------------------+
|                                                                         |
|  +------------------------------------------------------------------+   |
|  | Section: .symtab (Symbol Table)                                   |  |
|  |------------------------------------------------------------------ |  |
|  | Contains: Every function and global variable name with addresses  |  |
|  | Used for: Function names in backtraces, setting breakpoints       |  |
|  | Size impact: ~500KB for medium project                            |  |
|  |                                                                   |  |
|  | Example entry:                                                    |  |
|  |   Num:  Value          Size Type    Bind   Name                   |  |
|  |    45:  0000000000001149    87 FUNC    GLOBAL main                |  |
|  +------------------------------------------------------------------+   |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  | Section: .debug_info (DWARF Debug Information)                    |  |
|  |------------------------------------------------------------------ |  |
|  | Contains: Type definitions, variable locations, function params   |  |
|  | Used for: Displaying variable values, type information            |  |
|  | Size impact: ~30MB for medium project (LARGEST section!)          |  |
|  |                                                                   |  |
|  | Example content:                                                  |  |
|  |   <1><2d>: DW_TAG_subprogram                                      |  |
|  |      DW_AT_name        : main                                     |  |
|  |      DW_AT_type        : <0x52> (int)                             |  |
|  |      DW_AT_low_pc      : 0x1149                                   |  |
|  |      DW_AT_frame_base  : (DW_OP_reg6 (rbp))                       |  |
|  +------------------------------------------------------------------+   |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  | Section: .debug_line (Line Number Information)                    |  |
|  |------------------------------------------------------------------ |  |
|  | Contains: Mapping from addresses to source file:line              |  |
|  | Used for: Source-level stepping, showing current line             |  |
|  | Size impact: ~2MB for medium project                              |  |
|  |                                                                   |  |
|  | Example content:                                                  |  |
|  |   0x0000000000001149  [  42, 0] NS                                |  |
|  |   0x0000000000001159  [  43, 5] NS                                |  |
|  |   0x000000000000116a  [  44, 5] NS                                |  |
|  +------------------------------------------------------------------+   |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  | Section: .debug_frame (Call Frame Information)                    |  |
|  |------------------------------------------------------------------ |  |
|  | Contains: Instructions for unwinding the call stack               |  |
|  | Used for: Generating backtraces, exception handling               |  |
|  | Size impact: ~500KB for medium project                            |  |
|  |                                                                   |  |
|  | Example content:                                                  |  |
|  |   CIE: code_align=1, data_align=-8, ret_addr=16                   |  |
|  |   FDE: pc=0x1149..0x11a0                                          |  |
|  |        DW_CFA_def_cfa: r7 (rsp) ofs 8                             |  |
|  |        DW_CFA_offset: r16 (rip) at cfa-8                          |  |
|  +------------------------------------------------------------------+   |
+------------------------------------------------------------------------+
```

**中文说明：**
各调试段的详细内容：
1. **`.symtab` 符号表**：包含所有函数和全局变量的名称及地址，用于断点设置和回溯显示
2. **`.debug_info` 调试信息**：包含类型定义、变量位置、函数参数等，是最大的调试段（可达数十MB）
3. **`.debug_line` 行号信息**：地址到源文件行号的映射，用于单步调试和显示当前行
4. **`.debug_frame` 调用帧信息**：栈展开指令，用于生成调用栈回溯

---

## Key Requirements

```
+------------------------------------------------------------------------+
|                    Requirements for Symbol Matching                     |
+------------------------------------------------------------------------+
|                                                                         |
|  +------------------------------------------------------------------+   |
|  |                    MUST MATCH EXACTLY                             |  |
|  +------------------------------------------------------------------+   |
|  |                                                                   |  |
|  |  1. Source Code Version                                           |  |
|  |     +---------------------------------------------------------+   |  |
|  |     | Stripped: built from commit abc123                       |  |  |
|  |     | Debug:    built from commit abc123   <-- MUST BE SAME    |  |  |
|  |     +---------------------------------------------------------+   |  |
|  |                                                                   |  |
|  |  2. Compiler Version                                              |  |
|  |     +---------------------------------------------------------+   |  |
|  |     | Stripped: gcc 11.4.0                                     |  |  |
|  |     | Debug:    gcc 11.4.0                 <-- MUST BE SAME    |  |  |
|  |     +---------------------------------------------------------+   |  |
|  |                                                                   |  |
|  |  3. Optimization Level                                            |  |
|  |     +---------------------------------------------------------+   |  |
|  |     | Stripped: -O2                                            |  |  |
|  |     | Debug:    -O2                        <-- MUST BE SAME    |  |  |
|  |     |                                                          |  |  |
|  |     | WARNING: -O0 vs -O2 produces DIFFERENT code layout!      |  |  |
|  |     +---------------------------------------------------------+   |  |
|  |                                                                   |  |
|  |  4. Architecture                                                  |  |
|  |     +---------------------------------------------------------+   |  |
|  |     | Stripped: x86_64                                         |  |  |
|  |     | Debug:    x86_64                     <-- MUST BE SAME    |  |  |
|  |     +---------------------------------------------------------+   |  |
|  |                                                                   |  |
|  +------------------------------------------------------------------+   |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  |                    BUILD ID VERIFICATION                          |  |
|  +------------------------------------------------------------------+   |
|  |                                                                   |  |
|  |  Modern builds include a unique Build ID for verification:        |  |
|  |                                                                   |  |
|  |  $ readelf -n stripped_binary | grep "Build ID"                   |  |
|  |  Build ID: 5a3b8f2c1d4e6f7a8b9c0d1e2f3a4b5c6d7e8f9a               |  |
|  |                                                                   |  |
|  |  $ readelf -n debug_binary | grep "Build ID"                      |  |
|  |  Build ID: 5a3b8f2c1d4e6f7a8b9c0d1e2f3a4b5c6d7e8f9a               |  |
|  |            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^              |  |
|  |            MUST MATCH! GDB will warn if mismatch.                 |  |
|  |                                                                   |  |
|  +------------------------------------------------------------------+   |
|                                                                         |
|  +------------------------------------------------------------------+   |
|  |                    CONSEQUENCES OF MISMATCH                       |  |
|  +------------------------------------------------------------------+   |
|  |                                                                   |  |
|  |  If requirements don't match:                                     |  |
|  |                                                                   |  |
|  |  - Wrong function names at addresses                              |  |
|  |  - Source lines don't match execution                             |  |
|  |  - Variables show garbage values                                  |  |
|  |  - Breakpoints hit wrong locations                                |  |
|  |  - Stack traces are incorrect                                     |  |
|  |                                                                   |  |
|  |  Example of mismatch:                                             |  |
|  |    (gdb) break main                                               |  |
|  |    Breakpoint 1 at 0x1149: file main.c, line 42.                  |  |
|  |    (gdb) run                                                      |  |
|  |    Breakpoint 1, foo () at utils.c:87  <-- WRONG! Was main()      |  |
|  |                                                                   |  |
|  +------------------------------------------------------------------+   |
+------------------------------------------------------------------------+
```

**中文说明：**
符号匹配的关键要求：
1. **源代码版本**：必须是同一个 git commit 或代码版本
2. **编译器版本**：必须使用相同版本的 gcc/clang
3. **优化级别**：必须相同（-O0 和 -O2 产生完全不同的代码布局！）
4. **架构**：必须相同（x86_64 符号不能用于 ARM 二进制）

**Build ID 验证**：
- 现代编译器会嵌入唯一的 Build ID
- GDB 可以用它来验证符号文件是否匹配
- 如果不匹配，GDB 会发出警告

**不匹配的后果**：
- 函数名错误、源代码行不对应、变量值是垃圾、断点位置错误、栈回溯不正确

---

## Complete Practical Example

### Step 1: Create Sample Source Code

```
+------------------------------------------------------------------------+
|                         Sample Source Files                             |
+------------------------------------------------------------------------+

File: main.c
---------------------------------------------------------------------------
#include <stdio.h>
#include "utils.h"

int global_counter = 0;

int main(int argc, char *argv[]) {
    printf("Starting program...\n");
    
    int result = process_data(42);
    printf("Result: %d\n", result);
    
    cleanup();
    return 0;
}
---------------------------------------------------------------------------

File: utils.h
---------------------------------------------------------------------------
#ifndef UTILS_H
#define UTILS_H

int process_data(int input);
void cleanup(void);

#endif
---------------------------------------------------------------------------

File: utils.c
---------------------------------------------------------------------------
#include <stdio.h>
#include "utils.h"

extern int global_counter;

int process_data(int input) {
    int local_var = input * 2;
    global_counter++;
    printf("Processing: input=%d, local=%d\n", input, local_var);
    return local_var + global_counter;
}

void cleanup(void) {
    printf("Cleanup: counter was %d\n", global_counter);
    global_counter = 0;
}
---------------------------------------------------------------------------
```

### Step 2: Build Commands

```
+------------------------------------------------------------------------+
|                           Build Process                                 |
+------------------------------------------------------------------------+

# Method A: Build debug binary, then strip to create production version
# =====================================================================

# 1. Compile with debug symbols and optimization
$ gcc -g -O2 -o myapp_debug main.c utils.c

# 2. Check file size with symbols
$ ls -lh myapp_debug
-rwxr-xr-x 1 user user 48K Dec 15 10:00 myapp_debug

# 3. Verify debug sections exist
$ readelf -S myapp_debug | grep debug
  [26] .debug_aranges    PROGBITS         0000000000000000  00003040
  [27] .debug_info       PROGBITS         0000000000000000  00003070
  [28] .debug_abbrev     PROGBITS         0000000000000000  00003234
  [29] .debug_line       PROGBITS         0000000000000000  000032d4
  [30] .debug_str        PROGBITS         0000000000000000  00003398
  [31] .debug_line_str   PROGBITS         0000000000000000  00003458
  [32] .debug_rnglists   PROGBITS         0000000000000000  000034b8

# 4. Create stripped production version
$ cp myapp_debug myapp_prod
$ strip myapp_prod

# 5. Check stripped file size
$ ls -lh myapp_prod
-rwxr-xr-x 1 user user 16K Dec 15 10:00 myapp_prod

# 6. Verify debug sections are removed
$ readelf -S myapp_prod | grep debug
(no output - sections removed)

# 7. Verify Build IDs match
$ readelf -n myapp_debug | grep "Build ID"
    Build ID: 7f4a2b3c8d9e1f0a2b3c4d5e6f7a8b9c0d1e2f3a

$ readelf -n myapp_prod | grep "Build ID"
    Build ID: 7f4a2b3c8d9e1f0a2b3c4d5e6f7a8b9c0d1e2f3a
              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
              IDENTICAL - symbols will work!

# Method B: Generate separate .debug file (preferred for deployment)
# ==================================================================

# 1. Compile with debug symbols
$ gcc -g -O2 -o myapp main.c utils.c

# 2. Extract debug symbols to separate file
$ objcopy --only-keep-debug myapp myapp.debug

# 3. Strip the main binary
$ strip myapp

# 4. Add debug link to stripped binary (optional, for auto-discovery)
$ objcopy --add-gnu-debuglink=myapp.debug myapp

# 5. Result: Two files
$ ls -lh myapp*
-rwxr-xr-x 1 user user 16K Dec 15 10:00 myapp        # Production
-rw-r--r-- 1 user user 32K Dec 15 10:00 myapp.debug  # Debug symbols
```

**中文说明：**
构建过程有两种方法：

**方法 A**：先构建调试版本，然后复制并精简
- 使用 `-g -O2` 编译生成调试版本
- 复制后使用 `strip` 命令移除调试信息
- 两个文件的 Build ID 相同，可以配合使用

**方法 B**：生成独立的 `.debug` 文件（推荐用于部署）
- 使用 `objcopy --only-keep-debug` 提取调试符号到独立文件
- 使用 `strip` 精简主二进制
- 可选：使用 `--add-gnu-debuglink` 在二进制中添加指向调试文件的链接

### Step 3: GDB Debugging Session

```
+------------------------------------------------------------------------+
|                        GDB Debugging Session                            |
+------------------------------------------------------------------------+

# Start GDB with stripped binary
$ gdb ./myapp_prod

GNU gdb (Ubuntu 12.1-0ubuntu1) 12.1
Reading symbols from ./myapp_prod...
(No debugging symbols found in ./myapp_prod)    <-- Expected!
(gdb)

# Try to set breakpoint - fails without symbols
(gdb) break main
Function "main" not defined.
Make breakpoint pending on future shared library load? (y or [n]) n

# Load symbols from debug binary
(gdb) symbol-file ./myapp_debug
Reading symbols from ./myapp_debug...

# Verify symbols loaded
(gdb) info functions
All defined functions:

File main.c:
6:      int main(int, char **);

File utils.c:
7:      int process_data(int);
15:     void cleanup(void);

# Now set breakpoints - works!
(gdb) break main
Breakpoint 1 at 0x1169: file main.c, line 7.

(gdb) break process_data
Breakpoint 2 at 0x11a0: file utils.c, line 8.

# Run the program
(gdb) run
Starting program: /path/to/myapp_prod
Breakpoint 1, main (argc=1, argv=0x7fffffffe5a8) at main.c:7
7           printf("Starting program...\n");

# View source code
(gdb) list
2       #include "utils.h"
3
4       int global_counter = 0;
5
6       int main(int argc, char *argv[]) {
7           printf("Starting program...\n");
8
9           int result = process_data(42);
10          printf("Result: %d\n", result);
11

# Step to next line
(gdb) next
Starting program...
9           int result = process_data(42);

# Step into function
(gdb) step
Breakpoint 2, process_data (input=42) at utils.c:8
8           int local_var = input * 2;

# View local variables
(gdb) info locals
local_var = 0

(gdb) next
9           global_counter++;

(gdb) info locals
local_var = 84

# View global variable
(gdb) print global_counter
$1 = 0

(gdb) next
10          printf("Processing: input=%d, local=%d\n", input, local_var);

(gdb) print global_counter
$2 = 1

# View call stack
(gdb) backtrace
#0  process_data (input=42) at utils.c:10
#1  0x0000555555555186 in main (argc=1, argv=0x7fffffffe5a8) at main.c:9

# Continue to end
(gdb) continue
Continuing.
Processing: input=42, local=84
Result: 85
Cleanup: counter was 1
[Inferior 1 (process 12345) exited normally]

(gdb) quit
```

**中文说明：**
GDB 调试会话步骤：
1. 用精简二进制启动 GDB，会显示"没有找到调试符号"
2. 尝试设置断点会失败，因为没有符号信息
3. 使用 `symbol-file` 命令加载调试版本的符号
4. 使用 `info functions` 验证符号已加载
5. 现在可以：
   - 使用函数名设置断点
   - 查看源代码（`list`）
   - 单步执行（`next`, `step`）
   - 查看局部变量（`info locals`）
   - 查看全局变量（`print`）
   - 查看调用栈（`backtrace`）

### Step 4: Attaching to Running Process

```
+------------------------------------------------------------------------+
|                    Attaching to Running Process                         |
+------------------------------------------------------------------------+

# Terminal 1: Run the stripped binary (add sleep for demo)
$ ./myapp_prod
Starting program...
(waiting...)

# Terminal 2: Find the process ID
$ pgrep myapp_prod
12345

# Terminal 2: Attach GDB to running process
$ sudo gdb -p 12345

GNU gdb (Ubuntu 12.1-0ubuntu1) 12.1
Attaching to process 12345
Reading symbols from /path/to/myapp_prod...
(No debugging symbols found in /path/to/myapp_prod)
Reading symbols from /lib/x86_64-linux-gnu/libc.so.6...
(No debugging symbols found in /lib/x86_64-linux-gnu/libc.so.6)
0x00007ffff7e9e992 in ?? ()

# No symbols - can't see where we are
(gdb) backtrace
#0  0x00007ffff7e9e992 in ?? ()
#1  0x0000555555555186 in ?? ()
#2  0x00007ffff7df9083 in ?? ()

# Load symbols
(gdb) symbol-file ./myapp_debug
Reading symbols from ./myapp_debug...

# Now backtrace shows function names!
(gdb) backtrace
#0  0x00007ffff7e9e992 in __GI___clock_nanosleep ()
#1  0x0000555555555186 in main (argc=1, argv=0x7fffffffe5a8) at main.c:9
#2  0x00007ffff7df9083 in __libc_start_main ()

# Set breakpoint and continue
(gdb) break cleanup
Breakpoint 1 at 0x5555555551c0: file utils.c, line 16.

(gdb) continue
Continuing.
Processing: input=42, local=84
Result: 85

Breakpoint 1, cleanup () at utils.c:16
16          printf("Cleanup: counter was %d\n", global_counter);

(gdb) print global_counter
$1 = 1

(gdb) detach
Detaching from program: /path/to/myapp_prod, process 12345
```

**中文说明：**
附加到正在运行的进程：
1. 在一个终端运行精简二进制
2. 在另一个终端用 `gdb -p <PID>` 附加到进程
3. 初始时没有符号，`backtrace` 只显示地址
4. 加载符号文件后，可以看到函数名和源代码位置
5. 可以设置断点、查看变量
6. 使用 `detach` 分离，让程序继续运行

### Step 5: Symbol Auto-Discovery

```
+------------------------------------------------------------------------+
|                       Symbol Auto-Discovery                             |
+------------------------------------------------------------------------+

# GDB can automatically find debug symbols in standard locations:
#
# 1. Same directory as binary
# 2. .debug subdirectory  
# 3. /usr/lib/debug/ (system debug packages)
# 4. Debug link embedded in binary

# Setup for auto-discovery:

# Create .debug directory
$ mkdir -p .debug

# Move debug file there
$ mv myapp.debug .debug/

# Or use system debug directory
$ sudo mkdir -p /usr/lib/debug$(pwd)
$ sudo cp myapp.debug /usr/lib/debug$(pwd)/

# Now GDB finds symbols automatically!
$ gdb ./myapp
GNU gdb (Ubuntu 12.1-0ubuntu1) 12.1
Reading symbols from ./myapp...
Reading symbols from /path/to/.debug/myapp.debug...   <-- Auto-found!
(gdb)

# No need for manual symbol-file command

# Configure custom debug directories:
(gdb) set debug-file-directory /path/to/symbols:/another/path
(gdb) show debug-file-directory
The directory where separate debug symbols are searched for is "/path/to/symbols:/another/path".

# For shared libraries, use:
(gdb) set solib-search-path /path/to/lib/symbols

# Environment variable alternative:
$ export DEBUGINFOD_URLS="https://debuginfod.example.com"
$ gdb ./myapp
```

**中文说明：**
符号自动发现机制：
GDB 会在以下位置自动查找调试符号：
1. 与二进制同目录
2. `.debug` 子目录
3. `/usr/lib/debug/` 系统调试包目录
4. 二进制中嵌入的 debug link

配置方法：
- 创建 `.debug` 目录并放入符号文件
- 使用 `set debug-file-directory` 配置自定义目录
- 使用 `DEBUGINFOD_URLS` 环境变量配置远程符号服务器

---

## Summary

```
+------------------------------------------------------------------------+
|                              Summary                                    |
+------------------------------------------------------------------------+
|                                                                         |
|  KEY POINTS:                                                            |
|  ===========                                                            |
|                                                                         |
|  1. WHY IT WORKS:                                                       |
|     - Code addresses are identical in stripped and debug binaries       |
|     - Only metadata (symbols, debug info) differs                       |
|     - GDB simply maps symbols from one file to addresses in another     |
|                                                                         |
|  2. WHAT'S NEEDED:                                                      |
|     - Same source code version                                          |
|     - Same compiler and flags (especially optimization level)           |
|     - Same architecture                                                 |
|     - Matching Build ID (for verification)                              |
|                                                                         |
|  3. HOW TO USE:                                                         |
|     - symbol-file <debug_binary>    : Load symbols from file            |
|     - add-symbol-file <file> <addr> : Load at specific address          |
|     - set debug-file-directory      : Configure auto-discovery          |
|                                                                         |
|  4. BEST PRACTICES:                                                     |
|     - Always build with -g, strip for production                        |
|     - Keep debug binaries alongside releases                            |
|     - Use objcopy to create separate .debug files                       |
|     - Verify Build IDs match before debugging                           |
|                                                                         |
+------------------------------------------------------------------------+
```

**中文总结：**

1. **工作原理**：精简版和调试版的代码地址完全相同，只有元数据不同。GDB 将符号文件中的地址映射到运行进程的内存。

2. **必要条件**：相同的源代码版本、编译器版本、优化级别和目标架构。

3. **使用方法**：
   - `symbol-file` 加载符号
   - `add-symbol-file` 指定加载地址
   - 配置自动发现路径

4. **最佳实践**：
   - 始终用 `-g` 编译，部署时 strip
   - 保存调试版本与发布版本对应
   - 使用 `objcopy` 创建独立 `.debug` 文件
   - 调试前验证 Build ID 匹配

