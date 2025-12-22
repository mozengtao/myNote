# Linux Kernel Zero-Copy I/O Patterns (v3.2)

## Overview

This document explains **zero-copy I/O patterns** in Linux kernel v3.2, focusing on buffer ownership and copy avoidance.

---

## Why Copies are Expensive

```
+------------------------------------------------------------------+
|  THE COPY PROBLEM                                                |
+------------------------------------------------------------------+

    TRADITIONAL FILE → SOCKET TRANSFER:
    
    ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
    │    Disk      │────▶│  Page Cache  │────▶│ User Buffer  │
    │              │     │  (kernel)    │     │ (user space) │
    └──────────────┘     └──────────────┘     └──────┬───────┘
                              COPY 1                  │
                                                      │ COPY 2
                                                      ▼
    ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
    │   Network    │◀────│ Socket Buffer│◀────│ User Buffer  │
    │   (wire)     │     │  (kernel)    │     │ (user space) │
    └──────────────┘     └──────────────┘     └──────────────┘
                              COPY 3                 COPY 4

    4 COPIES TOTAL!
    
    COPY COSTS:
    +----------------------------------------------------------+
    | Memory bandwidth:    10-20 GB/s typical                   |
    | CPU cycles:          ~100 ns per 1KB copy                 |
    | Cache pollution:     Pushes useful data out of cache      |
    | Context switches:    2 (read syscall + write syscall)     |
    +----------------------------------------------------------+

    FOR 1 GB TRANSFER:
    +----------------------------------------------------------+
    | Traditional: 4 GB memory bandwidth consumed               |
    | Zero-copy:   1 GB memory bandwidth (disk to NIC DMA)      |
    | Savings:     75% bandwidth, significant CPU reduction     |
    +----------------------------------------------------------+
```

**中文解释：**
- 传统传输：4次复制（磁盘→页缓存→用户→socket→网络）
- 复制成本：内存带宽、CPU 周期、缓存污染、上下文切换
- 1GB 传输：传统消耗 4GB 带宽，零拷贝仅 1GB

---

## sendfile/splice Architecture

```
+------------------------------------------------------------------+
|  SENDFILE: FILE → SOCKET (zero user-space copy)                  |
+------------------------------------------------------------------+

    ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

    Data Flow:
    
    ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
    │    Disk      │────▶│  Page Cache  │────▶│ Socket Buffer│
    │              │     │              │     │              │
    └──────────────┘     └──────────────┘     └──────────────┘
                              │                     │
                              │ DMA read            │ DMA write
                              ▼                     ▼
                         ┌──────────────────────────────────────┐
                         │  With scatter-gather DMA:            │
                         │  Pages directly referenced           │
                         │  NO intermediate copy!               │
                         └──────────────────────────────────────┘

    COMPARISON:
    
    Traditional                          Sendfile
    ────────────                         ────────
    read(fd, buf, n);                    sendfile(sock_fd, file_fd,
    write(sock_fd, buf, n);                       &offset, count);
    
    - 4 copies                           - 2 copies (page cache only)
    - 2 syscalls                         - 1 syscall
    - User buffer needed                 - No user buffer

+------------------------------------------------------------------+
|  SPLICE: PIPE-BASED ZERO-COPY                                    |
+------------------------------------------------------------------+

    ssize_t splice(int fd_in, off_t *off_in,
                   int fd_out, off_t *off_out,
                   size_t len, unsigned int flags);

    Data Flow (using pipe as intermediate):
    
    ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
    │   Source     │────▶│    Pipe      │────▶│ Destination  │
    │   (file/sock)│     │   Buffer     │     │ (sock/file)  │
    └──────────────┘     └──────────────┘     └──────────────┘
                              │
                              │ Page references only
                              │ NO data copy!

    EXAMPLE: File → Socket via splice
    
    int pipefd[2];
    pipe(pipefd);
    
    /* Move from file to pipe (no copy) */
    splice(file_fd, &offset, pipefd[1], NULL, len, SPLICE_F_MOVE);
    
    /* Move from pipe to socket (no copy) */
    splice(pipefd[0], NULL, sock_fd, NULL, len, SPLICE_F_MOVE);
```

**中文解释：**
- sendfile：文件→socket，2次复制（仅页缓存），1次系统调用
- splice：通过管道，只移动页引用，无数据复制
- scatter-gather DMA：直接引用页，无中间复制

---

## Buffer Lifetime and Ownership

```
+------------------------------------------------------------------+
|  BUFFER OWNERSHIP RULES                                          |
+------------------------------------------------------------------+

    PAGE CACHE OWNERSHIP:
    +----------------------------------------------------------+
    | - Page cache "owns" pages during sendfile                 |
    | - Pages are reference-counted                             |
    | - NIC DMA may access pages concurrently                   |
    | - Pages must not be freed until DMA completes             |
    +----------------------------------------------------------+

    OWNERSHIP TRANSFER:
    
    ┌──────────────────────────────────────────────────────────────┐
    │                                                               │
    │  Page Cache                                                   │
    │       │                                                       │
    │       │ get_page() - increment refcount                      │
    │       ▼                                                       │
    │  Socket Buffer                                                │
    │       │                                                       │
    │       │ Reference to page (not copy)                         │
    │       ▼                                                       │
    │  NIC Driver                                                   │
    │       │                                                       │
    │       │ DMA from page address                                │
    │       ▼                                                       │
    │  DMA Complete IRQ                                             │
    │       │                                                       │
    │       │ put_page() - decrement refcount                      │
    │       ▼                                                       │
    │  Page may be freed (if refcount == 0)                        │
    │                                                               │
    └──────────────────────────────────────────────────────────────┘

    INVARIANT:
    +----------------------------------------------------------+
    | Page refcount > 0 while any component uses it             |
    | DMA completion must signal before refcount drops          |
    +----------------------------------------------------------+
```

**中文解释：**
- 页缓存所有权：sendfile 期间页缓存"拥有"页
- 页引用计数：NIC DMA 可并发访问，DMA 完成前不能释放
- 所有权转移：get_page 增加引用 → DMA → put_page 减少引用

---

## Failure Cases

```
+------------------------------------------------------------------+
|  ZERO-COPY FAILURE MODES                                         |
+------------------------------------------------------------------+

    1. COPY-ON-WRITE TRIGGERED
    +----------------------------------------------------------+
    | Scenario: Process writes to mmap'd file during sendfile   |
    |                                                           |
    | Problem:                                                  |
    | - Page is being DMA'd to network                          |
    | - Process writes to page                                  |
    | - Must copy to preserve original for network              |
    |                                                           |
    | Result: Zero-copy falls back to copy                      |
    +----------------------------------------------------------+

    2. PAGE EVICTION RACE
    +----------------------------------------------------------+
    | Scenario: Memory pressure during sendfile                 |
    |                                                           |
    | Problem:                                                  |
    | - Page cache under memory pressure                        |
    | - Wants to evict the page being sent                      |
    | - But DMA is in progress                                  |
    |                                                           |
    | Solution: Page pinned by refcount, can't be evicted       |
    +----------------------------------------------------------+

    3. NETWORK RETRANSMIT
    +----------------------------------------------------------+
    | Scenario: Packet lost, TCP retransmit needed              |
    |                                                           |
    | Problem:                                                  |
    | - Original page may have been modified/freed              |
    | - Need to resend data                                     |
    |                                                           |
    | Solution:                                                 |
    | - Copy to socket buffer for retransmit data               |
    | - Or re-read from page cache                              |
    +----------------------------------------------------------+

    4. UNALIGNED BUFFERS
    +----------------------------------------------------------+
    | Scenario: Data not aligned to page boundary               |
    |                                                           |
    | Problem:                                                  |
    | - DMA requires page-aligned buffers                       |
    | - Partial pages can't be zero-copy'd                      |
    |                                                           |
    | Result: Head and tail may require copies                  |
    +----------------------------------------------------------+
```

**中文解释：**
- 写时复制触发：sendfile 期间写入页，必须复制
- 页驱逐竞争：内存压力时页被引用计数固定
- 网络重传：丢包需要复制数据或重读页缓存
- 非对齐缓冲区：DMA 需要页对齐，部分页需要复制

---

## User-Space Streaming I/O

```c
/* User-space zero-copy inspired patterns */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*=================================================================
 * PATTERN 1: Use sendfile() for file-to-socket
 *================================================================*/
ssize_t send_file_zerocopy(int sock_fd, int file_fd, size_t count)
{
    off_t offset = 0;
    ssize_t sent = 0;
    
    while (count > 0) {
        ssize_t n = sendfile(sock_fd, file_fd, &offset, count);
        if (n <= 0) {
            if (n < 0) perror("sendfile");
            break;
        }
        sent += n;
        count -= n;
    }
    
    return sent;
}

/*=================================================================
 * PATTERN 2: Buffer pooling (avoid allocation in hot path)
 *================================================================*/
#define BUFFER_SIZE 65536
#define POOL_SIZE 16

struct buffer_pool {
    char *buffers[POOL_SIZE];
    int free_bitmap;
    pthread_mutex_t lock;
};

struct buffer_pool *pool_create(void)
{
    struct buffer_pool *pool = malloc(sizeof(*pool));
    pool->free_bitmap = (1 << POOL_SIZE) - 1;  /* All free */
    pthread_mutex_init(&pool->lock, NULL);
    
    for (int i = 0; i < POOL_SIZE; i++) {
        /* Page-aligned for potential DMA */
        posix_memalign((void **)&pool->buffers[i], 4096, BUFFER_SIZE);
    }
    return pool;
}

char *pool_get(struct buffer_pool *pool)
{
    pthread_mutex_lock(&pool->lock);
    
    if (pool->free_bitmap == 0) {
        pthread_mutex_unlock(&pool->lock);
        return NULL;  /* Pool exhausted */
    }
    
    int idx = __builtin_ffs(pool->free_bitmap) - 1;
    pool->free_bitmap &= ~(1 << idx);
    
    pthread_mutex_unlock(&pool->lock);
    return pool->buffers[idx];
}

void pool_put(struct buffer_pool *pool, char *buf)
{
    pthread_mutex_lock(&pool->lock);
    
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->buffers[i] == buf) {
            pool->free_bitmap |= (1 << i);
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->lock);
}

/*=================================================================
 * PATTERN 3: Reference-counted buffers
 *================================================================*/
struct refcounted_buffer {
    int refcount;
    size_t size;
    char data[];  /* Flexible array member */
};

struct refcounted_buffer *buffer_alloc(size_t size)
{
    struct refcounted_buffer *buf = malloc(sizeof(*buf) + size);
    buf->refcount = 1;
    buf->size = size;
    return buf;
}

void buffer_get(struct refcounted_buffer *buf)
{
    __atomic_fetch_add(&buf->refcount, 1, __ATOMIC_RELAXED);
}

void buffer_put(struct refcounted_buffer *buf)
{
    if (__atomic_fetch_sub(&buf->refcount, 1, __ATOMIC_ACQ_REL) == 1) {
        free(buf);
    }
}

/*=================================================================
 * PATTERN 4: Scatter-gather I/O (reduce syscalls)
 *================================================================*/
#include <sys/uio.h>

ssize_t writev_all(int fd, struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;
    
    while (iovcnt > 0) {
        ssize_t n = writev(fd, iov, iovcnt);
        if (n <= 0) {
            if (n < 0) perror("writev");
            break;
        }
        total += n;
        
        /* Adjust iovec for partial writes */
        while (n > 0 && iovcnt > 0) {
            if ((size_t)n >= iov->iov_len) {
                n -= iov->iov_len;
                iov++;
                iovcnt--;
            } else {
                iov->iov_base = (char *)iov->iov_base + n;
                iov->iov_len -= n;
                n = 0;
            }
        }
    }
    
    return total;
}

/* Example: Send multiple buffers in one syscall */
void send_message(int sock_fd, const char *header, 
                  const char *body, size_t body_len)
{
    struct iovec iov[2];
    
    iov[0].iov_base = (void *)header;
    iov[0].iov_len = strlen(header);
    
    iov[1].iov_base = (void *)body;
    iov[1].iov_len = body_len;
    
    writev_all(sock_fd, iov, 2);  /* One syscall for both */
}

/*=================================================================
 * DATA MOVEMENT SUMMARY
 *================================================================*/
/*
    TRADITIONAL:
    
    User → Kernel → Hardware
       copy   copy   DMA
    
    ZERO-COPY (sendfile):
    
    Kernel (page cache) → Hardware
                          DMA only!
    
    BEST PRACTICES:
    1. Use sendfile() for file→socket transfers
    2. Use buffer pools to avoid allocation overhead
    3. Use scatter-gather I/O to reduce syscalls
    4. Use reference counting for shared buffers
    5. Align buffers to page boundaries for DMA
*/
```

**中文解释：**
- 模式1：sendfile 文件到 socket（零拷贝）
- 模式2：缓冲池避免热路径分配
- 模式3：引用计数缓冲区共享
- 模式4：scatter-gather I/O 减少系统调用
- 最佳实践：sendfile、缓冲池、页对齐、引用计数

