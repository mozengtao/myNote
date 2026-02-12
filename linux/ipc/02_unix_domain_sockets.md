# 2. UNIX Domain Sockets

## Mental Model

### What Problem This Solves

UNIX domain sockets provide bidirectional, reliable, byte-stream (or datagram)
communication between processes on the **same host**.  They use the familiar
socket API (`socket`, `bind`, `listen`, `accept`, `send`, `recv`) but bypass
the entire TCP/IP network stack — no IP headers, no routing, no checksums.

They are the **workhorse IPC** for most modern Linux services.  D-Bus, systemd,
X11/Wayland, Docker, PostgreSQL, MySQL — all use UNIX domain sockets as their
primary IPC channel.

### When To Use

- Client/server communication on the same machine
- When you need bidirectional, reliable, ordered delivery
- When you want to pass file descriptors between processes (`SCM_RIGHTS`)
- When you want credential passing (peer PID/UID/GID via `SO_PEERCRED`)
- When your protocol already uses the socket API (easy drop-in for TCP→local)

### When NOT To Use

- Pure data sharing with no message framing (use shared memory)
- Simple signaling between processes (use semaphores or eventfd)
- Cross-machine communication (use TCP/UDP sockets)

### Communication Pattern

```
   SOCK_STREAM (connection-oriented, like TCP):

   Server                             Client
   ┌──────────┐  listen + accept     ┌──────────┐
   │core_proc │◀─────────────────────│noncore   │
   │ (server) │                      │ (client) │
   │          │─────── send ────────▶│          │
   │          │◀────── recv ─────────│          │
   │          │                      │          │
   │          │  bidirectional!      │          │
   └──────────┘                      └──────────┘

   SOCK_DGRAM (connectionless, like UDP):

   Process A           Process B
   ┌────────┐         ┌────────┐
   │sendto()│────────▶│recvfrom│  message-oriented
   │recvfrom│◀────────│sendto()│  preserves boundaries
   └────────┘         └────────┘
```

- **SOCK_STREAM**: 1→1 per connection, but server can accept many clients
- **SOCK_DGRAM**: many→many, connectionless, message boundaries preserved
- **SOCK_SEQPACKET**: connection-oriented + message boundaries (rarely used)

### Kernel Objects Involved

| Object | Role |
|--------|------|
| `struct socket` | Protocol-independent socket wrapper |
| `struct unix_sock` | AF_UNIX-specific sock, contains peer pointer + sk_buff queues |
| `struct sk_buff` | Buffer holding one chunk of data in the kernel |
| `struct sockaddr_un` | Address structure: sun_family + sun_path |
| `struct file` | VFS wrapper so the socket fd works with poll/select/epoll |
| Inode at socket path | Special inode (S_IFSOCK) linking filesystem name to socket |

### Blocking Behavior

| Operation | Blocks When |
|-----------|-------------|
| `accept()` | No pending connections |
| `connect()` | Never for UNIX domain (instant) — but fails if server not ready |
| `recv()` | No data available in receive buffer |
| `send()` | Peer's receive buffer is full (backpressure!) |

All can be made non-blocking with `O_NONBLOCK` or `SOCK_NONBLOCK`.

### Lifetime Rules

- **Filesystem path**: persists until `unlink()` — stale socket files are common
- **Abstract namespace** (`sun_path[0] = '\0'`): auto-removed when all fds close
- **Connected socket pair**: when one end closes, peer gets EOF (recv→0) or
  EPIPE/SIGPIPE on send
- On `fork()`: child inherits fds; both parent and child can send/recv on them
- On `exec()`: fds without `O_CLOEXEC` survive

### Performance Characteristics

- **Copy count**: 1 kernel copy (userspace → sk_buff → userspace)
- **Latency**: ~1-2 μs per small message (extremely fast)
- **Throughput**: ~5-10 GB/s for bulk transfer (limited by memcpy speed)
- **No network overhead**: no headers, no checksums, no routing lookups

---

## How It Works Internally

### Connection Lifecycle (SOCK_STREAM)

```
 SERVER                          KERNEL                           CLIENT
 ──────                          ──────                           ──────

 socket(AF_UNIX, STREAM)         Allocate unix_sock (S)
         │
 bind("/tmp/x.sock")             Create S_IFSOCK inode at path
         │                       Link inode → sock S
 listen(fd, backlog)             Mark S as LISTENING
         │                       Create accept queue (max=backlog)
         │
 accept(fd) ─── BLOCKS ──▶      Waits on accept queue
                                                                  socket(AF_UNIX, STREAM)
                                 Allocate unix_sock (C)            │
                                                                  connect("/tmp/x.sock")
                                 Lookup inode at path              │
                                 Find sock S from inode            │
                                 Allocate new unix_sock (S')       │
                                 Link: S'.peer = C, C.peer = S'    │
                                 Enqueue S' on accept queue        │
         │                                                         │
 accept() returns ◀──────        Dequeue S' from accept queue     connect() returns
 new_fd → sock S'                                                  │
         │                                                         │
 send(new_fd, data)              data → S'.sk_buff → C.receive_q   │
                                                                  recv(fd) → dequeue sk_buff
         │                                                         │
 recv(new_fd) ◀──── data ─────  C.sk_buff → S'.receive_q  ──────  send(fd, data)
```

### Data Flow Detail

```
  send(fd, userdata, len, 0)
       │
       ▼
  ┌──────────────────┐
  │ copy_from_user() │   copy userdata into kernel sk_buff
  └────────┬─────────┘
           │
           ▼
  ┌──────────────────┐
  │unix_stream_sendmsg │   enqueue sk_buff on PEER's receive queue
  └────────┬─────────┘
           │                no routing, no checksums, no headers
           ▼
  ┌──────────────────┐
  │ peer->sk_receive_queue │
  └────────┬─────────┘
           │
           ▼
  recv(fd, buf, len, 0)
       │
       ▼
  ┌──────────────────┐
  │ copy_to_user()   │   copy from sk_buff to userdata
  └──────────────────┘
```

**Total copies**: 2 (user→kernel, kernel→user).  There is no true zero-copy,
but splice/vmsplice can reduce copies in some cases.

### Filesystem vs Abstract Namespace

| Feature | Filesystem | Abstract |
|---------|-----------|----------|
| Binding | `sun_path = "/tmp/x.sock"` | `sun_path[0] = '\0'; sun_path[1..] = "name"` |
| Visibility | `ls /tmp/x.sock` | Invisible in filesystem |
| Cleanup | Must `unlink()` manually | Auto-removed on last fd close |
| Permissions | Filesystem permissions apply | Only network namespaces restrict |
| Portability | POSIX | Linux-specific |

### What Happens on fork/exec/crash

| Event | Behavior |
|-------|----------|
| `fork()` | Child inherits connected fds. Both can send/recv. If both write, data interleaves — coordinate externally. |
| `exec()` | Fds without `O_CLOEXEC` survive. Socket state preserved. |
| Process crash | Kernel closes all fds. Peer gets recv()=0 (EOF) or send()→SIGPIPE/EPIPE. Socket file NOT automatically removed — must unlink. |

---

## Key APIs

### Creation

```c
int fd = socket(AF_UNIX, SOCK_STREAM, 0);  /* or SOCK_DGRAM, SOCK_SEQPACKET */
int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
```

### Binding (Server)

```c
struct sockaddr_un addr;
memset(&addr, 0, sizeof(addr));
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "/tmp/my.sock", sizeof(addr.sun_path) - 1);

unlink("/tmp/my.sock");  /* remove stale socket */
bind(fd, (struct sockaddr *)&addr, sizeof(addr));
```

Abstract namespace:
```c
addr.sun_path[0] = '\0';
memcpy(addr.sun_path + 1, "my_abstract", 11);
/* addr_len = offsetof(struct sockaddr_un, sun_path) + 1 + 11 */
bind(fd, (struct sockaddr *)&addr, offsetof(struct sockaddr_un, sun_path) + 1 + 11);
```

### Listening and Accepting (Server)

```c
listen(fd, backlog);               /* backlog = max pending connections */
int client_fd = accept(fd, NULL, NULL);  /* blocks until client connects */
/* or: accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC) */
```

### Connecting (Client)

```c
connect(fd, (struct sockaddr *)&addr, sizeof(addr));
/* Returns -1 with errno=ECONNREFUSED if server not listening */
/* Returns -1 with errno=ENOENT if socket file doesn't exist */
```

### Data Transfer

```c
/* Stream: */
ssize_t n = send(fd, buf, len, 0);      /* or write(fd, buf, len) */
ssize_t n = recv(fd, buf, len, 0);      /* or read(fd, buf, len) */
ssize_t n = recv(fd, buf, len, MSG_WAITALL);  /* wait for full len bytes */
ssize_t n = recv(fd, buf, len, MSG_PEEK);     /* peek without consuming */

/* Datagram: */
sendto(fd, buf, len, 0, (struct sockaddr *)&peer, sizeof(peer));
recvfrom(fd, buf, len, 0, (struct sockaddr *)&sender, &addrlen);
```

### Advanced: Passing File Descriptors (SCM_RIGHTS)

```c
/* This is unique to UNIX domain sockets.
 * You can send an open file descriptor from one process to another.
 * The kernel duplicates the fd into the receiver's fd table. */
struct msghdr msg;
struct cmsghdr *cmsg;
char buf[CMSG_SPACE(sizeof(int))];
/* ... setup msg with cmsg containing the fd ... */
sendmsg(fd, &msg, 0);   /* sender */
recvmsg(fd, &msg, 0);   /* receiver extracts fd from cmsg */
```

### Advanced: Getting Peer Credentials

```c
struct ucred cred;
socklen_t len = sizeof(cred);
getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);
printf("Peer: PID=%d UID=%d GID=%d\n", cred.pid, cred.uid, cred.gid);
```

### Cleanup

```c
close(fd);              /* release our fd */
unlink("/tmp/my.sock"); /* remove the socket file */
```

---

## ASCII Diagram

### Full Connection Architecture

```
  Process A (server)                                 Process B (client)
  ┌──────────────────────┐                          ┌──────────────────────┐
  │ fd_listen = 3        │                          │ fd = 5               │
  │   └─▶ unix_sock (L)  │                          │   └─▶ unix_sock (C)  │
  │       state=LISTENING │                          │       peer ──────┐   │
  │                      │                          │                  │   │
  │ fd_client = 4        │                          └──────────────────│───┘
  │   └─▶ unix_sock (S') │◀── peer ───────────────────────────────────┘
  │       peer ──────────┼────────────────────────▶ unix_sock (C)
  │                      │                          
  │  sk_receive_queue:   │     Bidirectional        
  │  [skb][skb][skb]     │     data flow via       
  │                      │     sk_buff chains       
  └──────────────────────┘                          

  Filesystem:
  /tmp/my.sock ──▶ [inode: S_IFSOCK] ──▶ unix_sock (L)
```

---

## Complete Working Example

### core_process.c (Server)

```c
/* See: docs/ipc/examples/02_socket/core_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/ipc_unix.sock"
#define MSG_COUNT   5

struct ipc_msg {
    int  type;       /* 1=request, 2=response */
    int  seq;
    char data[128];
};

int
main(void)
{
    int listen_fd, client_fd;
    struct sockaddr_un addr;
    struct ipc_msg msg;
    int i;
    ssize_t n;

    unlink(SOCKET_PATH);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(listen_fd == -1) { perror("socket"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind"); exit(1);
    }

    if(listen(listen_fd, 5) == -1) {
        perror("listen"); exit(1);
    }

    printf("[server] listening on %s (PID=%d)\n", SOCKET_PATH, getpid());

    client_fd = accept(listen_fd, NULL, NULL);
    if(client_fd == -1) { perror("accept"); exit(1); }

    printf("[server] client connected\n");

    for(i = 0; i < MSG_COUNT; i++) {
        n = recv(client_fd, &msg, sizeof(msg), MSG_WAITALL);
        if(n <= 0) break;

        printf("[server] recv: type=%d seq=%d data=\"%s\"\n",
               msg.type, msg.seq, msg.data);

        memset(&msg, 0, sizeof(msg));
        msg.type = 2;
        msg.seq  = i;
        snprintf(msg.data, sizeof(msg.data), "ACK for request #%d", i);

        send(client_fd, &msg, sizeof(msg), 0);
        printf("[server] sent: seq=%d\n", msg.seq);
    }

    close(client_fd);
    close(listen_fd);
    unlink(SOCKET_PATH);
    printf("[server] done\n");
    return 0;
}
```

### noncore_process.c (Client)

```c
/* See: docs/ipc/examples/02_socket/noncore_process.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/ipc_unix.sock"
#define MSG_COUNT   5

struct ipc_msg {
    int  type;
    int  seq;
    char data[128];
};

int
main(void)
{
    int sock_fd;
    struct sockaddr_un addr;
    struct ipc_msg msg;
    int i;
    ssize_t n;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock_fd == -1) { perror("socket"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    printf("[client] connecting to %s (PID=%d)\n", SOCKET_PATH, getpid());

    if(connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect"); exit(1);
    }

    printf("[client] connected\n");

    for(i = 0; i < MSG_COUNT; i++) {
        memset(&msg, 0, sizeof(msg));
        msg.type = 1;
        msg.seq  = i;
        snprintf(msg.data, sizeof(msg.data), "Request #%d from client", i);

        send(sock_fd, &msg, sizeof(msg), 0);
        printf("[client] sent: seq=%d\n", msg.seq);

        n = recv(sock_fd, &msg, sizeof(msg), MSG_WAITALL);
        if(n <= 0) break;

        printf("[client] recv: type=%d seq=%d data=\"%s\"\n",
               msg.type, msg.seq, msg.data);
    }

    close(sock_fd);
    printf("[client] done\n");
    return 0;
}
```

---

## Execution Instructions

### Compile

```bash
cd docs/ipc/examples/02_socket
make
```

### Run

**Terminal 1** (start server first):
```bash
./core_process
```

**Terminal 2** (start client after server is listening):
```bash
./noncore_process
```

### Expected Output

**Terminal 1 (server):**
```
[server] listening on /tmp/ipc_unix.sock (PID=22001)
[server] client connected
[server] recv: type=1 seq=0 data="Request #0 from client"
[server] sent: seq=0
[server] recv: type=1 seq=1 data="Request #1 from client"
[server] sent: seq=1
[server] recv: type=1 seq=2 data="Request #2 from client"
[server] sent: seq=2
[server] recv: type=1 seq=3 data="Request #3 from client"
[server] sent: seq=3
[server] recv: type=1 seq=4 data="Request #4 from client"
[server] sent: seq=4
[server] done, socket cleaned up
```

**Terminal 2 (client):**
```
[client] connecting to /tmp/ipc_unix.sock (PID=22002)
[client] connected to server
[client] sent: seq=0 data="Request #0 from client"
[client] recv: type=2 seq=0 data="ACK for request #0"
[client] sent: seq=1 data="Request #1 from client"
[client] recv: type=2 seq=1 data="ACK for request #1"
...
[client] done
```

### Verify

```bash
# While running, check the socket file:
ls -la /tmp/ipc_unix.sock
# srwxr-xr-x 1 user user 0 ... /tmp/ipc_unix.sock

# Check socket connections:
ss -x | grep ipc_unix

# After both exit, the socket file should be gone:
ls /tmp/ipc_unix.sock 2>&1
# No such file or directory
```
