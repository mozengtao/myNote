# Identification Rules: State Machine Pattern

Five concrete rules to identify State Machine usage in Linux kernel source code.

---

## Rule 1: Look for State Enumerations

```
    SIGNAL: An enum defining mutually exclusive states

    /* TCP states */
    enum {
        TCP_ESTABLISHED = 1,
        TCP_SYN_SENT,
        TCP_SYN_RECV,
        TCP_FIN_WAIT1,
        TCP_CLOSE,
        TCP_LISTEN,
    };

    /* USB device states */
    enum usb_device_state {
        USB_STATE_NOTATTACHED = 0,
        USB_STATE_ATTACHED,
        USB_STATE_POWERED,
        USB_STATE_CONFIGURED,
    };

    NAMING: *_STATE_*, *_state, STATE_*
```

**中文说明：**

规则1：查找状态枚举——定义互斥状态的枚举，命名模式包括*_STATE_*等。

---

## Rule 2: State Field in Structure

```c
struct net_device {
    unsigned char operstate;  /* RFC 2863 */
    unsigned long state;
};

struct usb_device {
    enum usb_device_state state;
};

struct sock {
    volatile unsigned char sk_state;
};

/* Look for: "state" field, state enum type */
```

---

## Rule 3: State Transition Functions

```c
/* TCP state change */
static inline void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;
    /* Validation and side effects */
    sk->sk_state = state;
}

/* USB state change */
void usb_set_device_state(struct usb_device *udev,
                          enum usb_device_state new_state);

/* NAMING: set_*_state(), *_transition() */
```

**中文说明：**

规则3：状态转换函数——改变状态并验证的函数，命名如set_*_state()。

---

## Rule 4: Switch on State

```c
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
{
    switch (sk->sk_state) {
    case TCP_CLOSE:
        goto discard;
    case TCP_LISTEN:
        /* Handle SYN */
        break;
    case TCP_ESTABLISHED:
        /* Handle data */
        break;
    }
}
```

---

## Rule 5: State-Specific Behavior

```c
/* Operations valid only in certain states */
int tcp_sendmsg(struct sock *sk, ...)
{
    if (sk->sk_state != TCP_ESTABLISHED &&
        sk->sk_state != TCP_CLOSE_WAIT)
        return -ENOTCONN;
    ...
}
```

**中文说明：**

规则5：状态相关操作——只在特定状态有效的操作。

---

## Summary Checklist

```
    RULE 1: State enumeration exists
    RULE 2: State field in structure
    RULE 3: Transition functions with validation
    RULE 4: Switch statements on state
    RULE 5: State-dependent operations

    IF 3+ MATCH: State Machine Pattern
```

---

## Version

Based on **Linux kernel v3.2**.
