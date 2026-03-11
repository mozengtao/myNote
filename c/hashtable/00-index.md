# Hash Table Implementation Guide for C Programmers

A comprehensive guide covering 10 hash table implementation methods in C, with runnable code examples, performance characteristics, and practical advice.

---

## Table of Contents

### Part 1: Separate Chaining
**File:** `01-separate-chaining.md`

1. **Separate Chaining (Linked List Buckets)**
   - Classic approach, simple and flexible
   - Handles collisions with per-bucket linked lists

2. **Separate Chaining with Intrusive Lists**
   - Embed linkage in user structures
   - Zero-copy, memory-pool friendly

---

### Part 2: Open Addressing Basics
**File:** `02-open-addressing-basic.md`

3. **Linear Probing**
   - Sequential scan on collision
   - Best cache behavior, but clustering issues

4. **Quadratic Probing**
   - Non-linear probe sequence (h + i²)
   - Reduces primary clustering

5. **Double Hashing**
   - Two hash functions, key-dependent stride
   - Eliminates secondary clustering

---

### Part 3: Advanced Open Addressing
**File:** `03-open-addressing-advanced.md`

6. **Robin Hood Hashing**
   - Steal from the rich, give to the poor
   - Low variance, high load factor support

7. **Cuckoo Hashing**
   - Multiple tables, guaranteed O(1) lookup
   - ~50% space efficiency trade-off

---

### Part 4: Specialized Methods
**File:** `04-specialized-methods.md`

8. **Perfect Hashing**
   - No collisions by construction
   - Static key sets only

9. **Hopscotch Hashing**
   - Neighborhood-based with bitmap
   - Bounded search, concurrent-friendly

10. **Hash Array Mapped Trie (HAMT)**
    - Trie structure using hash bits
    - No resize, structural sharing

---

### Part 5: Comparison and Practical Advice
**File:** `05-comparison-and-advice.md`

- Summary comparison table (all 10 methods)
- Rules of thumb for choosing methods
- When NOT to use certain methods
- Common implementation mistakes
- When profiling justifies switching
- Closing perspective on systems design

---

## Quick Reference

### By Use Case

| Use Case | Recommended Method |
|----------|-------------------|
| General purpose | Separate Chaining |
| Performance-critical | Robin Hood |
| Guaranteed O(1) worst-case | Cuckoo |
| Static keys (e.g., keywords) | Perfect Hashing |
| Very large datasets | HAMT |
| Kernel/embedded (no malloc) | Intrusive Chaining |
| Concurrent access | Hopscotch |
| Simple read-heavy | Linear Probing |

### By Characteristic

| Need | Best Methods |
|------|-------------|
| Cache friendly | Linear Probing, Robin Hood, Hopscotch |
| High load factor | Robin Hood (0.9+), Separate Chaining |
| Easy deletion | Separate Chaining, Cuckoo |
| Low variance | Robin Hood, Cuckoo |
| No resize | HAMT |
| Low memory | Robin Hood, Linear Probing |

---

## Building the Examples

All code examples compile with:

```bash
gcc -Wall -Wextra -O2 -o <output> <source.c>
```

No external dependencies required—only standard libc.

---

## File Organization

```
hashtable-guide/
├── 00-index.md                    <- This file
├── 01-separate-chaining.md        <- Methods 1-2
├── 02-open-addressing-basic.md    <- Methods 3-5
├── 03-open-addressing-advanced.md <- Methods 6-7
├── 04-specialized-methods.md      <- Methods 8-10
└── 05-comparison-and-advice.md    <- Summary and advice
```

---

## Author's Note

This guide is written from the perspective of a systems programmer who has dealt with hash tables in production C code. The emphasis is on:

1. **Practical, runnable code** - Not pseudocode, real C
2. **Understanding trade-offs** - There's no perfect hash table
3. **When to use what** - Concrete guidance, not academic hand-waving
4. **Memory and cache behavior** - What actually matters for performance

The SELinux `hashtab.c` that inspired this guide is a good example of production-quality hash table code: simple, correct, appropriate for its use case. Most real systems should aim for similar clarity over cleverness.
