# 状态模式 (State Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                       STATE PATTERN                               |
+------------------------------------------------------------------+

    WITHOUT STATE PATTERN (if-else hell):
    
    handle_event(event) {
        if (current_state == IDLE) {
            if (event == PLAY) { /* ... */ }
            else if (event == STOP) { /* ... */ }
        } else if (current_state == PLAYING) {
            if (event == PAUSE) { /* ... */ }
            else if (event == STOP) { /* ... */ }
        } else if (current_state == PAUSED) {
            // ... more nested if-else
        }
    }


    WITH STATE PATTERN (Encapsulated States):
    
    +------------------+         +------------------+
    |     Context      |  state  |      State       |
    |    (Player)      |-------->|   (Interface)    |
    +------------------+         +------------------+
    | current_state    |         | handle_play()    |
    | set_state()      |         | handle_pause()   |
    | play()           |         | handle_stop()    |
    | pause()          |         +--------+---------+
    | stop()           |                  ^
    +------------------+                  |
                                +--------+---------+
                                |        |         |
                          +-----+--+ +---+---+ +---+-----+
                          | Idle   | |Playing| | Paused  |
                          | State  | | State | | State   |
                          +--------+ +-------+ +---------+


    STATE TRANSITIONS:
    
             play()              pause()
    +------+------->+-------+------->+--------+
    | IDLE |        |PLAYING|        | PAUSED |
    +------+<-------+-------+<-------+--------+
             stop()              play()
                 |                  |
                 +--------+---------+
                          |
                        stop()
```

**核心思想说明：**
- 将状态相关的行为封装到独立的状态类中
- 状态转换由状态对象自己控制
- 消除大量的条件判断语句
- 新增状态不需要修改现有状态代码

## 实现思路

1. **定义状态接口**：每个事件对应一个处理方法
2. **实现具体状态**：每个状态处理方式不同
3. **上下文持有当前状态**：委托给状态处理
4. **状态转换**：状态内部决定下一个状态

## 典型应用场景

- 媒体播放器状态
- TCP连接状态
- 订单状态流转
- 游戏角色状态
- 自动售货机

## 完整代码示例

```c
/*============================================================================
 * 状态模式示例 - 自动售货机
 *============================================================================*/

/*---------------------------------------------------------------------------
 * vending_state.h - 状态接口定义
 *---------------------------------------------------------------------------*/
#ifndef VENDING_STATE_H
#define VENDING_STATE_H

#include <stdbool.h>

/* 前向声明 */
typedef struct vending_machine vending_machine_t;
typedef struct vending_state vending_state_t;

/* 关键点：状态接口 - 每个事件一个处理方法 */
typedef struct {
    const char *name;
    void (*on_enter)(vending_state_t *state, vending_machine_t *vm);
    void (*on_exit)(vending_state_t *state, vending_machine_t *vm);
    
    /* 事件处理方法 */
    void (*insert_coin)(vending_state_t *state, vending_machine_t *vm, int amount);
    void (*select_item)(vending_state_t *state, vending_machine_t *vm, int item_id);
    void (*dispense)(vending_state_t *state, vending_machine_t *vm);
    void (*cancel)(vending_state_t *state, vending_machine_t *vm);
} state_ops_t;

struct vending_state {
    const state_ops_t *ops;
    void *data;
};

/* 状态创建函数 */
vending_state_t* idle_state_create(void);
vending_state_t* has_money_state_create(void);
vending_state_t* item_selected_state_create(void);
vending_state_t* dispensing_state_create(void);

#endif /* VENDING_STATE_H */


/*---------------------------------------------------------------------------
 * vending_machine.h - 上下文定义
 *---------------------------------------------------------------------------*/
#ifndef VENDING_MACHINE_H
#define VENDING_MACHINE_H

#include "vending_state.h"

#define MAX_ITEMS 10

typedef struct {
    int id;
    const char *name;
    int price;
    int quantity;
} vending_item_t;

struct vending_machine {
    vending_state_t *current_state;  /* 关键点：当前状态 */
    int balance;                      /* 当前余额 */
    int selected_item;                /* 选中的商品 */
    vending_item_t items[MAX_ITEMS];
    int item_count;
    
    /* 预创建的状态对象（避免重复创建） */
    vending_state_t *state_idle;
    vending_state_t *state_has_money;
    vending_state_t *state_selected;
    vending_state_t *state_dispensing;
};

vending_machine_t* vending_machine_create(void);
void vending_machine_destroy(vending_machine_t *vm);

/* 关键点：状态转换 */
void vending_machine_set_state(vending_machine_t *vm, vending_state_t *state);

/* 用户操作（委托给当前状态） */
void vending_machine_insert_coin(vending_machine_t *vm, int amount);
void vending_machine_select_item(vending_machine_t *vm, int item_id);
void vending_machine_dispense(vending_machine_t *vm);
void vending_machine_cancel(vending_machine_t *vm);

/* 辅助函数 */
void vending_machine_add_item(vending_machine_t *vm, const char *name, int price, int qty);
void vending_machine_print_status(vending_machine_t *vm);

#endif /* VENDING_MACHINE_H */


/*---------------------------------------------------------------------------
 * vending_machine.c - 上下文实现
 *---------------------------------------------------------------------------*/
#include "vending_machine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

vending_machine_t* vending_machine_create(void) {
    vending_machine_t *vm = malloc(sizeof(vending_machine_t));
    if (vm == NULL) return NULL;
    
    memset(vm, 0, sizeof(vending_machine_t));
    
    /* 预创建所有状态 */
    vm->state_idle = idle_state_create();
    vm->state_has_money = has_money_state_create();
    vm->state_selected = item_selected_state_create();
    vm->state_dispensing = dispensing_state_create();
    
    /* 初始状态 */
    vm->current_state = vm->state_idle;
    vm->balance = 0;
    vm->selected_item = -1;
    
    printf("[VendingMachine] Created, initial state: %s\n", 
           vm->current_state->ops->name);
    
    return vm;
}

void vending_machine_destroy(vending_machine_t *vm) {
    if (vm != NULL) {
        free(vm->state_idle);
        free(vm->state_has_money);
        free(vm->state_selected);
        free(vm->state_dispensing);
        free(vm);
    }
}

/* 关键点：状态转换 */
void vending_machine_set_state(vending_machine_t *vm, vending_state_t *state) {
    if (vm->current_state != state) {
        printf("\n[State] %s --> %s\n", 
               vm->current_state->ops->name, state->ops->name);
        
        /* 退出当前状态 */
        if (vm->current_state->ops->on_exit != NULL) {
            vm->current_state->ops->on_exit(vm->current_state, vm);
        }
        
        /* 进入新状态 */
        vm->current_state = state;
        
        if (state->ops->on_enter != NULL) {
            state->ops->on_enter(state, vm);
        }
    }
}

/* 关键点：委托给当前状态处理 */
void vending_machine_insert_coin(vending_machine_t *vm, int amount) {
    printf("\n>>> Insert coin: %d cents\n", amount);
    vm->current_state->ops->insert_coin(vm->current_state, vm, amount);
}

void vending_machine_select_item(vending_machine_t *vm, int item_id) {
    printf("\n>>> Select item: %d\n", item_id);
    vm->current_state->ops->select_item(vm->current_state, vm, item_id);
}

void vending_machine_dispense(vending_machine_t *vm) {
    printf("\n>>> Request dispense\n");
    vm->current_state->ops->dispense(vm->current_state, vm);
}

void vending_machine_cancel(vending_machine_t *vm) {
    printf("\n>>> Cancel transaction\n");
    vm->current_state->ops->cancel(vm->current_state, vm);
}

void vending_machine_add_item(vending_machine_t *vm, const char *name, int price, int qty) {
    if (vm->item_count < MAX_ITEMS) {
        vending_item_t *item = &vm->items[vm->item_count];
        item->id = vm->item_count;
        item->name = name;
        item->price = price;
        item->quantity = qty;
        vm->item_count++;
    }
}

void vending_machine_print_status(vending_machine_t *vm) {
    printf("\n========== VENDING MACHINE STATUS ==========\n");
    printf("State: %s\n", vm->current_state->ops->name);
    printf("Balance: %d cents\n", vm->balance);
    printf("Items:\n");
    for (int i = 0; i < vm->item_count; i++) {
        printf("  [%d] %s - %d cents (qty: %d)\n",
               vm->items[i].id,
               vm->items[i].name,
               vm->items[i].price,
               vm->items[i].quantity);
    }
    printf("=============================================\n");
}


/*---------------------------------------------------------------------------
 * idle_state.c - 空闲状态
 *---------------------------------------------------------------------------*/
#include "vending_state.h"
#include "vending_machine.h"
#include <stdlib.h>
#include <stdio.h>

static void idle_insert_coin(vending_state_t *state, vending_machine_t *vm, int amount) {
    (void)state;
    vm->balance += amount;
    printf("[Idle] Coin accepted. Balance: %d cents\n", vm->balance);
    
    /* 关键点：状态转换 */
    vending_machine_set_state(vm, vm->state_has_money);
}

static void idle_select_item(vending_state_t *state, vending_machine_t *vm, int item_id) {
    (void)state; (void)vm; (void)item_id;
    printf("[Idle] Please insert coins first!\n");
}

static void idle_dispense(vending_state_t *state, vending_machine_t *vm) {
    (void)state; (void)vm;
    printf("[Idle] No item selected!\n");
}

static void idle_cancel(vending_state_t *state, vending_machine_t *vm) {
    (void)state; (void)vm;
    printf("[Idle] Nothing to cancel.\n");
}

static const state_ops_t idle_ops = {
    .name = "IDLE",
    .on_enter = NULL,
    .on_exit = NULL,
    .insert_coin = idle_insert_coin,
    .select_item = idle_select_item,
    .dispense = idle_dispense,
    .cancel = idle_cancel
};

vending_state_t* idle_state_create(void) {
    vending_state_t *state = malloc(sizeof(vending_state_t));
    if (state != NULL) {
        state->ops = &idle_ops;
        state->data = NULL;
    }
    return state;
}


/*---------------------------------------------------------------------------
 * has_money_state.c - 有钱状态
 *---------------------------------------------------------------------------*/
static void has_money_insert_coin(vending_state_t *state, vending_machine_t *vm, int amount) {
    (void)state;
    vm->balance += amount;
    printf("[HasMoney] More coin accepted. Balance: %d cents\n", vm->balance);
}

static void has_money_select_item(vending_state_t *state, vending_machine_t *vm, int item_id) {
    (void)state;
    
    if (item_id < 0 || item_id >= vm->item_count) {
        printf("[HasMoney] Invalid item!\n");
        return;
    }
    
    vending_item_t *item = &vm->items[item_id];
    
    if (item->quantity <= 0) {
        printf("[HasMoney] Item '%s' is out of stock!\n", item->name);
        return;
    }
    
    if (vm->balance < item->price) {
        printf("[HasMoney] Insufficient balance! Need %d cents, have %d cents\n",
               item->price, vm->balance);
        return;
    }
    
    vm->selected_item = item_id;
    printf("[HasMoney] Selected: %s (%d cents)\n", item->name, item->price);
    
    /* 关键点：状态转换 */
    vending_machine_set_state(vm, vm->state_selected);
}

static void has_money_dispense(vending_state_t *state, vending_machine_t *vm) {
    (void)state; (void)vm;
    printf("[HasMoney] Please select an item first!\n");
}

static void has_money_cancel(vending_state_t *state, vending_machine_t *vm) {
    (void)state;
    printf("[HasMoney] Transaction cancelled. Returning %d cents.\n", vm->balance);
    vm->balance = 0;
    vending_machine_set_state(vm, vm->state_idle);
}

static const state_ops_t has_money_ops = {
    .name = "HAS_MONEY",
    .on_enter = NULL,
    .on_exit = NULL,
    .insert_coin = has_money_insert_coin,
    .select_item = has_money_select_item,
    .dispense = has_money_dispense,
    .cancel = has_money_cancel
};

vending_state_t* has_money_state_create(void) {
    vending_state_t *state = malloc(sizeof(vending_state_t));
    if (state != NULL) {
        state->ops = &has_money_ops;
        state->data = NULL;
    }
    return state;
}


/*---------------------------------------------------------------------------
 * item_selected_state.c - 商品已选状态
 *---------------------------------------------------------------------------*/
static void selected_on_enter(vending_state_t *state, vending_machine_t *vm) {
    (void)state;
    printf("[Selected] Ready to dispense. Press dispense button.\n");
    vending_item_t *item = &vm->items[vm->selected_item];
    printf("[Selected] Item: %s, Price: %d, Your balance: %d\n",
           item->name, item->price, vm->balance);
}

static void selected_insert_coin(vending_state_t *state, vending_machine_t *vm, int amount) {
    (void)state;
    vm->balance += amount;
    printf("[Selected] More coin accepted. Balance: %d cents\n", vm->balance);
}

static void selected_select_item(vending_state_t *state, vending_machine_t *vm, int item_id) {
    (void)state;
    
    if (item_id < 0 || item_id >= vm->item_count) {
        printf("[Selected] Invalid item!\n");
        return;
    }
    
    vending_item_t *item = &vm->items[item_id];
    
    if (vm->balance < item->price) {
        printf("[Selected] Insufficient balance for %s!\n", item->name);
        return;
    }
    
    vm->selected_item = item_id;
    printf("[Selected] Changed selection to: %s\n", item->name);
}

static void selected_dispense(vending_state_t *state, vending_machine_t *vm) {
    (void)state;
    vending_machine_set_state(vm, vm->state_dispensing);
}

static void selected_cancel(vending_state_t *state, vending_machine_t *vm) {
    (void)state;
    printf("[Selected] Selection cancelled.\n");
    vm->selected_item = -1;
    vending_machine_set_state(vm, vm->state_has_money);
}

static const state_ops_t selected_ops = {
    .name = "ITEM_SELECTED",
    .on_enter = selected_on_enter,
    .on_exit = NULL,
    .insert_coin = selected_insert_coin,
    .select_item = selected_select_item,
    .dispense = selected_dispense,
    .cancel = selected_cancel
};

vending_state_t* item_selected_state_create(void) {
    vending_state_t *state = malloc(sizeof(vending_state_t));
    if (state != NULL) {
        state->ops = &selected_ops;
        state->data = NULL;
    }
    return state;
}


/*---------------------------------------------------------------------------
 * dispensing_state.c - 出货状态
 *---------------------------------------------------------------------------*/
static void dispensing_on_enter(vending_state_t *state, vending_machine_t *vm) {
    (void)state;
    
    vending_item_t *item = &vm->items[vm->selected_item];
    
    printf("[Dispensing] 🎁 Dispensing: %s\n", item->name);
    printf("[Dispensing] *CLUNK* Item dropped!\n");
    
    /* 扣款和减库存 */
    vm->balance -= item->price;
    item->quantity--;
    
    /* 找零 */
    if (vm->balance > 0) {
        printf("[Dispensing] 💰 Change: %d cents\n", vm->balance);
        vm->balance = 0;
    }
    
    vm->selected_item = -1;
    
    /* 自动转回空闲状态 */
    vending_machine_set_state(vm, vm->state_idle);
}

static void dispensing_insert_coin(vending_state_t *state, vending_machine_t *vm, int amount) {
    (void)state; (void)vm; (void)amount;
    printf("[Dispensing] Please wait, dispensing in progress...\n");
}

static void dispensing_select_item(vending_state_t *state, vending_machine_t *vm, int item_id) {
    (void)state; (void)vm; (void)item_id;
    printf("[Dispensing] Please wait, dispensing in progress...\n");
}

static void dispensing_dispense(vending_state_t *state, vending_machine_t *vm) {
    (void)state; (void)vm;
    printf("[Dispensing] Already dispensing!\n");
}

static void dispensing_cancel(vending_state_t *state, vending_machine_t *vm) {
    (void)state; (void)vm;
    printf("[Dispensing] Cannot cancel during dispense!\n");
}

static const state_ops_t dispensing_ops = {
    .name = "DISPENSING",
    .on_enter = dispensing_on_enter,
    .on_exit = NULL,
    .insert_coin = dispensing_insert_coin,
    .select_item = dispensing_select_item,
    .dispense = dispensing_dispense,
    .cancel = dispensing_cancel
};

vending_state_t* dispensing_state_create(void) {
    vending_state_t *state = malloc(sizeof(vending_state_t));
    if (state != NULL) {
        state->ops = &dispensing_ops;
        state->data = NULL;
    }
    return state;
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "vending_machine.h"
#include <stdio.h>

int main(void) {
    printf("=== State Pattern Demo ===\n");
    printf("=== Vending Machine Simulation ===\n");
    
    /* 创建售货机 */
    vending_machine_t *vm = vending_machine_create();
    
    /* 添加商品 */
    vending_machine_add_item(vm, "Cola", 150, 5);
    vending_machine_add_item(vm, "Chips", 100, 3);
    vending_machine_add_item(vm, "Candy", 75, 10);
    
    vending_machine_print_status(vm);
    
    /* 场景1：正常购买流程 */
    printf("\n\n========== SCENARIO 1: Normal Purchase ==========\n");
    vending_machine_insert_coin(vm, 100);
    vending_machine_insert_coin(vm, 50);
    vending_machine_select_item(vm, 0);  /* Cola */
    vending_machine_dispense(vm);
    
    vending_machine_print_status(vm);
    
    /* 场景2：余额不足 */
    printf("\n\n========== SCENARIO 2: Insufficient Balance ==========\n");
    vending_machine_insert_coin(vm, 50);
    vending_machine_select_item(vm, 0);  /* Cola (150) - should fail */
    vending_machine_insert_coin(vm, 100);
    vending_machine_select_item(vm, 0);  /* Now should work */
    vending_machine_dispense(vm);
    
    /* 场景3：取消交易 */
    printf("\n\n========== SCENARIO 3: Cancel Transaction ==========\n");
    vending_machine_insert_coin(vm, 200);
    vending_machine_select_item(vm, 1);  /* Chips */
    vending_machine_cancel(vm);  /* Cancel selection */
    vending_machine_cancel(vm);  /* Cancel and get refund */
    
    /* 场景4：空闲状态下的无效操作 */
    printf("\n\n========== SCENARIO 4: Invalid Operations ==========\n");
    vending_machine_select_item(vm, 0);  /* No money inserted */
    vending_machine_dispense(vm);  /* Nothing selected */
    
    vending_machine_print_status(vm);
    
    /* 清理 */
    printf("\n========== CLEANUP ==========\n");
    vending_machine_destroy(vm);
    printf("Done!\n");
    
    return 0;
}
```

## 运行输出示例

```
=== State Pattern Demo ===
=== Vending Machine Simulation ===
[VendingMachine] Created, initial state: IDLE

========== VENDING MACHINE STATUS ==========
State: IDLE
Balance: 0 cents
Items:
  [0] Cola - 150 cents (qty: 5)
  [1] Chips - 100 cents (qty: 3)
  [2] Candy - 75 cents (qty: 10)
=============================================

========== SCENARIO 1: Normal Purchase ==========

>>> Insert coin: 100 cents
[Idle] Coin accepted. Balance: 100 cents
[State] IDLE --> HAS_MONEY

>>> Insert coin: 50 cents
[HasMoney] More coin accepted. Balance: 150 cents

>>> Select item: 0
[HasMoney] Selected: Cola (150 cents)
[State] HAS_MONEY --> ITEM_SELECTED
[Selected] Ready to dispense. Press dispense button.
[Selected] Item: Cola, Price: 150, Your balance: 150

>>> Request dispense
[State] ITEM_SELECTED --> DISPENSING
[Dispensing] 🎁 Dispensing: Cola
[Dispensing] *CLUNK* Item dropped!
[State] DISPENSING --> IDLE

========== SCENARIO 3: Cancel Transaction ==========

>>> Insert coin: 200 cents
[Idle] Coin accepted. Balance: 200 cents
[State] IDLE --> HAS_MONEY

>>> Select item: 1
[HasMoney] Selected: Chips (100 cents)
[State] HAS_MONEY --> ITEM_SELECTED

>>> Cancel transaction
[Selected] Selection cancelled.
[State] ITEM_SELECTED --> HAS_MONEY

>>> Cancel transaction
[HasMoney] Transaction cancelled. Returning 200 cents.
[State] HAS_MONEY --> IDLE

========== SCENARIO 4: Invalid Operations ==========

>>> Select item: 0
[Idle] Please insert coins first!

>>> Request dispense
[Idle] No item selected!

========== CLEANUP ==========
Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **消除条件语句** | 状态行为封装在各状态类中 |
| **状态独立** | 每个状态只关心自己的行为 |
| **转换清晰** | 状态转换逻辑在状态内部 |
| **易于扩展** | 新增状态不影响其他状态 |
| **单一职责** | 每个状态类职责单一 |

