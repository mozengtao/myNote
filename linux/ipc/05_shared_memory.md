# 5. Shared Memory (POSIX + mmap)

## Mental Model

### What Problem This Solves

Shared memory is the **fastest IPC mechanism** because it eliminates ALL data
copies between processes.  Two processes map the **same physical pages** of RAM
into their respective virtual address spaces.  After setup, reading and writing
shared data is a plain CPU load/store instruction — no syscall, no kernel
involvement, no copy.

The trade-off: the kernel provides no synchronization.  The processes must
coordinate access themselves using semaphores, mutexes, or atomic operations.
Shared memory gives you raw speed; you supply the correctness.

### When To Use

- High-throughput, low-latency data sharing (real-time audio, video, telemetry)
- Large data structures shared between cooperating processes
- When you need zero-copy transfer (no memcpy overhead)
- Ring buffers, lock-free queues between processes
- Memory-mapped files for fast persistent storage

### When NOT To Use

- Simple request/response (use sockets — they include synchronization)
- When processes don't trust each other (shared memory = shared attack surface)
- Small, infrequent messages (overhead of setup not worth it)
- When you need message framing or priority (use message queues)

### Communication Pattern

```
  Process A (producer)           Shared Memory              Process B (consumer)
  ┌──────────────────┐          ┌─────────────┐            ┌──────────────────┐
  │ Virtual Address  │          │ Physical    │            │ Virtual Address  │
  │ 0x7f4000        │◀────────▶│ RAM Pages   │◀──────────▶│ 0x7a2000        │
  │                  │   mmap   │             │    mmap    │                  │
  │ ring->slots[0]  │─ write ─▶│ ┌─────────┐ │◀─ read ───│ ring->slots[0]  │
  │ ring->slots[1]  │          │ │ Data    │ │            │ ring->slots[1]  │
  │ ring->slots[2]  │          │ │ lives   │ │            │ ring->slots[2]  │
  │ ring->slots[3]  │          │ │ HERE    │ │            │ ring->slots[3]  │
  └──────────────────┘          │ └─────────┘ │            └──────────────────┘
                                └─────────────┘
                                      │
                                 ZERO COPIES!
                                 Both processes access
                                 the same physical RAM

  Pattern: 1→1 (SPSC ring buffer)
  Also: many→many with proper locking
```

### Kernel Objects Involved

| Object | Role |
|--------|------|
| `/dev/shm/NAME` | tmpfs file backing the shared region |
| `struct inode` | inode of the tmpfs file |
| Page cache pages | Physical RAM pages backing the mapping |
| `struct vm_area_struct` (VMA) | Per-process record of the mapped region |
| Page table entries (PTEs) | MMU mappings: virtual addr → physical page |

### Blocking Behavior

Shared memory itself **never blocks** — reads and writes are just CPU
instructions.  Blocking comes from the synchronization primitives you layer
on top (semaphores, futexes, etc.).

The only exception: page faults.  On first access to a demand-paged mapping,
the CPU triggers a page fault.  The kernel allocates a physical page, updates
the PTE, and the instruction retries.  This is transparent but adds ~1-5 μs
on first access.

### Lifetime Rules

- `shm_open()` creates a file in tmpfs (`/dev/shm/`)
- The file persists until `shm_unlink()` removes it
- `mmap()` creates a VMA referencing the file's pages
- `close(shm_fd)` is safe after mmap — the mapping holds a reference
- `munmap()` removes the VMA; physical pages freed when no more references
- On `fork()`: child inherits the mapping (same physical pages, CoW for
  MAP_PRIVATE, truly shared for MAP_SHARED)
- On `exec()`: all mappings destroyed; child must re-open + re-mmap
- On crash: mapping destroyed, file persists in `/dev/shm/`

### Performance Characteristics

- **Copy count**: 0 (zero-copy — direct physical memory access)
- **Latency**: ~50-100 ns for cache-hot access (L1/L2 cache hit)
- **Throughput**: Memory bandwidth (~20-50 GB/s on modern systems)
- **Setup cost**: ~10-50 μs (shm_open + ftruncate + mmap)
- **Per-access cost**: Zero kernel involvement (no syscall)

---

## How It Works Internally

### Setup: shm_open → ftruncate → mmap

```
  shm_open("/my_shm", O_CREAT | O_RDWR, 0644)
  ┌──────────────────────────────────────┐
  │ 1. Create file in tmpfs /dev/shm/   │
  │ 2. Return fd (just like open())     │
  │ 3. File has size 0 initially        │
  └──────────────────────────────────────┘
           │
           ▼
  ftruncate(fd, sizeof(struct shared_data))
  ┌──────────────────────────────────────┐
  │ 1. Set file size to N bytes          │
  │ 2. Kernel may preallocate pages or  │
  │    defer until first access         │
  └──────────────────────────────────────┘
           │
           ▼
  mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)
  ┌──────────────────────────────────────┐
  │ 1. Allocate a VMA in process mm     │
  │ 2. Set VMA's vm_file → tmpfs inode  │
  │ 3. NO pages mapped yet (lazy)       │
  │ 4. Return virtual address pointer   │
  └──────────────────────────────────────┘
           │
           ▼
  First access: *(int *)ptr = 42;
  ┌──────────────────────────────────────┐
  │ 1. CPU walks page table — no PTE!   │
  │ 2. Page fault → kernel              │
  │ 3. Kernel allocates physical page   │
  │ 4. Installs PTE: vaddr → phys page  │
  │ 5. CPU retries the store            │
  └──────────────────────────────────────┘
```

### Two Processes Sharing the Same Physical Pages

```
  Process A                    Kernel                    Process B
  mm_struct A                                            mm_struct B
  ┌────────────┐                                        ┌────────────┐
  │ VMA:       │                                        │ VMA:       │
  │  start=0x7f│                                        │  start=0x7a│
  │  end=0x80  │                                        │  end=0x7b  │
  │  vm_file──┼──────▶ tmpfs inode ◀─────────────────────┼──vm_file  │
  └────────────┘       ┌─────────┐                      └────────────┘
                       │ pages:  │
  Page Table A         │ [0]─────┼──▶ Physical Page 0x1234
  ┌──────────┐         │ [1]─────┼──▶ Physical Page 0x5678
  │ 0x7f ─── │────────▶│         │◀────────── ┌──────────┐
  │ 0x80 ─── │────────▶│         │◀────────── │ 0x7a ─── │ Page Table B
  └──────────┘         └─────────┘             │ 0x7b ─── │
                                               └──────────┘

  BOTH page tables point to the SAME physical pages.
  Store by A at virtual 0x7f000010 modifies physical 0x1234010.
  Load  by B at virtual 0x7a000010 reads   physical 0x1234010.
  Same data.  Zero copies.
```

### Memory Ordering and Cache Coherency

On modern multi-core systems:

```
  Core 0 (Process A)               Core 1 (Process B)
  ┌──────────┐                     ┌──────────┐
  │ L1 Cache │                     │ L1 Cache │
  │ ┌──────┐ │                     │ ┌──────┐ │
  │ │line X│ │                     │ │line X│ │
  │ └──────┘ │                     │ └──────┘ │
  └────┬─────┘                     └────┬─────┘
       │     ┌─────────────────┐        │
       └─────┤ L2/L3 Cache    ├────────┘
             │ (shared)        │
             └────────┬────────┘
                      │
             ┌────────▼────────┐
             │ Physical RAM    │
             └─────────────────┘
```

**Hardware coherency (x86 MESI protocol):**
When Core 0 writes to a cache line, MESI invalidates Core 1's copy.
Core 1's next read fetches the updated line.  This is automatic.

**Software ordering problem:**
The CPU and compiler may reorder loads and stores for performance.
On x86, stores are ordered (Total Store Order), but loads can be
reordered with earlier stores.  On ARM/RISC-V, almost anything can
be reordered.

**Solution:** Use `atomic_store_explicit` / `atomic_load_explicit` with
appropriate memory orders, or `__sync_synchronize()` memory barriers.

In our example, we use `memory_order_release` on the writer and
`memory_order_acquire` on the reader to ensure the data is visible
before the index update.

### MAP_SHARED vs MAP_PRIVATE

| Flag | Behavior | Use Case |
|------|----------|----------|
| `MAP_SHARED` | All processes see each other's writes. Changes propagate to the underlying file (if file-backed). | IPC, shared state |
| `MAP_PRIVATE` | Copy-on-write. Each process gets a private copy on first write. No sharing after that. | Loading shared libraries, fork() |

### What Happens on fork/exec/crash

| Event | MAP_SHARED | MAP_PRIVATE |
|-------|-----------|-------------|
| `fork()` | Child shares the same pages. Writes by either are visible to both. | Child initially shares pages (CoW). First write triggers a copy — independent after that. |
| `exec()` | All mappings destroyed. Must re-setup. | Same — all gone. |
| Crash | Mapping destroyed. Physical pages remain (referenced by tmpfs file). Other process can still access data — but it may be in an inconsistent state. | Private pages freed. |

---

## Key APIs

### POSIX Shared Memory

```c
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Create / open shared memory object */
int fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0644);
int fd = shm_open("/my_shm", O_RDWR, 0);  /* open existing */

/* Set size (MUST do before mmap on new objects) */
ftruncate(fd, size);

/* Map into address space */
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);

/* Close fd (safe after mmap) */
close(fd);

/* Use the memory (zero-copy!) */
struct my_data *data = (struct my_data *)ptr;
data->field = 42;

/* Unmap when done */
munmap(ptr, size);

/* Remove the shared memory object */
shm_unlink("/my_shm");
```

### Anonymous Shared Memory (no name, for related processes)

```c
/* For parent-child sharing after fork() */
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
/* fd=-1, no file backing.  Shared between parent and child after fork. */
```

### mmap Flags

| Flag | Meaning |
|------|---------|
| `MAP_SHARED` | Changes visible to other mappers |
| `MAP_PRIVATE` | Copy-on-write private mapping |
| `MAP_ANONYMOUS` | No file backing (fd=-1) |
| `MAP_FIXED` | Map at exact address (dangerous) |
| `MAP_POPULATE` | Pre-fault all pages (avoid later page faults) |
| `MAP_HUGETLB` | Use huge pages (2MB/1GB) for large mappings |

### Memory Synchronization

```c
/* Flush changes to backing store (for file-backed mappings) */
msync(ptr, size, MS_SYNC);   /* synchronous */
msync(ptr, size, MS_ASYNC);  /* asynchronous */

/* For tmpfs (/dev/shm), msync is effectively a no-op
   since tmpfs is RAM-only. */
```

### Compile Flags

```bash
gcc -o program program.c -lrt -lpthread
# -lrt for shm_open/shm_unlink
# -lpthread for semaphores (if used for synchronization)
```

---

## ASCII Diagram

### Ring Buffer in Shared Memory

```
  Shared Memory Region (struct shm_ring):
  ┌──────────────────────────────────────────────────────┐
  │ write_idx: 7  (atomic)                               │
  │ read_idx:  5  (atomic)                               │
  │ done:      0                                          │
  │                                                       │
  │ slots[0]: id=4 "Item-4 [PID 1001]"   ← consumed     │
  │ slots[1]: id=5 "Item-5 [PID 1001]"   ← ready        │◀── read_idx%4=1
  │ slots[2]: id=6 "Item-6 [PID 1001]"   ← ready        │
  │ slots[3]: id=7 "Item-7 [PID 1001]"   ← just written │◀── write_idx%4=3
  │                                                       │
  └──────────────────────────────────────────────────────┘

  Semaphores for bounded buffer:
  ┌──────────────────────┐    ┌──────────────────────┐
  │ SEM_EMPTY: 1         │    │ SEM_FULL:  2         │
  │ (1 slot available    │    │ (2 items ready       │
  │  for writing)        │    │  for reading)        │
  └──────────────────────┘    └──────────────────────┘

  Producer                            Consumer
  ┌──────────┐                       ┌──────────┐
  │ sem_wait │ SEM_EMPTY (1→0)       │ sem_wait │ SEM_FULL (2→1)
  │ write    │ slots[write_idx%4]    │ read     │ slots[read_idx%4]
  │ write_idx│ 7→8                   │ read_idx │ 5→6
  │ sem_post │ SEM_FULL  (2→3)       │ sem_post │ SEM_EMPTY (1→2)
  └──────────┘                       └──────────┘
```

### Performance Comparison with Other IPC

```
  Operation               Copies    Syscalls    Approx Latency
  ─────────────────────── ──────── ────────── ──────────────
  Shared Memory (read)    0         0           50-100 ns
  Pipe / Socket           2         2           1-5 μs
  Message Queue           2         2           2-5 μs
  File I/O (cached)       2         2           1-10 μs
  File I/O (disk)         2         2+          1-10 ms
```

---

## Complete Working Example

### core_process.c (Producer)

```c
/* See: docs/ipc/examples/05_sharedmem/core_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdatomic.h>

#define SHM_NAME    "/ipc_demo_shm"
#define SEM_EMPTY   "/ipc_shm_empty"
#define SEM_FULL    "/ipc_shm_full"
#define RING_SIZE   4
#define TOTAL_ITEMS 12

struct shm_ring {
    _Atomic int write_idx;
    _Atomic int read_idx;
    int         done;
    struct {
        int  id;
        char data[60];
    } slots[RING_SIZE];
};

int
main(void)
{
    int shm_fd;
    struct shm_ring *ring;
    sem_t *sem_empty, *sem_full;
    int i;

    /* Cleanup from previous runs */
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);

    /* Create shared memory object */
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0644);
    if(shm_fd == -1) { perror("shm_open"); exit(1); }

    /* Set size */
    ftruncate(shm_fd, sizeof(struct shm_ring));

    /* Map into our address space */
    ring = mmap(NULL, sizeof(struct shm_ring),
                PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(ring == MAP_FAILED) { perror("mmap"); exit(1); }
    close(shm_fd);

    /* Initialize shared state */
    atomic_store(&ring->write_idx, 0);
    atomic_store(&ring->read_idx, 0);
    ring->done = 0;

    /* Create bounded-buffer semaphores */
    sem_empty = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0644, RING_SIZE);
    sem_full  = sem_open(SEM_FULL,  O_CREAT | O_EXCL, 0644, 0);
    if(sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
        perror("sem_open"); exit(1);
    }

    printf("[producer] PID=%d, ring=%d slots, items=%d\n",
           getpid(), RING_SIZE, TOTAL_ITEMS);

    for(i = 0; i < TOTAL_ITEMS; i++) {
        sem_wait(sem_empty);  /* wait for empty slot */

        int idx = atomic_load(&ring->write_idx) % RING_SIZE;
        ring->slots[idx].id = i;
        snprintf(ring->slots[idx].data, sizeof(ring->slots[idx].data),
                 "Item-%d [PID %d]", i, getpid());

        /* Release fence: data visible before index update */
        atomic_store_explicit(&ring->write_idx,
                              atomic_load(&ring->write_idx) + 1,
                              memory_order_release);

        printf("[producer] wrote slot[%d]: id=%d\n", idx, i);

        sem_post(sem_full);   /* signal: slot has data */
        usleep(80000);
    }

    ring->done = 1;
    sem_post(sem_full);  /* wake consumer for termination check */

    sem_close(sem_empty);
    sem_close(sem_full);
    munmap(ring, sizeof(struct shm_ring));

    printf("[producer] done\n");
    return 0;
}
```

### noncore_process.c (Consumer)

```c
/* See: docs/ipc/examples/05_sharedmem/noncore_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <errno.h>

#define SHM_NAME    "/ipc_demo_shm"
#define SEM_EMPTY   "/ipc_shm_empty"
#define SEM_FULL    "/ipc_shm_full"
#define RING_SIZE   4
#define TOTAL_ITEMS 12

struct shm_ring {
    _Atomic int write_idx;
    _Atomic int read_idx;
    int         done;
    struct {
        int  id;
        char data[60];
    } slots[RING_SIZE];
};

int
main(void)
{
    int shm_fd;
    struct shm_ring *ring;
    sem_t *sem_empty, *sem_full;
    int count = 0;

    printf("[consumer] PID=%d, waiting for shared memory...\n", getpid());

    /* Wait for producer to create the shared memory */
    for(;;) {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
        if(shm_fd != -1) break;
        if(errno != ENOENT) { perror("shm_open"); exit(1); }
        usleep(100000);
    }

    /* Map the SAME physical pages */
    ring = mmap(NULL, sizeof(struct shm_ring),
                PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(ring == MAP_FAILED) { perror("mmap"); exit(1); }
    close(shm_fd);

    /* Open existing semaphores */
    for(;;) {
        sem_empty = sem_open(SEM_EMPTY, 0);
        sem_full  = sem_open(SEM_FULL, 0);
        if(sem_empty != SEM_FAILED && sem_full != SEM_FAILED) break;
        usleep(50000);
    }

    printf("[consumer] attached, reading...\n");

    for(;;) {
        sem_wait(sem_full);  /* wait for data */

        if(ring->done &&
           atomic_load(&ring->read_idx) >= atomic_load(&ring->write_idx))
            break;

        int idx = atomic_load(&ring->read_idx) % RING_SIZE;

        printf("[consumer] read slot[%d]: id=%d data=\"%s\"\n",
               idx, ring->slots[idx].id, ring->slots[idx].data);

        atomic_store_explicit(&ring->read_idx,
                              atomic_load(&ring->read_idx) + 1,
                              memory_order_release);
        count++;

        sem_post(sem_empty);  /* signal: slot is free */
        usleep(120000);
    }

    printf("[consumer] consumed %d items\n", count);

    /* Full cleanup */
    munmap(ring, sizeof(struct shm_ring));
    shm_unlink(SHM_NAME);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);

    printf("[consumer] cleaned up\n");
    return 0;
}
```

---

## Execution Instructions

### Compile

```bash
cd docs/ipc/examples/05_sharedmem
make
# Manual: gcc -Wall -Wextra -o core_process core_process.c -lrt -lpthread
#         gcc -Wall -Wextra -o noncore_process noncore_process.c -lrt -lpthread
```

### Run

**Terminal 1:**
```bash
./core_process
```

**Terminal 2:**
```bash
./noncore_process
```

### Expected Output

**Terminal 1 (producer):**
```
[producer] PID=55001, shared memory + semaphores created
[producer] ring buffer: 4 slots, producing 12 items
[producer] wrote slot[0]: id=0
[producer] wrote slot[1]: id=1
[producer] wrote slot[2]: id=2
[producer] wrote slot[3]: id=3
[producer] wrote slot[0]: id=4    ← wraps around!
[producer] wrote slot[1]: id=5
...
[producer] wrote slot[3]: id=11
[producer] done
```

**Terminal 2 (consumer):**
```
[consumer] PID=55002, waiting for shared memory...
[consumer] attached to shared memory and semaphores
[consumer] read slot[0]: id=0 data="Item-0 [PID 55001]"
[consumer] read slot[1]: id=1 data="Item-1 [PID 55001]"
...
[consumer] read slot[3]: id=11 data="Item-11 [PID 55001]"
[consumer] consumed 12 items total
[consumer] cleaned up all IPC resources
```

### Verify

```bash
# While running, check shared memory:
ls -la /dev/shm/ipc_demo_shm
ls -la /dev/shm/sem.ipc_shm_*

# Check size:
stat /dev/shm/ipc_demo_shm

# After cleanup:
ls /dev/shm/ipc_demo_shm 2>&1
# No such file or directory

# Check for memory leaks with valgrind:
valgrind ./core_process &
valgrind ./noncore_process
```
