# Module 5: State

> **Pattern**: State (GoF)
> **Kernel idioms**: Enum-based state variables, transition tables, explicit state machines

---

## Mental Model

An object's behavior depends on its current state; transitions are explicit
and often centralized. In C: an enum (or integer constant set) holds the
current state, and a table or switch defines the next state and actions
on each event.

```
  ┌─────────┐   event X     ┌─────────┐   event Y     ┌─────────┐
  │ STATE A │ ────────────→ │ STATE B │ ────────────→ │ STATE C │
  └─────────┘               └─────────┘               └─────────┘
       ▲                                                    │
       │                    event Z                         │
       └────────────────────────────────────────────────────┘
```

**GoF mapping:**
- **Context** → the object holding the state variable (e.g. TCP socket)
- **State interface** → the set of valid states (enum)
- **ConcreteState** → each enum value + the behavior it enables
- **Transition** → code that changes the state variable, guarded by current state

---

## In the Kernel (v3.2)

### Example 1: TCP Connection State Machine

`include/net/tcp_states.h`, lines 16–30:

```c
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,

    TCP_MAX_STATES
};
```

Each TCP socket holds `sk->sk_state` as one of these values. The entire
TCP implementation is organized around this state machine.

**The state diagram (simplified):**

```
                          ┌──────────┐
                          │  CLOSED  │
                          └────┬─────┘
                 active open   │           passive open
              (connect())      │           (listen())
                    ┌──────────┼──────────────┐
                    ▼          │              ▼
              ┌──────────┐     │        ┌──────────┐
              │ SYN_SENT │     │        │  LISTEN  │
              └─────┬────┘     │        └─────┬────┘
           rcv SYN+ACK         │         rcv SYN
              send ACK         │         send SYN+ACK
                    │          │              │
                    ▼          │              ▼
              ┌──────────────────────────────────┐
              │          ESTABLISHED             │
              └──────────────┬───────────────────┘
                             │
              close()        │           rcv FIN
              send FIN       │           send ACK
                    │        │              │
                    ▼        │              ▼
              ┌──────────┐   │        ┌───────────┐
              │ FIN_WAIT1│   │        │CLOSE_WAIT │
              └─────┬────┘   │        └─────┬─────┘
              rcv ACK        │         close()
                    │        │         send FIN
                    ▼        │              │
              ┌──────────┐   │              ▼
              │ FIN_WAIT2│   │        ┌──────────┐
              └─────┬────┘   │        │ LAST_ACK │
              rcv FIN        │        └─────┬────┘
              send ACK       │         rcv ACK
                    │        │              │
                    ▼        │              ▼
              ┌──────────┐   │        ┌──────────┐
              │TIME_WAIT │   │        │  CLOSED  │
              └─────┬────┘   │        └──────────┘
              timeout        │
                    │        │
                    ▼        │
              ┌──────────┐   │
              │  CLOSED  │◄──┘
              └──────────┘
```

**How the state drives behavior:**

The TCP code checks `sk->sk_state` to determine what operations are
valid and what incoming packets mean:

```c
/* Simplified from net/ipv4/tcp_input.c */
switch (sk->sk_state) {
case TCP_ESTABLISHED:
    /* process data normally */
    tcp_data_queue(sk, skb);
    break;
case TCP_LISTEN:
    /* this is a new connection request */
    tcp_v4_conn_request(sk, skb);
    break;
case TCP_SYN_SENT:
    /* we're waiting for SYN+ACK */
    tcp_rcv_synsent_state_process(sk, skb, th, len);
    break;
/* ... etc for each state ... */
}
```

**Transitions are explicit:**

```c
/* net/ipv4/tcp.c — simplified */
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;

    /* Enforce valid transitions, update metrics */
    sk->sk_state = state;

    /* Side effects of entering the new state */
    if (state == TCP_CLOSE)
        /* release resources, timers, etc. */
}
```

**Bitmask companion:**

The kernel also defines bitmask versions for efficient multi-state checks:

```c
/* include/net/tcp_states.h */
enum {
    TCPF_ESTABLISHED = (1 << 1),
    TCPF_SYN_SENT    = (1 << 2),
    TCPF_SYN_RECV    = (1 << 3),
    TCPF_FIN_WAIT1   = (1 << 4),
    /* ... */
};
```

This allows checks like "is the socket in any of these states?" with a
single bitwise AND, rather than a chain of `||` comparisons.

### Example 2: Network Device Registration State

`include/linux/netdevice.h`, lines 1253–1259:

```c
enum {
    NETREG_UNINITIALIZED = 0,
    NETREG_REGISTERED,
    NETREG_UNREGISTERING,
    NETREG_UNREGISTERED,
    NETREG_RELEASED,
    NETREG_DUMMY,
};
```

A `net_device` transitions through these states during its lifecycle:

```
  UNINITIALIZED ──→ REGISTERED ──→ UNREGISTERING ──→ UNREGISTERED ──→ RELEASED
       │                                                                  │
       └──────────── DUMMY (for NAPI poll-only devices) ──────────────────┘
```

The state determines what operations are valid: you can't transmit on an
UNREGISTERED device; you can't re-register a RELEASED device.

### Example 3: Task (Process) State

`include/linux/sched.h`, around lines 176–193:

```c
#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define __TASK_STOPPED          4
#define __TASK_TRACED           8
#define EXIT_ZOMBIE             16
#define EXIT_DEAD               32
```

Each `task_struct` has `volatile long state`. The scheduler, signal
delivery, and waitqueue code all check this state to decide behavior:

```
  TASK_RUNNING ────────────────── on the run queue, eligible to run
       │
       │  sleep (wait_event, mutex, I/O)
       ▼
  TASK_INTERRUPTIBLE ──────────── sleeping, wakes on signal or event
  TASK_UNINTERRUPTIBLE ────────── sleeping, wakes only on event
       │
       │  wake_up / signal
       ▼
  TASK_RUNNING ────────────────── back on the run queue
       │
       │  exit()
       ▼
  EXIT_ZOMBIE ─────────────────── exited, waiting for parent to reap
       │
       │  wait() by parent
       ▼
  EXIT_DEAD ───────────────────── fully released
```

### Real Code Path Walkthrough: `connect()` — CLOSE → SYN_SENT → ESTABLISHED

Trace a TCP `connect()` call through its state transitions. This is the
active-open path of the TCP state machine.

```
  USERSPACE
  ─────────
  connect(sockfd, &server_addr, sizeof(server_addr))
       │
       │  syscall → sock->ops->connect → inet_stream_connect
       │    → tcp_v4_connect(sk, uaddr, addr_len)
       ▼
  net/ipv4/tcp_ipv4.c:147 — tcp_v4_connect(sk, uaddr, addr_len)
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  sk->sk_state == TCP_CLOSE   (starting state)                    │
  │                                                                  │
  │  /* Route lookup, source address selection, etc. */              │
  │  rt = ip_route_connect(...);                                     │
  │  inet->inet_daddr = daddr;                                       │
  │  inet->inet_dport = usin->sin_port;                              │
  │                                                                  │
  │  ═══════════════════════════════════════                         │
  │   STATE TRANSITION: TCP_CLOSE → TCP_SYN_SENT                     │
  │  ═══════════════════════════════════════                         │
  │                                                                  │
  │  tcp_set_state(sk, TCP_SYN_SENT);    ◄── line 239                │
  │       │                                                          │
  │       │  net/ipv4/tcp.c — tcp_set_state():                       │
  │       │    oldstate = sk->sk_state;   /* TCP_CLOSE */            │
  │       │    sk->sk_state = TCP_SYN_SENT;                          │
  │       │    /* update inet_diag, SNMP counters */                 │
  │       ▼                                                          │
  │  err = inet_hash_connect(&tcp_death_row, sk);                    │
  │       /* select source port, insert into hash table */           │
  │                                                                  │
  │  err = tcp_connect(sk);    ◄── builds and sends SYN packet       │
  │       │                                                          │
  │       │  net/ipv4/tcp_output.c — tcp_connect():                  │
  │       │    allocate sk_buff for SYN                              │
  │       │    fill in TCP header (SYN flag, ISN, window)            │
  │       │    tcp_transmit_skb(sk, skb)                             │
  │       │    start retransmit timer                                │
  │       ▼                                                          │
  │  return 0;    /* SYN is in flight */                             │
  │                                                                  │
  │  ON FAILURE:                                                     │
  │  tcp_set_state(sk, TCP_CLOSE);   /* revert transition */         │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘

  ──── LATER, when the SYN+ACK arrives: ────

  net/ipv4/tcp_input.c — tcp_rcv_synsent_state_process(sk, skb, ...)
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  sk->sk_state == TCP_SYN_SENT                                    │
  │                                                                  │
  │  /* Validate incoming SYN+ACK */                                 │
  │  if (th->ack && th->syn) {                                       │
  │      /* process ACK, negotiate options (MSS, timestamps, etc.) */│
  │      tcp_ack(sk, skb, ...);                                      │
  │                                                                  │
  │      ═══════════════════════════════════════                     │
  │       STATE TRANSITION: TCP_SYN_SENT → TCP_ESTABLISHED           │
  │      ═══════════════════════════════════════                     │
  │                                                                  │
  │      tcp_set_state(sk, TCP_ESTABLISHED);                         │
  │           /* sk->sk_state = TCP_ESTABLISHED */                   │
  │           /* update counters, notify socket waiters */           │
  │                                                                  │
  │      /* Send ACK to complete three-way handshake */              │
  │      tcp_send_ack(sk);                                           │
  │  }                                                               │
  └──────────────────────────────────────────────────────────────────┘
```

**The state variable drives everything:** When the SYN+ACK arrives, the
TCP input path checks `sk->sk_state`. Because it's `TCP_SYN_SENT`, the
code enters `tcp_rcv_synsent_state_process`. If the socket were in
`TCP_ESTABLISHED`, the same incoming packet would be handled by
`tcp_data_queue` instead. If the socket were in `TCP_LISTEN`, it would
create a new child socket. Same packet, radically different behavior —
determined entirely by the state enum.

---

## Enum vs. Booleans: Why State Machines Matter

Consider the alternative — representing TCP state with individual booleans:

```c
/* BAD: boolean flags instead of state */
struct tcp_sock {
    bool listening;
    bool connected;
    bool syn_sent;
    bool fin_sent;
    bool closing;
    /* ... how many can be true simultaneously? */
};
```

Problems:
1. **Invalid combinations.** `listening && connected` is nonsensical but
   nothing prevents it.
2. **Transition logic scattered.** Every flag set/clear is independent;
   no centralized point to enforce "from A you can only go to B or C."
3. **Exponential complexity.** N booleans = 2^N possible states, most
   invalid. An enum with M values has exactly M valid states.

With an enum:
- Valid states are explicitly enumerated.
- Transitions can be validated ("you're in SYN_SENT; the only valid next
  states are ESTABLISHED or CLOSE").
- Behavior per state is a clean switch, not a chain of boolean checks.

---

## The General Pattern in C

```c
enum widget_state {
    WIDGET_IDLE,
    WIDGET_ACTIVE,
    WIDGET_STOPPING,
    WIDGET_STOPPED,
};

struct widget {
    enum widget_state state;
    /* ... other fields ... */
};

int
widget_handle_event(struct widget *w, enum widget_event ev)
{
    switch (w->state) {
    case WIDGET_IDLE:
        if (ev == EV_START) {
            do_start_work(w);
            w->state = WIDGET_ACTIVE;
            return 0;
        }
        break;
    case WIDGET_ACTIVE:
        if (ev == EV_DATA)
            return do_process(w);
        if (ev == EV_STOP) {
            do_begin_stop(w);
            w->state = WIDGET_STOPPING;
            return 0;
        }
        break;
    case WIDGET_STOPPING:
        if (ev == EV_DONE) {
            do_cleanup(w);
            w->state = WIDGET_STOPPED;
            return 0;
        }
        break;
    case WIDGET_STOPPED:
        break;  /* ignore events in final state */
    }
    return -EINVAL;  /* invalid event for current state */
}
```

---

## Why State Here

Protocols and lifecycles have a fixed set of states and allowed transitions.
Making state explicit avoids ad-hoc flags and prevents invalid transitions.

**What would break without it:**
- TCP with boolean flags would allow "ESTABLISHED and CLOSING at the same time."
- Device registration without states would let code transmit on a half-freed
  device.
- Process scheduling without explicit states would confuse "sleeping" with
  "running" with "zombie."

---

## Pitfalls

1. **Race on state variable.** Multiple CPUs can read/write `sk->sk_state`
   concurrently. The kernel uses locking (socket lock) or atomic operations
   to make transitions safe.

2. **Missing transitions.** If a new event is added but the switch doesn't
   handle it in some state, the machine silently ignores it. Defensive code
   should log or WARN on unexpected state+event combinations.

3. **State explosion.** Complex protocols tempt adding more states. Each
   new state must be reachable and escapable. Draw the diagram first.

4. **Holding locks across transitions.** Some transitions trigger callbacks
   or notifications. If those callbacks try to acquire the same lock,
   deadlock results. Use careful lock ordering.

---

## Check Your Understanding

1. Draw a minimal state diagram for one small kernel state machine (e.g.
   a subset of TCP or `net_device` registration states). What event
   triggers each transition?

2. Why might the kernel encode state as an enum instead of many booleans?

3. Find one place in the TCP code where `sk->sk_state` is checked in a
   switch. How many states does that switch handle?

4. What role does the bitmask `TCPF_*` serve that the plain enum doesn't?

---

Proceed to [Module 6: Factory and Singleton](06_factory_and_singleton.md).
