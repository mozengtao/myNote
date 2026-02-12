# 3. POSIX Counting Semaphores

## Mental Model

### What Problem This Solves

A semaphore is a **synchronization primitive** — it does not transfer data.
It solves the problem: "How do I make Process B wait until Process A has
done something?"

A counting semaphore maintains an integer counter.  Two operations exist:

- **wait** (P / down): decrement.  If counter would go negative, block.
- **post** (V / up): increment.  If anyone is blocked, wake one.

This is the classic Dijkstra semaphore.  It generalizes a mutex (which is
a semaphore with max value 1) to allow counting — e.g., "there are N
resources available."

### When To Use

- Producer/consumer coordination (counting available items)
- Resource pool management (N database connections, M buffers)
- Ordering between processes ("don't start until I'm ready")
- When you need inter-process synchronization without data transfer

### When NOT To Use

- If you also need to transfer data (use message queues or sockets)
- If you need mutual exclusion within a single process (use pthread_mutex)
- If you need to wait on multiple conditions (use poll/epoll + eventfd)

### Communication Pattern

```
  Producer                 Semaphore              Consumer
  ┌────────┐              ┌─────────┐            ┌─────────┐
  │produce │              │ counter │            │ consume │
  │  item  │              │         │            │  item   │
  │        │──sem_post()──▶│  0→1→2  │            │         │
  │        │              │         │──sem_wait()─▶│         │
  │        │              │  2→1→0  │            │         │
  │        │              │         │            │ BLOCKED │
  │        │──sem_post()──▶│  0→1    │            │         │
  │        │              │         │──wakes up──▶│ resumes │
  └────────┘              └─────────┘            └─────────┘

  Pattern: 1→1 synchronization (can be many→many with care)
```

### Kernel Objects Involved

| Object | Role |
|--------|------|
| Futex word | The atomic integer counter (in shared memory / tmpfs) |
| Wait queue | Kernel list of tasks blocked in `sem_wait()` |
| `/dev/shm/sem.NAME` | Backing file for named semaphore (tmpfs) |

POSIX named semaphores on Linux are implemented using futexes:

- The counter lives in a memory-mapped file in `/dev/shm/`
- `sem_wait()` atomically decrements; if result < 0, calls `futex(FUTEX_WAIT)`
- `sem_post()` atomically increments; calls `futex(FUTEX_WAKE, 1)` to wake one waiter
- No syscall needed in the uncontested fast path (pure userspace atomic op)

### Blocking Behavior

| Function | Behavior |
|----------|----------|
| `sem_wait()` | Block if counter == 0.  Sleeps in `TASK_INTERRUPTIBLE`. Returns -1 with `EINTR` if a signal arrives. |
| `sem_trywait()` | Non-blocking.  Returns -1/`EAGAIN` if counter == 0. |
| `sem_timedwait()` | Block with absolute timeout (`CLOCK_REALTIME`). Returns -1/`ETIMEDOUT`. |
| `sem_post()` | Never blocks.  Always succeeds (unless SEM_VALUE_MAX overflow). |

### Lifetime Rules

- **Named** (`sem_open`): persists in `/dev/shm/` until `sem_unlink()` removes the name.
  Survives process exit.  Can leak if not properly cleaned up.
- **Unnamed** (`sem_init`): lives in the memory you provide.
  If in shared memory (`pshared=1`), works inter-process.
  If on stack/heap (`pshared=0`), intra-process only.
  Destroyed with `sem_destroy()`.

### Performance Characteristics

- **Uncontested**: ~10-50 ns (pure userspace atomic op, no syscall)
- **Contested**: ~1-5 μs (futex syscall + context switch to wake)
- **No data transfer**: semaphore carries no payload

---

## How It Works Internally

### Futex-Based Implementation

```
  sem_wait(sem):                        sem_post(sem):
  ┌──────────────────┐                  ┌──────────────────┐
  │ atomic_dec(counter)│                  │ atomic_inc(counter)│
  │                    │                  │                    │
  │ if counter >= 0:  │                  │ futex(FUTEX_WAKE, │
  │   return (fast!)  │                  │         addr, 1)  │
  │                    │                  │ (wake one waiter) │
  │ if counter < 0:   │                  └──────────────────┘
  │   futex(FUTEX_WAIT,│
  │         addr, ...)│
  │   (sleep in kernel)│
  │   (woken by post) │
  │   return          │
  └──────────────────┘

  The "fast path" (no contention) never enters the kernel.
  Only when blocking is needed does futex() make a syscall.
```

### Named Semaphore Memory Layout

```
  /dev/shm/sem.my_name
  ┌────────────────────┐
  │ futex word (4 bytes)│ ◀── atomic counter
  │ ... padding ...     │
  │ ... metadata ...    │
  └────────────────────┘
       ▲                    ▲
       │                    │
  mmap'd by Process A  mmap'd by Process B
  (sem_open returns     (sem_open returns
   pointer to this)      pointer to same page)
```

Both processes map the **same physical page** from tmpfs.  The futex word
is at a known offset.  The kernel uses the physical address of the futex
word to identify the wait queue.

### fork/exec Behavior

| Event | Named Semaphore | Unnamed Semaphore |
|-------|----------------|------------------|
| `fork()` | Child inherits mmap'd region. Both can use it. | If in shared memory (pshared=1): shared. If on heap: child gets a COPY — NOT shared. |
| `exec()` | Mappings are destroyed. Must `sem_open()` again. | Lost (all mappings gone). |
| Crash | Semaphore persists in `/dev/shm/`. Counter is whatever it was. Can be stale. | If in shared memory: persists. If on heap: gone. |

---

## Key APIs

### Named Semaphores

```c
#include <semaphore.h>
#include <fcntl.h>

/* Create */
sem_t *sem = sem_open("/my_sem", O_CREAT | O_EXCL, 0644, initial_value);

/* Open existing */
sem_t *sem = sem_open("/my_sem", 0);

/* Wait (decrement) */
sem_wait(sem);            /* blocking */
sem_trywait(sem);         /* non-blocking */
sem_timedwait(sem, &ts);  /* timeout (struct timespec, CLOCK_REALTIME) */

/* Post (increment) */
sem_post(sem);

/* Query current value */
int val;
sem_getvalue(sem, &val);  /* NOTE: value may change before you use it */

/* Close handle */
sem_close(sem);           /* unmap from this process */

/* Remove from system */
sem_unlink("/my_sem");    /* remove name; semaphore freed when all close */
```

### Unnamed Semaphores

```c
sem_t sem;

/* Initialize */
sem_init(&sem, 1, initial_value);  /* pshared=1 for inter-process */
/* &sem must be in shared memory (mmap/shm) for inter-process use */

/* Same wait/post API */
sem_wait(&sem);
sem_post(&sem);

/* Destroy */
sem_destroy(&sem);
```

### Error Handling

| Error | When | How to Handle |
|-------|------|--------------|
| `EINTR` | `sem_wait` interrupted by signal | Retry the `sem_wait` |
| `EAGAIN` | `sem_trywait` when counter == 0 | Try again later or fall back |
| `ETIMEDOUT` | `sem_timedwait` timeout expired | Handle timeout case |
| `EOVERFLOW` | `sem_post` would exceed `SEM_VALUE_MAX` | Bug — too many posts |

---

## ASCII Diagram

### Producer-Consumer with Counting Semaphore

```
  Time ──▶

  Producer:        [produce] [produce] [produce]   [produce]
  sem_post:              ↓        ↓        ↓            ↓
                   ┌─────┴────────┴────────┴────────────┴────┐
  Semaphore:       │  0 → 1 → 2 → 3 → 2 → 1 → 0 . . . → 1 │
                   └─────┬────────┬────────┬────────────┬────┘
  sem_wait:              ↓        ↓        ↓            ↓
  Consumer:                  [consume] [consume] [BLOCK]  [consume]
                                                   ▲
                                                   │
                                            counter=0, sleeps
                                            until next sem_post
```

### Counter States

```
  sem_post(sem):    counter++         sem_wait(sem):    counter--

    ┌───┐                               ┌───┐
    │ 0 │── post() ──▶ ┌───┐            │ 3 │── wait() ──▶ ┌───┐
    └───┘               │ 1 │            └───┘               │ 2 │
                        └───┘                                └───┘

    ┌───┐                               ┌───┐
    │ 0 │── wait() ──▶ BLOCK!           │ 0 │── trywait() ──▶ EAGAIN
    └───┘              (sleep)           └───┘
```

---

## Complete Working Example

### core_process.c (Producer)

```c
/* See: docs/ipc/examples/03_semaphore/core_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>

#define SEM_NAME   "/ipc_demo_sem"
#define DATA_FILE  "/tmp/ipc_sem_data.bin"
#define ITEM_COUNT 8

struct item {
    int  id;
    char description[64];
};

int
main(void)
{
    sem_t *sem;
    int fd;
    struct item it;
    int i;

    sem_unlink(SEM_NAME);
    unlink(DATA_FILE);

    /* Create semaphore with initial value 0 */
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 0);
    if(sem == SEM_FAILED) { perror("sem_open"); exit(1); }

    printf("[producer] PID=%d, semaphore created (initial=0)\n", getpid());

    fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd == -1) { perror("open"); exit(1); }

    for(i = 0; i < ITEM_COUNT; i++) {
        memset(&it, 0, sizeof(it));
        it.id = i;
        snprintf(it.description, sizeof(it.description),
                 "Item-%d produced by PID %d", i, getpid());

        write(fd, &it, sizeof(it));
        fsync(fd);

        printf("[producer] produced item %d\n", i);

        /* Signal: one more item available */
        sem_post(sem);

        usleep(100000 + (rand() % 200000));
    }

    close(fd);
    sem_close(sem);

    printf("[producer] done\n");
    return 0;
}
```

### noncore_process.c (Consumer)

```c
/* See: docs/ipc/examples/03_semaphore/noncore_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>

#define SEM_NAME   "/ipc_demo_sem"
#define DATA_FILE  "/tmp/ipc_sem_data.bin"
#define ITEM_COUNT 8

struct item {
    int  id;
    char description[64];
};

int
main(void)
{
    sem_t *sem;
    int fd;
    struct item it;
    int i;
    ssize_t n;

    printf("[consumer] PID=%d, opening semaphore...\n", getpid());

    /* Wait for producer to create the semaphore */
    for(;;) {
        sem = sem_open(SEM_NAME, 0);
        if(sem != SEM_FAILED) break;
        if(errno != ENOENT) { perror("sem_open"); exit(1); }
        usleep(100000);
    }

    /* Wait for data file */
    while(access(DATA_FILE, F_OK) == -1)
        usleep(50000);

    fd = open(DATA_FILE, O_RDONLY);
    if(fd == -1) { perror("open"); exit(1); }

    for(i = 0; i < ITEM_COUNT; i++) {
        /* Block until an item is available */
        if(sem_wait(sem) == -1) {
            if(errno == EINTR) { i--; continue; }
            perror("sem_wait"); break;
        }

        n = read(fd, &it, sizeof(it));
        if(n != sizeof(it)) {
            fprintf(stderr, "[consumer] short read\n");
            break;
        }

        printf("[consumer] consumed: id=%d desc=\"%s\"\n",
               it.id, it.description);

        usleep(50000 + (rand() % 100000));
    }

    close(fd);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    unlink(DATA_FILE);

    printf("[consumer] done, cleaned up\n");
    return 0;
}
```

---

## Execution Instructions

### Compile

```bash
cd docs/ipc/examples/03_semaphore
make
# Manual: gcc -Wall -Wextra -o core_process core_process.c -lpthread
#         gcc -Wall -Wextra -o noncore_process noncore_process.c -lpthread
```

### Run

You can start both in either order (consumer retries until producer creates the
semaphore), but starting the producer first is cleaner.

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
[producer] PID=33001, semaphore "/ipc_demo_sem" created (initial=0)
[producer] produced item 0
[producer] produced item 1
[producer] produced item 2
...
[producer] produced item 7
[producer] done, produced 8 items
```

**Terminal 2 (consumer):**
```
[consumer] PID=33002, opening semaphore "/ipc_demo_sem"...
[consumer] semaphore opened
[consumer] consumed: id=0 desc="Item-0 produced by PID 33001"
[consumer] consumed: id=1 desc="Item-1 produced by PID 33001"
...
[consumer] consumed: id=7 desc="Item-7 produced by PID 33001"
[consumer] done, cleaned up
```

### Verify

```bash
# While running, check the semaphore:
ls -la /dev/shm/sem.ipc_demo_sem

# Check semaphore value:
cat /dev/shm/sem.ipc_demo_sem | xxd | head

# After cleanup, verify removal:
ls /dev/shm/sem.ipc_demo_sem 2>&1
# No such file or directory
```
