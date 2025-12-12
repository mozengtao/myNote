# 嵌入式设计模式 - 状态机设计模式

本文档介绍嵌入式系统中用于实现状态机的设计模式，包括完整的 C 语言代码示例。

---

## 目录

1. [单事件接收器模式 (Single Event Receptor Pattern)](#1-单事件接收器模式)
2. [多事件接收器模式 (Multiple Event Receptor Pattern)](#2-多事件接收器模式)
3. [状态表模式 (State Table Pattern)](#3-状态表模式)
4. [状态模式 (State Pattern)](#4-状态模式)
5. [分解 AND 状态模式 (Decomposed And-State Pattern)](#5-分解-and-状态模式)

---

## 1. 单事件接收器模式

### 架构图

```
+------------------------------------------------------------------+
|               SINGLE EVENT RECEPTOR PATTERN                       |
+------------------------------------------------------------------+

    Event Structure:
    
    +------------------+
    |     Event        |
    +------------------+
    | - type           |  (BUTTON, TIMER, SENSOR, etc.)
    | - data           |  (union of event-specific data)
    | - timestamp      |
    +------------------+


    State Machine with Single Receptor:
    
    +--------+     +----------------+     +-------------+
    | Events |---->| event_receptor |---->| State Logic |
    +--------+     |    (single)    |     +------+------+
                   +----------------+            |
                                                 v
                                          +-------------+
                                          | Dispatch to |
                                          | current     |
                                          | state       |
                                          +-------------+


    Event Flow:
    
    Button Press --> create_event(BTN, data) --> sm_process_event(event)
                                                        |
    Timer Tick ----> create_event(TIMER, data) --------+
                                                        |
    Sensor Data ---> create_event(SENSOR, data) -------+
                                                        v
                                                  [State Machine]
```

**中文说明：**
- 单事件接收器模式使用统一的事件接口与状态机通信
- 所有事件通过同一个函数（接收器）进入状态机
- 事件包含类型和数据，可以支持同步和异步传递
- 实现简单，适合中小规模状态机

### 完整代码示例

```c
/*============================================================================
 * 单事件接收器模式示例 - 门禁系统状态机
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 事件定义
 *---------------------------------------------------------------------------*/
/* 关键点：抽象事件类型 */
typedef enum {
    EVT_NONE = 0,
    EVT_CARD_SWIPE,         /* 刷卡 */
    EVT_PIN_ENTERED,        /* 输入密码 */
    EVT_DOOR_OPENED,        /* 门被打开 */
    EVT_DOOR_CLOSED,        /* 门被关闭 */
    EVT_TIMEOUT,            /* 超时 */
    EVT_ALARM_TRIGGERED,    /* 报警触发 */
    EVT_RESET               /* 重置 */
} event_type_t;

/* 关键点：事件数据联合体 */
typedef union {
    struct {
        uint32_t card_id;
        uint8_t card_type;
    } card;
    
    struct {
        char pin[8];
    } pin;
    
    struct {
        uint32_t timeout_ms;
    } timeout;
    
    uint32_t raw_data;
} event_data_t;

/* 关键点：完整的事件结构 */
typedef struct {
    event_type_t type;
    event_data_t data;
    uint32_t timestamp;
} event_t;

/*---------------------------------------------------------------------------
 * 状态机定义
 *---------------------------------------------------------------------------*/
typedef enum {
    STATE_LOCKED,           /* 锁定状态 */
    STATE_CARD_READ,        /* 已读卡，等待密码 */
    STATE_UNLOCKED,         /* 已解锁 */
    STATE_DOOR_OPEN,        /* 门已打开 */
    STATE_ALARM             /* 报警状态 */
} state_t;

typedef struct {
    state_t current_state;
    uint32_t card_id;
    uint32_t state_enter_time;
    uint8_t failed_attempts;
    bool alarm_active;
    
    /* 配置 */
    uint32_t pin_timeout_ms;
    uint32_t door_timeout_ms;
    uint8_t max_failed_attempts;
} door_access_sm_t;

/*---------------------------------------------------------------------------
 * 辅助函数
 *---------------------------------------------------------------------------*/
static uint32_t g_time_ms = 0;

uint32_t get_time_ms(void) {
    return g_time_ms;
}

void advance_time(uint32_t ms) {
    g_time_ms += ms;
}

const char* state_to_string(state_t state) {
    static const char *names[] = {
        "LOCKED", "CARD_READ", "UNLOCKED", "DOOR_OPEN", "ALARM"
    };
    return names[state];
}

const char* event_to_string(event_type_t type) {
    static const char *names[] = {
        "NONE", "CARD_SWIPE", "PIN_ENTERED", "DOOR_OPENED",
        "DOOR_CLOSED", "TIMEOUT", "ALARM_TRIGGERED", "RESET"
    };
    return names[type];
}

/* 模拟验证函数 */
bool validate_card(uint32_t card_id) {
    /* 有效卡号: 1000-9999 */
    return card_id >= 1000 && card_id <= 9999;
}

bool validate_pin(const char *pin, uint32_t card_id) {
    /* 简化：PIN 是卡号的后4位 */
    char expected[8];
    sprintf(expected, "%04u", card_id % 10000);
    return strcmp(pin, expected) == 0;
}

/*---------------------------------------------------------------------------
 * 状态机初始化
 *---------------------------------------------------------------------------*/
void sm_init(door_access_sm_t *sm) {
    sm->current_state = STATE_LOCKED;
    sm->card_id = 0;
    sm->state_enter_time = get_time_ms();
    sm->failed_attempts = 0;
    sm->alarm_active = false;
    
    sm->pin_timeout_ms = 10000;     /* 10秒输入密码 */
    sm->door_timeout_ms = 30000;    /* 30秒关门 */
    sm->max_failed_attempts = 3;
    
    printf("[SM] Initialized, state=%s\n", state_to_string(sm->current_state));
}

/*---------------------------------------------------------------------------
 * 状态转换
 *---------------------------------------------------------------------------*/
static void sm_transition(door_access_sm_t *sm, state_t new_state) {
    printf("[SM] Transition: %s -> %s\n",
           state_to_string(sm->current_state),
           state_to_string(new_state));
    
    /* 退出当前状态的动作 */
    switch (sm->current_state) {
        case STATE_CARD_READ:
            /* 清除临时数据 */
            break;
        case STATE_ALARM:
            sm->alarm_active = false;
            printf("  [Action] Alarm deactivated\n");
            break;
        default:
            break;
    }
    
    sm->current_state = new_state;
    sm->state_enter_time = get_time_ms();
    
    /* 进入新状态的动作 */
    switch (new_state) {
        case STATE_LOCKED:
            sm->card_id = 0;
            sm->failed_attempts = 0;
            printf("  [Action] Door locked, display 'Ready'\n");
            break;
        case STATE_CARD_READ:
            printf("  [Action] Display 'Enter PIN'\n");
            break;
        case STATE_UNLOCKED:
            printf("  [Action] Door unlocked, LED green\n");
            break;
        case STATE_DOOR_OPEN:
            printf("  [Action] Start door timer\n");
            break;
        case STATE_ALARM:
            sm->alarm_active = true;
            printf("  [Action] ALARM ACTIVATED!\n");
            break;
    }
}

/*---------------------------------------------------------------------------
 * 关键点：单事件接收器（核心函数）
 *---------------------------------------------------------------------------*/
void sm_process_event(door_access_sm_t *sm, const event_t *event) {
    printf("\n[SM] Event: %s in state %s\n",
           event_to_string(event->type),
           state_to_string(sm->current_state));
    
    /* 关键点：根据当前状态和事件类型处理 */
    switch (sm->current_state) {
        case STATE_LOCKED:
            switch (event->type) {
                case EVT_CARD_SWIPE:
                    if (validate_card(event->data.card.card_id)) {
                        sm->card_id = event->data.card.card_id;
                        sm_transition(sm, STATE_CARD_READ);
                    } else {
                        printf("  [SM] Invalid card!\n");
                    }
                    break;
                    
                case EVT_DOOR_OPENED:
                    /* 非法开门 - 触发报警 */
                    sm_transition(sm, STATE_ALARM);
                    break;
                    
                default:
                    printf("  [SM] Event ignored in LOCKED state\n");
                    break;
            }
            break;
            
        case STATE_CARD_READ:
            switch (event->type) {
                case EVT_PIN_ENTERED:
                    if (validate_pin(event->data.pin.pin, sm->card_id)) {
                        sm_transition(sm, STATE_UNLOCKED);
                    } else {
                        sm->failed_attempts++;
                        printf("  [SM] Wrong PIN! Attempt %d/%d\n",
                               sm->failed_attempts, sm->max_failed_attempts);
                        
                        if (sm->failed_attempts >= sm->max_failed_attempts) {
                            sm_transition(sm, STATE_ALARM);
                        }
                    }
                    break;
                    
                case EVT_TIMEOUT:
                    printf("  [SM] PIN entry timeout\n");
                    sm_transition(sm, STATE_LOCKED);
                    break;
                    
                default:
                    printf("  [SM] Event ignored in CARD_READ state\n");
                    break;
            }
            break;
            
        case STATE_UNLOCKED:
            switch (event->type) {
                case EVT_DOOR_OPENED:
                    sm_transition(sm, STATE_DOOR_OPEN);
                    break;
                    
                case EVT_TIMEOUT:
                    printf("  [SM] Door not opened, relocking\n");
                    sm_transition(sm, STATE_LOCKED);
                    break;
                    
                default:
                    printf("  [SM] Event ignored in UNLOCKED state\n");
                    break;
            }
            break;
            
        case STATE_DOOR_OPEN:
            switch (event->type) {
                case EVT_DOOR_CLOSED:
                    sm_transition(sm, STATE_LOCKED);
                    break;
                    
                case EVT_TIMEOUT:
                    printf("  [SM] Door open too long!\n");
                    sm_transition(sm, STATE_ALARM);
                    break;
                    
                default:
                    printf("  [SM] Event ignored in DOOR_OPEN state\n");
                    break;
            }
            break;
            
        case STATE_ALARM:
            switch (event->type) {
                case EVT_RESET:
                    printf("  [SM] Alarm reset by administrator\n");
                    sm_transition(sm, STATE_LOCKED);
                    break;
                    
                default:
                    printf("  [SM] ALARM ACTIVE - event ignored\n");
                    break;
            }
            break;
    }
}

/*---------------------------------------------------------------------------
 * 事件创建辅助函数
 *---------------------------------------------------------------------------*/
event_t create_card_event(uint32_t card_id) {
    event_t evt = {0};
    evt.type = EVT_CARD_SWIPE;
    evt.data.card.card_id = card_id;
    evt.timestamp = get_time_ms();
    return evt;
}

event_t create_pin_event(const char *pin) {
    event_t evt = {0};
    evt.type = EVT_PIN_ENTERED;
    strncpy(evt.data.pin.pin, pin, sizeof(evt.data.pin.pin) - 1);
    evt.timestamp = get_time_ms();
    return evt;
}

event_t create_simple_event(event_type_t type) {
    event_t evt = {0};
    evt.type = type;
    evt.timestamp = get_time_ms();
    return evt;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void single_event_receptor_example(void) {
    printf("=== Single Event Receptor Pattern Demo ===\n");
    printf("=== Door Access Control System ===\n");
    
    door_access_sm_t sm;
    sm_init(&sm);
    
    /* 场景1：正常进入流程 */
    printf("\n--- Scenario 1: Normal Entry ---\n");
    event_t evt;
    
    evt = create_card_event(1234);              /* 刷有效卡 */
    sm_process_event(&sm, &evt);
    
    evt = create_pin_event("1234");             /* 输入正确密码 */
    sm_process_event(&sm, &evt);
    
    evt = create_simple_event(EVT_DOOR_OPENED); /* 开门 */
    sm_process_event(&sm, &evt);
    
    evt = create_simple_event(EVT_DOOR_CLOSED); /* 关门 */
    sm_process_event(&sm, &evt);
    
    /* 场景2：密码错误 */
    printf("\n--- Scenario 2: Wrong PIN ---\n");
    
    evt = create_card_event(5678);
    sm_process_event(&sm, &evt);
    
    evt = create_pin_event("0000");             /* 错误密码 */
    sm_process_event(&sm, &evt);
    
    evt = create_pin_event("1111");             /* 再次错误 */
    sm_process_event(&sm, &evt);
    
    evt = create_pin_event("5678");             /* 正确密码 */
    sm_process_event(&sm, &evt);
    
    /* 完成流程 */
    evt = create_simple_event(EVT_DOOR_OPENED);
    sm_process_event(&sm, &evt);
    evt = create_simple_event(EVT_DOOR_CLOSED);
    sm_process_event(&sm, &evt);
    
    /* 场景3：触发报警 */
    printf("\n--- Scenario 3: Alarm Triggered ---\n");
    
    evt = create_card_event(9999);
    sm_process_event(&sm, &evt);
    
    /* 连续3次错误密码 */
    for (int i = 0; i < 3; i++) {
        evt = create_pin_event("0000");
        sm_process_event(&sm, &evt);
    }
    
    /* 报警状态下的事件被忽略 */
    evt = create_card_event(1234);
    sm_process_event(&sm, &evt);
    
    /* 管理员重置 */
    evt = create_simple_event(EVT_RESET);
    sm_process_event(&sm, &evt);
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 2. 多事件接收器模式

### 架构图

```
+------------------------------------------------------------------+
|              MULTIPLE EVENT RECEPTOR PATTERN                      |
+------------------------------------------------------------------+

    Separate Entry Points for Each Event:
    
    +--------+     +-------------------+     +-------------+
    | Button |---->| on_button_press() |---->|             |
    +--------+     +-------------------+     |             |
                                             |   State     |
    +--------+     +-------------------+     |   Machine   |
    | Timer  |---->| on_timer_tick()   |---->|             |
    +--------+     +-------------------+     |             |
                                             |             |
    +--------+     +-------------------+     |             |
    | Sensor |---->| on_sensor_data()  |---->|             |
    +--------+     +-------------------+     +-------------+


    Advantages:
    
    +-----------------------+
    | + Type-safe interface |
    | + Clear API           |
    | + No event dispatch   |
    | + Compile-time check  |
    +-----------------------+
```

**中文说明：**
- 多事件接收器模式为每种事件提供独立的处理函数
- 避免了事件分发逻辑，接口更加类型安全
- 适合同步状态机，事件类型在编译时确定
- 每个事件处理函数内部包含状态判断逻辑

### 完整代码示例

```c
/*============================================================================
 * 多事件接收器模式示例 - 电梯控制系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 状态机定义
 *---------------------------------------------------------------------------*/
typedef enum {
    ELEVATOR_IDLE,          /* 空闲 */
    ELEVATOR_MOVING_UP,     /* 上行 */
    ELEVATOR_MOVING_DOWN,   /* 下行 */
    ELEVATOR_DOOR_OPENING,  /* 开门中 */
    ELEVATOR_DOOR_OPEN,     /* 门已开 */
    ELEVATOR_DOOR_CLOSING,  /* 关门中 */
    ELEVATOR_EMERGENCY      /* 紧急状态 */
} elevator_state_t;

#define MAX_FLOORS 10

typedef struct {
    elevator_state_t state;
    int8_t current_floor;
    int8_t target_floor;
    bool floor_requests[MAX_FLOORS];    /* 楼层请求 */
    bool door_obstructed;
    uint32_t state_timer;
} elevator_sm_t;

/*---------------------------------------------------------------------------
 * 辅助函数
 *---------------------------------------------------------------------------*/
static uint32_t g_tick = 0;

const char* elevator_state_name(elevator_state_t state) {
    static const char *names[] = {
        "IDLE", "MOVING_UP", "MOVING_DOWN",
        "DOOR_OPENING", "DOOR_OPEN", "DOOR_CLOSING", "EMERGENCY"
    };
    return names[state];
}

void elevator_init(elevator_sm_t *sm) {
    sm->state = ELEVATOR_IDLE;
    sm->current_floor = 1;
    sm->target_floor = 1;
    sm->door_obstructed = false;
    sm->state_timer = 0;
    
    for (int i = 0; i < MAX_FLOORS; i++) {
        sm->floor_requests[i] = false;
    }
    
    printf("[Elevator] Initialized at floor %d\n", sm->current_floor);
}

static void change_state(elevator_sm_t *sm, elevator_state_t new_state) {
    printf("[Elevator] State: %s -> %s\n",
           elevator_state_name(sm->state),
           elevator_state_name(new_state));
    sm->state = new_state;
    sm->state_timer = g_tick;
}

/* 找到下一个目标楼层 */
static int8_t find_next_target(elevator_sm_t *sm, bool going_up) {
    if (going_up) {
        for (int i = sm->current_floor; i < MAX_FLOORS; i++) {
            if (sm->floor_requests[i]) return i;
        }
    } else {
        for (int i = sm->current_floor; i >= 0; i--) {
            if (sm->floor_requests[i]) return i;
        }
    }
    
    /* 没找到同方向的，反向查找 */
    if (going_up) {
        for (int i = sm->current_floor; i >= 0; i--) {
            if (sm->floor_requests[i]) return i;
        }
    } else {
        for (int i = sm->current_floor; i < MAX_FLOORS; i++) {
            if (sm->floor_requests[i]) return i;
        }
    }
    
    return -1;  /* 无请求 */
}

/*---------------------------------------------------------------------------
 * 关键点：多事件接收器 - 每个事件类型有独立的处理函数
 *---------------------------------------------------------------------------*/

/* 事件接收器1：楼层请求 */
void elevator_on_floor_request(elevator_sm_t *sm, int8_t floor) {
    printf("\n[Event] Floor request: %d (current: %d, state: %s)\n",
           floor, sm->current_floor, elevator_state_name(sm->state));
    
    if (floor < 0 || floor >= MAX_FLOORS) {
        printf("  Invalid floor!\n");
        return;
    }
    
    sm->floor_requests[floor] = true;
    
    /* 关键点：根据当前状态响应 */
    switch (sm->state) {
        case ELEVATOR_IDLE:
            if (floor == sm->current_floor) {
                /* 同一层，直接开门 */
                change_state(sm, ELEVATOR_DOOR_OPENING);
            } else if (floor > sm->current_floor) {
                sm->target_floor = floor;
                change_state(sm, ELEVATOR_MOVING_UP);
            } else {
                sm->target_floor = floor;
                change_state(sm, ELEVATOR_MOVING_DOWN);
            }
            break;
            
        case ELEVATOR_MOVING_UP:
        case ELEVATOR_MOVING_DOWN:
            /* 记录请求，继续移动 */
            printf("  Request queued\n");
            break;
            
        case ELEVATOR_DOOR_OPEN:
            if (floor == sm->current_floor) {
                /* 当前楼层，重置门计时器 */
                sm->state_timer = g_tick;
                printf("  Extending door open time\n");
            }
            break;
            
        case ELEVATOR_EMERGENCY:
            printf("  EMERGENCY - request ignored\n");
            break;
            
        default:
            printf("  Request queued\n");
            break;
    }
}

/* 事件接收器2：到达楼层 */
void elevator_on_floor_arrival(elevator_sm_t *sm, int8_t floor) {
    printf("\n[Event] Arrived at floor %d (state: %s)\n",
           floor, elevator_state_name(sm->state));
    
    sm->current_floor = floor;
    
    switch (sm->state) {
        case ELEVATOR_MOVING_UP:
        case ELEVATOR_MOVING_DOWN:
            /* 检查是否需要在此楼层停靠 */
            if (sm->floor_requests[floor]) {
                sm->floor_requests[floor] = false;
                change_state(sm, ELEVATOR_DOOR_OPENING);
            } else if (floor == sm->target_floor) {
                /* 到达目标但无请求（可能被取消） */
                change_state(sm, ELEVATOR_IDLE);
            }
            /* 否则继续移动 */
            break;
            
        default:
            printf("  Unexpected floor arrival in state %s\n",
                   elevator_state_name(sm->state));
            break;
    }
}

/* 事件接收器3：门操作完成 */
void elevator_on_door_operation_complete(elevator_sm_t *sm, bool is_open) {
    printf("\n[Event] Door %s complete (state: %s)\n",
           is_open ? "open" : "closed", elevator_state_name(sm->state));
    
    switch (sm->state) {
        case ELEVATOR_DOOR_OPENING:
            if (is_open) {
                change_state(sm, ELEVATOR_DOOR_OPEN);
            }
            break;
            
        case ELEVATOR_DOOR_CLOSING:
            if (!is_open) {
                /* 门关闭，决定下一步 */
                int8_t next = find_next_target(sm, 
                    sm->current_floor < sm->target_floor);
                
                if (next < 0) {
                    change_state(sm, ELEVATOR_IDLE);
                } else if (next > sm->current_floor) {
                    sm->target_floor = next;
                    change_state(sm, ELEVATOR_MOVING_UP);
                } else if (next < sm->current_floor) {
                    sm->target_floor = next;
                    change_state(sm, ELEVATOR_MOVING_DOWN);
                } else {
                    change_state(sm, ELEVATOR_IDLE);
                }
            }
            break;
            
        default:
            break;
    }
}

/* 事件接收器4：门传感器 */
void elevator_on_door_obstruction(elevator_sm_t *sm, bool obstructed) {
    printf("\n[Event] Door obstruction: %s (state: %s)\n",
           obstructed ? "YES" : "NO", elevator_state_name(sm->state));
    
    sm->door_obstructed = obstructed;
    
    switch (sm->state) {
        case ELEVATOR_DOOR_CLOSING:
            if (obstructed) {
                printf("  Reversing door!\n");
                change_state(sm, ELEVATOR_DOOR_OPENING);
            }
            break;
            
        case ELEVATOR_DOOR_OPEN:
            if (obstructed) {
                /* 重置计时器 */
                sm->state_timer = g_tick;
            }
            break;
            
        default:
            break;
    }
}

/* 事件接收器5：定时器超时 */
void elevator_on_timeout(elevator_sm_t *sm) {
    printf("\n[Event] Timeout (state: %s)\n", elevator_state_name(sm->state));
    
    switch (sm->state) {
        case ELEVATOR_DOOR_OPEN:
            if (!sm->door_obstructed) {
                change_state(sm, ELEVATOR_DOOR_CLOSING);
            } else {
                printf("  Door obstructed, cannot close\n");
            }
            break;
            
        default:
            break;
    }
}

/* 事件接收器6：紧急按钮 */
void elevator_on_emergency(elevator_sm_t *sm, bool activate) {
    printf("\n[Event] Emergency %s (state: %s)\n",
           activate ? "ACTIVATED" : "cleared", elevator_state_name(sm->state));
    
    if (activate) {
        change_state(sm, ELEVATOR_EMERGENCY);
        /* 清除所有请求 */
        for (int i = 0; i < MAX_FLOORS; i++) {
            sm->floor_requests[i] = false;
        }
        printf("  All requests cleared, alarm triggered!\n");
    } else if (sm->state == ELEVATOR_EMERGENCY) {
        change_state(sm, ELEVATOR_IDLE);
        printf("  Emergency cleared, resuming normal operation\n");
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void multiple_event_receptor_example(void) {
    printf("=== Multiple Event Receptor Pattern Demo ===\n");
    printf("=== Elevator Control System ===\n\n");
    
    elevator_sm_t elevator;
    elevator_init(&elevator);
    
    /* 场景1：正常运行 */
    printf("\n--- Scenario 1: Normal Operation ---\n");
    
    elevator_on_floor_request(&elevator, 5);  /* 请求去5楼 */
    
    /* 模拟上行 */
    for (int floor = 2; floor <= 5; floor++) {
        elevator_on_floor_arrival(&elevator, floor);
    }
    
    elevator_on_door_operation_complete(&elevator, true);   /* 门开 */
    elevator_on_timeout(&elevator);                          /* 超时关门 */
    elevator_on_door_operation_complete(&elevator, false);  /* 门关 */
    
    /* 场景2：多楼层请求 */
    printf("\n--- Scenario 2: Multiple Floor Requests ---\n");
    
    elevator_on_floor_request(&elevator, 3);
    elevator_on_floor_request(&elevator, 7);
    elevator_on_floor_request(&elevator, 1);
    
    /* 先下到3楼 */
    elevator_on_floor_arrival(&elevator, 4);
    elevator_on_floor_arrival(&elevator, 3);
    elevator_on_door_operation_complete(&elevator, true);
    elevator_on_door_operation_complete(&elevator, false);
    
    /* 场景3：门阻挡 */
    printf("\n--- Scenario 3: Door Obstruction ---\n");
    
    elevator_on_floor_arrival(&elevator, 2);
    elevator_on_floor_arrival(&elevator, 1);
    elevator_on_door_operation_complete(&elevator, true);
    elevator_on_timeout(&elevator);  /* 开始关门 */
    elevator_on_door_obstruction(&elevator, true);   /* 被阻挡 */
    elevator_on_door_operation_complete(&elevator, true);  /* 重新开门 */
    elevator_on_door_obstruction(&elevator, false);  /* 阻挡清除 */
    elevator_on_timeout(&elevator);
    elevator_on_door_operation_complete(&elevator, false);
    
    /* 场景4：紧急情况 */
    printf("\n--- Scenario 4: Emergency ---\n");
    
    elevator_on_floor_request(&elevator, 8);
    elevator_on_floor_arrival(&elevator, 2);
    elevator_on_emergency(&elevator, true);   /* 紧急按钮 */
    elevator_on_floor_request(&elevator, 5);  /* 被忽略 */
    elevator_on_emergency(&elevator, false);  /* 解除紧急 */
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 3. 状态表模式

### 架构图

```
+------------------------------------------------------------------+
|                    STATE TABLE PATTERN                            |
+------------------------------------------------------------------+

    State-Event Matrix:
    
                    |  EVENT_A   |  EVENT_B   |  EVENT_C   |
    ----------------+------------+------------+------------+
    STATE_1         |  [trans1]  |  [trans2]  |    NULL    |
    ----------------+------------+------------+------------+
    STATE_2         |    NULL    |  [trans3]  |  [trans4]  |
    ----------------+------------+------------+------------+
    STATE_3         |  [trans5]  |    NULL    |  [trans6]  |
    ----------------+------------+------------+------------+


    Transition Entry Structure:
    
    +------------------+
    | Transition Entry |
    +------------------+
    | - guard()        |  --> Check condition
    | - exit_action()  |  --> Run before leaving state
    | - action()       |  --> Transition action
    | - entry_action() |  --> Run after entering state
    | - next_state     |  --> Target state
    +------------------+


    Execution Flow:
    
    lookup(state, event) --> if guard() --> exit_action()
                                        --> action()
                                        --> entry_action()
                                        --> next_state
```

**中文说明：**
- 状态表模式使用二维数组存储状态转换
- 数组索引为 [当前状态][事件类型]
- 对事件的响应时间与状态空间大小无关（O(1)）
- 适合大型、扁平的状态机

### 完整代码示例

```c
/*============================================================================
 * 状态表模式示例 - 自动售货机
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 状态和事件定义
 *---------------------------------------------------------------------------*/
typedef enum {
    ST_IDLE,            /* 空闲 */
    ST_COIN_INSERTED,   /* 已投币 */
    ST_SELECTING,       /* 选择商品 */
    ST_DISPENSING,      /* 出货中 */
    ST_CHANGE,          /* 找零 */
    ST_ERROR,           /* 错误 */
    ST_COUNT            /* 状态数量 */
} vending_state_t;

typedef enum {
    EV_COIN_INSERT,     /* 投币 */
    EV_COIN_RETURN,     /* 退币 */
    EV_SELECT_ITEM,     /* 选择商品 */
    EV_DISPENSE_DONE,   /* 出货完成 */
    EV_CHANGE_DONE,     /* 找零完成 */
    EV_ERROR,           /* 错误 */
    EV_RESET,           /* 重置 */
    EV_COUNT            /* 事件数量 */
} vending_event_t;

/*---------------------------------------------------------------------------
 * 状态机上下文
 *---------------------------------------------------------------------------*/
typedef struct {
    vending_state_t state;
    uint32_t balance;           /* 余额（分） */
    uint8_t selected_item;      /* 选择的商品 */
    uint32_t item_prices[10];   /* 商品价格 */
    uint32_t item_stock[10];    /* 库存 */
} vending_machine_t;

static vending_machine_t g_vm;

/*---------------------------------------------------------------------------
 * 转换表结构
 *---------------------------------------------------------------------------*/
typedef struct vending_machine vending_machine_ctx_t;

typedef bool (*guard_fn)(vending_machine_t *ctx, uint32_t param);
typedef void (*action_fn)(vending_machine_t *ctx, uint32_t param);

/* 关键点：转换条目结构 */
typedef struct {
    guard_fn guard;             /* 守卫条件 */
    action_fn exit_action;      /* 退出动作 */
    action_fn transition_action;/* 转换动作 */
    action_fn entry_action;     /* 进入动作 */
    vending_state_t next_state; /* 目标状态 */
} transition_t;

/*---------------------------------------------------------------------------
 * 守卫函数
 *---------------------------------------------------------------------------*/
bool guard_has_balance(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    return ctx->balance > 0;
}

bool guard_enough_money(vending_machine_t *ctx, uint32_t param) {
    uint8_t item = (uint8_t)param;
    return ctx->balance >= ctx->item_prices[item] && ctx->item_stock[item] > 0;
}

bool guard_need_change(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    return ctx->balance > 0;
}

bool guard_always(vending_machine_t *ctx, uint32_t param) {
    (void)ctx; (void)param;
    return true;
}

/*---------------------------------------------------------------------------
 * 动作函数
 *---------------------------------------------------------------------------*/
/* 进入动作 */
void entry_idle(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Entry] IDLE - Display 'Insert Coin'\n");
    ctx->balance = 0;
    ctx->selected_item = 0xFF;
}

void entry_coin_inserted(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Entry] COIN_INSERTED - Balance: $%.2f\n", ctx->balance / 100.0f);
}

void entry_selecting(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Entry] SELECTING - Display available items\n");
}

void entry_dispensing(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Entry] DISPENSING - Motor running for item %d\n", ctx->selected_item);
}

void entry_change(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Entry] CHANGE - Returning $%.2f\n", ctx->balance / 100.0f);
}

void entry_error(vending_machine_t *ctx, uint32_t param) {
    (void)ctx; (void)param;
    printf("  [Entry] ERROR - Display error message\n");
}

/* 退出动作 */
void exit_selecting(vending_machine_t *ctx, uint32_t param) {
    (void)ctx; (void)param;
    printf("  [Exit] SELECTING\n");
}

/* 转换动作 */
void action_add_coin(vending_machine_t *ctx, uint32_t param) {
    ctx->balance += param;
    printf("  [Action] Coin added: %u cents, total: $%.2f\n", 
           param, ctx->balance / 100.0f);
}

void action_return_coins(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Action] Returning all coins: $%.2f\n", ctx->balance / 100.0f);
    ctx->balance = 0;
}

void action_select_item(vending_machine_t *ctx, uint32_t param) {
    ctx->selected_item = (uint8_t)param;
    ctx->balance -= ctx->item_prices[param];
    ctx->item_stock[param]--;
    printf("  [Action] Selected item %d, remaining balance: $%.2f\n",
           param, ctx->balance / 100.0f);
}

void action_dispense_complete(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Action] Item %d dispensed!\n", ctx->selected_item);
}

void action_change_complete(vending_machine_t *ctx, uint32_t param) {
    (void)param;
    printf("  [Action] Change returned: $%.2f\n", ctx->balance / 100.0f);
    ctx->balance = 0;
}

/*---------------------------------------------------------------------------
 * 关键点：状态转换表
 *---------------------------------------------------------------------------*/
/* 空转换（无操作） */
static const transition_t null_transition = {NULL, NULL, NULL, NULL, ST_COUNT};

/* 关键点：二维状态表 [状态][事件] */
static const transition_t state_table[ST_COUNT][EV_COUNT] = {
    /* ST_IDLE */
    [ST_IDLE] = {
        [EV_COIN_INSERT] = {guard_always, NULL, action_add_coin, entry_coin_inserted, ST_COIN_INSERTED},
        [EV_COIN_RETURN] = {NULL, NULL, NULL, NULL, ST_COUNT},  /* 忽略 */
        [EV_SELECT_ITEM] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_DISPENSE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_CHANGE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_ERROR] = {guard_always, NULL, NULL, entry_error, ST_ERROR},
        [EV_RESET] = {NULL, NULL, NULL, NULL, ST_COUNT}
    },
    
    /* ST_COIN_INSERTED */
    [ST_COIN_INSERTED] = {
        [EV_COIN_INSERT] = {guard_always, NULL, action_add_coin, entry_coin_inserted, ST_COIN_INSERTED},
        [EV_COIN_RETURN] = {guard_has_balance, NULL, action_return_coins, entry_idle, ST_IDLE},
        [EV_SELECT_ITEM] = {guard_enough_money, exit_selecting, action_select_item, entry_dispensing, ST_DISPENSING},
        [EV_DISPENSE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_CHANGE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_ERROR] = {guard_always, NULL, NULL, entry_error, ST_ERROR},
        [EV_RESET] = {guard_always, NULL, action_return_coins, entry_idle, ST_IDLE}
    },
    
    /* ST_DISPENSING */
    [ST_DISPENSING] = {
        [EV_COIN_INSERT] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_COIN_RETURN] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_SELECT_ITEM] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_DISPENSE_DONE] = {guard_need_change, NULL, action_dispense_complete, entry_change, ST_CHANGE},
        /* 如果不需要找零，直接回到 IDLE */
        [EV_CHANGE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_ERROR] = {guard_always, NULL, NULL, entry_error, ST_ERROR},
        [EV_RESET] = {NULL, NULL, NULL, NULL, ST_COUNT}
    },
    
    /* ST_CHANGE */
    [ST_CHANGE] = {
        [EV_COIN_INSERT] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_COIN_RETURN] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_SELECT_ITEM] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_DISPENSE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_CHANGE_DONE] = {guard_always, NULL, action_change_complete, entry_idle, ST_IDLE},
        [EV_ERROR] = {guard_always, NULL, NULL, entry_error, ST_ERROR},
        [EV_RESET] = {NULL, NULL, NULL, NULL, ST_COUNT}
    },
    
    /* ST_ERROR */
    [ST_ERROR] = {
        [EV_COIN_INSERT] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_COIN_RETURN] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_SELECT_ITEM] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_DISPENSE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_CHANGE_DONE] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_ERROR] = {NULL, NULL, NULL, NULL, ST_COUNT},
        [EV_RESET] = {guard_always, NULL, action_return_coins, entry_idle, ST_IDLE}
    }
};

/*---------------------------------------------------------------------------
 * 状态机引擎
 *---------------------------------------------------------------------------*/
const char* state_name(vending_state_t state) {
    static const char *names[] = {
        "IDLE", "COIN_INSERTED", "SELECTING", 
        "DISPENSING", "CHANGE", "ERROR"
    };
    return (state < ST_COUNT) ? names[state] : "UNKNOWN";
}

const char* event_name(vending_event_t event) {
    static const char *names[] = {
        "COIN_INSERT", "COIN_RETURN", "SELECT_ITEM",
        "DISPENSE_DONE", "CHANGE_DONE", "ERROR", "RESET"
    };
    return (event < EV_COUNT) ? names[event] : "UNKNOWN";
}

void vm_init(vending_machine_t *vm) {
    vm->state = ST_IDLE;
    vm->balance = 0;
    vm->selected_item = 0xFF;
    
    /* 设置商品价格和库存 */
    for (int i = 0; i < 10; i++) {
        vm->item_prices[i] = 100 + i * 25;  /* $1.00 - $3.25 */
        vm->item_stock[i] = 5;
    }
    
    printf("[VM] Initialized\n");
    entry_idle(vm, 0);
}

/* 关键点：事件处理 - O(1) 查表 */
void vm_process_event(vending_machine_t *vm, vending_event_t event, uint32_t param) {
    printf("\n[VM] Event: %s in state %s\n",
           event_name(event), state_name(vm->state));
    
    /* 关键点：直接通过索引获取转换 */
    const transition_t *trans = &state_table[vm->state][event];
    
    /* 检查是否有有效转换 */
    if (trans->next_state >= ST_COUNT) {
        printf("  No transition defined, event ignored\n");
        return;
    }
    
    /* 检查守卫条件 */
    if (trans->guard && !trans->guard(vm, param)) {
        printf("  Guard condition failed, event ignored\n");
        return;
    }
    
    printf("  Transition: %s -> %s\n",
           state_name(vm->state), state_name(trans->next_state));
    
    /* 执行退出动作 */
    if (trans->exit_action) {
        trans->exit_action(vm, param);
    }
    
    /* 执行转换动作 */
    if (trans->transition_action) {
        trans->transition_action(vm, param);
    }
    
    /* 状态转换 */
    vm->state = trans->next_state;
    
    /* 执行进入动作 */
    if (trans->entry_action) {
        trans->entry_action(vm, param);
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void state_table_example(void) {
    printf("=== State Table Pattern Demo ===\n");
    printf("=== Vending Machine ===\n\n");
    
    vm_init(&g_vm);
    
    /* 场景1：正常购买 */
    printf("\n--- Scenario 1: Normal Purchase ---\n");
    
    vm_process_event(&g_vm, EV_COIN_INSERT, 100);  /* 投入 $1.00 */
    vm_process_event(&g_vm, EV_COIN_INSERT, 50);   /* 投入 $0.50 */
    vm_process_event(&g_vm, EV_SELECT_ITEM, 0);    /* 选择商品0（$1.00） */
    vm_process_event(&g_vm, EV_DISPENSE_DONE, 0);  /* 出货完成 */
    vm_process_event(&g_vm, EV_CHANGE_DONE, 0);    /* 找零完成 */
    
    /* 场景2：退币 */
    printf("\n--- Scenario 2: Coin Return ---\n");
    
    vm_process_event(&g_vm, EV_COIN_INSERT, 200);
    vm_process_event(&g_vm, EV_COIN_RETURN, 0);
    
    /* 场景3：余额不足 */
    printf("\n--- Scenario 3: Insufficient Balance ---\n");
    
    vm_process_event(&g_vm, EV_COIN_INSERT, 50);
    vm_process_event(&g_vm, EV_SELECT_ITEM, 0);    /* 需要 $1.00 但只有 $0.50 */
    vm_process_event(&g_vm, EV_COIN_INSERT, 100);  /* 再投 $1.00 */
    vm_process_event(&g_vm, EV_SELECT_ITEM, 0);    /* 现在可以了 */
    vm_process_event(&g_vm, EV_DISPENSE_DONE, 0);
    vm_process_event(&g_vm, EV_CHANGE_DONE, 0);
    
    /* 场景4：错误恢复 */
    printf("\n--- Scenario 4: Error Recovery ---\n");
    
    vm_process_event(&g_vm, EV_COIN_INSERT, 100);
    vm_process_event(&g_vm, EV_ERROR, 0);          /* 发生错误 */
    vm_process_event(&g_vm, EV_SELECT_ITEM, 0);    /* 被忽略 */
    vm_process_event(&g_vm, EV_RESET, 0);          /* 重置 */
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 4. 状态模式

### 架构图

```
+------------------------------------------------------------------+
|                       STATE PATTERN                               |
+------------------------------------------------------------------+

    Object-Oriented State Machine:
    
    +-----------------+         +------------------+
    |     Context     |         |   <<abstract>>   |
    +-----------------+         |      State       |
    | - current_state |-------->+------------------+
    +-----------------+         | + handle_event() |
    | + process()     |         +--------+---------+
    +--------+--------+                  |
             |                           |
             |           +---------------+---------------+
             |           |               |               |
             |    +------+------+ +------+------+ +------+------+
             |    | ConcreteA   | | ConcreteB   | | ConcreteC   |
             |    +-------------+ +-------------+ +-------------+
             |    |handle_event | |handle_event | |handle_event |
             |    +-------------+ +-------------+ +-------------+
             |           ^               ^               ^
             |           |               |               |
             +-----------+---------------+---------------+
                         state->handle_event()


    Polymorphic Dispatch:
    
    Context.process(event) --> current_state->handle_event(event)
                                     |
                         +-----------+-----------+
                         |           |           |
                     StateA()    StateB()    StateC()
```

**中文说明：**
- 状态模式将每个状态封装为独立的对象/结构体
- Context 维护当前状态对象，并将事件处理委托给它
- 通过函数指针实现多态分派
- 适合需要频繁修改状态行为的情况

### 完整代码示例

```c
/*============================================================================
 * 状态模式示例 - 交通信号灯控制
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 前向声明
 *---------------------------------------------------------------------------*/
typedef struct traffic_light traffic_light_t;
typedef struct state state_t;

/*---------------------------------------------------------------------------
 * 事件定义
 *---------------------------------------------------------------------------*/
typedef enum {
    TL_EVT_TIMER,           /* 定时器超时 */
    TL_EVT_EMERGENCY,       /* 紧急车辆 */
    TL_EVT_MANUAL_OVERRIDE, /* 手动覆盖 */
    TL_EVT_FAULT,           /* 故障 */
    TL_EVT_RESET            /* 重置 */
} tl_event_t;

/*---------------------------------------------------------------------------
 * 关键点：状态接口（函数指针表）
 *---------------------------------------------------------------------------*/
typedef void (*state_enter_fn)(traffic_light_t *ctx);
typedef void (*state_exit_fn)(traffic_light_t *ctx);
typedef void (*state_handle_fn)(traffic_light_t *ctx, tl_event_t event);

struct state {
    const char *name;
    state_enter_fn enter;
    state_exit_fn exit;
    state_handle_fn handle;
    uint32_t duration_ms;       /* 状态持续时间 */
};

/*---------------------------------------------------------------------------
 * Context 结构
 *---------------------------------------------------------------------------*/
struct traffic_light {
    const state_t *current_state;
    uint32_t state_timer;
    bool emergency_mode;
    bool manual_mode;
    
    /* 信号灯输出 */
    bool red_on;
    bool yellow_on;
    bool green_on;
};

/*---------------------------------------------------------------------------
 * 状态声明
 *---------------------------------------------------------------------------*/
extern const state_t state_red;
extern const state_t state_yellow;
extern const state_t state_green;
extern const state_t state_flashing;
extern const state_t state_emergency;

/*---------------------------------------------------------------------------
 * 状态转换辅助
 *---------------------------------------------------------------------------*/
static void tl_change_state(traffic_light_t *ctx, const state_t *new_state) {
    printf("[TL] State: %s -> %s\n",
           ctx->current_state->name, new_state->name);
    
    /* 退出当前状态 */
    if (ctx->current_state->exit) {
        ctx->current_state->exit(ctx);
    }
    
    ctx->current_state = new_state;
    ctx->state_timer = 0;
    
    /* 进入新状态 */
    if (new_state->enter) {
        new_state->enter(ctx);
    }
}

static void tl_set_lights(traffic_light_t *ctx, bool red, bool yellow, bool green) {
    ctx->red_on = red;
    ctx->yellow_on = yellow;
    ctx->green_on = green;
    printf("  [Lights] R:%d Y:%d G:%d\n", red, yellow, green);
}

/*---------------------------------------------------------------------------
 * 关键点：具体状态实现
 *---------------------------------------------------------------------------*/

/* --- 红灯状态 --- */
void state_red_enter(traffic_light_t *ctx) {
    printf("  [Enter] RED - Stop all traffic\n");
    tl_set_lights(ctx, true, false, false);
}

void state_red_handle(traffic_light_t *ctx, tl_event_t event) {
    switch (event) {
        case TL_EVT_TIMER:
            tl_change_state(ctx, &state_green);
            break;
        case TL_EVT_EMERGENCY:
            ctx->emergency_mode = true;
            tl_change_state(ctx, &state_emergency);
            break;
        case TL_EVT_FAULT:
            tl_change_state(ctx, &state_flashing);
            break;
        default:
            printf("  [RED] Event %d ignored\n", event);
            break;
    }
}

const state_t state_red = {
    .name = "RED",
    .enter = state_red_enter,
    .exit = NULL,
    .handle = state_red_handle,
    .duration_ms = 30000
};

/* --- 绿灯状态 --- */
void state_green_enter(traffic_light_t *ctx) {
    printf("  [Enter] GREEN - Traffic flowing\n");
    tl_set_lights(ctx, false, false, true);
}

void state_green_handle(traffic_light_t *ctx, tl_event_t event) {
    switch (event) {
        case TL_EVT_TIMER:
            tl_change_state(ctx, &state_yellow);
            break;
        case TL_EVT_EMERGENCY:
            ctx->emergency_mode = true;
            /* 先变黄灯 */
            tl_change_state(ctx, &state_yellow);
            break;
        case TL_EVT_FAULT:
            tl_change_state(ctx, &state_flashing);
            break;
        default:
            printf("  [GREEN] Event %d ignored\n", event);
            break;
    }
}

const state_t state_green = {
    .name = "GREEN",
    .enter = state_green_enter,
    .exit = NULL,
    .handle = state_green_handle,
    .duration_ms = 25000
};

/* --- 黄灯状态 --- */
void state_yellow_enter(traffic_light_t *ctx) {
    printf("  [Enter] YELLOW - Caution\n");
    tl_set_lights(ctx, false, true, false);
}

void state_yellow_handle(traffic_light_t *ctx, tl_event_t event) {
    switch (event) {
        case TL_EVT_TIMER:
            if (ctx->emergency_mode) {
                tl_change_state(ctx, &state_emergency);
            } else {
                tl_change_state(ctx, &state_red);
            }
            break;
        case TL_EVT_EMERGENCY:
            ctx->emergency_mode = true;
            /* 继续黄灯直到超时 */
            break;
        case TL_EVT_FAULT:
            tl_change_state(ctx, &state_flashing);
            break;
        default:
            printf("  [YELLOW] Event %d ignored\n", event);
            break;
    }
}

const state_t state_yellow = {
    .name = "YELLOW",
    .enter = state_yellow_enter,
    .exit = NULL,
    .handle = state_yellow_handle,
    .duration_ms = 5000
};

/* --- 闪烁状态（故障） --- */
void state_flashing_enter(traffic_light_t *ctx) {
    printf("  [Enter] FLASHING - Fault mode, caution!\n");
    /* 黄灯闪烁 */
    tl_set_lights(ctx, false, true, false);
}

void state_flashing_handle(traffic_light_t *ctx, tl_event_t event) {
    switch (event) {
        case TL_EVT_TIMER:
            /* 切换黄灯状态 */
            ctx->yellow_on = !ctx->yellow_on;
            printf("  [Lights] Flashing: Y:%d\n", ctx->yellow_on);
            break;
        case TL_EVT_RESET:
            tl_change_state(ctx, &state_red);
            break;
        default:
            printf("  [FLASHING] Event %d ignored\n", event);
            break;
    }
}

const state_t state_flashing = {
    .name = "FLASHING",
    .enter = state_flashing_enter,
    .exit = NULL,
    .handle = state_flashing_handle,
    .duration_ms = 500  /* 闪烁频率 */
};

/* --- 紧急状态 --- */
void state_emergency_enter(traffic_light_t *ctx) {
    printf("  [Enter] EMERGENCY - All RED for emergency vehicle\n");
    tl_set_lights(ctx, true, false, false);
}

void state_emergency_exit(traffic_light_t *ctx) {
    ctx->emergency_mode = false;
    printf("  [Exit] EMERGENCY - Resuming normal operation\n");
}

void state_emergency_handle(traffic_light_t *ctx, tl_event_t event) {
    switch (event) {
        case TL_EVT_TIMER:
            /* 紧急模式持续直到手动重置 */
            break;
        case TL_EVT_RESET:
            tl_change_state(ctx, &state_red);
            break;
        default:
            printf("  [EMERGENCY] Event %d ignored\n", event);
            break;
    }
}

const state_t state_emergency = {
    .name = "EMERGENCY",
    .enter = state_emergency_enter,
    .exit = state_emergency_exit,
    .handle = state_emergency_handle,
    .duration_ms = 0  /* 无自动超时 */
};

/*---------------------------------------------------------------------------
 * Context 操作
 *---------------------------------------------------------------------------*/
void tl_init(traffic_light_t *ctx) {
    ctx->current_state = &state_red;
    ctx->state_timer = 0;
    ctx->emergency_mode = false;
    ctx->manual_mode = false;
    
    printf("[TL] Initialized\n");
    if (ctx->current_state->enter) {
        ctx->current_state->enter(ctx);
    }
}

/* 关键点：通过当前状态处理事件 */
void tl_process_event(traffic_light_t *ctx, tl_event_t event) {
    printf("\n[TL] Event: %d in state %s\n",
           event, ctx->current_state->name);
    
    /* 关键点：多态分派到当前状态 */
    if (ctx->current_state->handle) {
        ctx->current_state->handle(ctx, event);
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void state_pattern_example(void) {
    printf("=== State Pattern Demo ===\n");
    printf("=== Traffic Light Controller ===\n\n");
    
    traffic_light_t tl;
    tl_init(&tl);
    
    /* 正常循环 */
    printf("\n--- Normal Operation Cycle ---\n");
    
    tl_process_event(&tl, TL_EVT_TIMER);  /* RED -> GREEN */
    tl_process_event(&tl, TL_EVT_TIMER);  /* GREEN -> YELLOW */
    tl_process_event(&tl, TL_EVT_TIMER);  /* YELLOW -> RED */
    tl_process_event(&tl, TL_EVT_TIMER);  /* RED -> GREEN */
    
    /* 紧急车辆 */
    printf("\n--- Emergency Vehicle ---\n");
    
    tl_process_event(&tl, TL_EVT_EMERGENCY);  /* GREEN -> YELLOW */
    tl_process_event(&tl, TL_EVT_TIMER);       /* YELLOW -> EMERGENCY */
    tl_process_event(&tl, TL_EVT_TIMER);       /* 保持 EMERGENCY */
    tl_process_event(&tl, TL_EVT_RESET);       /* EMERGENCY -> RED */
    
    /* 故障模式 */
    printf("\n--- Fault Mode ---\n");
    
    tl_process_event(&tl, TL_EVT_FAULT);      /* RED -> FLASHING */
    tl_process_event(&tl, TL_EVT_TIMER);       /* 闪烁 */
    tl_process_event(&tl, TL_EVT_TIMER);       /* 闪烁 */
    tl_process_event(&tl, TL_EVT_TIMER);       /* 闪烁 */
    tl_process_event(&tl, TL_EVT_RESET);       /* FLASHING -> RED */
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 5. 分解 AND 状态模式

### 架构图

```
+------------------------------------------------------------------+
|               DECOMPOSED AND-STATE PATTERN                        |
+------------------------------------------------------------------+

    AND-State (Orthogonal Regions):
    
    +--------------------------------------------------+
    |              Main State Machine                   |
    |  +--------------------+  +--------------------+  |
    |  |   Region A         |  |   Region B         |  |
    |  | (Operational Mode) |  | (Communication)    |  |
    |  |                    |  |                    |  |
    |  | [Idle]<-->[Active] |  | [Offline]<-->[Online] |
    |  +--------------------+  +--------------------+  |
    |                                                  |
    |  Both regions execute in parallel!               |
    +--------------------------------------------------+


    Decomposed Structure:
    
    +-----------------+
    |    Context      |
    |  (Main Object)  |
    +--------+--------+
             |
             | Owns/delegates
             |
    +--------+--------+--------+
    |                 |                 |
    v                 v                 v
    +-------------+ +-------------+ +-------------+
    | Sub-SM A    | | Sub-SM B    | | Sub-SM C    |
    | (Region A)  | | (Region B)  | | (Region C)  |
    +-------------+ +-------------+ +-------------+
    | [State A1]  | | [State B1]  | | [State C1]  |
    | [State A2]  | | [State B2]  | | [State C2]  |
    +-------------+ +-------------+ +-------------+
```

**中文说明：**
- 分解 AND 状态模式将正交区域分解为独立的子状态机
- 主对象（Context）管理顶层状态和子状态机的协调
- 各个子状态机可以并行执行，互不干扰
- 适合复杂系统中有多个独立但相关的状态变化

### 完整代码示例

```c
/*============================================================================
 * 分解 AND 状态模式示例 - 智能家居控制系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 子状态机：安防区域（Security Region）
 *---------------------------------------------------------------------------*/
typedef enum {
    SECURITY_DISARMED,      /* 已撤防 */
    SECURITY_ARMING,        /* 布防中 */
    SECURITY_ARMED,         /* 已布防 */
    SECURITY_ALARM          /* 报警 */
} security_state_t;

typedef enum {
    SEC_EVT_ARM,            /* 布防命令 */
    SEC_EVT_DISARM,         /* 撤防命令 */
    SEC_EVT_TIMER,          /* 定时器 */
    SEC_EVT_MOTION,         /* 检测到移动 */
    SEC_EVT_DOOR_OPEN       /* 门被打开 */
} security_event_t;

typedef struct {
    security_state_t state;
    uint32_t arm_delay_timer;
    bool motion_detected;
    bool door_sensor_active;
} security_sm_t;

void security_sm_init(security_sm_t *sm) {
    sm->state = SECURITY_DISARMED;
    sm->arm_delay_timer = 0;
    sm->motion_detected = false;
    sm->door_sensor_active = false;
    printf("  [Security] Initialized: DISARMED\n");
}

const char* security_state_name(security_state_t state) {
    static const char *names[] = {"DISARMED", "ARMING", "ARMED", "ALARM"};
    return names[state];
}

/* 关键点：子状态机独立处理自己的事件 */
void security_sm_process(security_sm_t *sm, security_event_t event) {
    security_state_t old_state = sm->state;
    
    switch (sm->state) {
        case SECURITY_DISARMED:
            if (event == SEC_EVT_ARM) {
                sm->state = SECURITY_ARMING;
                sm->arm_delay_timer = 30;  /* 30秒延迟 */
            }
            break;
            
        case SECURITY_ARMING:
            if (event == SEC_EVT_TIMER) {
                sm->arm_delay_timer--;
                if (sm->arm_delay_timer == 0) {
                    sm->state = SECURITY_ARMED;
                }
            } else if (event == SEC_EVT_DISARM) {
                sm->state = SECURITY_DISARMED;
            }
            break;
            
        case SECURITY_ARMED:
            if (event == SEC_EVT_MOTION || event == SEC_EVT_DOOR_OPEN) {
                sm->state = SECURITY_ALARM;
                sm->motion_detected = (event == SEC_EVT_MOTION);
                sm->door_sensor_active = (event == SEC_EVT_DOOR_OPEN);
            } else if (event == SEC_EVT_DISARM) {
                sm->state = SECURITY_DISARMED;
            }
            break;
            
        case SECURITY_ALARM:
            if (event == SEC_EVT_DISARM) {
                sm->state = SECURITY_DISARMED;
                sm->motion_detected = false;
                sm->door_sensor_active = false;
            }
            break;
    }
    
    if (old_state != sm->state) {
        printf("  [Security] %s -> %s\n",
               security_state_name(old_state),
               security_state_name(sm->state));
    }
}

/*---------------------------------------------------------------------------
 * 子状态机：照明区域（Lighting Region）
 *---------------------------------------------------------------------------*/
typedef enum {
    LIGHTING_OFF,           /* 关闭 */
    LIGHTING_ON,            /* 开启 */
    LIGHTING_DIMMED,        /* 调暗 */
    LIGHTING_AUTO           /* 自动模式 */
} lighting_state_t;

typedef enum {
    LIGHT_EVT_SWITCH,       /* 开关按下 */
    LIGHT_EVT_DIM,          /* 调暗 */
    LIGHT_EVT_AUTO,         /* 自动模式 */
    LIGHT_EVT_OCCUPANCY,    /* 有人 */
    LIGHT_EVT_NO_OCCUPANCY, /* 无人 */
    LIGHT_EVT_TIMER         /* 定时器 */
} lighting_event_t;

typedef struct {
    lighting_state_t state;
    uint8_t brightness;     /* 0-100 */
    bool occupancy;
    uint32_t auto_off_timer;
} lighting_sm_t;

void lighting_sm_init(lighting_sm_t *sm) {
    sm->state = LIGHTING_OFF;
    sm->brightness = 0;
    sm->occupancy = false;
    sm->auto_off_timer = 0;
    printf("  [Lighting] Initialized: OFF\n");
}

const char* lighting_state_name(lighting_state_t state) {
    static const char *names[] = {"OFF", "ON", "DIMMED", "AUTO"};
    return names[state];
}

void lighting_sm_process(lighting_sm_t *sm, lighting_event_t event) {
    lighting_state_t old_state = sm->state;
    
    switch (sm->state) {
        case LIGHTING_OFF:
            if (event == LIGHT_EVT_SWITCH) {
                sm->state = LIGHTING_ON;
                sm->brightness = 100;
            } else if (event == LIGHT_EVT_AUTO) {
                sm->state = LIGHTING_AUTO;
            }
            break;
            
        case LIGHTING_ON:
            if (event == LIGHT_EVT_SWITCH) {
                sm->state = LIGHTING_OFF;
                sm->brightness = 0;
            } else if (event == LIGHT_EVT_DIM) {
                sm->state = LIGHTING_DIMMED;
                sm->brightness = 30;
            } else if (event == LIGHT_EVT_AUTO) {
                sm->state = LIGHTING_AUTO;
            }
            break;
            
        case LIGHTING_DIMMED:
            if (event == LIGHT_EVT_SWITCH) {
                sm->state = LIGHTING_ON;
                sm->brightness = 100;
            } else if (event == LIGHT_EVT_DIM) {
                sm->state = LIGHTING_OFF;
                sm->brightness = 0;
            }
            break;
            
        case LIGHTING_AUTO:
            if (event == LIGHT_EVT_OCCUPANCY) {
                sm->occupancy = true;
                sm->brightness = 80;
                sm->auto_off_timer = 0;
            } else if (event == LIGHT_EVT_NO_OCCUPANCY) {
                sm->occupancy = false;
                sm->auto_off_timer = 300;  /* 5分钟后关闭 */
            } else if (event == LIGHT_EVT_TIMER && !sm->occupancy) {
                if (sm->auto_off_timer > 0) {
                    sm->auto_off_timer--;
                }
                if (sm->auto_off_timer == 0) {
                    sm->brightness = 0;
                }
            } else if (event == LIGHT_EVT_SWITCH) {
                sm->state = LIGHTING_OFF;
                sm->brightness = 0;
            }
            break;
    }
    
    if (old_state != sm->state) {
        printf("  [Lighting] %s -> %s (brightness: %d%%)\n",
               lighting_state_name(old_state),
               lighting_state_name(sm->state),
               sm->brightness);
    }
}

/*---------------------------------------------------------------------------
 * 子状态机：温控区域（HVAC Region）
 *---------------------------------------------------------------------------*/
typedef enum {
    HVAC_OFF,               /* 关闭 */
    HVAC_HEATING,           /* 制热 */
    HVAC_COOLING,           /* 制冷 */
    HVAC_FAN_ONLY           /* 仅通风 */
} hvac_state_t;

typedef enum {
    HVAC_EVT_TEMP_LOW,      /* 温度过低 */
    HVAC_EVT_TEMP_HIGH,     /* 温度过高 */
    HVAC_EVT_TEMP_OK,       /* 温度适中 */
    HVAC_EVT_MODE_CHANGE,   /* 模式切换 */
    HVAC_EVT_OFF            /* 关闭 */
} hvac_event_t;

typedef struct {
    hvac_state_t state;
    float current_temp;
    float target_temp;
    bool heating_enabled;
    bool cooling_enabled;
} hvac_sm_t;

void hvac_sm_init(hvac_sm_t *sm) {
    sm->state = HVAC_OFF;
    sm->current_temp = 22.0f;
    sm->target_temp = 22.0f;
    sm->heating_enabled = true;
    sm->cooling_enabled = true;
    printf("  [HVAC] Initialized: OFF (target: %.1f°C)\n", sm->target_temp);
}

const char* hvac_state_name(hvac_state_t state) {
    static const char *names[] = {"OFF", "HEATING", "COOLING", "FAN_ONLY"};
    return names[state];
}

void hvac_sm_process(hvac_sm_t *sm, hvac_event_t event, float temp_param) {
    hvac_state_t old_state = sm->state;
    
    if (temp_param > 0) {
        sm->current_temp = temp_param;
    }
    
    switch (sm->state) {
        case HVAC_OFF:
            if (event == HVAC_EVT_TEMP_LOW && sm->heating_enabled) {
                sm->state = HVAC_HEATING;
            } else if (event == HVAC_EVT_TEMP_HIGH && sm->cooling_enabled) {
                sm->state = HVAC_COOLING;
            } else if (event == HVAC_EVT_MODE_CHANGE) {
                sm->state = HVAC_FAN_ONLY;
            }
            break;
            
        case HVAC_HEATING:
            if (event == HVAC_EVT_TEMP_OK || event == HVAC_EVT_TEMP_HIGH) {
                sm->state = HVAC_OFF;
            } else if (event == HVAC_EVT_OFF) {
                sm->state = HVAC_OFF;
            }
            break;
            
        case HVAC_COOLING:
            if (event == HVAC_EVT_TEMP_OK || event == HVAC_EVT_TEMP_LOW) {
                sm->state = HVAC_OFF;
            } else if (event == HVAC_EVT_OFF) {
                sm->state = HVAC_OFF;
            }
            break;
            
        case HVAC_FAN_ONLY:
            if (event == HVAC_EVT_OFF) {
                sm->state = HVAC_OFF;
            } else if (event == HVAC_EVT_TEMP_LOW && sm->heating_enabled) {
                sm->state = HVAC_HEATING;
            } else if (event == HVAC_EVT_TEMP_HIGH && sm->cooling_enabled) {
                sm->state = HVAC_COOLING;
            }
            break;
    }
    
    if (old_state != sm->state) {
        printf("  [HVAC] %s -> %s (current: %.1f°C, target: %.1f°C)\n",
               hvac_state_name(old_state),
               hvac_state_name(sm->state),
               sm->current_temp, sm->target_temp);
    }
}

/*---------------------------------------------------------------------------
 * 关键点：主控制器（Context）- 协调所有子状态机
 *---------------------------------------------------------------------------*/
typedef enum {
    HOME_MODE_AWAY,         /* 外出模式 */
    HOME_MODE_HOME,         /* 在家模式 */
    HOME_MODE_NIGHT,        /* 夜间模式 */
    HOME_MODE_VACATION      /* 度假模式 */
} home_mode_t;

typedef struct {
    home_mode_t mode;
    
    /* 关键点：组合多个正交区域的子状态机 */
    security_sm_t security;
    lighting_sm_t lighting;
    hvac_sm_t hvac;
    
    uint32_t tick;
} smart_home_t;

void smart_home_init(smart_home_t *home) {
    home->mode = HOME_MODE_HOME;
    home->tick = 0;
    
    printf("[SmartHome] Initializing subsystems...\n");
    security_sm_init(&home->security);
    lighting_sm_init(&home->lighting);
    hvac_sm_init(&home->hvac);
    
    printf("[SmartHome] Ready, mode: HOME\n");
}

const char* home_mode_name(home_mode_t mode) {
    static const char *names[] = {"AWAY", "HOME", "NIGHT", "VACATION"};
    return names[mode];
}

/* 关键点：模式切换协调所有子系统 */
void smart_home_set_mode(smart_home_t *home, home_mode_t new_mode) {
    printf("\n[SmartHome] Mode change: %s -> %s\n",
           home_mode_name(home->mode), home_mode_name(new_mode));
    
    home->mode = new_mode;
    
    /* 关键点：根据模式协调各子系统 */
    switch (new_mode) {
        case HOME_MODE_AWAY:
            printf("  Configuring AWAY mode...\n");
            security_sm_process(&home->security, SEC_EVT_ARM);
            lighting_sm_process(&home->lighting, LIGHT_EVT_AUTO);
            hvac_sm_process(&home->hvac, HVAC_EVT_OFF, 0);
            break;
            
        case HOME_MODE_HOME:
            printf("  Configuring HOME mode...\n");
            security_sm_process(&home->security, SEC_EVT_DISARM);
            lighting_sm_process(&home->lighting, LIGHT_EVT_SWITCH);
            /* HVAC 根据温度自动调节 */
            break;
            
        case HOME_MODE_NIGHT:
            printf("  Configuring NIGHT mode...\n");
            security_sm_process(&home->security, SEC_EVT_ARM);
            lighting_sm_process(&home->lighting, LIGHT_EVT_DIM);
            home->hvac.target_temp = 20.0f;  /* 夜间温度 */
            break;
            
        case HOME_MODE_VACATION:
            printf("  Configuring VACATION mode...\n");
            security_sm_process(&home->security, SEC_EVT_ARM);
            lighting_sm_process(&home->lighting, LIGHT_EVT_AUTO);
            home->hvac.target_temp = 15.0f;  /* 节能温度 */
            hvac_sm_process(&home->hvac, HVAC_EVT_OFF, 0);
            break;
    }
}

/* 关键点：事件分发到相应子系统 */
void smart_home_on_motion(smart_home_t *home, bool detected) {
    printf("\n[SmartHome] Motion %s\n", detected ? "DETECTED" : "cleared");
    
    if (detected) {
        security_sm_process(&home->security, SEC_EVT_MOTION);
        lighting_sm_process(&home->lighting, LIGHT_EVT_OCCUPANCY);
    } else {
        lighting_sm_process(&home->lighting, LIGHT_EVT_NO_OCCUPANCY);
    }
}

void smart_home_on_door(smart_home_t *home, bool opened) {
    printf("\n[SmartHome] Door %s\n", opened ? "OPENED" : "closed");
    
    if (opened) {
        security_sm_process(&home->security, SEC_EVT_DOOR_OPEN);
    }
}

void smart_home_on_temperature(smart_home_t *home, float temp) {
    printf("\n[SmartHome] Temperature: %.1f°C (target: %.1f°C)\n",
           temp, home->hvac.target_temp);
    
    if (temp < home->hvac.target_temp - 1.0f) {
        hvac_sm_process(&home->hvac, HVAC_EVT_TEMP_LOW, temp);
    } else if (temp > home->hvac.target_temp + 1.0f) {
        hvac_sm_process(&home->hvac, HVAC_EVT_TEMP_HIGH, temp);
    } else {
        hvac_sm_process(&home->hvac, HVAC_EVT_TEMP_OK, temp);
    }
}

void smart_home_tick(smart_home_t *home) {
    home->tick++;
    
    /* 各子系统定时器处理 */
    security_sm_process(&home->security, SEC_EVT_TIMER);
    lighting_sm_process(&home->lighting, LIGHT_EVT_TIMER);
}

/* 状态查询 */
void smart_home_print_status(smart_home_t *home) {
    printf("\n=== Smart Home Status ===\n");
    printf("  Mode: %s\n", home_mode_name(home->mode));
    printf("  Security: %s\n", security_state_name(home->security.state));
    printf("  Lighting: %s (brightness: %d%%)\n",
           lighting_state_name(home->lighting.state),
           home->lighting.brightness);
    printf("  HVAC: %s (current: %.1f°C, target: %.1f°C)\n",
           hvac_state_name(home->hvac.state),
           home->hvac.current_temp,
           home->hvac.target_temp);
    printf("========================\n");
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void decomposed_and_state_example(void) {
    printf("=== Decomposed AND-State Pattern Demo ===\n");
    printf("=== Smart Home Control System ===\n\n");
    
    smart_home_t home;
    smart_home_init(&home);
    smart_home_print_status(&home);
    
    /* 场景1：外出 */
    printf("\n--- Scenario 1: Leaving Home ---\n");
    smart_home_set_mode(&home, HOME_MODE_AWAY);
    
    /* 布防延迟计时 */
    for (int i = 0; i < 30; i++) {
        smart_home_tick(&home);
    }
    smart_home_print_status(&home);
    
    /* 场景2：入侵检测 */
    printf("\n--- Scenario 2: Intrusion Detection ---\n");
    smart_home_on_motion(&home, true);
    smart_home_print_status(&home);
    
    /* 场景3：回家 */
    printf("\n--- Scenario 3: Coming Home ---\n");
    smart_home_set_mode(&home, HOME_MODE_HOME);
    smart_home_on_motion(&home, true);
    smart_home_print_status(&home);
    
    /* 场景4：温度控制 */
    printf("\n--- Scenario 4: Temperature Control ---\n");
    smart_home_on_temperature(&home, 18.0f);  /* 偏冷 */
    smart_home_print_status(&home);
    
    smart_home_on_temperature(&home, 22.0f);  /* 适中 */
    smart_home_print_status(&home);
    
    /* 场景5：夜间模式 */
    printf("\n--- Scenario 5: Night Mode ---\n");
    smart_home_set_mode(&home, HOME_MODE_NIGHT);
    smart_home_print_status(&home);
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 总结

| 模式 | 适用场景 | 主要特点 | 复杂度 |
|------|----------|----------|--------|
| 单事件接收器 | 通用状态机 | 统一事件接口 | 中等 |
| 多事件接收器 | 同步状态机 | 类型安全，无分发 | 简单 |
| 状态表 | 大型扁平状态机 | O(1) 查找 | 中等 |
| 状态模式 | 需要多态的状态机 | 面向对象，易扩展 | 中等 |
| 分解 AND 状态 | 正交区域状态机 | 并行状态，解耦 | 复杂 |

