# FSM Analysis: How Wireshark Implements Stateful Protocol Parsers

A comprehensive study of Finite State Machine patterns used in Wireshark's protocol dissectors.

## Overview

```
+------------------------------------------------------------------+
|     WIRESHARK FSM IMPLEMENTATION: A DEEP DIVE                     |
+------------------------------------------------------------------+
|                                                                   |
|  This analysis examines how Wireshark's dissectors implement      |
|  stateful protocol parsing for complex protocols like:            |
|                                                                   |
|    - TCP (connection tracking, sequence analysis)                 |
|    - TLS/SSL (handshake state, encryption state)                  |
|    - HTTP/2 (stream multiplexing, HPACK state)                    |
|    - SMB/SMB2 (session, tree, file handle tracking)               |
|    - TWAMP (explicit FSM example)                                 |
|                                                                   |
|  Key finding: Wireshark primarily uses IMPLICIT FSMs with         |
|  flag-based state tracking rather than explicit state enums.      |
|                                                                   |
+------------------------------------------------------------------+
```

## Table of Contents

| Section | Title | Description |
|---------|-------|-------------|
| [01](01-why-fsm.md) | Why Protocol Parsers Need FSMs | Stateless vs stateful, why FSMs are necessary |
| [02](02-fsm-boundaries.md) | FSM Boundaries | State storage, conversations, session management |
| [03](03-state-representation.md) | State Representation | Structs, flags, avoiding single enum FSMs |
| [04](04-events-transitions.md) | Events & Transitions | Event sources, implicit vs explicit transitions |
| [05](05-error-handling.md) | Error Handling & Resync | Malformed packets, recovery, robustness |
| [06](06-performance.md) | Performance Constraints | Memory allocation, hash tables, speed |
| [07](07-patterns.md) | Patterns Observed | Implicit FSMs, table-driven, flag-based |
| [08](08-comparative.md) | Comparative Analysis | vs Linux TCP, vs textbook FSMs |
| [09](09-design-lessons.md) | Design Lessons | When implicit/explicit, evolution strategies |
| [10](10-hands-on-exercise.md) | Hands-On Exercise | Build a stateful protocol parser |

## Key Insights Summary

### 1. Implicit FSMs Dominate

```
Wireshark Pattern:                    Textbook Pattern:
==================                    =================
uint32_t flags;                       enum state_t state;
flags |= SAW_HANDSHAKE;               state = STATE_HANDSHAKING;

// Derive state                       // Check state
if (flags & ALL_HANDSHAKE_FLAGS)      switch(state) { ... }
```

### 2. Conversations as FSM Containers

```c
// Core abstraction for per-connection state
conversation_t *conv = find_or_create_conversation(pinfo);
my_state_t *state = conversation_get_proto_data(conv, proto_id);
```

### 3. Flag-Based State Tracking

```c
// TCP completeness (example)
#define TCP_COMPLETENESS_SYNSENT  0x01
#define TCP_COMPLETENESS_SYNACK   0x02
#define TCP_COMPLETENESS_ACK      0x04
#define TCP_COMPLETENESS_DATA     0x08

// "ESTABLISHED" = all three handshake flags set
```

### 4. Never Crash, Always Show Something

```c
// Wireshark philosophy
if (malformed) {
    expert_add_info(pinfo, item, &ei_malformed);
    // Continue parsing anyway!
}
```

## Source Files Analyzed

| File | Description |
|------|-------------|
| `epan/conversation.h` | Conversation API definition |
| `epan/conversation.c` | Conversation implementation |
| `epan/dissectors/packet-tcp.h` | TCP state structures |
| `epan/dissectors/packet-tcp.c` | TCP dissector implementation |
| `epan/dissectors/packet-tls-utils.h` | TLS state definitions |
| `epan/dissectors/packet-http2.c` | HTTP/2 session/stream state |
| `epan/dissectors/packet-smb2.c` | SMB2 session tracking |
| `epan/dissectors/packet-twamp.c` | Explicit FSM example |
| `epan/reassemble.h` | Fragment reassembly API |
| `epan/tvbparse.c` | Parsing utilities |

## Quick Reference: Core APIs

```c
// Find or create conversation
conversation_t *find_or_create_conversation(packet_info *pinfo);

// Attach protocol state to conversation
void conversation_add_proto_data(conversation_t *conv, int proto, void *data);

// Retrieve protocol state
void *conversation_get_proto_data(const conversation_t *conv, int proto);

// Memory allocation scopes
wmem_packet_scope()  // Per-packet temporary (fast, auto-freed)
wmem_file_scope()    // Per-capture file (persistent state)
wmem_epan_scope()    // Application lifetime (static data)

// Expert information (error flagging)
expert_add_info(pinfo, proto_item, &ei_error_type);
```

## Authors & References

- Analysis based on Wireshark source code (master branch)
- References: RFC 793 (TCP), RFC 8446 (TLS 1.3), RFC 7540 (HTTP/2)
- Wireshark Developer Guide: https://www.wireshark.org/docs/wsdg_html/

---

## 中文概述

### Wireshark FSM实现深度分析

本文档分析了Wireshark如何为复杂协议（TCP、TLS、HTTP/2、SMB等）实现有状态协议解析。

**主要发现：**
1. Wireshark主要使用隐式FSM，通过标志位追踪状态
2. "conversation"是存储每连接状态的核心抽象
3. 设计哲学：永不崩溃，总是显示信息，标记而非拒绝

**目录：**
1. 为什么需要FSM
2. FSM边界与状态存储
3. 状态表示方法
4. 事件与状态转换
5. 错误处理与重同步
6. 性能约束
7. 观察到的FSM模式
8. 与教科书FSM的比较
9. 用户空间设计经验
10. 实践练习

每个章节都包含ASCII图表和中文解释。
