# SCALE | Hierarchical FSMs (Avoiding State Explosion)

## 1. When Flat FSMs Break Down

```
STATE EXPLOSION SYMPTOMS
========================

Flat FSM for Media Player:

States: STOPPED, PLAYING, PAUSED
Modes:  NORMAL, SHUFFLE, REPEAT

Combined states (flat):
  STOPPED_NORMAL, STOPPED_SHUFFLE, STOPPED_REPEAT,
  PLAYING_NORMAL, PLAYING_SHUFFLE, PLAYING_REPEAT,
  PAUSED_NORMAL, PAUSED_SHUFFLE, PAUSED_REPEAT

3 states × 3 modes = 9 combined states
Add MUTED mode: 9 × 2 = 18 states
Add EQUALIZER_ON: 18 × 2 = 36 states

STATE EXPLOSION!

+----------------------------------------------------------+
| N independent dimensions with M options each:             |
| Total states = M₁ × M₂ × M₃ × ... × Mₙ                   |
|                                                          |
| 3 dimensions × 3 options = 27 states                     |
| 4 dimensions × 3 options = 81 states                     |
| 5 dimensions × 3 options = 243 states                    |
+----------------------------------------------------------+
```

```
REPEATED TRANSITION LOGIC
=========================

In a flat FSM, same logic appears multiple times:

STOPPED_NORMAL + PLAY -> PLAYING_NORMAL
STOPPED_SHUFFLE + PLAY -> PLAYING_SHUFFLE
STOPPED_REPEAT + PLAY -> PLAYING_REPEAT

All three do the same thing: start playing!
But each is a separate transition.

+----------------------------------------------------------+
| If adding a new feature requires adding transitions       |
| to EVERY existing state, your FSM is too flat.           |
+----------------------------------------------------------+


Visual: Repeated Transitions

+----------------+                 +----------------+
| STOPPED_NORMAL |----PLAY-------->| PLAYING_NORMAL |
+----------------+                 +----------------+

+----------------+                 +----------------+
| STOPPED_SHUFFLE|----PLAY-------->| PLAYING_SHUFFLE|
+----------------+                 +----------------+

+----------------+                 +----------------+
| STOPPED_REPEAT |----PLAY-------->| PLAYING_REPEAT |
+----------------+                 +----------------+

All identical logic, replicated 3 times.
With more modes: 10, 20, 100 times...
```

```
WHEN TO SUSPECT STATE EXPLOSION
===============================

Checklist:
[ ] State names contain multiple concepts (PLAYING_SHUFFLE_MUTED)
[ ] Adding one feature multiplies states
[ ] Same transition appears in multiple state pairs
[ ] State diagram becomes "grid-like" pattern
[ ] Transition table is mostly repetitive

Grid Pattern (symptom):

    STOPPED     PLAYING     PAUSED
      |           |           |
NORMAL--+---------+-----------+-------
      |           |           |
SHUFFLE-+---------+-----------+-------
      |           |           |
REPEAT--+---------+-----------+-------

Every intersection is a state.
This is NOT a natural FSM structure.
```

---

## 2. Parent-Child State Relationships

```
HIERARCHICAL FSM STRUCTURE
==========================

Instead of: PLAYING_NORMAL, PLAYING_SHUFFLE, PLAYING_REPEAT

Use nested states:
+--------------------------------------------+
|                  PLAYING                    |
|  +----------+ +----------+ +----------+    |
|  |  NORMAL  | | SHUFFLE  | |  REPEAT  |    |
|  +----------+ +----------+ +----------+    |
+--------------------------------------------+

The "PLAYING" state contains sub-states.
PLAY/PAUSE/STOP transitions apply to PLAYING as a whole.
MODE_CHANGE transitions only change the sub-state.


Parent vs Child Events:
  - PLAY, PAUSE, STOP: Handled by parent (playback state)
  - SET_SHUFFLE, SET_REPEAT: Handled by child (mode state)


                +------- STOPPED -------+
                |                       |
           PLAY |                       | STOP
                v                       |
        +-----------------+             |
        |     PLAYING     |-------------+
        |  +-----------+  |
        |  | Mode FSM  |  |  <-- Nested FSM
        |  +-----------+  |
        +-----------------+
                |
          PAUSE |
                v
        +-----------------+
        |     PAUSED      |
        |  +-----------+  |
        |  | Mode FSM  |  |  <-- Same nested FSM
        |  +-----------+  |
        +-----------------+
```

```
SHARED TRANSITIONS (PARENT LEVEL)
=================================

Parent state handles events for ALL children:

+-------------------------------------------------+
|                    PLAYING                       |
|                                                 |
|   +--------+     +--------+     +--------+      |
|   | NORMAL |     | SHUFFLE|     | REPEAT |      |
|   +--------+     +--------+     +--------+      |
|                                                 |
+-------------------------------------------------+
        |
        | STOP (applies to entire PLAYING state)
        v
+-------------------------------------------------+
|                    STOPPED                       |
+-------------------------------------------------+


In code:
  /* Parent FSM handles STOP regardless of child state */
  if (parent_state == PLAYING && event == STOP) {
      parent_state = STOPPED;
      /* Child state preserved or reset */
  }

  /* Child FSM handles mode changes */
  if (event == SET_SHUFFLE) {
      child_state = SHUFFLE;
      /* Parent state unchanged */
  }
```

```
OVERRIDE RULES
==============

What happens when both parent and child can handle an event?

Option 1: Child First (Override)
  - Child tries to handle event
  - If child handles it, done
  - If child doesn't handle, parent tries
  
Option 2: Parent First (Intercept)
  - Parent tries to handle event
  - If parent handles it, done
  - If parent doesn't handle, child tries
  
Option 3: Explicit Delegation
  - Event explicitly tagged for parent or child
  - No ambiguity


RECOMMENDATION: Option 1 (Child First)
  - More intuitive
  - Child can "specialize" parent behavior
  - Parent provides "default" behavior

+----------------------------------------------------------+
|  int dispatch(hierarchical_fsm_t *h, event_t e) {        |
|      /* Try child first */                               |
|      if (child_can_handle(h->child, e)) {                |
|          return child_dispatch(h->child, e);             |
|      }                                                   |
|                                                          |
|      /* Fall back to parent */                           |
|      return parent_dispatch(h->parent, e);               |
|  }                                                       |
+----------------------------------------------------------+
```

```c
/* Hierarchical FSM Implementation */

/* Parent states */
typedef enum {
    PARENT_STOPPED,
    PARENT_PLAYING,
    PARENT_PAUSED,
    PARENT_COUNT
} parent_state_t;

/* Child states (mode) */
typedef enum {
    MODE_NORMAL,
    MODE_SHUFFLE,
    MODE_REPEAT,
    MODE_COUNT
} mode_state_t;

/* Events */
typedef enum {
    EVENT_PLAY,
    EVENT_STOP,
    EVENT_PAUSE,
    EVENT_SET_NORMAL,
    EVENT_SET_SHUFFLE,
    EVENT_SET_REPEAT,
    EVENT_COUNT
} event_t;

/* Hierarchical FSM structure */
typedef struct {
    parent_state_t parent_state;
    mode_state_t mode_state;
} media_player_t;

/* Parent transition table */
static const parent_state_t parent_table[PARENT_COUNT][EVENT_COUNT] = {
    /*               PLAY            STOP            PAUSE           SET_NORMAL  SET_SHUFFLE SET_REPEAT */
    [PARENT_STOPPED] = {PARENT_PLAYING, PARENT_STOPPED, PARENT_STOPPED, PARENT_COUNT, PARENT_COUNT, PARENT_COUNT},
    [PARENT_PLAYING] = {PARENT_PLAYING, PARENT_STOPPED, PARENT_PAUSED,  PARENT_COUNT, PARENT_COUNT, PARENT_COUNT},
    [PARENT_PAUSED]  = {PARENT_PLAYING, PARENT_STOPPED, PARENT_PAUSED,  PARENT_COUNT, PARENT_COUNT, PARENT_COUNT},
};
/* PARENT_COUNT means "not handled by parent" */

/* Mode transition table */
static const mode_state_t mode_table[MODE_COUNT][EVENT_COUNT] = {
    /*             PLAY       STOP       PAUSE      SET_NORMAL   SET_SHUFFLE  SET_REPEAT */
    [MODE_NORMAL]  = {MODE_COUNT, MODE_COUNT, MODE_COUNT, MODE_NORMAL, MODE_SHUFFLE, MODE_REPEAT},
    [MODE_SHUFFLE] = {MODE_COUNT, MODE_COUNT, MODE_COUNT, MODE_NORMAL, MODE_SHUFFLE, MODE_REPEAT},
    [MODE_REPEAT]  = {MODE_COUNT, MODE_COUNT, MODE_COUNT, MODE_NORMAL, MODE_SHUFFLE, MODE_REPEAT},
};
/* MODE_COUNT means "not handled by mode" */

/* Dispatch with hierarchy */
int media_dispatch(media_player_t *m, event_t e) {
    /* Try mode (child) first */
    mode_state_t new_mode = mode_table[m->mode_state][e];
    if (new_mode != MODE_COUNT) {
        m->mode_state = new_mode;
        return 0;
    }
    
    /* Try parent */
    parent_state_t new_parent = parent_table[m->parent_state][e];
    if (new_parent != PARENT_COUNT) {
        m->parent_state = new_parent;
        return 0;
    }
    
    /* Neither handled it - illegal */
    return -1;
}
```

---

## 3. Design Discipline

```
WHEN HIERARCHY IS JUSTIFIED
===========================

Justify hierarchy when:

1. ORTHOGONAL CONCERNS
   - Two independent dimensions of behavior
   - Example: Playback state (stop/play/pause) vs Mode (normal/shuffle)
   - Changes to one don't affect the other

2. REPEATED TRANSITIONS
   - Same transition in multiple states
   - Example: "STOP" works the same in all modes
   - Hierarchy eliminates duplication

3. NATURAL GROUPING
   - States form logical clusters
   - Example: All "playing" variants share behavior
   - Hierarchy reflects domain structure


JUSTIFIED:
+----------------------------------------------------------+
|  Playback State (STOPPED, PLAYING, PAUSED)               |
|      × Mode (NORMAL, SHUFFLE, REPEAT)                    |
|  = 2 independent dimensions, hierarchy appropriate       |
+----------------------------------------------------------+


NOT JUSTIFIED:
+----------------------------------------------------------+
|  TCP States (CLOSED, LISTEN, SYN_SENT, ESTABLISHED, ...) |
|  These are NOT orthogonal - strict sequence              |
|  Hierarchy would be artificial, use flat FSM             |
+----------------------------------------------------------+
```

```
WHEN HIERARCHY BECOMES HARMFUL
==============================

Avoid hierarchy when:

1. STATES ARE SEQUENTIAL, NOT ORTHOGONAL
   INIT -> CONNECTING -> CONNECTED -> CLOSING -> CLOSED
   This is a linear progression, not nested states.

2. "PARENT" HAS ONLY ONE CHILD
   +-------------+
   |   PARENT    |
   |  +-------+  |
   |  | CHILD |  |  <-- Pointless nesting!
   |  +-------+  |
   +-------------+
   Just use the child state directly.

3. CHILD BEHAVIOR DEPENDS ON PARENT
   If child transitions change based on parent state,
   they're not truly independent.
   
4. DEEP NESTING (> 2 levels)
   +------------------+
   |  Level 1         |
   |  +------------+  |
   |  | Level 2    |  |
   |  | +--------+ |  |
   |  | | Lvl 3  | |  |  <-- Too deep!
   |  | +--------+ |  |
   |  +------------+  |
   +------------------+
   
   Deep hierarchies are hard to reason about.
   If you need > 2 levels, reconsider design.
```

```
HIERARCHY DECISION TREE
=======================

                Do you have > 6 states?
                         |
              +----------+----------+
              |                     |
             NO                    YES
              |                     |
              v                     v
        Flat FSM OK          Are states orthogonal?
                                    |
                         +----------+----------+
                         |                     |
                        NO                    YES
                         |                     |
                         v                     v
                  Flat FSM              Can you identify
                  (sequential)          2 independent FSMs?
                                              |
                                   +----------+----------+
                                   |                     |
                                  NO                    YES
                                   |                     |
                                   v                     v
                             Flat FSM            Use Hierarchy
                             (accept              (2 levels max)
                              complexity)
```

```
HIERARCHY CHECKLIST
===================

Before implementing hierarchical FSM:

[ ] Can I name the parent FSM independently?
    (e.g., "Playback FSM", not "Outer State")

[ ] Can I name the child FSM independently?
    (e.g., "Mode FSM", not "Inner State")

[ ] Are parent and child events disjoint?
    (Different events for each, mostly)

[ ] Does parent transition work for ALL children?
    (STOP works regardless of mode)

[ ] Is the nesting at most 2 levels deep?
    (No grandchild states)

[ ] Would flat FSM have > 10 states?
    (Otherwise, flat is simpler)


If ANY answer is NO, reconsider hierarchy.
```

```c
/* When NOT to use hierarchy - TCP example */

/* TCP states are SEQUENTIAL, not orthogonal */
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
    TCP_COUNT
} tcp_state_t;

/* This is 11 states, but hierarchy would be WRONG:
   
   WRONG attempt at hierarchy:
   +-- DISCONNECTED (CLOSED, LISTEN) --+
   +-- CONNECTING (SYN_SENT, SYN_RECEIVED) --+
   +-- CONNECTED (ESTABLISHED) --+
   +-- DISCONNECTING (FIN_WAIT_1, FIN_WAIT_2, ...) --+
   
   Problem: Transitions don't align with "parent" boundaries
   ESTABLISHED can go to FIN_WAIT_1 (different "parent")
   
   Solution: Keep it FLAT. 11 states is manageable.
*/

/* Flat transition table - correct for TCP */
static const tcp_state_t tcp_table[TCP_COUNT][EVENT_COUNT] = {
    /* Complex but flat - this is correct for TCP */
    [TCP_CLOSED] = { TCP_LISTEN, TCP_SYN_SENT, ... },
    [TCP_LISTEN] = { TCP_SYN_RECEIVED, ... },
    /* ... etc */
};
```

---

## Summary: Hierarchical FSMs

```
+----------------------------------------------------------+
|              HIERARCHICAL FSM GUIDELINES                  |
+----------------------------------------------------------+
|                                                          |
|  USE HIERARCHY WHEN:                                     |
|    ✓ States are orthogonal (independent dimensions)      |
|    ✓ Transitions repeat across state groups              |
|    ✓ Natural domain grouping exists                      |
|    ✓ Flat FSM would have > 10 states                     |
|                                                          |
|  AVOID HIERARCHY WHEN:                                   |
|    ✗ States are sequential (linear progression)          |
|    ✗ Parent has only one child                           |
|    ✗ Child behavior depends on parent                    |
|    ✗ Would require > 2 levels                            |
|                                                          |
|  IMPLEMENTATION:                                         |
|    - Child-first dispatch (child can override)           |
|    - Parent handles "global" events                      |
|    - Child handles "local" events                        |
|    - Use SENTINEL value for "not handled"                |
|                                                          |
|  REMEMBER:                                               |
|    Hierarchy is a TOOL OF LAST RESORT.                   |
|    Flat FSMs are simpler. Prefer them.                   |
|    Only add hierarchy when benefits outweigh complexity. |
|                                                          |
+----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**状态爆炸**

当 FSM 有多个独立维度时，扁平 FSM 会发生状态爆炸：
- 3 个状态 × 3 个模式 = 9 个组合状态
- 添加静音模式：9 × 2 = 18 个状态
- N 个独立维度，每个 M 个选项 = M₁ × M₂ × ... × Mₙ 个状态

症状：
- 状态名包含多个概念（PLAYING_SHUFFLE_MUTED）
- 添加一个功能使状态数翻倍
- 相同转换出现在多个状态对中
- 状态图呈"网格"模式

**父子状态关系**

层次化 FSM 结构：
- 父状态包含子状态
- 父事件（PLAY/STOP）适用于整个父状态
- 子事件（SET_SHUFFLE）只改变子状态

共享转换：父状态处理所有子状态共有的事件。

覆盖规则：推荐"子状态优先"——子状态先尝试处理事件，未处理则由父状态处理。

**设计纪律**

**何时使用层次化**：
- 正交关注点（两个独立的行为维度）
- 重复转换（相同转换在多个状态中）
- 自然分组（状态形成逻辑集群）

**何时避免层次化**：
- 状态是顺序的而非正交的（如 TCP 状态）
- 父状态只有一个子状态
- 子状态行为依赖父状态
- 需要超过 2 层嵌套

**记住**：层次化是**最后手段**。扁平 FSM 更简单，优先使用。只有当收益大于复杂性时才添加层次。
