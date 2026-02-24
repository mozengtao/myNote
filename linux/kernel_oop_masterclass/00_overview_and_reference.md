# Object-Oriented Design Patterns in the Linux Kernel (v3.2)

> **Audience**: C systems programmer with ~5 years experience
> **Goal**: Master the object-oriented design patterns hidden in plain C throughout the Linux kernel
> **Kernel version**: 3.2 (released January 2012)

---

## Why This Guide Exists

The Linux kernel is, by line count and contributor count, one of the largest
C programs ever written. It has no classes, no `virtual` keyword, no templates,
no exceptions. And yet it implements a rich object-oriented architecture that
rivals any C++ or Java framework in sophistication.

This guide teaches you to **read kernel C through an OOP lens**. Once you see
the patterns, you will never read kernel code the same way again.

---

## How to Use This Guide

### Interactive mode
Feed the system prompt (below) to an LLM, then use the conversation starters
to begin. The LLM will teach module by module, with exercises and Socratic
questions.

### Self-study mode
Read the modules top to bottom. Open the kernel source alongside
(`git checkout v3.2` or browse at
[elixir.bootlin.com/linux/v3.2/source](https://elixir.bootlin.com/linux/v3.2/source)).
Complete each exercise before moving on.

### Reference mode
Use the Quick Reference Card as a cheat sheet when reading unfamiliar kernel
code. For the canonical mental model, pitfalls, and v3.2 file anchors per
pattern, use [Mental models and v3.2 anchors](00b_mental_models_and_anchors.md).

---

## Module Index

| Module | Title | File | Core Concept |
|--------|-------|------|--------------|
| 0 | Overview & Reference | [This file](00_overview_and_reference.md) | Roadmap, reference card |
| — | Mental models & anchors | [00b_mental_models_and_anchors.md](00b_mental_models_and_anchors.md) | Mind map, checklist, v3.2 file anchors |
| 1 | Encapsulation | [01_encapsulation.md](01_encapsulation.md) | Opaque pointers, `static`, header splits |
| 2 | Inheritance | [02_inheritance.md](02_inheritance.md) | Struct embedding, `container_of` |
| 3 | Polymorphism | [03_polymorphism.md](03_polymorphism.md) | Function pointer tables (vtables) |
| 4 | The kobject Hierarchy | [04_kobject_hierarchy.md](04_kobject_hierarchy.md) | Formal object system, refcounting |
| 5 | Classic Design Patterns | [05_design_patterns.md](05_design_patterns.md) | GoF patterns in kernel C |
| 6 | Synthesis | [06_synthesis.md](06_synthesis.md) | Reading new code through an OOP lens |

Each module builds on the previous. Complete one before moving to the next.

**Deep reference**: [Mental models and v3.2 anchors](00b_mental_models_and_anchors.md) — canonical mind map per pattern, how/why checklist, and primary file anchors.

---

## Quick Reference Card

| OOP Concept              | C++ / Java                    | Linux Kernel C (v3.2)                          |
|--------------------------|-------------------------------|------------------------------------------------|
| Class                    | `class Foo { ... };`          | `struct foo { ... };`                          |
| Private members          | `private:`                    | `static` functions, `void *private_data`       |
| Inheritance              | `class D : public B`          | Embed `struct B` inside `struct D`             |
| Downcasting              | `static_cast<D*>(b)`          | `container_of(b, struct D, base_member)`       |
| Virtual method table     | `virtual void f();`           | `struct foo_operations { int (*f)(...); };`    |
| Pure virtual / Interface | `virtual void f() = 0;`       | Function pointer set to NULL (caller checks)   |
| Constructor              | `Foo::Foo()`                  | `foo_alloc()` / `foo_init()` / `probe()`       |
| Destructor               | `Foo::~Foo()`                 | `foo_free()` / `foo_release()` / `remove()`    |
| Reference counting       | `std::shared_ptr<T>`          | `struct kref` + `kref_get/put`                 |
| Abstract base class      | `class Base` with virtuals    | `struct kobject` + `struct kobj_type`           |
| Template Method          | Base defines algorithm         | VFS skeleton calls ops struct methods           |
| Strategy                 | Inject algorithm object        | Swap `sched_class` / `tcp_congestion_ops`      |
| Observer                 | Listener/Event pattern         | `struct notifier_block` chains                  |
| Iterator                 | `begin()` / `end()` / `next()`| `list_for_each_entry()` / `seq_file` protocol  |
| Factory                  | `new T()` / factory method     | `kmem_cache_create()` + `kmem_cache_alloc()`   |
| Singleton                | `static instance`              | `__initcall` / global subsystem structs         |

---

## Conversation Starters

Use these to begin an interactive session. Each question naturally leads into
the corresponding module.

1. **"I see `void *private_data` everywhere in kernel structs. What's the
   design rationale? Isn't it unsafe compared to typed pointers?"**
   → Leads to Module 1 (Encapsulation)

2. **"I need to add a custom field to `struct inode` for my filesystem.
   What's the proper way — do I modify the struct definition?"**
   → Leads to Module 2 (Inheritance via embedding)

3. **"I want to write a character device driver. I see I need to fill in a
   `struct file_operations`. Why is it designed this way instead of using a
   switch on device type?"**
   → Leads to Module 3 (Polymorphism)

4. **"My kernel module's objects are getting freed while sysfs files still
   reference them. What am I doing wrong?"**
   → Leads to Module 4 (kobject lifecycle)

5. **"I'm implementing a network monitoring feature that needs to react when
   interfaces go up or down. What's the kernel's mechanism for this kind of
   event notification?"**
   → Leads to Module 5 (Observer pattern / notifier chains)

6. **"I've been assigned to review a patch to the USB subsystem but the code
   structure is unfamiliar. How do I quickly understand the architecture?"**
   → Leads to Module 6 (Reading code through an OOP lens)

---

## Recommended Source Files to Study (v3.2)

| File                                  | Why                                             |
|---------------------------------------|-------------------------------------------------|
| `include/linux/kobject.h`             | The base class of the kernel object model       |
| `include/linux/fs.h`                  | `file_operations`, `inode_operations` — vtables |
| `include/linux/device.h`              | `struct device` — the device inheritance tree   |
| `include/linux/netdevice.h`           | `struct net_device` — rich OOP in networking    |
| `include/linux/kref.h`                | Reference counting primitives                   |
| `include/linux/list.h`                | Linked list + iterator macros                   |
| `include/linux/notifier.h`            | Observer pattern infrastructure                 |
| `include/linux/sched.h`               | `struct sched_class` — strategy pattern         |
| `fs/ext4/super.c`                     | Filesystem "class" registration                 |
| `drivers/char/mem.c`                  | Simple char device — clean vtable examples      |
| `lib/kobject.c`                       | kobject lifecycle implementation                |
| `kernel/sched/core.c`                 | Scheduler dispatch through `sched_class`        |

---

## System Prompt (for LLM-assisted study)

```text
You are an expert Linux kernel developer and computer science educator who
specializes in software architecture. You have 20+ years of experience reading,
writing, and teaching kernel internals. Your task is to teach a seasoned C
systems programmer how the Linux kernel (v3.2) implements object-oriented
design patterns using only ANSI C — no C++ features, no compiler extensions
beyond GCC __attribute__s.

The student already knows:
  - C fluently (pointers, structs, function pointers, unions, bitfields)
  - Systems programming (memory management, concurrency, I/O)
  - Basic OOP vocabulary (encapsulation, inheritance, polymorphism)

The student does NOT yet know:
  - How these OOP concepts map onto idiomatic kernel C
  - The architectural "why" behind kernel data structure conventions
  - How to read kernel code through an OOP lens

Teaching style:
  1. Always start with a REAL kernel v3.2 code snippet (file path + line range)
  2. Then explain the OOP pattern it demonstrates
  3. Then show the equivalent concept in a language the student knows (C++ or Java)
  4. Then discuss WHY the kernel chose this approach over alternatives
  5. Use diagrams (ASCII art) when struct relationships are complex
  6. Ask Socratic questions to check understanding before moving on
  7. When the student gives a wrong answer, don't just correct — guide them
     to discover the error themselves

Constraints:
  - All code examples must be from Linux kernel v3.2 source tree
  - Use exact file paths (e.g., include/linux/fs.h, drivers/char/...)
  - Never invent fake kernel code — only use real structs, functions, macros
  - If asked about a pattern that doesn't exist in the kernel, say so honestly
```

---

## Prerequisites

Before starting, make sure you can:

- [ ] Read C fluently (pointers, structs, function pointers, unions, bitfields)
- [ ] Explain what `offsetof(type, member)` computes
- [ ] Navigate a large C project by header/implementation split
- [ ] Explain what `static` means for a function defined at file scope
- [ ] Describe what a function pointer is and how to call through one
