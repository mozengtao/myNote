# Case 2: TCP Connection States

The TCP state machine is a canonical example of protocol-defined state management in the Linux kernel.

---

## Subsystem Context

```
+=============================================================================+
|                    TCP STATE MACHINE (RFC 793)                               |
+=============================================================================+

    TCP connections follow a well-defined state machine
    specified in RFC 793. Linux implements this exactly.

    States handle:
    - Connection establishment (3-way handshake)
    - Data transfer
    - Connection termination (4-way teardown)
    - Error conditions


    TCP STATE DIAGRAM:
    ==================

                              CLOSED
                                |
                    +-----------+-----------+
                    |                       |
              passive open            active open
                    |                       |
                    v                       v
                 LISTEN               SYN_SENT
                    |                       |
               rcv SYN                 rcv SYN,ACK
              send SYN,ACK             send ACK
                    |                       |
                    v                       v
               SYN_RCVD  -------->   ESTABLISHED
                          rcv ACK          |
                                    close/rcv FIN
                                           |
                    +----------------------+
                    |                      |
              close |                      | rcv FIN
                    v                      v
              FIN_WAIT_1             CLOSE_WAIT
                    |                      |
              rcv ACK                  close
                    |                      |
                    v                      v
              FIN_WAIT_2              LAST_ACK
                    |                      |
              rcv FIN                  rcv ACK
                    |                      |
                    v                      v
              TIME_WAIT                 CLOSED
                    |
              timeout
                    |
                    v
                 CLOSED
```

**中文说明：**

TCP状态机（RFC 793）：TCP连接遵循RFC 793定义的状态机。Linux完全实现了这个状态机。状态处理连接建立（三次握手）、数据传输、连接终止（四次挥手）、错误条件。

---

## State Definitions

```c
/* include/net/tcp_states.h */
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
    TCP_CLOSING,    /* RFC 1122 */
    TCP_MAX_STATES  /* For state count */
};

#define TCP_STATE_MASK  0xF
```

---

## Key Functions

```c
/* State change function - net/ipv4/tcp.c */
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;

    switch (state) {
    case TCP_ESTABLISHED:
        if (oldstate != TCP_SYN_SENT &&
            oldstate != TCP_SYN_RECV)
            break;
        /* Fall through for valid transitions */
    case TCP_SYN_RECV:
    case TCP_FIN_WAIT1:
    case TCP_CLOSE_WAIT:
        /* ... */
    }

    /* Update state */
    sk->sk_state = state;

    /* Actions on entering certain states */
    switch (state) {
    case TCP_ESTABLISHED:
        /* Update socket statistics */
        break;
    case TCP_CLOSE:
        /* Release socket resources */
        sk->sk_prot->unhash(sk);
        break;
    }
}

/* State-driven packet processing */
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
{
    switch (sk->sk_state) {
    case TCP_CLOSE:
        goto discard;
        
    case TCP_LISTEN:
        if (th->syn) {
            /* Create child socket */
            tcp_v4_conn_request(sk, skb);
        }
        goto discard;
        
    case TCP_SYN_SENT:
        /* Expecting SYN-ACK */
        if (th->ack && th->syn)
            tcp_set_state(sk, TCP_ESTABLISHED);
        break;
    }
    return 0;
}
```

**中文说明：**

关键函数：tcp_set_state()改变状态并执行进入/退出动作；tcp_rcv_state_process()根据当前状态处理收到的数据包。不同状态对数据包有不同处理方式。

---

## Minimal C Simulation

```c
/* Simplified TCP state machine simulation */

#include <stdio.h>

/* TCP states */
enum tcp_state {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
};

const char *tcp_state_name(enum tcp_state s)
{
    static const char *names[] = {
        "CLOSED", "LISTEN", "SYN_SENT", "SYN_RECV",
        "ESTABLISHED", "FIN_WAIT1", "FIN_WAIT2",
        "CLOSE_WAIT", "LAST_ACK", "TIME_WAIT"
    };
    return names[s];
}

/* TCP events */
enum tcp_event {
    EV_PASSIVE_OPEN,   /* Server: listen() */
    EV_ACTIVE_OPEN,    /* Client: connect() */
    EV_CLOSE,          /* close() */
    EV_RCV_SYN,        /* Received SYN */
    EV_RCV_SYN_ACK,    /* Received SYN+ACK */
    EV_RCV_ACK,        /* Received ACK */
    EV_RCV_FIN,        /* Received FIN */
    EV_TIMEOUT,        /* Timer expired */
};

/* TCP socket (simplified) */
struct tcp_sock {
    enum tcp_state state;
    int socket_id;
};

/* State transition function */
int tcp_handle_event(struct tcp_sock *sk, enum tcp_event event)
{
    enum tcp_state old_state = sk->state;
    enum tcp_state new_state = old_state;
    
    printf("[TCP %d] Event in state %s: ", 
           sk->socket_id, tcp_state_name(old_state));
    
    switch (sk->state) {
    case TCP_CLOSED:
        switch (event) {
        case EV_PASSIVE_OPEN:
            printf("passive open -> LISTEN\n");
            new_state = TCP_LISTEN;
            break;
        case EV_ACTIVE_OPEN:
            printf("active open, send SYN -> SYN_SENT\n");
            new_state = TCP_SYN_SENT;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_LISTEN:
        switch (event) {
        case EV_RCV_SYN:
            printf("rcv SYN, send SYN+ACK -> SYN_RECV\n");
            new_state = TCP_SYN_RECV;
            break;
        case EV_CLOSE:
            printf("close -> CLOSED\n");
            new_state = TCP_CLOSED;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_SYN_SENT:
        switch (event) {
        case EV_RCV_SYN_ACK:
            printf("rcv SYN+ACK, send ACK -> ESTABLISHED\n");
            new_state = TCP_ESTABLISHED;
            break;
        case EV_CLOSE:
            printf("close -> CLOSED\n");
            new_state = TCP_CLOSED;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_SYN_RECV:
        switch (event) {
        case EV_RCV_ACK:
            printf("rcv ACK -> ESTABLISHED\n");
            new_state = TCP_ESTABLISHED;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_ESTABLISHED:
        switch (event) {
        case EV_CLOSE:
            printf("close, send FIN -> FIN_WAIT1\n");
            new_state = TCP_FIN_WAIT1;
            break;
        case EV_RCV_FIN:
            printf("rcv FIN, send ACK -> CLOSE_WAIT\n");
            new_state = TCP_CLOSE_WAIT;
            break;
        default:
            printf("data transfer\n");
        }
        break;
        
    case TCP_FIN_WAIT1:
        switch (event) {
        case EV_RCV_ACK:
            printf("rcv ACK -> FIN_WAIT2\n");
            new_state = TCP_FIN_WAIT2;
            break;
        case EV_RCV_FIN:
            printf("rcv FIN, send ACK -> TIME_WAIT\n");
            new_state = TCP_TIME_WAIT;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_FIN_WAIT2:
        switch (event) {
        case EV_RCV_FIN:
            printf("rcv FIN, send ACK -> TIME_WAIT\n");
            new_state = TCP_TIME_WAIT;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_CLOSE_WAIT:
        switch (event) {
        case EV_CLOSE:
            printf("close, send FIN -> LAST_ACK\n");
            new_state = TCP_LAST_ACK;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_LAST_ACK:
        switch (event) {
        case EV_RCV_ACK:
            printf("rcv ACK -> CLOSED\n");
            new_state = TCP_CLOSED;
            break;
        default:
            printf("ignored\n");
        }
        break;
        
    case TCP_TIME_WAIT:
        switch (event) {
        case EV_TIMEOUT:
            printf("2MSL timeout -> CLOSED\n");
            new_state = TCP_CLOSED;
            break;
        default:
            printf("ignored\n");
        }
        break;
    }
    
    sk->state = new_state;
    return 0;
}

int main(void)
{
    struct tcp_sock server = { .state = TCP_CLOSED, .socket_id = 1 };
    struct tcp_sock client = { .state = TCP_CLOSED, .socket_id = 2 };
    
    printf("=== TCP STATE MACHINE SIMULATION ===\n\n");
    
    /* Server: listen() */
    printf("--- Server calls listen() ---\n");
    tcp_handle_event(&server, EV_PASSIVE_OPEN);
    
    /* Client: connect() */
    printf("\n--- Client calls connect() ---\n");
    tcp_handle_event(&client, EV_ACTIVE_OPEN);
    
    /* Server receives SYN */
    printf("\n--- Server receives SYN ---\n");
    tcp_handle_event(&server, EV_RCV_SYN);
    
    /* Client receives SYN+ACK */
    printf("\n--- Client receives SYN+ACK ---\n");
    tcp_handle_event(&client, EV_RCV_SYN_ACK);
    
    /* Server receives ACK */
    printf("\n--- Server receives ACK ---\n");
    tcp_handle_event(&server, EV_RCV_ACK);
    
    printf("\n--- Connection ESTABLISHED ---\n");
    printf("Server state: %s\n", tcp_state_name(server.state));
    printf("Client state: %s\n", tcp_state_name(client.state));
    
    /* Client initiates close */
    printf("\n--- Client calls close() ---\n");
    tcp_handle_event(&client, EV_CLOSE);
    
    /* Server receives FIN */
    printf("\n--- Server receives FIN ---\n");
    tcp_handle_event(&server, EV_RCV_FIN);
    
    /* Client receives ACK */
    printf("\n--- Client receives ACK ---\n");
    tcp_handle_event(&client, EV_RCV_ACK);
    
    /* Server calls close */
    printf("\n--- Server calls close() ---\n");
    tcp_handle_event(&server, EV_CLOSE);
    
    /* Client receives FIN */
    printf("\n--- Client receives FIN ---\n");
    tcp_handle_event(&client, EV_RCV_FIN);
    
    /* Server receives ACK */
    printf("\n--- Server receives ACK ---\n");
    tcp_handle_event(&server, EV_RCV_ACK);
    
    /* Client TIME_WAIT timeout */
    printf("\n--- Client TIME_WAIT expires ---\n");
    tcp_handle_event(&client, EV_TIMEOUT);
    
    printf("\n--- Final States ---\n");
    printf("Server: %s\n", tcp_state_name(server.state));
    printf("Client: %s\n", tcp_state_name(client.state));
    
    return 0;
}
```

---

## What Core Does NOT Control

```
    Core Controls:
    --------------
    [X] State definitions (enum)
    [X] Valid state transitions
    [X] Actions on state changes
    [X] Packet processing per state

    Core Does NOT Control:
    ----------------------
    [ ] When packets arrive
    [ ] Network conditions
    [ ] Remote peer behavior
    [ ] Application close timing
```

---

## Version

Based on **Linux kernel v3.2** and **RFC 793**.

Key files:
- `include/net/tcp_states.h` - State definitions
- `net/ipv4/tcp.c` - tcp_set_state()
- `net/ipv4/tcp_input.c` - tcp_rcv_state_process()
