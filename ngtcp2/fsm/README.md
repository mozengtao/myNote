# ngtcp2 FSM Analysis: Mastering Modern State Machine Implementation

A comprehensive study of Finite State Machine (FSM) implementation patterns in the ngtcp2 QUIC library.

## Table of Contents

| Section | Title | Description |
|---------|-------|-------------|
| [01](01-fsm-role-in-quic.md) | FSM Role in QUIC | Why QUIC needs complex FSM, comparison with TCP |
| [02](02-state-modeling.md) | State Modeling | Where state is stored, layered state model |
| [03](03-events-and-drivers.md) | Events & Drivers | Network, timer, and API events |
| [04](04-transition-implementation.md) | Transition Implementation | How transitions are distributed across functions |
| [05](05-fsm-async-design.md) | FSM + Async Design | Integration with async I/O, timer handling |
| [06](06-error-states-teardown.md) | Error States & Teardown | CLOSING, DRAINING, and cleanup |
| [07](07-design-patterns.md) | Design Patterns | Event-driven, layered, guarded transitions |
| [08](08-comparison-with-tcp.md) | Comparison with TCP | Explicitness, extensibility, maintainability |
| [09](09-user-space-takeaways.md) | User-Space Takeaways | FSM design principles for async systems |
| [10](10-hands-on-design-task.md) | Hands-On Design Task | QUIC-inspired protocol FSM design exercise |

## Quick Reference

### ngtcp2 Primary States

```c
typedef enum {
  NGTCP2_CS_CLIENT_INITIAL,      // Client sends first Initial
  NGTCP2_CS_CLIENT_WAIT_HANDSHAKE, // Client waiting for handshake
  NGTCP2_CS_SERVER_INITIAL,      // Server receives first Initial
  NGTCP2_CS_SERVER_WAIT_HANDSHAKE, // Server waiting for handshake
  NGTCP2_CS_POST_HANDSHAKE,      // Normal 1-RTT operation
  NGTCP2_CS_CLOSING,             // Sent CONNECTION_CLOSE
  NGTCP2_CS_DRAINING,            // Received CONNECTION_CLOSE
} ngtcp2_conn_state;
```

### Key Design Patterns

1. **Layered FSM**: Primary states + Flags + Sub-FSMs
2. **Event-Driven**: State changes only on external events
3. **Timestamp Injection**: Application provides time
4. **Aggregated Timers**: Single expiry for all timers
5. **Guard Conditions**: Preconditions before transitions

### State Transition Diagram

```
CLIENT_INITIAL ──────────────────────────────┐
      │                                       │
      │ write_initial()                       │
      ▼                                       │
CLIENT_WAIT_HANDSHAKE ────────────────────────┤
      │                                       │
      │ handshake_complete                    │
      ▼                                       │
POST_HANDSHAKE ◀──────────────────────────────┤
      │                                       │
      ├───── send_close() ────▶ CLOSING ──────┤
      │                                       │
      └───── recv_close() ────▶ DRAINING ─────┘
```

## Prerequisites

- C programming knowledge
- Basic understanding of QUIC protocol
- Familiarity with async I/O patterns

## How to Use

1. Read sections 1-4 for understanding ngtcp2's FSM architecture
2. Read sections 5-6 for async integration and error handling
3. Read sections 7-8 for design patterns and comparison with TCP
4. Read sections 9-10 for applying lessons to your own protocols

## Files Analyzed

Primary source: `lib/ngtcp2_conn.c` (~14,000 lines)

Supporting files:
- `lib/ngtcp2_conn.h` - Connection structure and flags
- `lib/ngtcp2_pv.h` - Path validation sub-FSM
- `lib/ngtcp2_pktns_id.h` - Packet number space definitions

## Key Takeaways

1. **Avoid state explosion** with layered state model
2. **Separate state from I/O** for testability
3. **Inject timestamps** for deterministic behavior
4. **Use guard conditions** for safe transitions
5. **Design for evolution** with flags and sub-FSMs

---

*Last updated: January 2026*
*Based on ngtcp2 source code analysis*
