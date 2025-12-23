# C++ Systems Engineering Mastery Guide

## Overview

This guide covers 27 essential C++ topics for systems programming, organized into 8 categories. Each topic includes:
- Problem statement
- Core concepts
- Complete examples
- Failure modes
- Trade-offs

---

## Topic Index

### I. Resource & Lifetime Management ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 1 | RAII | [01_raii.md](01_resource_lifetime/01_raii.md) ✅ | Automatic, exception-safe resource cleanup |
| 2 | Value vs Reference Semantics | [02_value_vs_reference_semantics.md](01_resource_lifetime/02_value_vs_reference_semantics.md) ✅ | Clear ownership and aliasing behavior |
| 3 | Rule of Zero / Rule of Five | [03_rule_of_zero_five.md](01_resource_lifetime/03_rule_of_zero_five.md) ✅ | Correct copy/move behavior |
| 4 | Move Semantics | [04_move_semantics.md](01_resource_lifetime/04_move_semantics.md) ✅ | Efficient ownership transfer |

### II. Interfaces & Abstractions ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 5 | Zero-Cost Abstractions | [05_zero_cost_abstractions.md](02_interfaces_abstractions/05_zero_cost_abstractions.md) ✅ | High-level code without runtime overhead |
| 6 | Static Polymorphism | [06_static_polymorphism.md](02_interfaces_abstractions/06_static_polymorphism.md) ✅ | Compile-time dispatch, no vtable |
| 7 | Dynamic Polymorphism | [07_dynamic_polymorphism.md](02_interfaces_abstractions/07_dynamic_polymorphism.md) ✅ | Runtime flexibility via virtual |
| 8 | Type Erasure | [08_type_erasure.md](02_interfaces_abstractions/08_type_erasure.md) ✅ | Polymorphism without inheritance |

### III. Error Handling & Robustness ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 9 | Exception Safety Guarantees | [09_exception_safety.md](03_error_handling/09_exception_safety.md) ✅ | Predictable behavior on failure |
| 10 | Exceptions vs optional/expected | [10_exceptions_vs_optional.md](03_error_handling/10_exceptions_vs_optional.md) ✅ | Right error handling for context |
| 11 | Error-Resilient APIs | [11_error_resilient_apis.md](03_error_handling/11_error_resilient_apis.md) ✅ | APIs that are hard to misuse |

### IV. Memory & Performance Awareness ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 12 | Object Layout & Alignment | [12_object_layout.md](04_memory_performance/12_object_layout.md) ✅ | Cache efficiency, ABI correctness |
| 13 | Heap vs Stack Allocation | [13_heap_vs_stack.md](04_memory_performance/13_heap_vs_stack.md) ✅ | Optimal memory usage |
| 14 | Custom Allocators & std::pmr | [14_custom_allocators.md](04_memory_performance/14_custom_allocators.md) ✅ | Memory control and performance |

### V. Concurrency & Parallelism ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 15 | C++ Memory Model | [15_memory_model.md](05_concurrency/15_memory_model.md) ✅ | Correct multithreaded reasoning |
| 16 | Atomics & Memory Ordering | [16_atomics.md](05_concurrency/16_atomics.md) ✅ | Lock-free synchronization |
| 17 | Mutexes & RAII Locking | [17_mutexes_raii_locking.md](05_concurrency/17_mutexes_raii_locking.md) ✅ | Deadlock-free locking |
| 18 | Why Lock-Free Is Hard | [18_lockfree_hard.md](05_concurrency/18_lockfree_hard.md) ✅ | Knowing when NOT to go lock-free |

### VI. Compile-Time Programming ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 19 | Templates as a Language | [19_templates.md](06_compile_time/19_templates.md) ✅ | Generic, reusable code |
| 20 | constexpr Evaluation | [20_constexpr.md](06_compile_time/20_constexpr.md) ✅ | Compile-time computation |
| 21 | Concepts | [21_concepts.md](06_compile_time/21_concepts.md) ✅ | Readable template constraints |

### VII. Build, Tooling & Real-World Survival ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 22 | Header Hygiene | [22_header_hygiene.md](07_build_tooling/22_header_hygiene.md) ✅ | Fast compilation |
| 23 | PIMPL Idiom | [23_pimpl.md](07_build_tooling/23_pimpl.md) ✅ | ABI stability, reduced coupling |
| 24 | Sanitizers & Static Analysis | [24_tooling.md](07_build_tooling/24_tooling.md) ✅ | Finding bugs early |

### VIII. Design & API Thinking ✅

| # | Topic | File | Primary Benefit |
|---|-------|------|-----------------|
| 25 | Composition over Inheritance | [25_composition_over_inheritance.md](08_design_api/25_composition_over_inheritance.md) ✅ | Flexible, maintainable designs |
| 26 | API Design with Invariants | [26_api_invariants.md](08_design_api/26_api_invariants.md) ✅ | APIs that enforce correctness |
| 27 | Expressing Ownership in Types | [27_expressing_ownership_in_types.md](08_design_api/27_expressing_ownership_in_types.md) ✅ | Self-documenting memory ownership |

---

## All 27 Topics Complete! ✅

```
cpp_mastery/
├── 00_index.md                              ← This file
├── 01_resource_lifetime/
│   ├── 01_raii.md                           ✅
│   ├── 02_value_vs_reference_semantics.md   ✅
│   ├── 03_rule_of_zero_five.md              ✅
│   └── 04_move_semantics.md                 ✅
├── 02_interfaces_abstractions/
│   ├── 05_zero_cost_abstractions.md         ✅
│   ├── 06_static_polymorphism.md            ✅
│   ├── 07_dynamic_polymorphism.md           ✅
│   └── 08_type_erasure.md                   ✅
├── 03_error_handling/
│   ├── 09_exception_safety.md               ✅
│   ├── 10_exceptions_vs_optional.md         ✅
│   └── 11_error_resilient_apis.md           ✅
├── 04_memory_performance/
│   ├── 12_object_layout.md                  ✅
│   ├── 13_heap_vs_stack.md                  ✅
│   └── 14_custom_allocators.md              ✅
├── 05_concurrency/
│   ├── 15_memory_model.md                   ✅
│   ├── 16_atomics.md                        ✅
│   ├── 17_mutexes_raii_locking.md           ✅
│   └── 18_lockfree_hard.md                  ✅
├── 06_compile_time/
│   ├── 19_templates.md                      ✅
│   ├── 20_constexpr.md                      ✅
│   └── 21_concepts.md                       ✅
├── 07_build_tooling/
│   ├── 22_header_hygiene.md                 ✅
│   ├── 23_pimpl.md                          ✅
│   └── 24_tooling.md                        ✅
└── 08_design_api/
    ├── 25_composition_over_inheritance.md   ✅
    ├── 26_api_invariants.md                 ✅
    └── 27_expressing_ownership_in_types.md  ✅
```

---

## Summary Table: Topics → Benefits

```
+------------------------------------------------------------------+
|              TOPIC → PRIMARY BENEFIT MAPPING                      |
+------------------------------------------------------------------+
| RESOURCE MANAGEMENT                                              |
|   RAII                    → No resource leaks, exception-safe    |
|   Value Semantics         → Simple, thread-safe reasoning        |
|   Rule of Zero            → Less code, fewer bugs                |
|   Move Semantics          → Efficient transfers, no copies       |
+------------------------------------------------------------------+
| ABSTRACTIONS                                                     |
|   Zero-Cost Abstractions  → High-level = fast                    |
|   Static Polymorphism     → No vtable, full inlining             |
|   Dynamic Polymorphism    → Runtime flexibility                  |
|   Type Erasure            → Polymorphism without hierarchy       |
+------------------------------------------------------------------+
| ERROR HANDLING                                                   |
|   Exception Safety        → Predictable failure modes            |
|   optional/expected       → Explicit error paths                 |
|   Error-Resilient APIs    → Hard to misuse                       |
+------------------------------------------------------------------+
| MEMORY                                                           |
|   Object Layout           → Cache efficiency, correct ABI        |
|   Stack vs Heap           → Optimal memory usage                 |
|   Custom Allocators       → Performance control                  |
+------------------------------------------------------------------+
| CONCURRENCY                                                      |
|   Memory Model            → Correct reasoning about threads      |
|   Atomics                 → Lock-free when appropriate           |
|   RAII Locking            → No deadlocks, no forgotten unlocks   |
|   Lock-Free Caution       → Know when NOT to use it              |
+------------------------------------------------------------------+
| COMPILE-TIME                                                     |
|   Templates               → Generic, type-safe code              |
|   constexpr               → Zero runtime cost for constants      |
|   Concepts                → Clear template requirements          |
+------------------------------------------------------------------+
| TOOLING                                                          |
|   Header Hygiene          → Fast builds                          |
|   PIMPL                   → ABI stability                        |
|   Sanitizers              → Early bug detection                  |
+------------------------------------------------------------------+
| DESIGN                                                           |
|   Composition             → Flexible combinations                |
|   Invariants              → Compile-time correctness             |
|   Ownership Types         → Self-documenting lifetime            |
+------------------------------------------------------------------+
```

---

## Recommended Learning Order

### For Real-World Engineers

```
PHASE 1: FOUNDATIONS (Week 1-2)
┌─────────────────────────────────────────────────────────────────┐
│ 1. RAII                    - The most important C++ concept     │
│ 2. Rule of Zero/Five       - When to write special members      │
│ 3. Move Semantics          - Efficient resource transfer        │
│ 4. Value vs Reference      - Choose the right semantics         │
└─────────────────────────────────────────────────────────────────┘
                              ↓
PHASE 2: SAFETY & CORRECTNESS (Week 3-4)
┌─────────────────────────────────────────────────────────────────┐
│ 5. Expressing Ownership    - Smart pointer guidelines           │
│ 6. Exception Safety        - Writing robust code                │
│ 7. Mutexes & RAII Locking  - Thread-safe patterns               │
│ 8. API Invariants          - Designing for correctness          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
PHASE 3: PERFORMANCE (Week 5-6)
┌─────────────────────────────────────────────────────────────────┐
│ 9.  Zero-Cost Abstractions - Why C++ can be fast                │
│ 10. Object Layout          - Cache-friendly structures          │
│ 11. Stack vs Heap          - Allocation strategies              │
│ 12. Static Polymorphism    - Templates for performance          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
PHASE 4: ADVANCED TOPICS (Week 7-8)
┌─────────────────────────────────────────────────────────────────┐
│ 13. C++ Memory Model       - Understanding happens-before       │
│ 14. Type Erasure           - Advanced polymorphism              │
│ 15. Concepts (C++20)       - Template constraints               │
│ 16. Custom Allocators      - Memory control                     │
└─────────────────────────────────────────────────────────────────┘
                              ↓
PHASE 5: PRODUCTION SKILLS (Ongoing)
┌─────────────────────────────────────────────────────────────────┐
│ 17. Header Hygiene         - Build time optimization            │
│ 18. PIMPL                  - ABI stability                      │
│ 19. Sanitizers             - Continuous bug finding             │
│ 20. Lock-Free Caution      - Know your limits                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference: C++ vs C/Kernel Style

```
+------------------------------------------------------------------+
|           C++ IDIOM              │    C / KERNEL EQUIVALENT       |
+------------------------------------------------------------------+
| std::unique_ptr<T>               │ T* + explicit free()           |
| RAII constructor/destructor      │ init()/cleanup() functions     |
| std::lock_guard                  │ spin_lock()/spin_unlock()      |
| std::vector                      │ malloc + realloc + size track  |
| std::string_view                 │ const char* + length           |
| std::optional<T>                 │ T + bool valid                 |
| throw exception                  │ return error code              |
| template<typename T>             │ void* + macros                 |
| virtual function                 │ function pointer table         |
| constexpr                        │ #define or enum                |
+------------------------------------------------------------------+
```

---

## Key Principles

### The C++ Philosophy

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  1. "You don't pay for what you don't use"                      │
│     Abstractions should have zero overhead when not needed      │
│                                                                 │
│  2. "What you use, you couldn't hand-code any better"           │
│     STL should be as fast as hand-written C                     │
│                                                                 │
│  3. "Express intent in code, not comments"                      │
│     Types should encode semantics (ownership, nullability)      │
│                                                                 │
│  4. "Make interfaces easy to use correctly, hard to misuse"     │
│     The type system should prevent bugs at compile time         │
│                                                                 │
│  5. "Leave no room for a lower-level language"                  │
│     C++ should handle all systems programming needs             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

*This guide focuses on practical systems programming. For language lawyer details, see the C++ standard.*

*Last updated: December 2024*

