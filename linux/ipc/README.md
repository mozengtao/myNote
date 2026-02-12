# Linux Userspace IPC — Deep Dive

A comprehensive, implementation-level guide to Linux Inter-Process Communication
mechanisms in C.  Every section includes kernel internals, ASCII diagrams, and
complete, compilable working examples.

## Table of Contents

| # | Mechanism | File | Key Concept |
|---|-----------|------|-------------|
| 1 | [File-Based IPC](01_file_based_ipc.md) | Regular files + advisory locks | Simplest IPC; page cache mediated; 2 copies |
| 2 | [UNIX Domain Sockets](02_unix_domain_sockets.md) | AF_UNIX stream/datagram | Bidirectional; sk_buff chains; the workhorse |
| 3 | [POSIX Semaphores](03_posix_semaphores.md) | Named counting semaphores | Futex-based sync; no data transfer |
| 4 | [POSIX Message Queues](04_posix_message_queues.md) | Kernel message buffers | Priority ordering; message boundaries |
| 5 | [Shared Memory](05_shared_memory.md) | POSIX shm + mmap | Zero-copy; fastest; manual sync required |
| 6 | [Comparison & Pitfalls](06_comparison.md) | Cross-mechanism analysis | Decision guide; common bugs; debug tools |

## Quick Start

Each mechanism has a complete working example in `examples/`:

```
examples/
├── 01_file/           # File-based IPC with fcntl locking
│   ├── core_process.c
│   ├── noncore_process.c
│   └── Makefile
├── 02_socket/         # UNIX domain socket client/server
│   ├── core_process.c
│   ├── noncore_process.c
│   └── Makefile
├── 03_semaphore/      # POSIX semaphore producer/consumer
│   ├── core_process.c
│   ├── noncore_process.c
│   └── Makefile
├── 04_msgqueue/       # POSIX message queue sender/receiver
│   ├── core_process.c
│   ├── noncore_process.c
│   └── Makefile
└── 05_sharedmem/      # Shared memory ring buffer
    ├── core_process.c
    ├── noncore_process.c
    └── Makefile
```

### Build and Run Any Example

```bash
# Build
cd docs/ipc/examples/02_socket
make

# Terminal 1: start server
./core_process

# Terminal 2: start client
./noncore_process

# Clean up
make clean
```

### Build All Examples

```bash
for dir in docs/ipc/examples/*/; do
    echo "=== Building $dir ==="
    make -C "$dir"
done
```

## Requirements

- Linux (any recent kernel, tested on 5.x+)
- GCC (`gcc`)
- POSIX libraries: `-lpthread` (semaphores), `-lrt` (mq, shm)
- No external dependencies

## At A Glance

```
                    Speed
                      ▲
                      │
   Shared Memory ─────┤  ★ Zero-copy, ~100ns
                      │
   Semaphore ─────────┤  ★ Sync only, ~50ns (fast path)
                      │
   UNIX Socket ───────┤  ★ General purpose, ~2μs
                      │
   Message Queue ─────┤  ★ Priority msgs, ~3μs
                      │
   File-Based ────────┤  ★ Persistent, ~1μs-10ms
                      │
                      └──────────────────────────▶ Complexity
                         Low      Med      High
```

## License

Educational material.  Code examples are public domain.
