# C Data Structures — Technical Reference Library

> Production-ready, generic C implementations for systems programming.
> Standard: C99/C11 | No global variables | `void *` generics | Full memory safety

## Contents

| # | Data Structure | File | Key Focus |
|---|----------------|------|-----------|
| 1 | [Dynamic Array](01-dynamic-array.md) | `01-dynamic-array.md` | Amortized O(1) append, geometric growth |
| 2 | [Linked List](02-linked-list.md) | `02-linked-list.md` | Singly & Doubly linked, intrusive option |
| 3 | [Hash Table](03-hash-table.md) | `03-hash-table.md` | Separate chaining, `void *` keys/values |
| 4 | [Red-Black Tree](04-red-black-tree.md) | `04-red-black-tree.md` | Self-balancing BST for indexing |
| 5 | [B+ Tree](05-bplus-tree.md) | `05-bplus-tree.md` | Disk/cache-friendly ordered storage |
| 6 | [Stack & Queue](06-stack-queue.md) | `06-stack-queue.md` | Array-based and list-based variants |
| 7 | [Ring Buffer](07-ring-buffer.md) | `07-ring-buffer.md` | Lock-free SPSC for producer-consumer |

## Design Principles

All implementations share a consistent API philosophy:

```
+------------------+------------------------------------------+
| Pattern          | Description                              |
+------------------+------------------------------------------+
| _create / _init  | Allocate or initialize the structure     |
| _destroy / _free | Release all resources, call destructors  |
| _insert / _push  | Add elements                             |
| _remove / _pop   | Remove elements                          |
| _search / _get   | Lookup elements                          |
+------------------+------------------------------------------+
```

### Memory Management Contract

- Every `_create` has a matching `_destroy`.
- Structures accept an optional destructor (`void (*free_fn)(void *)`) for owned data.
- All functions validate inputs; NULL pointers return error codes, never crash.
- Return values: `0` on success, `-1` on failure (with `errno` set where applicable).

### Compilation

All files are single-header-capable. Compile with:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o test_ds ds_file.c
```

For the ring buffer (atomics):

```bash
gcc -std=c11 -Wall -Wextra -O2 -pthread -o test_ring ring_buffer.c
```
