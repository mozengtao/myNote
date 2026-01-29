# Linux Kernel Module Mechanism Flashcards (v3.2)

## Section 1: Module Fundamentals

Q: What is a Linux kernel module?
A: A kernel module is a piece of code that can be loaded into and unloaded from the kernel at runtime, extending kernel functionality without requiring a reboot or recompilation of the entire kernel.
[Basic]

Q: What file extension do compiled Linux kernel modules use?
A: `.ko` (kernel object)
[Basic]

Q: What type of ELF file is a kernel module (.ko file)?
A: An ELF relocatable object file (ET_REL type), not an executable or shared library.
[Basic]

Q: Why would you use a kernel module instead of compiling code directly into the kernel (built-in)?
A: 1) Reduces kernel memory footprint (modules loaded only when needed), 2) Allows runtime loading/unloading without reboot, 3) Enables third-party driver distribution, 4) Simplifies development and testing cycles.
[Basic]

Q: What is the trade-off of using modules vs built-in kernel code?
A: Modules: More flexible but have slight loading overhead and can't be used for code needed at early boot. Built-in: Always available but increases kernel size and requires reboot to change.
[Intermediate]

Q: Which command loads a kernel module directly without resolving dependencies?
A: `insmod <module.ko>`
[Basic]

Q: Which command loads a kernel module AND automatically resolves dependencies?
A: `modprobe <module_name>`
[Basic]

Q: Which command removes a loaded kernel module?
A: `rmmod <module_name>`
[Basic]

Q: Which command lists all currently loaded kernel modules?
A: `lsmod`
[Basic]

Q: Which command displays information about a kernel module file?
A: `modinfo <module.ko>` or `modinfo <module_name>`
[Basic]

Q: What does the output of `lsmod` show?
A: Three columns: Module name, Size (in bytes), and Used by (reference count and list of dependent modules).
[Basic]

Q: [Reverse] A tool that reads module information from /proc/modules and formats it as a table showing module name, size, and dependencies.
A: `lsmod`
[Basic]

Q: Where does `modprobe` look for module files by default?
A: `/lib/modules/$(uname -r)/` - organized by kernel version.
[Basic]

Q: What file does `modprobe` use to find module dependencies?
A: `modules.dep` (and `modules.dep.bin` for binary format), located in `/lib/modules/$(uname -r)/`.
[Intermediate]

Q: Which command regenerates the module dependency file?
A: `depmod` (usually run as `depmod -a`)
[Intermediate]

Q: What is the difference between `insmod` and `modprobe` when a module has dependencies?
A: `insmod` fails if dependencies are not already loaded. `modprobe` automatically loads all required dependencies first by consulting modules.dep.
[Intermediate]

Q: What happens if you try to `rmmod` a module that is in use by another module?
A: The command fails with "ERROR: Module X is in use by: Y" - you must first unload the dependent modules.
[Basic]

Q: What does `modprobe -r <module>` do differently than `rmmod`?
A: `modprobe -r` removes the module AND its unused dependencies, while `rmmod` only removes the specified module.
[Intermediate]

## Section 2: Module Structure and Macros

Q: What are the two essential functions every kernel module must have?
A: An initialization function (called when module loads) and an optional cleanup/exit function (called when module unloads).
[Basic]

Q: What macro registers the module initialization function?
A: `module_init(init_function_name)`
[Basic]

Q: What macro registers the module cleanup/exit function?
A: `module_exit(exit_function_name)`
[Basic]

Q: What is the required return type and value for a module init function on success?
A: Return type is `int`. Returns `0` on success, negative error code (e.g., `-ENOMEM`, `-EINVAL`) on failure.
[Basic]

Q: What is the return type of a module exit function?
A: `void` - exit functions cannot fail and return nothing.
[Basic]

Q: What does the `__init` macro/attribute do when applied to a function?
A: Marks the function to be placed in the `.init.text` section, which is freed after module initialization completes, saving memory.
[Intermediate]

Q: What does the `__exit` macro/attribute do when applied to a function?
A: Marks the function to be placed in the `.exit.text` section. For built-in code (not modules), this section is discarded entirely since built-ins cannot be unloaded.
[Intermediate]

Q: What does the `__initdata` macro do?
A: Marks data to be placed in the `.init.data` section, which is freed after initialization, saving memory for data only needed during init.
[Intermediate]

Q: What is the purpose of `MODULE_LICENSE()` macro?
A: Declares the module's license (e.g., "GPL", "GPL v2", "Proprietary"). Required for accessing GPL-only exported symbols and affects kernel taint status.
[Basic]

Q: What happens if a module doesn't declare a license with `MODULE_LICENSE()`?
A: The kernel assumes proprietary license, taints the kernel, and the module cannot use `EXPORT_SYMBOL_GPL` symbols.
[Intermediate]

Q: List valid license strings for `MODULE_LICENSE()` that are GPL-compatible.
A: "GPL", "GPL v2", "GPL and additional rights", "Dual BSD/GPL", "Dual MIT/GPL", "Dual MPL/GPL"
[Intermediate]

Q: What does `MODULE_AUTHOR()` macro do?
A: Stores the module author's name/email in the `.modinfo` section. Displayed by `modinfo` command.
[Basic]

Q: What does `MODULE_DESCRIPTION()` macro do?
A: Stores a brief description of the module in the `.modinfo` section.
[Basic]

Q: What does `MODULE_VERSION()` macro do?
A: Stores version information in the `.modinfo` section. Format is typically "major.minor" or similar.
[Basic]

Q: What ELF section contains module metadata like license, author, description?
A: The `.modinfo` section.
[Intermediate]

Q: Write the minimal skeleton for a kernel module with init and exit functions.
A:
```c
#include <linux/module.h>
#include <linux/init.h>

static int __init mymod_init(void)
{
    pr_info("Module loaded\n");
    return 0;
}

static void __exit mymod_exit(void)
{
    pr_info("Module unloaded\n");
}

module_init(mymod_init);
module_exit(mymod_exit);
MODULE_LICENSE("GPL");
```
[Basic]

Q: What header file must be included for basic module macros?
A: `<linux/module.h>`
[Basic]

Q: What header file provides `__init` and `__exit` macros?
A: `<linux/init.h>`
[Basic]

Q: What does `MODULE_ALIAS()` macro do?
A: Creates an alias name for the module, allowing it to be loaded by alternative names. Stored in `.modinfo` section.
[Intermediate]

Q: What is `MODULE_INFO(tag, info)` macro used for?
A: Generic macro to add arbitrary tag=info pairs to the `.modinfo` section. Other macros like MODULE_AUTHOR expand to this.
[Intermediate]

Q: [Cloze] The `__init` attribute places the function in the _____ section which is freed after _____.
A: `.init.text` section; module initialization completes
[Intermediate]

Q: Why should you NOT use `__init` data from a non-`__init` function?
A: Because `__init` data is freed after initialization. Accessing it later causes undefined behavior or crashes.
[Advanced]

## Section 3: struct module Deep Dive

Q: What is `struct module` in the Linux kernel?
A: The core data structure representing a loaded kernel module. Contains all metadata, symbol tables, memory regions, state, and reference counts for the module.
[Intermediate]

Q: Where is `struct module` defined in kernel v3.2?
A: In `include/linux/module.h` (approximately lines 208-361).
[Intermediate]

Q: What field in `struct module` holds the module's name?
A: `char name[MODULE_NAME_LEN]` - a fixed-size array holding the module name string.
[Intermediate]

Q: What is `MODULE_NAME_LEN` typically defined as?
A: 56 characters (defined as `(64 - sizeof(unsigned long))` to fit cache line alignment).
[Advanced]

Q: What field tracks the current state of a module (loading, live, unloading)?
A: `enum module_state state`
[Intermediate]

Q: What field links this module into the global list of all loaded modules?
A: `struct list_head list`
[Intermediate]

Q: What field holds the pointer to the module's initialization function?
A: `int (*init)(void)` - function pointer to the init function.
[Intermediate]

Q: What field holds the pointer to the module's cleanup function?
A: `void (*exit)(void)` - function pointer to the exit function.
[Intermediate]

Q: What is the difference between `module_core` and `module_init` fields in `struct module`?
A: `module_core`: Points to persistent module memory (code + data that remains loaded). `module_init`: Points to initialization memory that is freed after init completes.
[Intermediate]

Q: What fields store the sizes of core and init memory regions?
A: `core_size` and `init_size` for total sizes; `core_text_size` and `init_text_size` for code sections.
[Intermediate]

Q: What fields in `struct module` store exported symbol information?
A: `syms` (pointer to kernel_symbol array), `num_syms` (count), and `crcs` (CRC checksums for versioning).
[Intermediate]

Q: What fields store GPL-only exported symbols?
A: `gpl_syms`, `num_gpl_syms`, and `gpl_crcs` - same structure as regular exports but for GPL-restricted symbols.
[Intermediate]

Q: What field stores module parameters?
A: `struct kernel_param *kp` (pointer to array) and `unsigned int num_kp` (count).
[Intermediate]

Q: What is `struct module_kobject mkobj` used for?
A: Provides sysfs integration - creates `/sys/module/<name>/` directory structure for the module.
[Intermediate]

Q: What fields track module dependencies?
A: `struct list_head source_list` (modules that depend on this one) and `struct list_head target_list` (modules this one depends on).
[Intermediate]

Q: How is reference counting implemented in `struct module`?
A: Using `struct module_ref __percpu *refptr` - per-CPU counters to avoid cache line bouncing on SMP systems.
[Advanced]

Q: What is `struct module_ref` and why is it per-CPU?
A: Contains `incs` and `decs` counters. Per-CPU to eliminate lock contention and cache bouncing when multiple CPUs access module references simultaneously.
[Advanced]

Q: What field stores the module's exception table?
A: `struct exception_table_entry *extable` and `unsigned int num_exentries`.
[Advanced]

Q: What is the exception table used for in a module?
A: Handles page faults in kernel code - maps faulting addresses to fixup code for handling expected exceptions (e.g., user memory access failures).
[Advanced]

Q: What field stores the module's version string?
A: `const char *version` - points to version info from `.modinfo` section.
[Intermediate]

Q: What field stores the source version/checksum?
A: `const char *srcversion` - MD4 hash of source files, used to track module source changes.
[Advanced]

Q: [Cloze] The `struct module` uses _____ for reference counting to avoid _____ on SMP systems.
A: per-CPU counters (`struct module_ref __percpu *refptr`); lock contention and cache line bouncing
[Advanced]

Q: What does `core_ro_size` field represent?
A: Size of the read-only portion of core module memory (set read-only after loading for security).
[Advanced]

Q: What is the `.gnu.linkonce.this_module` section?
A: Special ELF section containing the `struct module` instance for this module. Used by the loader to find module metadata.
[Advanced]

Q: How does the kernel find the `struct module` when loading a .ko file?
A: Searches for the `.gnu.linkonce.this_module` section in the ELF file, which contains the pre-initialized `struct module`.
[Advanced]

## Section 4: Module Loading Process

Q: What system call is used to load a kernel module?
A: `sys_init_module()` - takes userspace pointer to module image, length, and parameter arguments string.
[Basic]

Q: What is the signature of sys_init_module()?
A: `SYSCALL_DEFINE3(init_module, void __user *, umod, unsigned long, len, const char __user *, uargs)`
[Intermediate]

Q: What capability is required to load a kernel module?
A: `CAP_SYS_MODULE` - checked at the start of sys_init_module().
[Intermediate]

Q: What is the main function called by sys_init_module() to perform the actual loading?
A: `load_module()` - handles all ELF parsing, memory allocation, symbol resolution, and relocation.
[Intermediate]

Q: Describe the high-level flow of module loading.
A:
```
sys_init_module()
  └─> load_module()
        ├─> copy_and_check()     [copy ELF, validate]
        ├─> layout_and_allocate() [allocate memory]
        ├─> simplify_symbols()   [resolve symbols]
        ├─> apply_relocations()  [apply relocs]
        └─> post_relocation()    [finalize]
  └─> do_one_initcall(mod->init)
  └─> set state = MODULE_STATE_LIVE
```
[Intermediate]

Q: What does `copy_and_check()` do during module loading?
A: Copies ELF module image from userspace to kernel memory, validates ELF header (magic, type=ET_REL, architecture), checks section header table integrity.
[Intermediate]

Q: What ELF type must a kernel module be?
A: `ET_REL` (relocatable object file) - NOT ET_EXEC or ET_DYN.
[Intermediate]

Q: What function validates that the module is for the correct architecture?
A: `elf_check_arch()` - architecture-specific macro checking ELF machine type.
[Intermediate]

Q: What is `struct load_info` used for?
A: Temporary structure holding ELF parsing state during loading: ELF header pointer, section headers, symbol tables, string tables. Freed after loading completes.
[Intermediate]

Q: What does `layout_and_allocate()` do?
A: Calls setup_load_info(), check_modinfo(), layout_sections(), and move_module() to prepare memory layout and allocate final module memory regions.
[Intermediate]

Q: What does `setup_load_info()` populate?
A: Fills `struct load_info` with pointers to ELF sections: section headers, symbol table (.symtab), string tables (.strtab), section name strings.
[Intermediate]

Q: What does `check_modinfo()` verify?
A: Validates vermagic string against kernel version, checks license, detects staging/out-of-tree modules, applies any intree/staging taints.
[Intermediate]

Q: What is vermagic and why is it checked?
A: Version magic string containing kernel version, SMP config, preemption model. Ensures module was compiled for this exact kernel configuration.
[Intermediate]

Q: What does `layout_sections()` do?
A: Calculates final memory layout by grouping sections into core and init regions, ordered by type (executable, read-only, read-write, small data).
[Advanced]

Q: Why are module sections grouped by type in layout_sections()?
A: To enable memory protection: executable sections together (for NX on data), read-only sections together (for write protection), and to optimize memory allocation.
[Advanced]

Q: What does `move_module()` do?
A: Allocates final memory regions (module_core, module_init) using module_alloc(), then copies all sections from temporary buffer to their final locations.
[Intermediate]

Q: What function allocates memory for module code/data?
A: `module_alloc()` - architecture-specific function that allocates memory suitable for executable code (may need special address ranges or permissions).
[Advanced]

Q: What happens after symbol resolution and relocation in load_module()?
A: Module is added to the global module list, state set to MODULE_STATE_COMING, sysfs entries created, module parameters parsed.
[Intermediate]

Q: When is the module's init function actually called?
A: In sys_init_module() AFTER load_module() returns successfully, via `do_one_initcall(mod->init)`.
[Intermediate]

Q: What happens if mod->init() returns a non-zero (error) value?
A: Module loading fails, module is removed from the module list, memory is freed, and sys_init_module() returns the error code.
[Intermediate]

Q: What memory optimizations happen after successful module initialization?
A: The init section (module_init memory region) is freed since initialization code is no longer needed. Only core section remains.
[Intermediate]

Q: What does `post_relocation()` do?
A: Final setup after relocations: sorts exception table, copies per-CPU data, sets up kallsyms entries, calls architecture-specific finalization.
[Advanced]

Q: What is the purpose of sorting the exception table?
A: Enables binary search when handling exceptions - the kernel needs to quickly find fixup code for faulting addresses.
[Advanced]

Q: [ASCII Diagram] Draw the module loading flow from syscall to init.
A:
```
User Space (insmod/modprobe)
         |
         v
  sys_init_module()
         |
         +---> capability_check(CAP_SYS_MODULE)
         |
         +---> load_module()
         |       |
         |       +---> copy_and_check()
         |       +---> layout_and_allocate()
         |       +---> simplify_symbols()
         |       +---> apply_relocations()
         |       +---> post_relocation()
         |       +---> add to module list
         |       +---> state = COMING
         |
         +---> set_memory_ro/nx()
         +---> do_mod_ctors()
         +---> do_one_initcall(mod->init)
         +---> state = LIVE
         +---> free init section
```
[Intermediate]

Q: What are module constructors (do_mod_ctors) and when are they called?
A: Functions in the `.ctors` section (C++ style constructors). Called after memory protection is set but before mod->init(), used for static object initialization.
[Advanced]

Q: What memory protection is applied before calling mod->init()?
A: Read-only (RO) protection on code and const data sections, No-Execute (NX) on data sections via set_memory_ro() and set_memory_nx().
[Advanced]

Q: [Cloze] The module loading function `load_module()` returns a pointer to _____, or an error pointer on failure.
A: `struct module` (the newly loaded module's structure)
[Intermediate]

## Section 5: Symbol Resolution and Relocations

Q: What is symbol resolution in the context of module loading?
A: The process of finding addresses for undefined symbols in the module by searching kernel symbol tables and other loaded modules.
[Basic]

Q: What function performs symbol resolution during module loading?
A: `simplify_symbols()` - iterates through the module's symbol table and resolves undefined symbols.
[Intermediate]

Q: What ELF symbol table index indicates an undefined symbol that needs resolution?
A: `SHN_UNDEF` (section header index = 0) - symbol is referenced but not defined in this module.
[Intermediate]

Q: What does `SHN_ABS` mean for a symbol?
A: Absolute symbol - the value is a fixed constant, not relative to any section. No relocation needed.
[Intermediate]

Q: What happens to symbols with `SHN_COMMON` in Linux kernel modules?
A: They cause an error - common symbols (uninitialized globals) are not allowed in kernel modules.
[Advanced]

Q: What function actually looks up a symbol by name during resolution?
A: `resolve_symbol()` which calls `find_symbol()` to search kernel and module symbol tables.
[Intermediate]

Q: In what order does `find_symbol()` search for symbols?
A: 1) Kernel regular symbols (__ksymtab), 2) Kernel GPL symbols (__ksymtab_gpl), 3) Kernel GPL-future symbols, 4) Each loaded module's symbols in same order.
[Intermediate]

Q: How are kernel symbol tables searched efficiently?
A: Binary search (bsearch) - symbol tables are sorted alphabetically by name at build/load time.
[Intermediate]

Q: What is the return value of `find_symbol()` on success?
A: Returns a pointer to `struct kernel_symbol` containing the symbol name and value (address).
[Intermediate]

Q: What additional check does `resolve_symbol()` perform after finding a symbol?
A: `check_version()` - validates CRC checksums if CONFIG_MODVERSIONS is enabled, ensuring ABI compatibility.
[Intermediate]

Q: What happens if symbol resolution fails for any undefined symbol?
A: Module loading fails with "Unknown symbol" error message and -ENOENT return.
[Intermediate]

Q: What is ELF relocation?
A: Process of adjusting addresses in code/data to reflect actual loaded addresses. Necessary because modules are compiled as relocatable objects.
[Basic]

Q: What function applies relocations during module loading?
A: `apply_relocations()` - iterates through relocation sections and applies each relocation.
[Intermediate]

Q: What are the two types of ELF relocation sections?
A: `SHT_REL` (relocation without addend - addend stored in place) and `SHT_RELA` (relocation with explicit addend field).
[Intermediate]

Q: Which architecture uses SHT_REL relocations?
A: x86 (32-bit) primarily uses SHT_REL. The addend is stored at the relocation target location.
[Advanced]

Q: Which architectures typically use SHT_RELA relocations?
A: x86_64, ARM64, and most modern architectures use SHT_RELA with explicit addends.
[Advanced]

Q: What functions handle architecture-specific relocations?
A: `apply_relocate()` for SHT_REL and `apply_relocate_add()` for SHT_RELA - defined in `arch/*/kernel/module.c`.
[Advanced]

Q: What information does a relocation entry contain?
A: Offset (where to apply), symbol index (what symbol), and type (how to calculate the value). RELA also includes addend.
[Intermediate]

Q: What is a PC-relative relocation?
A: A relocation where the final value is calculated relative to the instruction pointer/program counter. Used for relative jumps and calls.
[Intermediate]

Q: What is an absolute relocation?
A: A relocation where the final value is the absolute address of the symbol. Used for direct address references.
[Intermediate]

Q: [Cloze] Symbol resolution searches _____ symbol tables first, then _____ symbol tables.
A: kernel; loaded module
[Intermediate]

Q: What does `resolve_symbol_wait()` do that `resolve_symbol()` doesn't?
A: Waits for a module in MODULE_STATE_COMING state to finish loading before resolving, handling race conditions during parallel loading.
[Advanced]

Q: Why might a symbol be found but resolution still fail?
A: 1) GPL-only symbol accessed by non-GPL module, 2) CRC version mismatch (CONFIG_MODVERSIONS), 3) Symbol owner module is unloading.
[Intermediate]

Q: What is the relationship between symbol resolution and module dependencies?
A: When a symbol is resolved from another module, a dependency is created via `ref_module()`, incrementing that module's reference count.
[Intermediate]

Q: What error message indicates a GPL symbol access violation?
A: "FATAL: modpost: GPL-incompatible module X uses GPL-only symbol Y" at build time, or "X: module license 'proprietary' taints kernel" at load time.
[Intermediate]

Q: [ASCII Diagram] Show the symbol resolution flow.
A:
```
simplify_symbols()
       |
       v
  For each symbol in .symtab:
       |
       +--[SHN_UNDEF?]---> resolve_symbol_wait()
       |                          |
       |                          v
       |                    find_symbol()
       |                          |
       |      +-------------------+-------------------+
       |      v                   v                   v
       |   kernel             kernel GPL         module
       |   __ksymtab         __ksymtab_gpl       exports
       |      |                   |                   |
       |      +-------------------+-------------------+
       |                          |
       |                          v
       |                   check_version() [if MODVERSIONS]
       |                          |
       |                          v
       |                   ref_module() [create dependency]
       |
       +--[SHN_ABS?]----> no change needed
       |
       +--[regular?]----> add section base address
```
[Advanced]

## Section 6: EXPORT_SYMBOL Mechanism

Q: What does `EXPORT_SYMBOL(sym)` do?
A: Makes a kernel symbol (function or variable) available for use by loadable modules. Creates an entry in the kernel symbol table (__ksymtab section).
[Basic]

Q: What is the difference between `EXPORT_SYMBOL()` and `EXPORT_SYMBOL_GPL()`?
A: EXPORT_SYMBOL: Available to all modules. EXPORT_SYMBOL_GPL: Only available to modules with GPL-compatible licenses.
[Basic]

Q: Where is EXPORT_SYMBOL defined?
A: In `include/linux/export.h`
[Intermediate]

Q: What structure represents an exported symbol?
A: `struct kernel_symbol` containing `value` (symbol address) and `name` (symbol name string pointer).
[Intermediate]

Q: What ELF sections are created by EXPORT_SYMBOL macros?
A: `__ksymtab` (or `__ksymtab_gpl`) for kernel_symbol entries, `__ksymtab_strings` for symbol names, `__kcrctab` (or `__kcrctab_gpl`) for CRC checksums.
[Intermediate]

Q: Why does EXPORT_SYMBOL create a CRC entry?
A: For CONFIG_MODVERSIONS support - CRC checksums verify that the symbol's type signature matches between kernel and module compilation.
[Intermediate]

Q: What does the `___ksymtab` prefix (three underscores) indicate vs `__ksymtab` (two)?
A: `___ksymtab` is the initial section name; the linker script sorts and combines them into final `__ksymtab` sections for efficient binary search.
[Advanced]

Q: How are kernel symbol tables sorted?
A: Alphabetically by symbol name at link time (kernel) or load time (modules), enabling binary search during symbol resolution.
[Intermediate]

Q: What is `EXPORT_SYMBOL_GPL_FUTURE()`?
A: Exports a symbol that is currently available to all modules but will become GPL-only in the future. Prints a warning when used by non-GPL modules.
[Advanced]

Q: What is `EXPORT_UNUSED_SYMBOL()`?
A: Marks a symbol as exported but unused - typically for symbols that were exported but no known modules use them. May be removed.
[Advanced]

Q: How does the kernel enforce GPL-only symbol access?
A: During symbol resolution, `check_symbol()` compares the requesting module's license against GPL-compatibility and rejects non-GPL access to GPL-only symbols.
[Intermediate]

Q: What function checks if a module license is GPL-compatible?
A: `license_is_gpl_compatible()` - checks against list of GPL-compatible license strings.
[Intermediate]

Q: Why export symbols rather than making everything public?
A: Controlled API surface: 1) Defines stable kernel ABI for modules, 2) GPL enforcement, 3) Prevents modules from depending on internal implementation details.
[Intermediate]

Q: [Code] What does this macro expand to?
```c
EXPORT_SYMBOL(my_function);
```
A: Creates: 1) Entry in __ksymtab: {&my_function, "my_function"}, 2) "my_function" string in __ksymtab_strings, 3) CRC in __kcrctab (if MODVERSIONS).
[Intermediate]

Q: How does a module export its own symbols for other modules?
A: Using EXPORT_SYMBOL() in the module code. Module's symbol tables are searched during symbol resolution for subsequently loaded modules.
[Intermediate]

Q: What field in struct module stores the module's exported symbols?
A: `const struct kernel_symbol *syms` and `unsigned int num_syms` for regular exports, plus gpl_syms/num_gpl_syms for GPL-only.
[Intermediate]

Q: What does `verify_export_symbols()` check during module loading?
A: Ensures the module's exported symbols don't conflict with kernel or already-loaded module symbols (no duplicate exports).
[Intermediate]

Q: What error indicates a duplicate exported symbol?
A: "exports duplicate symbol" - module tries to export a symbol already exported by kernel or another module.
[Intermediate]

Q: Can a module use a symbol exported by another module?
A: Yes, if that module is loaded first. The loader searches loaded modules' symbol tables after kernel tables.
[Intermediate]

Q: [Cloze] EXPORT_SYMBOL creates entries in _____ section for the symbol and _____ section for the CRC.
A: `__ksymtab` (or `__ksymtab_gpl`); `__kcrctab` (or `__kcrctab_gpl`)
[Intermediate]

Q: What happens if you try to EXPORT_SYMBOL a static function?
A: Compilation succeeds, but the symbol may be optimized away or have limited visibility. Best practice: export non-static functions.
[Intermediate]

Q: [Reverse] This macro makes kernel functions available to modules with any license, stored in __ksymtab section.
A: `EXPORT_SYMBOL()`
[Basic]

## Section 7: Module Parameters

Q: What are module parameters?
A: Runtime-configurable values that can be set when loading a module, allowing customization of module behavior without recompilation.
[Basic]

Q: What macro declares a module parameter?
A: `module_param(name, type, perm)` - name is the variable, type is data type, perm is sysfs permissions.
[Basic]

Q: What header file provides module parameter macros?
A: `<linux/moduleparam.h>`
[Basic]

Q: List the standard types available for module_param().
A: byte, short, ushort, int, uint, long, ulong, bool, invbool, charp (char pointer), string
[Intermediate]

Q: How do you pass parameters when loading a module with insmod?
A: `insmod module.ko param1=value1 param2=value2`
[Basic]

Q: How do you pass parameters with modprobe?
A: `modprobe module_name param1=value1` or via config file in `/etc/modprobe.d/`
[Basic]

Q: What does the permission (perm) argument in module_param() control?
A: Visibility and access in sysfs: 0 = no sysfs entry, 0444 = read-only, 0644 = read-write.
[Intermediate]

Q: Where do module parameters appear in sysfs?
A: `/sys/module/<module_name>/parameters/<param_name>`
[Basic]

Q: What is `struct kernel_param`?
A: Structure representing a module parameter: contains name, ops (callbacks), permissions, flags, and pointer to the actual variable.
[Intermediate]

Q: What is `struct kernel_param_ops`?
A: Contains operation callbacks for a parameter type: `set()` to parse/write value, `get()` to format/read value, optional `free()` to cleanup.
[Intermediate]

Q: What function parses module parameters during loading?
A: `parse_args()` in kernel/params.c - iterates through parameter string and calls appropriate set() callbacks.
[Intermediate]

Q: What does `module_param_named(name, var, type, perm)` do differently than module_param()?
A: Allows the parameter name (seen by user) to differ from the C variable name.
[Intermediate]

Q: How do you declare an array parameter?
A: `module_param_array(name, type, nump, perm)` where nump is pointer to int receiving actual count of values provided.
[Intermediate]

Q: What is the maximum length of a string parameter by default?
A: 1024 characters (can be customized with module_param_string).
[Intermediate]

Q: What does `module_param_cb()` do?
A: Declares a parameter with custom callback operations (custom struct kernel_param_ops).
[Advanced]

Q: [Code] What does this create?
```c
static int debug_level = 0;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug verbosity level");
```
A: An integer parameter "debug_level" defaulting to 0, readable/writable via sysfs at /sys/module/<name>/parameters/debug_level, with description visible in modinfo.
[Intermediate]

Q: What does `MODULE_PARM_DESC()` do?
A: Adds a description string for the parameter, stored in .modinfo section and displayed by `modinfo` command.
[Basic]

Q: What ELF section contains module parameter information?
A: `__param` section - array of struct kernel_param entries.
[Intermediate]

Q: How does the kernel find all parameters for a module?
A: The __param section boundaries are stored in struct module as `kp` (pointer) and `num_kp` (count).
[Intermediate]

Q: What is `invbool` parameter type?
A: Inverted boolean - setting the parameter to 1/true sets the variable to 0/false and vice versa.
[Intermediate]

Q: What is `charp` parameter type?
A: Character pointer (char *) - stores a pointer to the parameter string. Caution: points to module's memory, not a copy.
[Intermediate]

Q: [Cloze] Module parameters are stored in the _____ ELF section and appear in sysfs under _____.
A: `__param`; `/sys/module/<name>/parameters/`
[Intermediate]

Q: Can module parameters be changed at runtime after loading?
A: Yes, if the sysfs permission includes write access (e.g., 0644). Writing to /sys/module/<name>/parameters/<param> calls the set() callback.
[Intermediate]

Q: What happens if a required parameter is not provided during module load?
A: The variable retains its default value (initialized in the C code). There's no built-in "required" parameter concept.
[Intermediate]

## Section 8: Module States and Lifecycle

Q: What are the three module states defined in `enum module_state`?
A: `MODULE_STATE_LIVE` (operational), `MODULE_STATE_COMING` (being loaded), `MODULE_STATE_GOING` (being unloaded).
[Basic]

Q: When is a module in MODULE_STATE_COMING?
A: During the loading process after load_module() adds it to the module list but before mod->init() completes successfully.
[Intermediate]

Q: When does a module transition to MODULE_STATE_LIVE?
A: After mod->init() returns success (0) in sys_init_module(), indicating the module is fully operational.
[Intermediate]

Q: When is a module in MODULE_STATE_GOING?
A: During unloading after sys_delete_module() marks it for removal but before memory is freed.
[Intermediate]

Q: What does the `module_is_live()` helper function check?
A: Returns true if module state is NOT MODULE_STATE_GOING (i.e., module is usable). Used to prevent new references to unloading modules.
[Intermediate]

Q: What happens if mod->init() returns a non-zero value?
A: The module transitions back from COMING, is removed from the module list, memory is freed, and loading fails.
[Intermediate]

Q: [State Diagram] Draw the module state transitions.
A:
```
                    load_module()
                         |
                         v
                  +-------------+
                  |   COMING    |<----+
                  +-------------+     |
                         |            |
          init() success |            | init() fails
                         v            |
                  +-------------+     |
                  |    LIVE     |-----+
                  +-------------+     (cleanup & free)
                         |
           delete_module |
                         v
                  +-------------+
                  |   GOING     |
                  +-------------+
                         |
                         v
                   (exit & free)
```
[Intermediate]

Q: What memory protection is applied after a module becomes LIVE?
A: Read-only (RO) protection on text and rodata sections, No-Execute (NX) on data sections to prevent code injection attacks.
[Intermediate]

Q: What functions set memory protection on module sections?
A: `set_memory_ro()` for read-only and `set_memory_nx()` for no-execute, using architecture-specific page table manipulation.
[Advanced]

Q: What happens to the module_init memory region after successful initialization?
A: It is freed via `module_free()` since init code/data is no longer needed, reducing the module's memory footprint.
[Intermediate]

Q: Why does the kernel free init sections after module initialization?
A: Memory optimization - init code and data (marked with __init/__initdata) are only needed once. Freeing them reduces kernel memory usage.
[Intermediate]

Q: Can a module be accessed while in MODULE_STATE_COMING?
A: Only partially - other modules loading concurrently use resolve_symbol_wait() which waits for COMING modules to finish or fail.
[Advanced]

Q: What prevents using a module that is in MODULE_STATE_GOING?
A: `try_module_get()` checks module_is_live() and fails for GOING modules, preventing new references.
[Intermediate]

Q: What is the sequence of events when a module unloads successfully?
A: 1) State set to GOING, 2) Wait for refcount=0, 3) Call mod->exit(), 4) Remove from lists, 5) Free memory.
[Intermediate]

Q: [Cloze] A module in _____ state has completed initialization and is fully operational.
A: MODULE_STATE_LIVE
[Basic]

Q: Why have a COMING state instead of going directly to LIVE?
A: Allows concurrent module loading to wait for dependencies, handles init failure cleanup, and enables proper notification sequencing.
[Advanced]

Q: What notifications are sent during module state transitions?
A: MODULE_STATE_COMING (before init), MODULE_STATE_LIVE (after init success), MODULE_STATE_GOING (before exit) via blocking notifier chain.
[Advanced]

Q: How can kernel code register for module state notifications?
A: Using `register_module_notifier()` with a notifier_block callback, called on state transitions.
[Advanced]

Q: What is `struct module_use` for in the context of module lifecycle?
A: Tracks dependencies between modules - ensures dependent modules can't unload before modules they depend on.
[Intermediate]

## Section 9: Reference Counting and Dependencies

Q: Why do kernel modules need reference counting?
A: To prevent unloading a module while its code is still being used (e.g., active device drivers, open file handles, callbacks registered).
[Basic]

Q: What function increments a module's reference count?
A: `try_module_get(struct module *mod)` - returns true on success, false if module is unloading.
[Basic]

Q: What function decrements a module's reference count?
A: `module_put(struct module *mod)` - decrements the reference count, allowing unloading when it reaches zero.
[Basic]

Q: Why is it called "try_module_get" instead of just "module_get"?
A: Because it can fail (returns bool) - fails if the module is in MODULE_STATE_GOING or NULL.
[Intermediate]

Q: What is `struct module_ref`?
A: Per-CPU structure containing `incs` and `decs` counters for module reference counting, avoiding cache line contention.
[Intermediate]

Q: Why are module reference counters per-CPU?
A: Performance optimization - avoids atomic operations and cache line bouncing when multiple CPUs take/release references simultaneously.
[Intermediate]

Q: How is the actual reference count calculated from per-CPU counters?
A: `module_refcount()` sums (incs - decs) across all CPUs to get the total reference count.
[Intermediate]

Q: What does `__module_get(struct module *mod)` do?
A: Unconditionally increments reference count without checking module state. Only safe when you know module won't unload (e.g., calling from module's own code).
[Advanced]

Q: When should you use __module_get() vs try_module_get()?
A: Use try_module_get() from external code (may fail). Use __module_get() only from within the module itself or when holding other references.
[Intermediate]

Q: What is `struct module_use`?
A: Structure linking two modules in a dependency relationship, with list_head entries for both source (user) and target (used) module lists.
[Intermediate]

Q: What are source_list and target_list in struct module?
A: `source_list`: Modules that USE this module's symbols. `target_list`: Modules whose symbols THIS module uses.
[Intermediate]

Q: What function creates a module dependency relationship?
A: `ref_module()` - called during symbol resolution when module A uses a symbol from module B.
[Intermediate]

Q: What does `ref_module()` do internally?
A: 1) Checks if dependency exists (already_uses()), 2) Calls strong_try_module_get() on target, 3) Creates module_use structure, 4) Links into both modules' lists.
[Advanced]

Q: What function checks if one module already depends on another?
A: `already_uses()` - walks source_list to see if dependency already exists.
[Advanced]

Q: How does dependency tracking prevent unsafe unloading?
A: A module cannot unload if its source_list is non-empty (other modules depend on it), checked in sys_delete_module().
[Intermediate]

Q: [Cloze] Module A uses a symbol from module B, so A is added to B's _____ and B is added to A's _____.
A: source_list; target_list
[Intermediate]

Q: What error do you see when trying to unload a module with dependents?
A: "ERROR: Module X is in use by: Y, Z" where Y, Z are modules that depend on X.
[Basic]

Q: Can circular module dependencies exist?
A: No - module B can only depend on already-loaded modules. Since A depends on B, B must load first, so B cannot simultaneously depend on A.
[Intermediate]

Q: What happens to dependencies when a module unloads?
A: The module_use structures are freed, references to target modules are released via module_put().
[Intermediate]

Q: What is `strong_try_module_get()`?
A: Same as try_module_get() but may be used with additional constraints. In v3.2, essentially an alias for try_module_get().
[Advanced]

Q: [ASCII Diagram] Show module dependency data structure relationships.
A:
```
Module A uses symbol from Module B:

  Module A                          Module B
  +-------------+                   +-------------+
  | target_list |---+           +---| source_list |
  +-------------+   |           |   +-------------+
                    |           |
                    v           v
              +------------------------+
              |    struct module_use   |
              |------------------------|
              | source = A             |
              | target = B             |
              | source_list -> B's list|
              | target_list -> A's list|
              +------------------------+
```
[Advanced]

Q: What kernel config option enables module reference counting?
A: `CONFIG_MODULE_UNLOAD` - without it, modules cannot be unloaded at all, and reference counting is simplified.
[Intermediate]

## Section 10: Module Unloading

Q: What system call unloads a kernel module?
A: `sys_delete_module()` - takes module name and flags as arguments.
[Basic]

Q: What is the signature of sys_delete_module()?
A: `SYSCALL_DEFINE2(delete_module, const char __user *, name_user, unsigned int, flags)`
[Intermediate]

Q: What capability is required to unload a module?
A: `CAP_SYS_MODULE` - same as loading, checked at the start of sys_delete_module().
[Intermediate]

Q: What conditions must be met before a module can be unloaded?
A: 1) Module exists and is LIVE, 2) No other modules depend on it (source_list empty), 3) Reference count is zero, 4) Module has exit function (or O_NONBLOCK flag).
[Intermediate]

Q: What happens if you try to unload a module with non-zero reference count?
A: By default, sys_delete_module() waits for reference count to reach zero. With O_NONBLOCK flag, it returns -EWOULDBLOCK immediately.
[Intermediate]

Q: What does the O_TRUNC flag do for sys_delete_module()?
A: Force unload - unloads the module even if reference count is non-zero. Dangerous and can cause kernel crashes.
[Advanced]

Q: What kernel config option is required for force unloading?
A: `CONFIG_MODULE_FORCE_UNLOAD` - must be enabled at kernel build time.
[Intermediate]

Q: What happens if a module lacks an exit function?
A: Cannot be unloaded normally - returns -EBUSY. Built-in modules or modules without module_exit() macro cannot be unloaded.
[Intermediate]

Q: What is the sequence of operations in sys_delete_module()?
A: 1) Find module by name, 2) Check dependencies/state, 3) Set state=GOING, 4) Wait for refcount=0, 5) Call mod->exit(), 6) free_module().
[Intermediate]

Q: What does `free_module()` do?
A: Removes module from lists, frees per-CPU data, releases module memory (core and init regions), cleans up sysfs entries.
[Intermediate]

Q: What notification is sent before calling mod->exit()?
A: `MODULE_STATE_GOING` notification via blocking_notifier_call_chain(), allowing other subsystems to clean up references.
[Intermediate]

Q: What should a module's exit function do?
A: Undo everything done in init: unregister devices, free allocated memory, remove /proc or sysfs entries, release resources.
[Basic]

Q: Why can't you unload a module that another module depends on?
A: The dependent module's code references symbols from this module - unloading would leave dangling pointers causing crashes.
[Basic]

Q: [Cloze] The flag _____ forces module unloading even with non-zero reference count, requiring _____ config option.
A: O_TRUNC; CONFIG_MODULE_FORCE_UNLOAD
[Intermediate]

Q: What function waits for module reference count to become zero?
A: `wait_for_zero_refcount()` - called in sys_delete_module() after setting state to GOING.
[Advanced]

Q: Why is force unloading (O_TRUNC) dangerous?
A: Code may still be executing in the module, data structures may still be referenced, callbacks may still be registered - leads to crashes.
[Intermediate]

Q: [ASCII Diagram] Show the module unloading flow.
A:
```
rmmod module_name
        |
        v
sys_delete_module()
        |
        +---> check CAP_SYS_MODULE
        +---> find_module(name)
        +---> check source_list empty
        +---> check state == LIVE
        +---> check has exit function
        |
        +---> state = MODULE_STATE_GOING
        +---> notify MODULE_STATE_GOING
        +---> wait_for_zero_refcount()
        +---> mod->exit()
        +---> free_module()
              |
              +---> remove from lists
              +---> free per-cpu data
              +---> module_free(core)
              +---> module_free(init) [if not freed]
              +---> cleanup sysfs
```
[Intermediate]

Q: What does `blocking_notifier_call_chain()` do during module unload?
A: Notifies registered listeners (e.g., debuggers, tracers) that the module is going away, allowing them to remove breakpoints/hooks.
[Advanced]

## Section 11: Verification and Security

Q: What Linux capability is required to load or unload kernel modules?
A: `CAP_SYS_MODULE` - without it, init_module() and delete_module() syscalls fail with -EPERM.
[Basic]

Q: What is the vermagic string?
A: A string embedded in modules containing kernel version, SMP configuration, and preemption model - must match the running kernel.
[Intermediate]

Q: What macro generates the kernel's vermagic string?
A: `VERMAGIC_STRING` - defined based on kernel configuration options.
[Intermediate]

Q: What function validates the vermagic during module loading?
A: `check_modinfo()` - compares module's vermagic from .modinfo section against kernel's VERMAGIC_STRING.
[Intermediate]

Q: What happens if vermagic doesn't match?
A: Module loading fails with "version magic 'X' should be 'Y'" unless force loading is enabled.
[Intermediate]

Q: What is CONFIG_MODVERSIONS?
A: Kernel option that adds CRC checksums to exported symbols, detecting ABI changes between kernel/module compilation.
[Intermediate]

Q: How does CONFIG_MODVERSIONS detect incompatible changes?
A: CRC is computed from symbol's type signature. If function prototype changes, CRC changes, causing "disagrees about version of symbol" error.
[Intermediate]

Q: What is kernel tainting?
A: A flag system marking the kernel as "tainted" when potentially problematic events occur (proprietary modules, forced loads, etc.).
[Basic]

Q: What taint flag is set for proprietary modules?
A: `TAINT_PROPRIETARY_MODULE` (flag 'P') - set when loading a module without GPL-compatible license.
[Intermediate]

Q: What taint flag is set for force-loaded modules?
A: `TAINT_FORCED_MODULE` (flag 'F') - set when loading module with vermagic mismatch using force flag.
[Intermediate]

Q: Where can you see current kernel taint flags?
A: `/proc/sys/kernel/tainted` - non-zero value indicates tainted, each bit represents different taint reason.
[Intermediate]

Q: Why does tainting matter?
A: Bug reports from tainted kernels may be deprioritized by kernel developers since non-standard configurations make debugging difficult.
[Intermediate]

Q: What is the TAINT_OOT_MODULE flag?
A: Out-of-tree module (flag 'O') - module built outside the kernel source tree. Set based on "intree" modinfo.
[Intermediate]

Q: What memory protections are applied to loaded modules?
A: Read-only (RO) on text/const sections via set_memory_ro(), No-execute (NX) on data sections via set_memory_nx().
[Intermediate]

Q: What does set_memory_ro() prevent?
A: Prevents modification of code sections after loading - protects against code injection attacks that try to modify kernel code.
[Intermediate]

Q: What does set_memory_nx() prevent?
A: Prevents execution of data sections - protects against exploits that try to execute shellcode placed in data areas.
[Intermediate]

Q: [Cloze] CONFIG_MODVERSIONS adds _____ to exported symbols, stored in the _____ section.
A: CRC checksums; `__kcrctab` (or `__kcrctab_gpl`)
[Intermediate]

Q: What happens if a module uses a symbol with mismatched CRC?
A: Loading fails with "module: disagrees about version of symbol X" indicating ABI incompatibility.
[Intermediate]

## Section 12: kallsyms and Debugging

Q: What is kallsyms?
A: Kernel symbol table subsystem providing address-to-name and name-to-address lookups for kernel and module symbols, used for debugging.
[Basic]

Q: What file exposes the kernel symbol table to userspace?
A: `/proc/kallsyms` - lists all kernel and module symbols with addresses and types.
[Basic]

Q: What is the format of entries in /proc/kallsyms?
A: `address type name [module]` - e.g., "ffffffff810a0b20 T do_fork" or "ffffffffa0001000 t mymod_init [mymod]"
[Intermediate]

Q: What does the 'T' type mean in kallsyms output?
A: Text (code) section, global symbol. Lowercase 't' means text section, local symbol.
[Intermediate]

Q: What does the 'D' type mean in kallsyms output?
A: Data section, global symbol. 'd' means data section, local symbol.
[Intermediate]

Q: What function looks up a symbol address by name?
A: `kallsyms_lookup_name(const char *name)` - returns address or 0 if not found.
[Intermediate]

Q: What function looks up a symbol name by address?
A: `kallsyms_lookup(addr, symbolsize, offset, modname, namebuf)` - fills buffer with symbol name.
[Intermediate]

Q: How are kallsyms symbol names stored in the kernel?
A: Compressed using token table compression - common substrings replaced with single-byte tokens to save memory.
[Advanced]

Q: What file shows currently loaded modules?
A: `/proc/modules` - lists module name, size, reference count, and dependencies.
[Basic]

Q: What information does /proc/modules provide for each module?
A: Module name, memory size, reference count (use count), list of modules depending on it, state, and memory address.
[Intermediate]

Q: What is the sysfs interface for modules?
A: `/sys/module/<module_name>/` - contains parameters/, sections/, and various module attributes.
[Basic]

Q: What does `/sys/module/<name>/sections/` contain?
A: Files showing addresses of various ELF sections (e.g., .text, .data, .bss) - useful for debuggers like gdb.
[Intermediate]

Q: How can you get section addresses for debugging a module?
A: Read `/sys/module/<name>/sections/.text` etc., or use `modinfo -F sectionname module.ko` for on-disk info.
[Intermediate]

Q: What tool displays detailed information about a module file?
A: `modinfo module.ko` - shows filename, license, author, description, parameters, dependencies, vermagic.
[Basic]

Q: How do you enable verbose module loading messages?
A: Boot with `debug` kernel parameter, or `echo 8 > /proc/sys/kernel/printk` to see debug messages.
[Intermediate]

Q: What kernel config enables module debugging symbols?
A: `CONFIG_DEBUG_INFO` - includes debug symbols in vmlinux and modules for use with gdb/crash.
[Intermediate]

Q: How can you debug a loaded module with gdb?
A: Use section addresses from /sys/module/<name>/sections/ with `add-symbol-file` command in gdb attached to /proc/kcore or crash dump.
[Advanced]

Q: [Cloze] The /proc/kallsyms file provides _____ to _____ mapping for kernel debugging.
A: address; symbol name (and vice versa)
[Basic]

Q: What does `sprint_symbol()` do?
A: Formats a kernel address as "symbol+offset/size [module]" string - useful for stack traces and debugging output.
[Advanced]

Q: What function prints a kernel stack trace?
A: `dump_stack()` - uses kallsyms to convert return addresses to symbol names for readable output.
[Intermediate]

