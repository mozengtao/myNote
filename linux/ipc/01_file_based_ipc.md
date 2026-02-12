# 1. File-Based IPC

## Mental Model

### What Problem This Solves

Files are the most primitive IPC mechanism: two processes communicate by reading
and writing bytes to the same file on disk (or in a RAM-backed filesystem).

Every process on a UNIX system already knows how to work with files — `open`,
`read`, `write`, `close` — so file-based IPC requires zero new abstractions.
The cost is simplicity of coordination: files have no built-in notification or
synchronization, so the communicating processes must layer that on themselves.

### When To Use

- Exchanging bulk data that also needs persistence (log shipping, config reload)
- Simple one-shot data hand-off between unrelated processes
- When no other IPC mechanism is available (e.g., restrictive containers)
- Prototyping before switching to a faster channel

### When NOT To Use

- High-frequency, low-latency exchanges (use shared memory or sockets)
- Real-time streaming (use pipes or sockets)
- When you need notification on arrival (use message queues or sockets)

### Communication Pattern

```
  Process A (writer)              Process B (reader)
  ┌─────────────┐                ┌─────────────┐
  │write(fd,..) │───────────────▶│ read(fd,..) │
  └──────┬──────┘    FILE        └──────┬──────┘
         │       ┌─────────┐            │
         └──────▶│ /tmp/dat │◀───────────┘
                 │  (inode) │
                 └────┬────┘
                      │
                 ┌────▼────┐
                 │ Page    │
                 │ Cache   │
                 └────┬────┘
                      │
                 ┌────▼────┐
                 │ Disk /  │
                 │ tmpfs   │
                 └─────────┘
```

- **Direction**: Unidirectional or bidirectional (with locking)
- **Cardinality**: 1→1, many→1, or 1→many (all share the same file)

### Kernel Objects Involved

| Object | Role |
|--------|------|
| `struct inode` | Represents the file on the filesystem; holds metadata + page tree |
| `struct file` | Per-open-fd state: current offset (`f_pos`), flags, pointer to inode |
| `struct file_lock` | Advisory lock record (linked list off the inode) |
| Page cache | Kernel buffer of file pages in RAM — writes go here first |

### Blocking Behavior

- `write()` to a regular file almost never blocks (data goes to page cache)
- `read()` blocks only on I/O if the page is not cached
- Advisory locks (`fcntl F_SETLKW`) block if a conflicting lock is held
- There is no built-in "wait for new data" — the reader must poll or use
  an external notification (inotify, semaphore, signal)

### Lifetime Rules

- The file persists on disk until `unlink()` removes the directory entry
- An open fd keeps the inode alive even after unlink (reference counting)
- Advisory locks are released when the process closes ANY fd to that inode
  (not just the locked fd — this is a classic footgun with `fcntl` locks)

### Performance Characteristics

- **Copy count**: 2 copies minimum (userspace → page cache → userspace)
- **Latency**: Microseconds if page is cached; milliseconds if disk I/O needed
- **Throughput**: Limited by disk I/O or memory bandwidth
- **Overhead**: Context switch into kernel per read/write syscall

---

## How It Works Internally

### Write Path

```
 userspace buffer
      │
      ▼  write(fd, buf, n)
 ┌─────────────────┐
 │VFS: vfs_write() │
 └────────┬────────┘
          │ copies bytes from user buffer
          ▼
 ┌─────────────────┐
 │ Page Cache      │  (struct address_space)
 │ marks page dirty│
 └────────┬────────┘
          │ (later, asynchronously)
          ▼
 ┌─────────────────┐
 │ Block Layer     │  writeback flushes dirty pages
 │ → Disk / tmpfs  │
 └─────────────────┘
```

1. `write()` traps into kernel via syscall
2. VFS resolves the `fd` to `struct file`, then to `struct inode`
3. The filesystem's `write_iter` operation copies data from userspace into the
   page cache (file-backed pages in the `address_space`)
4. The page is marked dirty; `fsync()` forces immediate writeback
5. `write()` returns; `f_pos` advances

### Read Path

1. `read()` traps into kernel
2. VFS finds the page in the page cache (by page index = offset / PAGE_SIZE)
3. If cached: `copy_to_user()` from page cache → userspace buffer
4. If NOT cached: triggers a page fault / readahead — blocks until I/O completes

### Advisory Locking (fcntl)

```
  Process A                  Kernel (per-inode lock list)         Process B
  ─────────                  ───────────────────────────          ─────────
  fcntl(F_SETLKW, WRLCK) ──▶ Insert lock record ─────────────▶  (no conflict)
  ... writing ...             [lock held: A, WRLCK, 0..EOF]
                                                                  fcntl(F_SETLKW, RDLCK)
                                                                  ──▶ Conflict! Sleep...
  fcntl(F_UNLCK) ───────────▶ Remove lock record ──▶ Wake B
                                                                  ◀── Lock granted, returns
                                                                  ... reading ...
```

The kernel maintains a linked list of `struct file_lock` on each inode.
When a process requests a lock:

1. Scan the list for conflicts (WRLCK conflicts with everything; RDLCK conflicts
   with WRLCK)
2. If conflict: add the caller to a wait queue (TASK_INTERRUPTIBLE)
3. When the conflicting lock is released, wake one waiter

**Important**: `fcntl()` locks are per-process, per-inode — NOT per-fd.
Closing *any* fd that points to the same inode releases *all* locks that
process holds on that inode.  This means `dup()`, `fork()`, or opening the
same file twice can cause surprising lock releases.

### fork/exec Behavior

| Event | What Happens |
|-------|-------------|
| `fork()` | Child gets copies of all fds (refcount incremented). Advisory locks are NOT inherited by the child — the child is a different process. |
| `exec()` | Fds without `O_CLOEXEC` survive. Locks survive because the process identity doesn't change. |
| Crash | All fds are closed → all advisory locks released. File data persists on disk. |

---

## Key APIs

### Creation and Opening

```c
int fd = open(path, flags, mode);
```

| Flag | Meaning |
|------|---------|
| `O_CREAT` | Create file if it doesn't exist |
| `O_EXCL` | With O_CREAT, fail if file exists (atomic create) |
| `O_TRUNC` | Truncate to zero length on open |
| `O_RDONLY` / `O_WRONLY` / `O_RDWR` | Access mode |
| `O_APPEND` | Writes always go to end of file (atomic on local fs) |
| `O_CLOEXEC` | Close fd on exec (prevents leaking fds to child programs) |

### Reading and Writing

```c
ssize_t n = read(fd, buf, count);    /* returns bytes read, 0=EOF, -1=error */
ssize_t n = write(fd, buf, count);   /* returns bytes written, -1=error */
off_t pos = lseek(fd, offset, whence); /* reposition f_pos */
int r = fsync(fd);                   /* flush to disk */
```

### Advisory Locking (fcntl)

```c
struct flock fl = {
    .l_type   = F_WRLCK,   /* F_RDLCK, F_WRLCK, F_UNLCK */
    .l_whence = SEEK_SET,
    .l_start  = 0,
    .l_len    = 0,         /* 0 = entire file */
};
fcntl(fd, F_SETLKW, &fl);  /* blocking */
fcntl(fd, F_SETLK, &fl);   /* non-blocking (returns -1/EAGAIN on conflict) */
fcntl(fd, F_GETLK, &fl);   /* query: who holds the lock? */
```

### Advisory Locking (flock — simpler but coarser)

```c
flock(fd, LOCK_EX);          /* exclusive lock (blocking) */
flock(fd, LOCK_SH);          /* shared lock (blocking) */
flock(fd, LOCK_EX | LOCK_NB); /* non-blocking */
flock(fd, LOCK_UN);          /* unlock */
```

`flock()` locks are per-fd (not per-process like `fcntl`), so `dup()`
shares the lock.  Simpler semantics but no byte-range locking.

### Cleanup

```c
close(fd);         /* release fd; if last fd to inode, release locks */
unlink(path);      /* remove directory entry; inode freed when refcount=0 */
```

### Error Handling Patterns

- Always check return values of `open`, `read`, `write`, `fcntl`
- `read()` returning 0 means EOF, not error
- `write()` may write fewer bytes than requested — loop on short writes
- `EINTR` — syscall interrupted by signal; retry

---

## ASCII Diagram

```
 ┌──────────────────────────────────────────────────────────┐
 │                    KERNEL SPACE                          │
 │                                                          │
 │  struct file (A)        struct inode                     │
 │  ┌──────────┐          ┌──────────────┐                  │
 │  │ f_pos=0  │─────────▶│ i_mode       │                  │
 │  │ f_flags  │          │ i_size       │                  │
 │  │ f_op     │          │ i_flock ─────┼──▶ lock list     │
 │  └──────────┘          │ i_mapping ───┼──▶ page cache    │
 │                        └──────────────┘                  │
 │  struct file (B)             ▲                           │
 │  ┌──────────┐                │                           │
 │  │ f_pos=0  │────────────────┘                           │
 │  │ f_flags  │    (same inode, different file objects)    │
 │  └──────────┘                                            │
 │                                                          │
 └──────────────────────────────────────────────────────────┘
 ┌─────────────┐                          ┌─────────────┐
 │ Process A   │                          │ Process B   │
 │ fd=3 ───────┼── points to file (A)     │ fd=4 ───────┼── points to file (B)
 │ (writer)    │                          │ (reader)    │
 └─────────────┘                          └─────────────┘
```

---

## Complete Working Example

### core_process.c

```c
/* See: docs/ipc/examples/01_file/core_process.c */

/*
 * core_process.c — File-Based IPC: Writer with Advisory Locking
 *
 * Demonstrates:
 *   - Writing structured data to a regular file
 *   - Using fcntl() advisory write locks to prevent races
 *   - Coordinating with a reader process via a ready-flag file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define DATA_FILE   "/tmp/ipc_data.bin"
#define READY_FILE  "/tmp/ipc_ready"
#define MSG_COUNT   5

struct ipc_msg {
    int   seq;
    pid_t sender;
    char  payload[64];
};

static int
acquire_write_lock(int fd)
{
    struct flock fl;

    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;       /* entire file */

    if(fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl F_SETLKW");
        return -1;
    }
    return 0;
}

static int
release_lock(int fd)
{
    struct flock fl;

    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    if(fcntl(fd, F_SETLK, &fl) == -1) {
        perror("fcntl F_UNLCK");
        return -1;
    }
    return 0;
}

int
main(void)
{
    int fd;
    struct ipc_msg msg;
    int i;

    unlink(DATA_FILE);
    unlink(READY_FILE);

    fd = open(DATA_FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if(fd == -1) {
        perror("open data file");
        exit(EXIT_FAILURE);
    }

    printf("[core] PID=%d writing %d messages to %s\n",
           getpid(), MSG_COUNT, DATA_FILE);

    for(i = 0; i < MSG_COUNT; i++) {
        if(acquire_write_lock(fd) == -1) {
            close(fd);
            exit(EXIT_FAILURE);
        }

        memset(&msg, 0, sizeof(msg));
        msg.seq    = i;
        msg.sender = getpid();
        snprintf(msg.payload, sizeof(msg.payload),
                 "Message #%d from core", i);

        if(write(fd, &msg, sizeof(msg)) != sizeof(msg)) {
            perror("write");
            release_lock(fd);
            close(fd);
            exit(EXIT_FAILURE);
        }

        fsync(fd);
        printf("[core] wrote seq=%d\n", msg.seq);

        if(release_lock(fd) == -1) {
            close(fd);
            exit(EXIT_FAILURE);
        }

        usleep(200000);
    }

    /* Signal completion with a flag file */
    fd = open(READY_FILE, O_CREAT | O_WRONLY, 0644);
    if(fd == -1) {
        perror("open ready file");
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("[core] all messages written, ready file created\n");
    return 0;
}
```

### noncore_process.c

```c
/* See: docs/ipc/examples/01_file/noncore_process.c */

/*
 * noncore_process.c — File-Based IPC: Reader with Advisory Locking
 *
 * Demonstrates:
 *   - Polling for file existence (coordination)
 *   - Reading structured data written by another process
 *   - Using fcntl() advisory read (shared) locks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define DATA_FILE   "/tmp/ipc_data.bin"
#define READY_FILE  "/tmp/ipc_ready"

struct ipc_msg {
    int   seq;
    pid_t sender;
    char  payload[64];
};

static int
acquire_read_lock(int fd)
{
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    if(fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl F_RDLCK");
        return -1;
    }
    return 0;
}

static int
release_lock(int fd)
{
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    if(fcntl(fd, F_SETLK, &fl) == -1) {
        perror("fcntl F_UNLCK");
        return -1;
    }
    return 0;
}

static void
wait_for_ready(void)
{
    printf("[noncore] waiting for ready file...\n");
    while(access(READY_FILE, F_OK) == -1)
        usleep(100000);
    printf("[noncore] ready file found\n");
}

int
main(void)
{
    int fd;
    struct ipc_msg msg;
    ssize_t n;
    int count = 0;

    wait_for_ready();

    fd = open(DATA_FILE, O_RDONLY);
    if(fd == -1) {
        perror("open data file");
        exit(EXIT_FAILURE);
    }

    printf("[noncore] PID=%d reading from %s\n", getpid(), DATA_FILE);

    for(;;) {
        if(acquire_read_lock(fd) == -1) {
            close(fd);
            exit(EXIT_FAILURE);
        }

        n = read(fd, &msg, sizeof(msg));

        if(release_lock(fd) == -1) {
            close(fd);
            exit(EXIT_FAILURE);
        }

        if(n == 0)
            break;

        if(n != sizeof(msg)) {
            fprintf(stderr, "[noncore] partial read: %zd bytes\n", n);
            break;
        }

        printf("[noncore] read seq=%d sender=%d payload=\"%s\"\n",
               msg.seq, msg.sender, msg.payload);
        count++;
    }

    close(fd);
    unlink(DATA_FILE);
    unlink(READY_FILE);

    printf("[noncore] total messages read: %d\n", count);
    return 0;
}
```

---

## Execution Instructions

### Compile

```bash
cd docs/ipc/examples/01_file
make
# or manually:
# gcc -Wall -Wextra -o core_process core_process.c
# gcc -Wall -Wextra -o noncore_process noncore_process.c
```

### Run (two terminals)

**Terminal 1:**
```bash
./core_process
```

**Terminal 2** (after core_process finishes or while it runs):
```bash
./noncore_process
```

### Expected Output

**Terminal 1:**
```
[core] PID=12345 writing 5 messages to /tmp/ipc_data.bin
[core] wrote seq=0
[core] wrote seq=1
[core] wrote seq=2
[core] wrote seq=3
[core] wrote seq=4
[core] all messages written, ready file created
[core] done
```

**Terminal 2:**
```
[noncore] waiting for ready file...
[noncore] ready file found
[noncore] PID=12346 reading from /tmp/ipc_data.bin
[noncore] read seq=0 sender=12345 payload="Message #0 from core"
[noncore] read seq=1 sender=12345 payload="Message #1 from core"
[noncore] read seq=2 sender=12345 payload="Message #2 from core"
[noncore] read seq=3 sender=12345 payload="Message #3 from core"
[noncore] read seq=4 sender=12345 payload="Message #4 from core"
[noncore] total messages read: 5
[noncore] cleaned up temp files
```

### Verify Correctness

```bash
# While core_process is running, check the file exists:
ls -la /tmp/ipc_data.bin

# Check lock status (from a third terminal):
lslocks | grep ipc_data

# After both finish, verify cleanup:
ls /tmp/ipc_data.bin /tmp/ipc_ready 2>&1
# Should report "No such file or directory"
```
