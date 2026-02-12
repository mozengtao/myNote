# 6. Cross-Mechanism Comparison

## Comparison Table

```
┌───────────────────┬────────┬───────────┬────────────┬────────────┬───────────────────────────────────┐
│ Mechanism         │ Copies │ Speed     │ Complexity │ Direction  │ Best Use Case                     │
├───────────────────┼────────┼───────────┼────────────┼────────────┼───────────────────────────────────┤
│ File-Based IPC    │ 2      │ Slow      │ Low        │ Any        │ Bulk data + persistence           │
│                   │        │ (disk I/O)│            │            │                                   │
├───────────────────┼────────┼───────────┼────────────┼────────────┼───────────────────────────────────┤
│ UNIX Domain       │ 2      │ Fast      │ Medium     │ Bidir     │ Client/server, general-purpose    │
│ Sockets           │        │ (~2 μs)   │            │            │ local IPC                         │
├───────────────────┼────────┼───────────┼────────────┼────────────┼───────────────────────────────────┤
│ POSIX Semaphore   │ 0      │ Very Fast │ Low        │ N/A       │ Synchronization only              │
│                   │        │ (~50 ns)  │            │ (signal)   │ (no data transfer)                │
├───────────────────┼────────┼───────────┼────────────┼────────────┼───────────────────────────────────┤
│ POSIX Message     │ 2      │ Fast      │ Medium     │ Unidir    │ Prioritized async messaging       │
│ Queue             │        │ (~3 μs)   │            │ (per queue)│                                   │
├───────────────────┼────────┼───────────┼────────────┼────────────┼───────────────────────────────────┤
│ Shared Memory     │ 0      │ Fastest   │ High       │ Any        │ High-throughput, low-latency      │
│ (POSIX + mmap)    │        │ (~100 ns) │            │            │ data sharing                      │
└───────────────────┴────────┴───────────┴────────────┴────────────┴───────────────────────────────────┘
```

## Detailed Feature Matrix

```
┌───────────────────┬──────────┬──────────┬───────────┬──────────┬───────────┐
│ Feature           │ File     │ Socket   │ Semaphore │ MsgQueue │ SharedMem │
├───────────────────┼──────────┼──────────┼───────────┼──────────┼───────────┤
│ Data transfer     │ Yes      │ Yes      │ No        │ Yes      │ Yes       │
│ Synchronization   │ Manual   │ Built-in │ Yes       │ Built-in │ Manual    │
│ Message boundary  │ No       │ DGRAM:Yes│ N/A       │ Yes      │ No        │
│                   │          │ STREAM:No│           │          │           │
│ Priority          │ No       │ No       │ N/A       │ Yes      │ No        │
│ Bidirectional     │ Yes*     │ Yes      │ N/A       │ No†      │ Yes       │
│ Persistence       │ Yes      │ No       │ Partial‡  │ Partial‡ │ Partial‡  │
│ Survives crash    │ Data:Yes │ No       │ Name:Yes  │ Msgs:Yes │ Data:Yes  │
│ Zero-copy         │ No       │ No       │ N/A       │ No       │ Yes       │
│ Kernel buffering  │ PageCache│ sk_buff  │ Futex     │ MQ array │ None      │
│ poll/epoll        │ inotify  │ Yes      │ No        │ Yes      │ No        │
│ File descriptors  │ fd       │ fd       │ sem_t*    │ mqd_t(fd)│ fd+ptr    │
│ Namespace         │ FS path  │ FS/Abstr │ /dev/shm  │/dev/mqueue│/dev/shm  │
│ Max data size     │ FS limit │ ~128KB** │ N/A       │ ~8KB def │ RAM       │
│ Setup overhead    │ Low      │ Medium   │ Low       │ Medium   │ Medium    │
└───────────────────┴──────────┴──────────┴───────────┴──────────┴───────────┘

*  Bidirectional with locking
†  Use two queues for bidirectional
‡  Persists in tmpfs until unlink (lost on reboot)
** Per-message; total limited by kernel buffer memory
```

---

## When To Use Each

### File-Based IPC

**Best for:**
- Configuration reload (write config, signal process to re-read)
- Log aggregation (multiple writers append to shared log)
- Data export/import between unrelated systems
- Simple one-shot data hand-off

**Real-world examples:**
- `/var/run/service.pid` — PID files for service management
- `/tmp/` exchange files between shell scripts
- SQLite databases used by multiple processes (with WAL mode)
- Systemd drop-in configuration directories

**Choose when:** You need persistence, simplicity, and can tolerate latency.

---

### UNIX Domain Sockets

**Best for:**
- Client/server architectures on the same host
- Request/response protocols
- When you need credential verification (SO_PEERCRED)
- When you need to pass file descriptors (SCM_RIGHTS)
- Drop-in replacement for TCP when going local

**Real-world examples:**
- Docker daemon (`/var/run/docker.sock`)
- D-Bus (`/var/run/dbus/system_bus_socket`)
- X11 display server (`/tmp/.X11-unix/X0`)
- PostgreSQL local connections
- systemd activation sockets
- Wayland compositor

**Choose when:** You need bidirectional, reliable, general-purpose IPC with
good tooling (netcat, socat, strace can inspect it).

---

### POSIX Semaphores

**Best for:**
- Producer/consumer coordination
- Resource counting (N connections available)
- Process ordering (A must finish before B starts)
- Lightweight inter-process signaling

**Real-world examples:**
- Database connection pools (semaphore counts available connections)
- Print spooler (semaphore counts available printers)
- Barrier synchronization in parallel computation
- Rate limiting (token bucket with semaphore as token count)

**Choose when:** You need synchronization, not data transfer. Often used
WITH shared memory to coordinate access.

---

### POSIX Message Queues

**Best for:**
- Asynchronous task dispatch
- Event notification systems
- Command queues with priority
- Decoupled producer/consumer with different lifetimes

**Real-world examples:**
- Job scheduling systems (high-priority jobs first)
- Sensor data collection (prioritize alarm data)
- Inter-service command channels in embedded systems
- Audit logging (high-prio security events first)

**Choose when:** You need discrete messages with priority ordering and
kernel-managed buffering. Good middle ground between sockets and shared memory.

---

### Shared Memory (POSIX + mmap)

**Best for:**
- High-frequency data sharing (real-time systems)
- Large shared data structures
- Lock-free algorithms between processes
- Performance-critical paths where every microsecond matters

**Real-world examples:**
- `/dev/shm/` used by Chrome for tab isolation
- PulseAudio / PipeWire audio buffers
- Real-time trading systems (market data distribution)
- Video frame sharing between capture and encoder processes
- DPDK (Data Plane Development Kit) packet buffers
- Shared hash tables (memcached-style)

**Choose when:** You need maximum throughput and minimum latency, and you're
willing to handle synchronization yourself.

---

## Decision Flowchart

```
  Need IPC?
  │
  ├── Need persistence on disk?
  │   └── YES ──▶ File-Based IPC
  │
  ├── Need bidirectional request/response?
  │   └── YES ──▶ UNIX Domain Socket
  │
  ├── Need only synchronization (no data)?
  │   └── YES ──▶ POSIX Semaphore
  │
  ├── Need discrete messages with priority?
  │   └── YES ──▶ POSIX Message Queue
  │
  ├── Need maximum speed / zero-copy?
  │   └── YES ──▶ Shared Memory + Semaphore
  │
  └── Not sure?
      └── UNIX Domain Socket (most versatile)
```

---

## Common Pitfalls

### 1. Race Conditions

**File-Based IPC:**
```
  Problem:  Reader sees partial write (torn read).
  Cause:    No locking, or wrong lock type.
  Fix:      Use fcntl(F_WRLCK) for writer, F_RDLCK for reader.
            Or use O_APPEND for atomic appends.

  Problem:  Reader reads stale data from page cache.
  Cause:    Writer didn't fsync(), reader's cache not invalidated.
  Fix:      Writer calls fsync() after write.
            On same-host tmpfs, this is usually not an issue.
```

**Shared Memory:**
```
  Problem:  Consumer reads partially-written struct.
  Cause:    No memory barrier between field writes and index update.
  Fix:      Use atomic_store_explicit(memory_order_release) for index.
            Use atomic_load_explicit(memory_order_acquire) for reader.

  Problem:  Both processes write to same slot simultaneously.
  Cause:    No mutual exclusion.
  Fix:      Use semaphores or a proper mutex in shared memory.
```

### 2. Deadlocks

```
  Problem:  Process A holds sem_X, waits for sem_Y.
            Process B holds sem_Y, waits for sem_X.
  Cause:    Inconsistent lock ordering.
  Fix:      Always acquire semaphores in the same global order.
            Use sem_timedwait() to detect and break deadlocks.

  Problem:  Process A holds a file lock and crashes.
            Process B waits forever (actually: fcntl locks ARE
            released on process exit, so this doesn't happen
            with advisory locks — but DOES happen with robust
            mutexes if not marked PTHREAD_MUTEX_ROBUST).
  Fix:      Use timeouts.  Use process-robust synchronization.
```

### 3. Resource Leaks

```
  Stale socket files:
    /tmp/my.sock exists but no process is listening.
    Fix: unlink() before bind(), or use abstract namespace.

  Stale semaphores:
    /dev/shm/sem.my_sem exists from a crashed process.
    Fix: sem_unlink() at startup, or clean up in signal handler.
    Manual: rm /dev/shm/sem.my_sem

  Stale shared memory:
    /dev/shm/my_shm persists after crash.
    Fix: shm_unlink() in cleanup path and signal handler.
    Manual: rm /dev/shm/my_shm

  Stale message queues:
    /dev/mqueue/my_queue persists after crash.
    Fix: mq_unlink() in cleanup path.
    Manual: rm /dev/mqueue/my_queue

  Leaked file descriptors:
    fd not closed before exec, leaks to child process.
    Fix: Always use O_CLOEXEC / SOCK_CLOEXEC.
```

### 4. Permission Issues

```
  Problem:  mq_open() / sem_open() / shm_open() fails with EACCES.
  Cause:    Object created with restrictive permissions.
  Fix:      Create with 0644 or 0666.  Check umask.

  Problem:  UNIX socket connect() fails with EACCES.
  Cause:    Socket file permissions don't allow access.
  Fix:      chmod the socket file, or put it in an accessible directory.

  Problem:  mq_open() fails with EMFILE or ENOMEM.
  Cause:    Hit RLIMIT_MSGQUEUE or per-user limit.
  Fix:      Increase limits: ulimit -q <bytes>
            Or: /proc/sys/fs/mqueue/msg_max
```

### 5. Size and Limit Surprises

```
  Message Queue:
    Default mq_maxmsg ≈ 10, mq_msgsize ≈ 8192.
    Unprivileged processes can't exceed /proc/sys/fs/mqueue/ limits.
    RLIMIT_MSGQUEUE limits total bytes of queue memory per user.

  UNIX Socket:
    send() blocks or returns EAGAIN if peer's receive buffer is full.
    Buffer size: /proc/sys/net/core/wmem_default (usually 128KB).
    Can increase with setsockopt(SO_SNDBUF).

  Shared Memory:
    Limited by available RAM + swap.
    /dev/shm is typically mounted with size=50% of RAM.
    Check: df -h /dev/shm

  Socket path:
    sun_path is only 108 bytes.  Long paths silently truncate or fail.
    Use abstract namespace or short paths.
```

### 6. Signal Handling

```
  Problem:  sem_wait() / recv() / mq_receive() returns -1 with EINTR.
  Cause:    A signal was delivered while the process was blocked.
  Fix:      Always check for EINTR and retry:

    while(sem_wait(sem) == -1) {
        if(errno == EINTR) continue;
        perror("sem_wait");
        break;
    }
```

### 7. The fcntl Lock Footgun

```
  Process opens /tmp/data.bin twice:
    fd1 = open("/tmp/data.bin", O_RDWR);
    fd2 = open("/tmp/data.bin", O_RDONLY);

  Acquires lock on fd1:
    fcntl(fd1, F_SETLKW, &write_lock);

  Closes fd2:
    close(fd2);    // BUG: this releases the lock on fd1 too!

  Why: fcntl locks are per-process, per-inode.  Closing ANY fd
  to the same inode releases ALL that process's locks on it.

  Fix: Use flock() instead (per-fd locks), or be very careful
  about opening the same file twice.
```

---

## Debugging IPC Issues

### Tools

| Tool | What It Shows |
|------|--------------|
| `strace -e trace=ipc,network,file` | All IPC-related syscalls |
| `strace -e trace=sendmsg,recvmsg` | Socket message traffic |
| `ls -la /dev/shm/` | Shared memory objects and semaphores |
| `ls -la /dev/mqueue/` | Message queues |
| `cat /dev/mqueue/queue_name` | Queue attributes and current state |
| `lslocks` | All advisory file locks system-wide |
| `ss -x` | UNIX domain socket connections |
| `ss -xlp` | ... with process info |
| `lsof -U` | Open UNIX domain sockets by process |
| `ipcs -a` | System V IPC resources (not POSIX, but useful) |
| `lsof /dev/shm/name` | Processes using a shared memory object |
| `/proc/PID/maps` | Memory mappings of a process |
| `/proc/PID/fd/` | Open file descriptors |
| `ltrace -e sem_wait,sem_post` | Trace semaphore operations |
| `perf trace` | Low-overhead syscall tracing |

### Common Debug Patterns

```bash
# See what IPC a process uses:
strace -f -e trace=socket,connect,bind,sendto,recvfrom,semop,shmget,mmap \
  -p <PID>

# Find stale IPC resources:
ls /dev/shm/ /dev/mqueue/ /tmp/*.sock 2>/dev/null

# Check who has a socket open:
ss -xlp | grep my_socket

# Monitor file locks in real time:
watch lslocks

# See shared memory mappings:
grep '/dev/shm' /proc/<PID>/maps
```
