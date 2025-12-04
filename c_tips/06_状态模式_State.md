# 状态模式 (State Pattern)

## 定义

状态模式是一种行为型设计模式，允许对象在其内部状态改变时改变其行为，使对象看起来像是改变了其类。状态模式将状态相关的行为封装在独立的状态对象中。

## 适用场景

- 对象的行为取决于其状态，且必须在运行时根据状态改变行为
- 代码中包含大量与状态相关的条件语句
- 状态机的实现
- TCP连接状态管理
- 订单状态流转
- 游戏角色状态（站立、行走、跳跃、攻击）
- 文档审批流程

## ASCII 图解

```
+------------------------------------------------------------------------+
|                          STATE PATTERN                                  |
+------------------------------------------------------------------------+
|                                                                         |
|                        +------------------+                             |
|                        |     Context      |                             |
|                        +------------------+                             |
|                        | - currentState   |---+                         |
|                        +------------------+   |                         |
|                        | + request()      |   |                         |
|                        | + setState()     |   |                         |
|                        +------------------+   |                         |
|                                               |                         |
|                               +---------------+                         |
|                               |                                         |
|                               v                                         |
|                     +-------------------+                               |
|                     | <<interface>>     |                               |
|                     |      State        |                               |
|                     +-------------------+                               |
|                     | + handle()        |                               |
|                     +-------------------+                               |
|                               ^                                         |
|                               |                                         |
|          +--------------------+--------------------+                    |
|          |                    |                    |                    |
|   +------+------+      +------+------+      +------+------+            |
|   |  StateA     |      |  StateB     |      |  StateC     |            |
|   +-------------+      +-------------+      +-------------+            |
|   | + handle()  |      | + handle()  |      | + handle()  |            |
|   +-------------+      +-------------+      +-------------+            |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   State Transition Flow:                                                |
|                                                                         |
|   +--------+     +--------+     +--------+     +--------+              |
|   | State  |---->| State  |---->| State  |---->| State  |              |
|   |   A    |     |   B    |     |   C    |     |   A    |              |
|   +--------+     +--------+     +--------+     +--------+              |
|       |              |              |              |                    |
|       v              v              v              v                    |
|   [Action A]    [Action B]    [Action C]    [Action A]                 |
|                                                                         |
|   Each state determines:                                                |
|   1. What behavior to execute                                           |
|   2. What the next state should be                                      |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Order State Machine Example:                                          |
|                                                                         |
|   +----------+   pay    +-----------+  ship   +-----------+            |
|   | Created  |--------->|   Paid    |-------->| Shipped   |            |
|   +----------+          +-----------+         +-----------+            |
|        |                      |                     |                   |
|        | cancel               | refund              | deliver           |
|        v                      v                     v                   |
|   +----------+          +-----------+         +-----------+            |
|   | Cancelled|          | Refunded  |         | Delivered |            |
|   +----------+          +-----------+         +-----------+            |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了状态模式的核心结构。Context（上下文）维护一个当前状态引用，当调用 `request()` 时，实际行为委托给当前状态对象的 `handle()` 方法。每个具体状态（StateA、B、C）实现自己的行为逻辑，并可以触发状态转换。中间部分展示了状态转换的流程，下方是一个订单状态机的实际示例，展示了订单从创建到完成的各种状态转换路径。

## 实现方法

在C语言中实现状态模式：

1. 定义状态接口（包含行为函数指针）
2. 为每种状态创建具体实现
3. 上下文持有当前状态指针
4. 状态切换通过更新指针实现

## C语言代码示例

### 状态接口定义

```c
// state.h
#ifndef STATE_H
#define STATE_H

// 前向声明
typedef struct StateMachine StateMachine;
typedef struct State State;

// 状态接口
typedef struct {
    void (*enter)(State* self, StateMachine* sm);
    void (*execute)(State* self, StateMachine* sm);
    void (*exit)(State* self, StateMachine* sm);
    const char* (*get_name)(State* self);
} StateVTable;

struct State {
    const StateVTable* vtable;
};

// 状态操作函数
void state_enter(State* state, StateMachine* sm);
void state_execute(State* state, StateMachine* sm);
void state_exit(State* state, StateMachine* sm);
const char* state_get_name(State* state);

#endif
```

### 状态机上下文

```c
// state_machine.h
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "state.h"

struct StateMachine {
    State* current_state;
    void* context_data;
    const char* name;
};

StateMachine* sm_create(const char* name);
void sm_set_state(StateMachine* sm, State* new_state);
void sm_update(StateMachine* sm);
void sm_destroy(StateMachine* sm);

#endif
```

```c
// state_machine.c
#include "state_machine.h"
#include <stdio.h>
#include <stdlib.h>

StateMachine* sm_create(const char* name) {
    StateMachine* sm = (StateMachine*)malloc(sizeof(StateMachine));
    if (sm) {
        sm->current_state = NULL;
        sm->context_data = NULL;
        sm->name = name;
        printf("[SM:%s] Created\n", name);
    }
    return sm;
}

void sm_set_state(StateMachine* sm, State* new_state) {
    if (!sm) return;
    
    // 退出当前状态
    if (sm->current_state) {
        state_exit(sm->current_state, sm);
    }
    
    // 切换状态
    const char* old_name = sm->current_state ? 
        state_get_name(sm->current_state) : "(none)";
    const char* new_name = new_state ? 
        state_get_name(new_state) : "(none)";
    
    printf("[SM:%s] Transition: %s -> %s\n", sm->name, old_name, new_name);
    
    sm->current_state = new_state;
    
    // 进入新状态
    if (sm->current_state) {
        state_enter(sm->current_state, sm);
    }
}

void sm_update(StateMachine* sm) {
    if (sm && sm->current_state) {
        state_execute(sm->current_state, sm);
    }
}

void sm_destroy(StateMachine* sm) {
    if (sm) {
        printf("[SM:%s] Destroyed\n", sm->name);
        free(sm);
    }
}
```

### 具体状态实现：交通灯

```c
// traffic_light_states.h
#ifndef TRAFFIC_LIGHT_STATES_H
#define TRAFFIC_LIGHT_STATES_H

#include "state.h"

// 获取各状态实例（单例）
State* get_red_state(void);
State* get_yellow_state(void);
State* get_green_state(void);

// 交通灯数据
typedef struct {
    int time_in_state;
    int total_cycles;
} TrafficLightData;

#endif
```

```c
// traffic_light_states.c
#include "traffic_light_states.h"
#include "state_machine.h"
#include <stdio.h>
#include <stdlib.h>

// ==================== Red State ====================

static void red_enter(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state = 0;
    printf("  [RED] STOP! All vehicles must stop.\n");
    printf("  +-----+\n");
    printf("  | (*) | <- RED ON\n");
    printf("  | ( ) |\n");
    printf("  | ( ) |\n");
    printf("  +-----+\n");
}

static void red_execute(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state++;
    
    printf("  [RED] Waiting... (%d/3)\n", data->time_in_state);
    
    // 红灯持续3个周期后切换到绿灯
    if (data->time_in_state >= 3) {
        sm_set_state(sm, get_green_state());
    }
}

static void red_exit(State* self, StateMachine* sm) {
    (void)self; (void)sm;
    printf("  [RED] Transitioning...\n");
}

static const char* red_name(State* self) {
    (void)self;
    return "RED";
}

static const StateVTable red_vtable = {
    .enter = red_enter,
    .execute = red_execute,
    .exit = red_exit,
    .get_name = red_name
};

static State red_state = { .vtable = &red_vtable };

State* get_red_state(void) { return &red_state; }

// ==================== Yellow State ====================

static void yellow_enter(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state = 0;
    printf("  [YELLOW] CAUTION! Prepare to stop.\n");
    printf("  +-----+\n");
    printf("  | ( ) |\n");
    printf("  | (*) | <- YELLOW ON\n");
    printf("  | ( ) |\n");
    printf("  +-----+\n");
}

static void yellow_execute(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state++;
    
    printf("  [YELLOW] Warning... (%d/1)\n", data->time_in_state);
    
    // 黄灯持续1个周期后切换到红灯
    if (data->time_in_state >= 1) {
        data->total_cycles++;
        sm_set_state(sm, get_red_state());
    }
}

static void yellow_exit(State* self, StateMachine* sm) {
    (void)self; (void)sm;
    printf("  [YELLOW] Transitioning...\n");
}

static const char* yellow_name(State* self) {
    (void)self;
    return "YELLOW";
}

static const StateVTable yellow_vtable = {
    .enter = yellow_enter,
    .execute = yellow_execute,
    .exit = yellow_exit,
    .get_name = yellow_name
};

static State yellow_state = { .vtable = &yellow_vtable };

State* get_yellow_state(void) { return &yellow_state; }

// ==================== Green State ====================

static void green_enter(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state = 0;
    printf("  [GREEN] GO! Vehicles may proceed.\n");
    printf("  +-----+\n");
    printf("  | ( ) |\n");
    printf("  | ( ) |\n");
    printf("  | (*) | <- GREEN ON\n");
    printf("  +-----+\n");
}

static void green_execute(State* self, StateMachine* sm) {
    (void)self;
    TrafficLightData* data = (TrafficLightData*)sm->context_data;
    data->time_in_state++;
    
    printf("  [GREEN] Vehicles passing... (%d/2)\n", data->time_in_state);
    
    // 绿灯持续2个周期后切换到黄灯
    if (data->time_in_state >= 2) {
        sm_set_state(sm, get_yellow_state());
    }
}

static void green_exit(State* self, StateMachine* sm) {
    (void)self; (void)sm;
    printf("  [GREEN] Transitioning...\n");
}

static const char* green_name(State* self) {
    (void)self;
    return "GREEN";
}

static const StateVTable green_vtable = {
    .enter = green_enter,
    .execute = green_execute,
    .exit = green_exit,
    .get_name = green_name
};

static State green_state = { .vtable = &green_vtable };

State* get_green_state(void) { return &green_state; }
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "state_machine.h"
#include "traffic_light_states.h"

int main() {
    printf("=== State Pattern Demo - Traffic Light ===\n\n");
    
    // 创建交通灯数据
    TrafficLightData data = { .time_in_state = 0, .total_cycles = 0 };
    
    // 创建状态机
    StateMachine* traffic_light = sm_create("TrafficLight");
    traffic_light->context_data = &data;
    
    // 设置初始状态
    printf("\n--- Initialize to RED ---\n");
    sm_set_state(traffic_light, get_red_state());
    
    // 模拟运行多个周期
    printf("\n--- Running simulation ---\n");
    for (int tick = 1; tick <= 15; tick++) {
        printf("\n[Tick %d]\n", tick);
        sm_update(traffic_light);
        
        if (data.total_cycles >= 2) {
            printf("\nCompleted %d full cycles. Stopping.\n", 
                   data.total_cycles);
            break;
        }
    }
    
    // 清理
    printf("\n--- Cleanup ---\n");
    sm_destroy(traffic_light);
    
    return 0;
}

/* 输出示例:
=== State Pattern Demo - Traffic Light ===

[SM:TrafficLight] Created

--- Initialize to RED ---
[SM:TrafficLight] Transition: (none) -> RED
  [RED] STOP! All vehicles must stop.
  +-----+
  | (*) | <- RED ON
  | ( ) |
  | ( ) |
  +-----+

--- Running simulation ---

[Tick 1]
  [RED] Waiting... (1/3)

[Tick 2]
  [RED] Waiting... (2/3)

[Tick 3]
  [RED] Waiting... (3/3)
  [RED] Transitioning...
[SM:TrafficLight] Transition: RED -> GREEN
  [GREEN] GO! Vehicles may proceed.
  +-----+
  | ( ) |
  | ( ) |
  | (*) | <- GREEN ON
  +-----+

[Tick 4]
  [GREEN] Vehicles passing... (1/2)

[Tick 5]
  [GREEN] Vehicles passing... (2/2)
  [GREEN] Transitioning...
[SM:TrafficLight] Transition: GREEN -> YELLOW
  [YELLOW] CAUTION! Prepare to stop.
  +-----+
  | ( ) |
  | (*) | <- YELLOW ON
  | ( ) |
  +-----+

[Tick 6]
  [YELLOW] Warning... (1/1)
  [YELLOW] Transitioning...
[SM:TrafficLight] Transition: YELLOW -> RED
  [RED] STOP! All vehicles must stop.
...
*/
```

## 优缺点

### 优点
- 将与特定状态相关的行为局部化
- 使状态转换显式化
- 避免大量条件语句
- 状态对象可以被共享

### 缺点
- 可能会产生大量状态类
- 如果状态转换逻辑复杂，可能难以维护
- 状态切换开销

