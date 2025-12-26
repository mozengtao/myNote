# Linux Networking Subsystem: Lifecycle Management and State Machines

## 1. Socket Lifecycle

```
+------------------------------------------------------------------+
|  SOCKET LIFECYCLE: CREATE → BIND → CONNECT → CLOSE               |
+------------------------------------------------------------------+

    COMPLETE LIFECYCLE DIAGRAM:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   ┌───────────┐                                             │
    │   │  (none)   │                                             │
    │   └─────┬─────┘                                             │
    │         │ socket()                                          │
    │         ▼                                                   │
    │   ┌───────────┐                                             │
    │   │SS_UNCONNECTED│◄─────────────────────────────┐           │
    │   └─────┬─────┘                                 │           │
    │         │                                       │           │
    │    ┌────┴────┐                                  │           │
    │    │         │                                  │           │
    │    ▼         ▼                                  │           │
    │  bind()   connect()                             │           │
    │    │         │                                  │           │
    │    ▼         ▼                                  │           │
    │ (bound)  ┌───────────┐                          │           │
    │    │     │SS_CONNECTING│ (non-blocking)         │           │
    │    │     └─────┬─────┘                          │           │
    │    │           │ SYN+ACK received               │           │
    │    │           ▼                                │           │
    │    │     ┌───────────┐                          │           │
    │    └────►│SS_CONNECTED│ ◄── accept() creates   │           │
    │          └─────┬─────┘                          │           │
    │                │                                │           │
    │                │ shutdown() or close()          │           │
    │                ▼                                │           │
    │          ┌───────────────┐                      │           │
    │          │SS_DISCONNECTING│                     │           │
    │          └───────┬───────┘                      │           │
    │                  │                              │           │
    │                  │ close()                      │           │
    │                  ▼                              │           │
    │          ┌───────────┐                          │           │
    │          │  SS_FREE  │──────────────────────────┘           │
    │          └───────────┘                                      │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SOCKET STATE ENCODING:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* include/linux/net.h */                                 │
    │                                                              │
    │  typedef enum {                                             │
    │      SS_FREE = 0,          /* not allocated */             │
    │      SS_UNCONNECTED,       /* unconnected to any socket */ │
    │      SS_CONNECTING,        /* connecting in progress */    │
    │      SS_CONNECTED,         /* connected to socket */       │
    │      SS_DISCONNECTING      /* disconnecting in progress */ │
    │  } socket_state;                                            │
    │                                                              │
    │  struct socket {                                           │
    │      socket_state state;   /* THE STATE FIELD */           │
    │      short type;                                           │
    │      unsigned long flags;                                  │
    │      struct sock *sk;                                      │
    │      const struct proto_ops *ops;                          │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    STATE TRANSITIONS VIA CALL ORDER:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  socket()  → Sets state = SS_UNCONNECTED                   │
    │  bind()    → State unchanged (still SS_UNCONNECTED)        │
    │  listen()  → State unchanged, but sk->sk_state = TCP_LISTEN│
    │  connect() → Sets state = SS_CONNECTING, then SS_CONNECTED │
    │  accept()  → New socket starts SS_CONNECTED                │
    │  close()   → Eventually SS_FREE (and socket freed)         │
    │                                                              │
    │  Note: socket_state is HIGH-LEVEL BSD state                │
    │        sk->sk_state is PROTOCOL-SPECIFIC state             │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. TCP Connection State Machine

```
+------------------------------------------------------------------+
|  TCP STATE MACHINE (RFC 793)                                     |
+------------------------------------------------------------------+

    COMPLETE STATE DIAGRAM:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │                      ┌────────────┐                         │
    │                      │   CLOSED   │                         │
    │                      └──────┬─────┘                         │
    │              ┌──────────────┼──────────────┐                │
    │              │ passive open │ active open  │                │
    │              │   (listen)   │  (connect)   │                │
    │              ▼              │              ▼                │
    │        ┌──────────┐        │        ┌───────────┐          │
    │        │  LISTEN  │        │        │ SYN_SENT  │          │
    │        └────┬─────┘        │        └─────┬─────┘          │
    │             │ rcv SYN      │              │ rcv SYN+ACK    │
    │             │ send SYN+ACK │              │ send ACK       │
    │             ▼              │              ▼                │
    │        ┌──────────┐        │        ┌───────────┐          │
    │        │ SYN_RCVD │        │        │           │          │
    │        └────┬─────┘        │        │           │          │
    │             │ rcv ACK      │        │           │          │
    │             └───────────►ESTABLISHED◄───────────┘          │
    │                          ┌────┴────┐                        │
    │                          │         │                        │
    │              ┌───────────┤         ├───────────┐           │
    │     close    │           │         │           │ rcv FIN   │
    │     send FIN │           │         │           │ send ACK  │
    │              ▼           │         │           ▼           │
    │        ┌───────────┐     │         │     ┌───────────┐     │
    │        │FIN_WAIT_1 │     │         │     │CLOSE_WAIT │     │
    │        └─────┬─────┘     │         │     └─────┬─────┘     │
    │   rcv ACK    │           │         │           │ close     │
    │              ▼           │         │           │ send FIN  │
    │        ┌───────────┐     │         │           ▼           │
    │        │FIN_WAIT_2 │     │         │     ┌───────────┐     │
    │        └─────┬─────┘     │         │     │ LAST_ACK  │     │
    │   rcv FIN    │           │         │     └─────┬─────┘     │
    │   send ACK   │           │         │    rcv ACK│           │
    │              ▼           │         │           ▼           │
    │        ┌───────────┐     │         │     ┌───────────┐     │
    │        │TIME_WAIT  │─────┴─────────┴────►│  CLOSED   │     │
    │        └───────────┘     2MSL timeout    └───────────┘     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STATE ENCODING:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* include/net/tcp_states.h */                            │
    │                                                              │
    │  enum {                                                     │
    │      TCP_ESTABLISHED = 1,                                  │
    │      TCP_SYN_SENT,                                         │
    │      TCP_SYN_RECV,                                         │
    │      TCP_FIN_WAIT1,                                        │
    │      TCP_FIN_WAIT2,                                        │
    │      TCP_TIME_WAIT,                                        │
    │      TCP_CLOSE,                                            │
    │      TCP_CLOSE_WAIT,                                       │
    │      TCP_LAST_ACK,                                         │
    │      TCP_LISTEN,                                           │
    │      TCP_CLOSING,                                          │
    │      TCP_NEW_SYN_RECV,                                     │
    │      TCP_MAX_STATES                                        │
    │  };                                                         │
    │                                                              │
    │  /* Stored in: */                                          │
    │  struct sock {                                              │
    │      struct sock_common __sk_common;                       │
    │      /* sk_state is in sock_common: */                     │
    │      /* volatile unsigned char skc_state; */               │
    │  };                                                         │
    │  #define sk_state __sk_common.skc_state                    │
    └─────────────────────────────────────────────────────────────┘

    STATE TRANSITIONS IN CODE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/tcp_input.c - tcp_rcv_state_process() */      │
    │                                                              │
    │  int tcp_rcv_state_process(struct sock *sk,                │
    │                            struct sk_buff *skb,             │
    │                            const struct tcphdr *th,         │
    │                            unsigned int len)                │
    │  {                                                          │
    │      switch (sk->sk_state) {                               │
    │      case TCP_CLOSE:                                       │
    │          /* Can't receive on closed socket */              │
    │          goto discard;                                      │
    │                                                              │
    │      case TCP_LISTEN:                                      │
    │          if (th->ack)                                      │
    │              return 1;  /* Send RST */                     │
    │          if (th->syn) {                                    │
    │              /* Handle incoming connection */              │
    │              /* Create child socket in SYN_RECV */         │
    │          }                                                   │
    │          break;                                             │
    │                                                              │
    │      case TCP_SYN_SENT:                                    │
    │          /* Waiting for SYN+ACK */                         │
    │          queued = tcp_rcv_synsent_state_process(...);      │
    │          /* If successful, transition to ESTABLISHED */   │
    │          break;                                             │
    │                                                              │
    │      case TCP_SYN_RECV:                                    │
    │          /* Received SYN, sent SYN+ACK, waiting ACK */     │
    │          if (th->ack && !th->syn) {                        │
    │              tcp_set_state(sk, TCP_ESTABLISHED);           │
    │          }                                                   │
    │          break;                                             │
    │                                                              │
    │      /* ... other states ... */                            │
    │      }                                                       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    STATE CHANGE HELPER:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/tcp.c */                                      │
    │                                                              │
    │  void tcp_set_state(struct sock *sk, int state)            │
    │  {                                                          │
    │      int oldstate = sk->sk_state;                          │
    │                                                              │
    │      /* Notify state change */                             │
    │      switch (state) {                                      │
    │      case TCP_ESTABLISHED:                                 │
    │          if (oldstate != TCP_ESTABLISHED)                  │
    │              TCP_INC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);│
    │          break;                                             │
    │                                                              │
    │      case TCP_CLOSE:                                       │
    │          /* Various cleanup */                             │
    │          break;                                             │
    │      }                                                       │
    │                                                              │
    │      sk->sk_state = state;  /* THE STATE CHANGE */         │
    │                                                              │
    │      /* Wake up waiters */                                 │
    │      sk_state_change(sk);                                  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Why Explicit FSM Tables Are Rarely Used

```
+------------------------------------------------------------------+
|  IMPLICIT vs EXPLICIT STATE MACHINES                             |
+------------------------------------------------------------------+

    EXPLICIT FSM (Textbook Approach - NOT USED):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* Hypothetical explicit FSM table */                     │
    │  struct state_transition {                                 │
    │      int current_state;                                    │
    │      int event;                                             │
    │      int next_state;                                       │
    │      int (*action)(struct sock *sk, ...);                  │
    │  };                                                         │
    │                                                              │
    │  struct state_transition tcp_fsm[] = {                     │
    │      { TCP_CLOSED, EV_CONNECT, TCP_SYN_SENT, send_syn },   │
    │      { TCP_SYN_SENT, EV_SYN_ACK, TCP_ESTABLISHED, send_ack },│
    │      { TCP_LISTEN, EV_SYN, TCP_SYN_RECV, send_syn_ack },   │
    │      /* ... 100+ transitions ... */                        │
    │  };                                                         │
    │                                                              │
    │  void tcp_process_event(struct sock *sk, int event) {      │
    │      for (t = tcp_fsm; t->current_state != END; t++) {     │
    │          if (t->current_state == sk->sk_state &&           │
    │              t->event == event) {                           │
    │              sk->sk_state = t->next_state;                 │
    │              t->action(sk, ...);                           │
    │              return;                                        │
    │          }                                                   │
    │      }                                                       │
    │  }                                                          │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    WHY LINUX USES IMPLICIT FSM INSTEAD:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  1. COMPLEX GUARDS                                         │
    │     Transitions depend on many conditions:                 │
    │       • TCP flags combination                              │
    │       • Sequence number validity                           │
    │       • Window size                                        │
    │       • Timestamps                                         │
    │     A simple event enum can't capture this.                │
    │                                                              │
    │  2. RICH ACTIONS                                           │
    │     Each transition may:                                   │
    │       • Update multiple state variables                    │
    │       • Send packets                                       │
    │       • Set timers                                         │
    │       • Update statistics                                  │
    │     Too complex for action function pointer.               │
    │                                                              │
    │  3. PERFORMANCE                                            │
    │     • switch() compiles to jump table                      │
    │     • No table lookup overhead                             │
    │     • Compiler can inline code                             │
    │     • Branch predictor friendly                            │
    │                                                              │
    │  4. READABILITY                                            │
    │     Code in switch case directly shows:                    │
    │       • What state we're in                                │
    │       • What packet we received                            │
    │       • What we do in response                             │
    │     All in one place.                                      │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    ACTUAL LINUX PATTERN:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* State is checked with switch/if */                     │
    │  switch (sk->sk_state) {                                   │
    │  case TCP_ESTABLISHED:                                     │
    │      /* Handle packet in established connection */         │
    │      if (th->fin) {                                        │
    │          /* Transition to CLOSE_WAIT */                    │
    │          tcp_set_state(sk, TCP_CLOSE_WAIT);                │
    │          /* Actions: ACK the FIN, notify userspace */      │
    │      }                                                      │
    │      break;                                                 │
    │                                                              │
    │  case TCP_FIN_WAIT1:                                       │
    │      /* Complex guard conditions */                        │
    │      if (th->ack && after(ack, tp->snd_una)) {             │
    │          if (th->fin) {                                    │
    │              /* Simultaneous close */                      │
    │              tcp_set_state(sk, TCP_CLOSING);               │
    │          } else {                                           │
    │              tcp_set_state(sk, TCP_FIN_WAIT2);             │
    │          }                                                   │
    │      }                                                       │
    │      break;                                                 │
    │  }                                                          │
    │                                                              │
    │  /* This is more readable and efficient than FSM table */  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Network Device Lifecycle

```
+------------------------------------------------------------------+
|  NET_DEVICE LIFECYCLE                                            |
+------------------------------------------------------------------+

    STATE TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   ┌──────────┐                                              │
    │   │ (none)   │                                              │
    │   └────┬─────┘                                              │
    │        │ alloc_netdev()                                     │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │ALLOCATED │ ◄─── Device struct exists                   │
    │   └────┬─────┘      netdev_ops assigned                    │
    │        │ register_netdev()                                  │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │REGISTERED│ ◄─── Visible in /sys/class/net              │
    │   └────┬─────┘      Can be configured                      │
    │        │ dev_open() via "ip link set up"                    │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │  UP/     │ ◄─── IFF_UP flag set                        │
    │   │ RUNNING  │      Can transmit/receive                    │
    │   └────┬─────┘                                              │
    │        │ dev_close() via "ip link set down"                 │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │  DOWN    │ ◄─── IFF_UP cleared                         │
    │   └────┬─────┘      Hardware stopped                       │
    │        │ unregister_netdev()                                │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │UNREGISTERING│ ◄─── Removing from stack                 │
    │   └────┬─────┘      Waiting for references                 │
    │        │ RCU grace period                                   │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │UNREGISTERED│ ◄─── Removed from lists                   │
    │   └────┬─────┘      Still allocated                        │
    │        │ free_netdev()                                      │
    │        ▼                                                    │
    │   ┌──────────┐                                              │
    │   │  FREED   │                                              │
    │   └──────────┘                                              │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    STATE ENCODING:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* include/linux/netdevice.h */                           │
    │                                                              │
    │  /* Device state bits in dev->state */                     │
    │  enum netdev_state_t {                                     │
    │      __LINK_STATE_START,      /* Being opened */           │
    │      __LINK_STATE_PRESENT,    /* Device present */         │
    │      __LINK_STATE_NOCARRIER,  /* No link detected */       │
    │      __LINK_STATE_LINKWATCH_PENDING,                       │
    │      __LINK_STATE_DORMANT,    /* Waiting for event */      │
    │  };                                                         │
    │                                                              │
    │  /* Device flags (dev->flags) */                           │
    │  IFF_UP         /* Interface is up */                      │
    │  IFF_BROADCAST  /* Broadcast capability */                 │
    │  IFF_DEBUG      /* Debugging enabled */                    │
    │  IFF_LOOPBACK   /* Is loopback */                          │
    │  IFF_PROMISC    /* Promiscuous mode */                     │
    │  IFF_RUNNING    /* Resources allocated */                  │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. sk_buff Lifecycle

```
+------------------------------------------------------------------+
|  SK_BUFF LIFECYCLE                                               |
+------------------------------------------------------------------+

    RECEIVE PATH LIFECYCLE:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ alloc_skb() / netdev_alloc_skb()                      │ │
    │   │   → users = 1, cloned = 0                             │ │
    │   │   → Driver owns skb                                   │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ netif_receive_skb()               │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ IN STACK                                              │ │
    │   │   → Stack owns skb                                    │ │
    │   │   → May be cloned for tcpdump                         │ │
    │   │   → Headers parsed, data pointer adjusted             │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ skb_queue_tail(sk->sk_receive_queue)
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ IN SOCKET QUEUE                                       │ │
    │   │   → Socket owns skb                                   │ │
    │   │   → Waiting for recvmsg()                             │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ recvmsg() → copy to user          │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ kfree_skb() / consume_skb()                           │ │
    │   │   → users--, if 0: free                               │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    TRANSMIT PATH LIFECYCLE:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ sock_alloc_send_skb() (from sendmsg)                  │ │
    │   │   → Socket allocates skb                              │ │
    │   │   → Data copied from user                             │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ tcp_transmit_skb()                │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ IN PROTOCOL LAYER                                     │ │
    │   │   → TCP adds headers                                  │ │
    │   │   → May be cloned for retransmission                  │ │
    │   │   → skb may stay in retransmit queue                  │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ dev_queue_xmit()                  │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ IN QDISC QUEUE                                        │ │
    │   │   → Traffic control owns skb                          │ │
    │   │   → May be delayed/shaped                             │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                         │                                    │
    │                         ▼ dev->netdev_ops->ndo_start_xmit() │
    │   ┌───────────────────────────────────────────────────────┐ │
    │   │ IN DRIVER                                             │ │
    │   │   → Driver owns skb                                   │ │
    │   │   → DMA to hardware                                   │ │
    │   │   → dev_kfree_skb() after TX completion               │ │
    │   └───────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  LIFECYCLE MANAGEMENT SUMMARY                                    |
+------------------------------------------------------------------+

    SOCKET LIFECYCLE:
    ┌─────────────────────────────────────────────────────────────┐
    │  SS_UNCONNECTED → SS_CONNECTING → SS_CONNECTED             │
    │  → SS_DISCONNECTING → SS_FREE                              │
    │                                                              │
    │  State in: struct socket.state                             │
    │  Transitions via: system calls (connect, close, ...)       │
    └─────────────────────────────────────────────────────────────┘

    TCP STATE MACHINE:
    ┌─────────────────────────────────────────────────────────────┐
    │  CLOSED → LISTEN/SYN_SENT → ESTABLISHED                    │
    │  → FIN_WAIT_1 → TIME_WAIT → CLOSED                        │
    │                                                              │
    │  State in: struct sock.sk_state                            │
    │  Transitions via: packet events + tcp_set_state()          │
    │  Encoded as: switch/case (not explicit FSM table)          │
    └─────────────────────────────────────────────────────────────┘

    NET_DEVICE LIFECYCLE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ALLOCATED → REGISTERED → UP → DOWN → UNREGISTERED → FREED│
    │                                                              │
    │  State in: dev->state bits + dev->flags                    │
    │  Transitions via: register/unregister, open/close          │
    └─────────────────────────────────────────────────────────────┘

    SK_BUFF LIFECYCLE:
    ┌─────────────────────────────────────────────────────────────┐
    │  alloc → owned by layer → queued → processed → freed      │
    │                                                              │
    │  Ownership: Explicit transfer between layers               │
    │  Reference counting: users field + dataref for sharing     │
    └─────────────────────────────────────────────────────────────┘

    WHY IMPLICIT FSM:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Complex guards (many conditions per transition)         │
    │  • Rich actions (side effects beyond state change)         │
    │  • Performance (switch compiles to jump table)             │
    │  • Readability (all logic for state in one place)          │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **套接字生命周期**：SS_UNCONNECTED→SS_CONNECTED→SS_FREE，状态在socket.state
- **TCP状态机**：完整RFC 793状态，存储在sk->sk_state，通过tcp_set_state()转换
- **设备生命周期**：分配→注册→UP→DOWN→注销→释放，状态在dev->state/flags
- **sk_buff生命周期**：分配→层间传递→队列→处理→释放，所有权显式传递
- **为何用隐式FSM**：复杂守卫条件、丰富动作、switch性能好、可读性强

