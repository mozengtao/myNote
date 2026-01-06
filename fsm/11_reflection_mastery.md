# Reflection and Mastery Checklist

## Complete FSM Mastery Verification

```
+----------------------------------------------------------+
|                    FSM MASTERY LEVELS                     |
+----------------------------------------------------------+
|                                                          |
|  Level 1: RECOGNITION                                    |
|    □ Can identify FSM problems in code                   |
|    □ Can spot boolean flag explosion                     |
|    □ Can detect hidden states                            |
|                                                          |
|  Level 2: DESIGN                                         |
|    □ Can design states (mutually exclusive, named well)  |
|    □ Can design events (past tense, external facts)      |
|    □ Can design transitions (explicit, no defaults)      |
|                                                          |
|  Level 3: IMPLEMENTATION                                 |
|    □ Can implement table-driven FSM in C                 |
|    □ Can separate FSM logic from work logic              |
|    □ Can handle illegal transitions properly             |
|                                                          |
|  Level 4: INTEGRATION                                    |
|    □ Can integrate FSM with event loops                  |
|    □ Can model timeouts and failures                     |
|    □ Can manage object lifecycle with FSM                |
|                                                          |
|  Level 5: SCALING                                        |
|    □ Can decide when hierarchy is needed                 |
|    □ Can design hierarchical FSM correctly               |
|    □ Can avoid over-engineering                          |
|                                                          |
+----------------------------------------------------------+
```

---

## 1. Identifying FSM Problems in Existing Code

### Checklist

```
□ I can recognize boolean flag explosion
  - Multiple related booleans (is_connected, is_ready, has_error)
  - Complex compound conditions (if a && !b && c || d)
  - Most combinations are invalid

□ I can detect temporal logic bugs
  - "Must happen before" comments in code
  - Sequence-dependent operations
  - Race conditions from ordering issues

□ I can find hidden states
  - Status variables with special values
  - Step counters
  - Implicit modes in code flow

□ I can distinguish FSM problems from simple conditionals
  - Behavior depends on history → FSM
  - Stateless decisions → conditionals
```

### Self-Test

```c
/* Does this code need an FSM? */

// Example 1
bool connected;
bool authenticated;
bool ready;
if (connected && authenticated && !ready) {
    // ...
}

// Answer: YES - Boolean flag explosion, states are not orthogonal

// Example 2
int validate_input(const char *s) {
    if (s == NULL) return -1;
    if (strlen(s) == 0) return -2;
    return 0;
}

// Answer: NO - Stateless validation, no history needed

// Example 3
void process_request(request_t *r) {
    parse(r);        // Must happen first
    validate(r);     // Assumes parsed
    execute(r);      // Assumes validated
    respond(r);      // Assumes executed
}

// Answer: MAYBE - Implicit ordering, consider FSM if errors possible
```

---

## 2. Designing Small, Correct FSMs

### Checklist

```
□ I can design mutually exclusive states
  - Only one state active at any time
  - No overlapping states
  - States named as conditions (nouns/adjectives)

□ I can design proper events
  - Past tense or noun phrases
  - External facts, not commands
  - Separate events for request and completion

□ I can create complete transition tables
  - Every state × event cell defined
  - Explicit illegal transitions
  - No default handlers
```

### Self-Test

```
Design an FSM for a simple light switch:

States:
  - OFF: Light is off
  - ON: Light is on

Events:
  - SWITCH_PRESSED: User pressed the switch

Transition Table:
+------+-----------------+-----------+
| State| Event           | Next State|
+------+-----------------+-----------+
| OFF  | SWITCH_PRESSED  | ON        |
| ON   | SWITCH_PRESSED  | OFF       |
+------+-----------------+-----------+

Add a BROKEN state and appropriate transitions:

States: OFF, ON, BROKEN

Events: SWITCH_PRESSED, BULB_BURNED_OUT, BULB_REPLACED

Transition Table:
+--------+------------------+------------+
| State  | Event            | Next State |
+--------+------------------+------------+
| OFF    | SWITCH_PRESSED   | ON         |
| OFF    | BULB_BURNED_OUT  | OFF        | (already off)
| OFF    | BULB_REPLACED    | OFF        | (still off)
| ON     | SWITCH_PRESSED   | OFF        |
| ON     | BULB_BURNED_OUT  | BROKEN     |
| ON     | BULB_REPLACED    | ON         | (illegal? or no-op)
| BROKEN | SWITCH_PRESSED   | BROKEN     | (still broken)
| BROKEN | BULB_BURNED_OUT  | BROKEN     | (still broken)
| BROKEN | BULB_REPLACED    | OFF        | (fixed, starts off)
+--------+------------------+------------+
```

---

## 3. Explicitly Modeling Illegal Behavior

### Checklist

```
□ I understand why illegal transitions are important
  - 80% of FSM bugs are illegal transitions
  - Silent ignore hides bugs

□ I can implement proper rejection strategies
  - Assert (debug builds)
  - Log + error return (production)
  - Error state transition (graceful degradation)

□ I never silently ignore events
  - Every unhandled event is logged or asserted
  - No default: case that ignores
```

### Self-Test

```c
/* Implement illegal transition handling */

int fsm_dispatch(fsm_t *f, event_t e) {
    const transition_t *t = &table[f->state][e];
    
    if (t->next_state == STATE_ILLEGAL) {
        /* What goes here? */
        
        /* Option A: Assert (debug) */
        assert(!"Illegal transition");
        
        /* Option B: Log and return error (production) */
        log_error("Illegal: state=%s event=%s",
                  state_name(f->state), event_name(e));
        return -EINVAL;
        
        /* Option C: Transition to error state */
        f->state = STATE_ERROR;
        return -1;
        
        /* WRONG: return 0; (silent ignore) */
    }
    
    f->state = t->next_state;
    if (t->action) return t->action(f->ctx);
    return 0;
}
```

---

## 4. Separating Control Logic from Execution Logic

### Checklist

```
□ I understand the FSM/Action split
  - FSM decides WHAT happens next
  - Actions perform HOW it happens

□ I can implement pure transitions
  - Transitions are lookups, not operations
  - No I/O in transition logic
  - Actions are function pointers

□ I can test FSM logic independently
  - Test transitions without real I/O
  - Mock actions for integration tests
```

### Self-Test

```c
/* Which is correct? */

// Version A (WRONG - mixed concerns)
int fsm_handle(fsm_t *f, event_t e) {
    if (f->state == IDLE && e == CONNECT) {
        f->socket = socket(AF_INET, SOCK_STREAM, 0);
        connect(f->socket, ...);
        f->state = CONNECTING;
    }
}

// Version B (CORRECT - separated)
int fsm_handle(fsm_t *f, event_t e) {
    transition_t *t = lookup(f->state, e);
    if (!t) return -1;
    
    f->state = t->next_state;
    if (t->action) return t->action(f->ctx);
    return 0;
}

int action_start_connect(void *ctx) {
    connection_t *c = ctx;
    c->socket = socket(AF_INET, SOCK_STREAM, 0);
    return connect(c->socket, ...);
}
```

---

## 5. Implementing Table-Driven FSMs in C

### Checklist

```
□ I can define state and event enums
  - Use STATE_COUNT/EVENT_COUNT for sizing
  - Meaningful names

□ I can create transition tables
  - 2D array indexed by [state][event]
  - Include next_state and action pointer
  - Mark illegal transitions explicitly

□ I can implement dispatch function
  - Bounds checking
  - Illegal transition detection
  - Action execution
```

### Self-Test: Complete FSM Template

```c
/* Fill in the blanks */

typedef enum {
    STATE_A,
    STATE_B,
    STATE_C,
    STATE_____  /* Sentinel */
} state_t;

typedef enum {
    EVENT_X,
    EVENT_Y,
    EVENT_____  /* Sentinel */
} event_t;

typedef int (*action_fn)(void *ctx);

typedef struct {
    state_t next_state;
    action_fn action;
} transition_t;

#define ILLEGAL { STATE_____, NULL }

static const transition_t table[_____][_____] = {
    [STATE_A] = { {STATE_B, act_x}, {STATE_C, act_y} },
    [STATE_B] = { ILLEGAL, {STATE_A, NULL} },
    [STATE_C] = { {STATE_A, act_x}, ILLEGAL },
};

int fsm_dispatch(fsm_t *f, event_t e) {
    if (e >= _____) return -1;
    
    const transition_t *t = &table[f->state][e];
    
    if (t->next_state == _____) {
        return -1;  /* Illegal */
    }
    
    f->state = _____;
    if (_____) return t->action(f->ctx);
    return 0;
}

/* Answers:
   STATE_COUNT, EVENT_COUNT, STATE_COUNT (for ILLEGAL),
   STATE_COUNT, EVENT_COUNT, STATE_COUNT,
   t->next_state, t->action */
```

---

## 6. Integrating FSMs into Event-Driven Systems

### Checklist

```
□ I can integrate FSM with event loop
  - FSM is a consumer
  - Event sources generate events
  - FSM responds and returns

□ I understand single-threaded FSM benefits
  - No races
  - FIFO event ordering
  - Deterministic behavior

□ I can model timers as events
  - Timer fires → EVENT_TIMEOUT
  - FSM never sleeps or blocks
```

---

## 7. Using FSMs to Model Object Lifetimes

### Checklist

```
□ I can design lifecycle FSMs
  - CREATED → INITIALIZING → RUNNING → STOPPING → STOPPED
  - Each state defines valid resources

□ I can prevent use-after-free via state
  - Resource only valid in certain states
  - State check replaces NULL check

□ I understand lifecycle vs protocol FSM
  - Lifecycle: Does object exist?
  - Protocol: What is object doing?
```

---

## 8. Avoiding Premature Hierarchical FSMs

### Checklist

```
□ I can recognize when hierarchy is needed
  - State explosion (N × M states)
  - Orthogonal dimensions
  - Repeated transitions

□ I can recognize when hierarchy is harmful
  - Sequential states (not orthogonal)
  - Single child per parent
  - > 2 levels deep

□ I default to flat FSMs
  - Simpler to understand
  - Easier to implement
  - Only add hierarchy when justified
```

---

## 9. Confidently Scaling FSM Designs

### Final Checklist

```
□ I can start with minimal FSM
  - Few states
  - Clear events
  - Simple table

□ I can add complexity incrementally
  - Add states as needed
  - Add events as needed
  - Refactor to hierarchy only when required

□ I can refactor FSMs
  - Extract common transitions
  - Split orthogonal concerns
  - Merge redundant states

□ I avoid over-engineering
  - No hierarchy unless N×M explosion
  - No guards unless truly conditional
  - No frameworks unless team requires
```

---

## Quick Reference Card

```
+----------------------------------------------------------+
|                    FSM QUICK REFERENCE                    |
+----------------------------------------------------------+
|                                                          |
|  STATE: "Where we are" - mutually exclusive, named well  |
|  EVENT: "What happened" - past tense, external fact      |
|  TRANSITION: "Legal move" - explicit, no defaults        |
|  ACTION: "Side effect" - separate from decision          |
|  GUARD: "Condition" - pure, evaluated before transition  |
|                                                          |
+----------------------------------------------------------+
|                                                          |
|  FSM DOES: Decide, track state, enforce legality         |
|  FSM DOES NOT: Sleep, block, poll, do I/O               |
|                                                          |
+----------------------------------------------------------+
|                                                          |
|  TABLE STRUCTURE:                                        |
|    transition_t table[STATE_COUNT][EVENT_COUNT]          |
|    table[state][event] = { next_state, action }          |
|    ILLEGAL = { STATE_COUNT, NULL }                       |
|                                                          |
+----------------------------------------------------------+
|                                                          |
|  DISPATCH PATTERN:                                       |
|    1. Lookup transition                                  |
|    2. Check for illegal                                  |
|    3. Update state                                       |
|    4. Execute action (if any)                            |
|                                                          |
+----------------------------------------------------------+
```

---

## Final Words

```
+----------------------------------------------------------+
|                                                          |
|  FSM is a THINKING TOOL, not a syntax trick.             |
|                                                          |
|  It forces you to:                                       |
|    - Enumerate all states explicitly                     |
|    - Consider all events in all states                   |
|    - Define what is legal and what is not                |
|    - Separate decision from execution                    |
|                                                          |
|  The result:                                             |
|    - Fewer bugs (illegal transitions caught)             |
|    - Better documentation (table IS the spec)            |
|    - Easier testing (deterministic behavior)             |
|    - Clearer code (state logic centralized)              |
|                                                          |
|  Master FSMs, and you master a fundamental tool          |
|  for building correct, maintainable systems.             |
|                                                          |
+----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**掌握程度检查清单**

本文档提供了 FSM 掌握程度的完整验证：

**第1级 - 识别**：能识别代码中的 FSM 问题、布尔标志爆炸、隐藏状态

**第2级 - 设计**：能设计状态（互斥、命名良好）、事件（过去时、外部事实）、转换（显式、无默认）

**第3级 - 实现**：能用 C 实现表驱动 FSM、分离 FSM 逻辑和工作逻辑、正确处理非法转换

**第4级 - 集成**：能将 FSM 与事件循环集成、建模超时和故障、用 FSM 管理对象生命周期

**第5级 - 扩展**：能决定何时需要层次化、正确设计层次化 FSM、避免过度工程

**最终要点**

FSM 是一种**思维工具**，而非语法技巧。它强迫你：
- 显式枚举所有状态
- 考虑所有状态中的所有事件
- 定义什么是合法的、什么不是
- 分离决策和执行

结果是：更少 bug、更好文档、更容易测试、更清晰代码。

掌握 FSM，你就掌握了构建正确、可维护系统的基本工具。
