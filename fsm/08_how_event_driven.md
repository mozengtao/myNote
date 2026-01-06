# HOW | FSM + Event-Driven Architecture

## 1. Event Loops

```
FSM AS AN EVENT CONSUMER
========================

Event-Driven Architecture:

+---------------+     +---------------+     +---------------+
|  Event Source |     |  Event Source |     |  Event Source |
|   (Network)   |     |   (Timer)     |     |    (User)     |
+-------+-------+     +-------+-------+     +-------+-------+
        |                     |                     |
        v                     v                     v
+-------------------------------------------------------+
|                    EVENT QUEUE                        |
+---------------------------+---------------------------+
                            |
                            | dequeue
                            v
                    +---------------+
                    |  EVENT LOOP   |
                    +-------+-------+
                            |
                            | dispatch
                            v
                    +---------------+
                    |      FSM      |
                    +---------------+
                            |
                            | action
                            v
                    +---------------+
                    |    HANDLERS   |
                    +---------------+


The FSM is a CONSUMER in this architecture:
  - It does not generate events
  - It does not poll for events
  - It receives events from the event loop
  - It responds synchronously and returns
```

```
DECOUPLING EVENT SOURCES FROM STATE LOGIC
=========================================

Without FSM (Coupled):
+---------------------------------------------------------------+
|  void on_socket_readable(int fd) {                            |
|      if (waiting_for_header) {                                |
|          read_header(fd);                                     |
|          if (header_complete) {                               |
|              waiting_for_header = false;                      |
|              waiting_for_body = true;                         |
|          }                                                    |
|      } else if (waiting_for_body) {                           |
|          read_body(fd);                                       |
|          // ... more nested conditionals                      |
|      }                                                        |
|  }                                                            |
+---------------------------------------------------------------+

With FSM (Decoupled):
+---------------------------------------------------------------+
|  /* Event source: just generates events */                    |
|  void on_socket_readable(int fd) {                            |
|      fsm_dispatch(&conn->fsm, EVENT_DATA_AVAILABLE);          |
|  }                                                            |
|                                                               |
|  void on_timer_expired(timer_t *t) {                          |
|      fsm_dispatch(&conn->fsm, EVENT_TIMEOUT);                 |
|  }                                                            |
|                                                               |
|  /* FSM handles all state logic */                            |
|  /* Transition table defines behavior */                      |
+---------------------------------------------------------------+


Benefits:
  1. Event sources are simple (just translate to events)
  2. State logic is centralized
  3. Easy to add new event sources
  4. Easy to test FSM independently
```

```
INTEGRATION PATTERN: FSM + SELECT/POLL/EPOLL
============================================

Traditional Event Loop:
+---------------------------------------------------------------+
|  while (running) {                                            |
|      int n = poll(fds, nfds, timeout_ms);                     |
|                                                               |
|      for (int i = 0; i < nfds; i++) {                         |
|          if (fds[i].revents & POLLIN) {                       |
|              handle_readable(fds[i].fd);   // Direct handler  |
|          }                                                    |
|      }                                                        |
|  }                                                            |
+---------------------------------------------------------------+

FSM-Integrated Event Loop:
+---------------------------------------------------------------+
|  while (running) {                                            |
|      int n = poll(fds, nfds, timeout_ms);                     |
|                                                               |
|      /* Generate events from poll results */                  |
|      for (int i = 0; i < nfds; i++) {                         |
|          fsm_t *fsm = fd_to_fsm[fds[i].fd];                   |
|                                                               |
|          if (fds[i].revents & POLLIN) {                       |
|              fsm_dispatch(fsm, EVENT_READABLE);               |
|          }                                                    |
|          if (fds[i].revents & POLLOUT) {                      |
|              fsm_dispatch(fsm, EVENT_WRITABLE);               |
|          }                                                    |
|          if (fds[i].revents & POLLERR) {                      |
|              fsm_dispatch(fsm, EVENT_ERROR);                  |
|          }                                                    |
|      }                                                        |
|                                                               |
|      /* Generate timeout events */                            |
|      process_timers();  /* Calls fsm_dispatch(EVENT_TIMEOUT) */
|  }                                                            |
+---------------------------------------------------------------+
```

```c
/* Complete Event Loop + FSM Example */

#include <poll.h>
#include <stdbool.h>

/* FSM types (from previous examples) */
typedef enum { STATE_IDLE, STATE_CONNECTING, STATE_CONNECTED, STATE_COUNT } state_t;
typedef enum { EVENT_CONNECT, EVENT_READABLE, EVENT_WRITABLE, EVENT_ERROR, EVENT_TIMEOUT, EVENT_COUNT } event_t;

typedef struct {
    state_t state;
    int fd;
    void *data;
} fsm_t;

/* Forward declaration */
int fsm_dispatch(fsm_t *fsm, event_t event);

/* Connection pool */
#define MAX_CONNS 64
static fsm_t connections[MAX_CONNS];
static struct pollfd pollfds[MAX_CONNS];
static int nconns = 0;

/* Map fd to FSM */
static fsm_t *fd_to_fsm(int fd) {
    for (int i = 0; i < nconns; i++) {
        if (connections[i].fd == fd) {
            return &connections[i];
        }
    }
    return NULL;
}

/* Event loop */
void event_loop(void) {
    while (true) {
        /* Build poll array */
        for (int i = 0; i < nconns; i++) {
            pollfds[i].fd = connections[i].fd;
            pollfds[i].events = POLLIN;
            
            /* Only poll for writable if we have data to send */
            if (connections[i].state == STATE_CONNECTING) {
                pollfds[i].events |= POLLOUT;
            }
        }
        
        /* Wait for events (with 1 second timeout) */
        int n = poll(pollfds, nconns, 1000);
        
        if (n < 0) {
            perror("poll");
            break;
        }
        
        if (n == 0) {
            /* Timeout - generate timeout events */
            for (int i = 0; i < nconns; i++) {
                fsm_dispatch(&connections[i], EVENT_TIMEOUT);
            }
            continue;
        }
        
        /* Process poll results */
        for (int i = 0; i < nconns; i++) {
            short revents = pollfds[i].revents;
            fsm_t *fsm = &connections[i];
            
            if (revents & POLLERR) {
                fsm_dispatch(fsm, EVENT_ERROR);
            }
            else if (revents & POLLOUT) {
                fsm_dispatch(fsm, EVENT_WRITABLE);
            }
            else if (revents & POLLIN) {
                fsm_dispatch(fsm, EVENT_READABLE);
            }
        }
    }
}
```

---

## 2. Single-Threaded FSM Design

```
WHY MOST FSMs SHOULD NOT BE MULTITHREADED
=========================================

Problem: Concurrent event delivery to same FSM

Thread 1:                       Thread 2:
  fsm.state = A                   fsm.state = A
  event = X                       event = Y
  |                               |
  | fsm_dispatch()                | fsm_dispatch()
  v                               v
  lookup(A, X) -> B               lookup(A, Y) -> C
  |                               |
  | fsm.state = B                 | fsm.state = C  ← RACE!
  v                               v

Result: state = C or B? Unpredictable!


Solution 1: Lock the FSM
+---------------------------------------------------------------+
|  int fsm_dispatch(fsm_t *fsm, event_t e) {                    |
|      pthread_mutex_lock(&fsm->lock);                          |
|      /* transition logic */                                   |
|      pthread_mutex_unlock(&fsm->lock);                        |
|  }                                                            |
+---------------------------------------------------------------+
- Serializes events
- Adds latency
- Can deadlock if action calls back


Solution 2: Single-threaded event queue (PREFERRED)
+---------------------------------------------------------------+
|  /* Events go to queue, not directly to FSM */                |
|  void post_event(fsm_t *fsm, event_t e) {                     |
|      enqueue(&fsm->event_queue, e);                           |
|  }                                                            |
|                                                               |
|  /* Single thread processes queue */                          |
|  void process_events(fsm_t *fsm) {                            |
|      while (!queue_empty(&fsm->event_queue)) {                |
|          event_t e = dequeue(&fsm->event_queue);              |
|          fsm_dispatch(fsm, e);                                |
|      }                                                        |
|  }                                                            |
+---------------------------------------------------------------+
```

```
EVENT SERIALIZATION GUARANTEES
==============================

Single-threaded FSM guarantees:
  1. Events processed in order (FIFO)
  2. No concurrent state access
  3. Actions complete before next event
  4. Deterministic behavior

                Event Queue (Thread-safe)
                         |
                         v
    +-------------------+-----------------------+
    | E1 | E2 | E3 | E4 |                      |
    +-------------------+-----------------------+
                         |
                         | Single consumer thread
                         v
                    +---------+
                    |   FSM   |
                    +---------+

Events E1, E2, E3, E4 processed strictly in order.
No interleaving. No races.


Multi-FSM Pattern:
+---------------------------------------------------------------+
|  /* Multiple FSMs, each single-threaded */                    |
|                                                               |
|  FSM_A <---- Thread_A (event loop for A)                      |
|  FSM_B <---- Thread_B (event loop for B)                      |
|  FSM_C <---- Thread_C (event loop for C)                      |
|                                                               |
|  Each FSM is single-threaded.                                 |
|  Communication between FSMs via event posting.                |
+---------------------------------------------------------------+
```

```
ACTOR MODEL PATTERN
===================

Each FSM instance is an "actor":
  - Has its own event queue
  - Processes events sequentially
  - Communicates via messages (events)

+-------------+          +-------------+          +-------------+
|   Actor A   |  event   |   Actor B   |  event   |   Actor C   |
|  +-------+  | -------> |  +-------+  | -------> |  +-------+  |
|  | Queue |  |          |  | Queue |  |          |  | Queue |  |
|  +---+---+  |          |  +---+---+  |          |  +---+---+  |
|      |      |          |      |      |          |      |      |
|      v      |          |      v      |          |      v      |
|  +-------+  |          |  +-------+  |          |  +-------+  |
|  |  FSM  |  |          |  |  FSM  |  |          |  |  FSM  |  |
|  +-------+  |          |  +-------+  |          |  +-------+  |
+-------------+          +-------------+          +-------------+

Benefits:
  - No shared state between actors
  - No locks needed within actor
  - Scales across cores
  - Fault isolation (one actor crash doesn't affect others)
```

---

## 3. Integration Boundaries

```
FSMs INSIDE DAEMONS
===================

Typical daemon structure with FSMs:

+---------------------------------------------------------------+
|                         DAEMON                                 |
|                                                               |
|  +------------------+       +-----------------------+         |
|  |   Main Thread    |       |   Worker Threads      |         |
|  |                  |       |                       |         |
|  |  - Signal handler|       |  - Connection FSM     |         |
|  |  - Config reload |       |  - Protocol FSM       |         |
|  |  - Shutdown coord|       |  - Session FSM        |         |
|  +--------+---------+       +-----------+-----------+         |
|           |                             |                     |
|           |     post EVENT_SHUTDOWN     |                     |
|           +---------------------------->|                     |
|                                         |                     |
|           |     post EVENT_RELOAD       |                     |
|           +---------------------------->|                     |
|                                                               |
+---------------------------------------------------------------+

Key patterns:
  1. Main thread handles signals, posts events to FSMs
  2. Each connection has its own FSM
  3. FSMs don't know about signals, only events
```

```
FSMs INSIDE LIBRARIES
=====================

Library with internal FSM:

+---------------------------------------------------------------+
|                      LIBRARY API                               |
|                                                               |
|  +----------------------------------------------------------+ |
|  |  User-visible API                                        | |
|  |                                                          | |
|  |  int lib_connect(handle_t *h, const char *host);         | |
|  |  int lib_send(handle_t *h, const void *data, size_t n);  | |
|  |  int lib_poll(handle_t *h, int timeout_ms);              | |
|  |  void lib_close(handle_t *h);                            | |
|  +----------------------------------------------------------+ |
|                            |                                  |
|                            v                                  |
|  +----------------------------------------------------------+ |
|  |  Internal FSM (hidden from user)                         | |
|  |                                                          | |
|  |  struct handle {                                         | |
|  |      state_t state;  /* FSM state */                     | |
|  |      int fd;         /* Socket */                        | |
|  |      /* ... */                                           | |
|  |  };                                                       | |
|  |                                                          | |
|  |  lib_connect() -> fsm_dispatch(EVENT_CONNECT_REQ)        | |
|  |  lib_send()    -> fsm_dispatch(EVENT_SEND_REQ)           | |
|  |  lib_poll()    -> event loop, calls fsm_dispatch()       | |
|  |  lib_close()   -> fsm_dispatch(EVENT_CLOSE_REQ)          | |
|  +----------------------------------------------------------+ |
+---------------------------------------------------------------+

Key patterns:
  1. FSM is an implementation detail
  2. User API maps to events
  3. User doesn't know about states
```

```c
/* Library FSM Example: HTTP Client */

/* Public API */
typedef struct http_client http_client_t;

http_client_t *http_client_create(void);
int http_client_connect(http_client_t *c, const char *url);
int http_client_send(http_client_t *c, const char *request);
int http_client_poll(http_client_t *c, int timeout_ms);
char *http_client_get_response(http_client_t *c);
void http_client_close(http_client_t *c);

/* Internal implementation */
struct http_client {
    /* FSM */
    state_t state;
    
    /* Network */
    int fd;
    
    /* Buffers */
    char *request;
    char *response;
    size_t response_len;
};

/* Map API calls to events */
int http_client_connect(http_client_t *c, const char *url) {
    if (c->state != STATE_IDLE) {
        return -1;  /* Invalid state for connect */
    }
    
    /* Parse URL, store host/port */
    parse_url(url, &c->host, &c->port);
    
    /* Dispatch connect event */
    return fsm_dispatch(c, EVENT_CONNECT_REQ);
}

int http_client_send(http_client_t *c, const char *request) {
    if (c->state != STATE_CONNECTED) {
        return -1;  /* Must be connected */
    }
    
    c->request = strdup(request);
    return fsm_dispatch(c, EVENT_SEND_REQ);
}

int http_client_poll(http_client_t *c, int timeout_ms) {
    struct pollfd pfd = {
        .fd = c->fd,
        .events = POLLIN | POLLOUT
    };
    
    int n = poll(&pfd, 1, timeout_ms);
    
    if (n > 0) {
        if (pfd.revents & POLLIN) {
            fsm_dispatch(c, EVENT_READABLE);
        }
        if (pfd.revents & POLLOUT) {
            fsm_dispatch(c, EVENT_WRITABLE);
        }
        if (pfd.revents & POLLERR) {
            fsm_dispatch(c, EVENT_ERROR);
        }
    } else if (n == 0) {
        fsm_dispatch(c, EVENT_TIMEOUT);
    }
    
    return n;
}
```

---

## Summary: FSM Integration

```
+----------------------------------------------------------+
|              FSM IN EVENT-DRIVEN SYSTEMS                  |
+----------------------------------------------------------+
|                                                          |
|  EVENT LOOP:                                             |
|    - FSM is a consumer, not a producer                   |
|    - Event sources generate events                       |
|    - FSM responds and returns                            |
|                                                          |
|  THREADING:                                              |
|    - Single-threaded FSM is simplest                     |
|    - Event queue + single consumer = safe                |
|    - Actor model for multi-FSM systems                   |
|                                                          |
|  INTEGRATION:                                            |
|    - Daemons: FSM per connection/session                 |
|    - Libraries: FSM as hidden implementation             |
|    - API calls map to events                             |
|                                                          |
+----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**事件循环**

FSM 在事件驱动架构中是**消费者**：
- 不生成事件
- 不轮询事件
- 从事件循环接收事件
- 同步响应并返回

解耦事件源和状态逻辑：事件源只负责翻译原始事件（如 socket 可读）为 FSM 事件，所有状态逻辑集中在 FSM 中。好处：事件源简单、状态逻辑集中、易于添加新事件源、易于独立测试 FSM。

**单线程 FSM 设计**

为什么大多数 FSM 不应该是多线程的：并发事件传递到同一个 FSM 会导致竞态条件，状态不可预测。

解决方案：
1. **锁 FSM**：序列化事件，但增加延迟，可能死锁
2. **单线程事件队列**（推荐）：事件进入队列，单线程处理队列

单线程 FSM 保证：
- 事件按顺序处理（FIFO）
- 无并发状态访问
- 动作在下一事件之前完成
- 确定性行为

**Actor 模式**：每个 FSM 实例是一个 actor，有自己的事件队列，顺序处理事件，通过消息（事件）通信。好处：无共享状态、无需锁、可跨核心扩展、故障隔离。

**集成边界**

**守护进程中的 FSM**：
- 主线程处理信号，向 FSM 发送事件
- 每个连接有自己的 FSM
- FSM 不知道信号，只知道事件

**库中的 FSM**：
- FSM 是实现细节，对用户隐藏
- 用户 API 映射到事件
- 用户不知道状态存在

关键模式：将用户可见的 API 调用转换为内部 FSM 事件，隐藏状态机的复杂性。
