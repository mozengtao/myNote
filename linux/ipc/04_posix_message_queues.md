# 4. POSIX Message Queues

## Mental Model

### What Problem This Solves

A message queue is a **kernel-managed ordered buffer** that combines data
transfer with synchronization.  Unlike pipes (raw byte streams), message
queues preserve **message boundaries** and support **priority ordering**.

The sender enqueues discrete messages.  The receiver dequeues them.
The kernel handles buffering, blocking, and ordering — the processes
don't need to be running at the same time.

Think of it as a mailbox: the sender drops in a letter, the receiver
picks it up later.  The kernel is the postal service.

### When To Use

- Asynchronous command/event dispatch (task queues, job scheduling)
- When you need message boundaries without framing a byte stream yourself
- When message priority matters (urgent events processed first)
- When sender and receiver have different lifetimes (decoupled timing)
- When you need kernel-level persistence (messages survive sender exit)

### When NOT To Use

- High-throughput bulk data transfer (use shared memory — mq copies data)
- Streaming data (use pipes or sockets)
- When you need bidirectional communication (mq is unidirectional per queue)
- When messages exceed a few KB (limited by `mq_msgsize` and RLIMIT_MSGQUEUE)

### Communication Pattern

```
  Sender(s)              Kernel Message Queue              Receiver(s)
  ┌────────┐            ┌────────────────────┐            ┌────────┐
  │mq_send │──msg──────▶│ [prio=5] ←─ highest│──msg──────▶│mq_recv │
  │        │            │ [prio=3]           │            │        │
  │mq_send │──msg──────▶│ [prio=1]           │            │        │
  │        │            │ [prio=1] ←─ FIFO   │            │        │
  └────────┘            │ [prio=0] ←─ lowest │            └────────┘
                        └────────────────────┘
                         ▲                  ▲
                         │                  │
                    Messages sorted:     mq_receive always
                    highest prio first,  dequeues the top
                    FIFO within prio     (highest priority)

  Pattern: many→1 (multiple senders, one receiver)
           or many→many (but each message delivered to ONE receiver)
```

### Kernel Objects Involved

| Object | Role |
|--------|------|
| `struct mqueue_inode_info` | Per-queue kernel state: message list, attributes, wait queues |
| Message array | Kernel-allocated array of `mq_maxmsg` slots, each `mq_msgsize` bytes |
| Wait queues | Two: one for blocked senders (queue full), one for blocked receivers (queue empty) |
| `/dev/mqueue/NAME` | Filesystem entry in the mqueue pseudo-filesystem |

### Blocking Behavior

| Situation | Blocking Mode | Non-blocking Mode (`O_NONBLOCK`) |
|-----------|--------------|----------------------------------|
| `mq_receive`, queue empty | Block until message arrives | Return -1, errno=`EAGAIN` |
| `mq_send`, queue full | Block until space available | Return -1, errno=`EAGAIN` |
| `mq_receive`, has messages | Return immediately | Return immediately |
| `mq_send`, has space | Return immediately | Return immediately |

### Lifetime Rules

- The queue persists in `/dev/mqueue/` until `mq_unlink()` removes it
- Messages persist in the queue even if the sender exits
- Unlinked queue is destroyed only when all descriptors are closed
  (same as unlink(2) for regular files)
- Queue survives reboot only if mqueue filesystem is on persistent storage
  (typically tmpfs, so NO — lost on reboot)

### Performance Characteristics

- **Copy count**: 2 copies (user→kernel message slot, kernel→user on receive)
- **Latency**: ~2-5 μs per message (kernel memcpy + possible wake)
- **Throughput**: Good for small messages; not designed for bulk transfer
- **Max message size**: Limited by `mq_msgsize` (default max ~8KB, configurable)
- **Max messages**: Limited by `mq_maxmsg` (default max ~10, configurable)
- **Memory**: `mq_maxmsg * mq_msgsize` pinned kernel memory per queue

---

## How It Works Internally

### Queue Structure in Kernel

```
  struct mqueue_inode_info
  ┌─────────────────────────────────────────────┐
  │ attr.mq_maxmsg  = 10                        │
  │ attr.mq_msgsize = 256                       │
  │ attr.mq_curmsgs = 3   (current count)       │
  │                                              │
  │ Message Array (priority-sorted):             │
  │ ┌─────────────────────────────────────────┐  │
  │ │ [0] prio=5  len=45  "Critical event..." │  │ ◀── mq_receive gets this
  │ │ [1] prio=3  len=22  "Alert: disk 80%"   │  │
  │ │ [2] prio=1  len=30  "Background task"   │  │
  │ │ [3] (empty)                              │  │
  │ │ ...                                      │  │
  │ │ [9] (empty)                              │  │
  │ └─────────────────────────────────────────┘  │
  │                                              │
  │ Sender wait queue:    (tasks blocked on full)│
  │ Receiver wait queue:  (tasks blocked on empty)│
  │                                              │
  │ Notification:  sigevent (optional SIGEV_*)   │
  └─────────────────────────────────────────────┘
```

### Send/Receive Flow

```
  mq_send(mq, "hello", 5, priority=3)
  ┌──────────────────────────────┐
  │ 1. Verify len <= mq_msgsize  │
  │ 2. If queue full:            │
  │    - blocking: sleep on      │
  │      sender wait queue       │
  │    - non-blocking: EAGAIN    │
  │ 3. copy_from_user(msg_slot,  │
  │    user_buf, len)            │
  │ 4. Insert into sorted        │
  │    position by priority      │
  │ 5. mq_curmsgs++              │
  │ 6. Wake one receiver (if any │
  │    sleeping on recv wait q)  │
  └──────────────────────────────┘

  mq_receive(mq, buf, bufsize, &prio)
  ┌──────────────────────────────┐
  │ 1. Verify bufsize >= msgsize │
  │ 2. If queue empty:           │
  │    - blocking: sleep on      │
  │      receiver wait queue     │
  │    - non-blocking: EAGAIN    │
  │ 3. Dequeue highest-priority  │
  │    message (FIFO within prio)│
  │ 4. copy_to_user(user_buf,    │
  │    msg_slot, msg_len)        │
  │ 5. mq_curmsgs--              │
  │ 6. Wake one sender (if any   │
  │    sleeping on send wait q)  │
  │ 7. Return msg_len, set *prio │
  └──────────────────────────────┘
```

### Notification Mechanism

Instead of blocking in `mq_receive()`, you can register for async notification:

```c
struct sigevent sev;
sev.sigev_notify = SIGEV_SIGNAL;  /* or SIGEV_THREAD */
sev.sigev_signo  = SIGUSR1;
mq_notify(mq, &sev);
/* Kernel sends SIGUSR1 when a message arrives in an EMPTY queue */
/* Notification fires ONCE — must re-register after each */
```

This is useful with event loops (epoll + signalfd).

### mqd_t is a File Descriptor

On Linux, `mqd_t` is just an `int` (file descriptor).  This means:

- You can use `select()`, `poll()`, `epoll()` on it
- `POLLIN` = message available for receive
- `POLLOUT` = space available for send
- You can integrate MQ into an event-driven architecture

### fork/exec Behavior

| Event | Behavior |
|-------|----------|
| `fork()` | Child inherits mqd_t descriptors. Both can send/recv. Messages are not duplicated — they're in the kernel queue. |
| `exec()` | mqd_t descriptors are closed (mq_open sets close-on-exec by default on some implementations). Re-open with mq_open if needed. |
| Crash | Descriptors closed. Messages remain in the queue. Queue persists until mq_unlink. |

---

## Key APIs

### Creation and Opening

```c
#include <mqueue.h>

/* Create with attributes */
struct mq_attr attr = {
    .mq_maxmsg  = 10,    /* max messages in queue */
    .mq_msgsize = 256,   /* max bytes per message */
};
mqd_t mq = mq_open("/my_queue", O_CREAT | O_RDWR, 0644, &attr);

/* Open existing */
mqd_t mq = mq_open("/my_queue", O_RDONLY);  /* or O_WRONLY, O_RDWR */

/* Non-blocking mode */
mqd_t mq = mq_open("/my_queue", O_RDONLY | O_NONBLOCK);
```

### Sending

```c
int r = mq_send(mq, msg_ptr, msg_len, priority);
/* msg_len must be <= mq_msgsize */
/* priority: higher number = dequeued first */
/* Blocks if queue full (unless O_NONBLOCK) */

/* With timeout */
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  /* 5 second timeout */
int r = mq_timedsend(mq, msg_ptr, msg_len, priority, &ts);
```

### Receiving

```c
char buf[MAX_MSG_SZ];
unsigned int prio;
ssize_t n = mq_receive(mq, buf, MAX_MSG_SZ, &prio);
/* buf must be >= mq_msgsize (NOT msg_len) — else EMSGSIZE */
/* Returns: message length */
/* Blocks if queue empty (unless O_NONBLOCK) */

/* With timeout */
ssize_t n = mq_timedreceive(mq, buf, MAX_MSG_SZ, &prio, &ts);
```

### Query Attributes

```c
struct mq_attr attr;
mq_getattr(mq, &attr);
printf("Current messages: %ld\n", attr.mq_curmsgs);
printf("Max messages:     %ld\n", attr.mq_maxmsg);
printf("Max msg size:     %ld\n", attr.mq_msgsize);

/* Set non-blocking (only mq_flags can be changed) */
struct mq_attr new_attr = { .mq_flags = O_NONBLOCK };
mq_setattr(mq, &new_attr, &old_attr);
```

### Cleanup

```c
mq_close(mq);            /* close descriptor */
mq_unlink("/my_queue");   /* remove queue name */
```

### Compile Flags

```bash
gcc -o program program.c -lrt   # link with librt for mq_* functions
```

---

## ASCII Diagram

### Message Queue Priority Ordering

```
  mq_send(mq, "low",  3, prio=0)    ──▶ ┌─────────────────────────┐
  mq_send(mq, "med",  3, prio=2)    ──▶ │  Queue (sorted):        │
  mq_send(mq, "high", 4, prio=5)    ──▶ │                         │
  mq_send(mq, "med2", 4, prio=2)    ──▶ │  [prio=5] "high"  ◀─── │── mq_receive gets this first
                                         │  [prio=2] "med"        │
                                         │  [prio=2] "med2"  FIFO │── same prio: arrival order
                                         │  [prio=0] "low"   ◀─── │── last to be received
                                         │                         │
                                         │  mq_curmsgs = 4        │
                                         │  mq_maxmsg  = 10       │
                                         └─────────────────────────┘

  Receiver calls mq_receive() four times:
    1st: "high" (prio=5)
    2nd: "med"  (prio=2)
    3rd: "med2" (prio=2)  ← same priority, FIFO order
    4th: "low"  (prio=0)
```

### System Limits

```
  /proc/sys/fs/mqueue/
  ├── msg_max       # max messages per queue (default: 10)
  ├── msgsize_max   # max bytes per message (default: 8192)
  └── queues_max    # max queues system-wide (default: 256)

  Per-user limit:
  $ ulimit -q       # RLIMIT_MSGQUEUE (bytes of queue memory)
  # default: 819200 bytes
```

---

## Complete Working Example

### core_process.c (Receiver)

```c
/* See: docs/ipc/examples/04_msgqueue/core_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>

#define QUEUE_NAME  "/ipc_demo_mq"
#define MAX_MSGS    10
#define MAX_MSG_SZ  256
#define EXPECTED_MSGS 6

int
main(void)
{
    mqd_t mq;
    struct mq_attr attr;
    char buf[MAX_MSG_SZ];
    unsigned int prio;
    ssize_t n;
    int i;

    mq_unlink(QUEUE_NAME);

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg  = MAX_MSGS;
    attr.mq_msgsize = MAX_MSG_SZ;

    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if(mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    printf("[receiver] PID=%d, queue created\n", getpid());
    printf("[receiver] waiting for %d messages...\n", EXPECTED_MSGS);

    for(i = 0; i < EXPECTED_MSGS; i++) {
        /* Blocks until a message arrives */
        n = mq_receive(mq, buf, MAX_MSG_SZ, &prio);
        if(n == -1) { perror("mq_receive"); break; }

        buf[n] = '\0';
        printf("[receiver] msg %d: prio=%u size=%zd data=\"%s\"\n",
               i, prio, n, buf);
    }

    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    printf("[receiver] done, queue removed\n");
    return 0;
}
```

### noncore_process.c (Sender)

```c
/* See: docs/ipc/examples/04_msgqueue/noncore_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>

#define QUEUE_NAME  "/ipc_demo_mq"
#define MAX_MSG_SZ  256

struct msg_def {
    const char *text;
    unsigned int prio;
};

static struct msg_def messages[] = {
    { "Low priority background task",     0 },
    { "Normal log entry",                 1 },
    { "Another normal message",           1 },
    { "High priority alert",              3 },
    { "Critical system event",            5 },
    { "Medium priority notification",     2 },
};

#define MSG_COUNT (sizeof(messages) / sizeof(messages[0]))

int
main(void)
{
    mqd_t mq;
    unsigned int i;

    printf("[sender] PID=%d, opening queue...\n", getpid());

    /* Retry until receiver creates the queue */
    for(;;) {
        mq = mq_open(QUEUE_NAME, O_WRONLY);
        if(mq != (mqd_t)-1) break;
        if(errno != ENOENT) { perror("mq_open"); exit(1); }
        usleep(100000);
    }

    printf("[sender] queue opened, sending %zu messages\n", MSG_COUNT);

    for(i = 0; i < MSG_COUNT; i++) {
        if(mq_send(mq, messages[i].text,
                   strlen(messages[i].text),
                   messages[i].prio) == -1) {
            perror("mq_send");
            break;
        }
        printf("[sender] sent prio=%u: \"%s\"\n",
               messages[i].prio, messages[i].text);
        usleep(100000);
    }

    mq_close(mq);
    printf("[sender] done\n");
    return 0;
}
```

---

## Execution Instructions

### Compile

```bash
cd docs/ipc/examples/04_msgqueue
make
# Manual: gcc -Wall -Wextra -o core_process core_process.c -lrt
#         gcc -Wall -Wextra -o noncore_process noncore_process.c -lrt
```

### Run

**Terminal 1** (start receiver first):
```bash
./core_process
```

**Terminal 2** (start sender):
```bash
./noncore_process
```

### Expected Output

**Terminal 1 (receiver):**
```
[receiver] PID=44001, queue "/ipc_demo_mq" created
[receiver] waiting for 6 messages...
[receiver] msg 0: prio=0 size=31 data="Low priority background task"
[receiver] msg 1: prio=1 size=16 data="Normal log entry"
[receiver] msg 2: prio=1 size=22 data="Another normal message"
[receiver] msg 3: prio=3 size=19 data="High priority alert"
[receiver] msg 4: prio=5 size=22 data="Critical system event"
[receiver] msg 5: prio=2 size=30 data="Medium priority notification"
[receiver] done, queue removed
```

**Note**: Because the sender sends messages with small delays and the receiver
blocks on each receive, messages are often received in send order.
To see priority reordering, start the sender first and let all messages
accumulate, THEN start the receiver.

**Terminal 2 (sender):**
```
[sender] PID=44002, opening queue "/ipc_demo_mq"...
[sender] queue opened, sending 6 messages
[sender] sent prio=0: "Low priority background task"
[sender] sent prio=1: "Normal log entry"
[sender] sent prio=1: "Another normal message"
[sender] sent prio=3: "High priority alert"
[sender] sent prio=5: "Critical system event"
[sender] sent prio=2: "Medium priority notification"
[sender] done
```

### To See Priority Ordering

```bash
# Start sender first (receiver not running, messages accumulate):
./noncore_process &
sleep 2
# Now start receiver — it dequeues all messages in priority order:
./core_process
```

### Verify

```bash
# While running, list the queue:
ls -la /dev/mqueue/

# Check queue attributes:
cat /dev/mqueue/ipc_demo_mq

# After cleanup:
ls /dev/mqueue/ipc_demo_mq 2>&1
# No such file or directory
```
