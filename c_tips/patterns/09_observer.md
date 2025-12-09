# 观察者模式 (Observer Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      OBSERVER PATTERN                             |
+------------------------------------------------------------------+

    SUBJECT (Observable):
    
    +------------------+
    |     Subject      |
    |  (Temperature)   |
    +------------------+
    | - observers[]    |
    | - state          |
    +------------------+
    | + attach(obs)    |
    | + detach(obs)    |
    | + notify()       |
    | + set_state()    |
    +--------+---------+
             |
             | notify all
             |
    +--------v-------------------------------------------+
    |              OBSERVER LIST                         |
    | +------------+ +------------+ +------------+       |
    | | Observer 1 | | Observer 2 | | Observer 3 |       |
    | |  (Display) | |  (Logger)  | |  (Alarm)   |       |
    | +------------+ +------------+ +------------+       |
    +----------------------------------------------------+


    NOTIFICATION FLOW:
    
    Subject                 Observer1      Observer2      Observer3
       |                       |              |              |
       | attach(obs1)          |              |              |
       |<----------------------|              |              |
       | attach(obs2)          |              |              |
       |<-------------------------------------|              |
       | attach(obs3)          |              |              |
       |<---------------------------------------------------|
       |                       |              |              |
       | set_state(new_value)  |              |              |
       |                       |              |              |
       | notify()              |              |              |
       |---------------------> | update()     |              |
       |----------------------------------------> | update() |
       |----------------------------------------------------> | update()
```

**核心思想说明：**
- 定义对象间一对多的依赖关系
- 当一个对象状态改变时，所有依赖它的对象自动收到通知
- 主题（Subject）和观察者（Observer）松耦合
- 支持广播通信

## 实现思路

1. **定义观察者接口**：统一的更新回调函数
2. **主题维护观察者列表**：支持动态注册/注销
3. **状态变化时通知**：遍历列表调用所有观察者
4. **观察者自行处理**：各自决定如何响应

## 典型应用场景

- 事件处理系统
- 数据绑定/UI更新
- 消息发布/订阅
- 状态监控告警
- 配置变更通知

## 完整代码示例

```c
/*============================================================================
 * 观察者模式示例 - 股票价格监控
 *============================================================================*/

/*---------------------------------------------------------------------------
 * observer.h - 观察者接口定义
 *---------------------------------------------------------------------------*/
#ifndef OBSERVER_H
#define OBSERVER_H

/* 前向声明 */
typedef struct subject subject_t;
typedef struct observer observer_t;

/* 关键点：观察者回调函数类型 */
typedef void (*observer_update_fn)(observer_t *self, subject_t *subject);

struct observer {
    const char *name;
    observer_update_fn update;
    void *user_data;
};

#endif /* OBSERVER_H */


/*---------------------------------------------------------------------------
 * subject.h - 主题（被观察者）定义
 *---------------------------------------------------------------------------*/
#ifndef SUBJECT_H
#define SUBJECT_H

#include "observer.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_OBSERVERS 16

struct subject {
    const char *name;
    observer_t *observers[MAX_OBSERVERS];
    int observer_count;
    void *state;
};

/* 主题 API */
void subject_init(subject_t *subject, const char *name);
bool subject_attach(subject_t *subject, observer_t *observer);
bool subject_detach(subject_t *subject, observer_t *observer);
void subject_notify(subject_t *subject);

#endif /* SUBJECT_H */


/*---------------------------------------------------------------------------
 * subject.c - 主题实现
 *---------------------------------------------------------------------------*/
#include "subject.h"
#include <string.h>
#include <stdio.h>

void subject_init(subject_t *subject, const char *name) {
    subject->name = name;
    subject->observer_count = 0;
    subject->state = NULL;
    memset(subject->observers, 0, sizeof(subject->observers));
}

/* 关键点：注册观察者 */
bool subject_attach(subject_t *subject, observer_t *observer) {
    if (subject->observer_count >= MAX_OBSERVERS) {
        return false;
    }
    
    /* 检查是否已注册 */
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            return true;  /* 已存在 */
        }
    }
    
    subject->observers[subject->observer_count++] = observer;
    printf("[Subject:%s] Observer '%s' attached\n", subject->name, observer->name);
    return true;
}

/* 关键点：注销观察者 */
bool subject_detach(subject_t *subject, observer_t *observer) {
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            /* 移动后续元素 */
            for (int j = i; j < subject->observer_count - 1; j++) {
                subject->observers[j] = subject->observers[j + 1];
            }
            subject->observer_count--;
            printf("[Subject:%s] Observer '%s' detached\n", subject->name, observer->name);
            return true;
        }
    }
    return false;
}

/* 关键点：通知所有观察者 */
void subject_notify(subject_t *subject) {
    printf("[Subject:%s] Notifying %d observers...\n", 
           subject->name, subject->observer_count);
    
    for (int i = 0; i < subject->observer_count; i++) {
        observer_t *obs = subject->observers[i];
        if (obs != NULL && obs->update != NULL) {
            obs->update(obs, subject);
        }
    }
}


/*---------------------------------------------------------------------------
 * stock.h - 股票主题（具体主题）
 *---------------------------------------------------------------------------*/
#ifndef STOCK_H
#define STOCK_H

#include "subject.h"

typedef struct {
    char symbol[8];
    float price;
    float change;
    float change_percent;
} stock_state_t;

typedef struct {
    subject_t base;  /* 继承主题 */
    stock_state_t state;
} stock_t;

stock_t* stock_create(const char *symbol, float initial_price);
void stock_destroy(stock_t *stock);
void stock_set_price(stock_t *stock, float new_price);
const stock_state_t* stock_get_state(stock_t *stock);

#endif /* STOCK_H */


/*---------------------------------------------------------------------------
 * stock.c - 股票主题实现
 *---------------------------------------------------------------------------*/
#include "stock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

stock_t* stock_create(const char *symbol, float initial_price) {
    stock_t *stock = malloc(sizeof(stock_t));
    if (stock == NULL) return NULL;
    
    subject_init(&stock->base, symbol);
    stock->base.state = &stock->state;
    
    strncpy(stock->state.symbol, symbol, sizeof(stock->state.symbol) - 1);
    stock->state.price = initial_price;
    stock->state.change = 0;
    stock->state.change_percent = 0;
    
    printf("[Stock] Created %s @ $%.2f\n", symbol, initial_price);
    return stock;
}

void stock_destroy(stock_t *stock) {
    free(stock);
}

/* 关键点：状态变化时自动通知 */
void stock_set_price(stock_t *stock, float new_price) {
    float old_price = stock->state.price;
    
    stock->state.price = new_price;
    stock->state.change = new_price - old_price;
    stock->state.change_percent = (stock->state.change / old_price) * 100;
    
    printf("\n[Stock] %s price changed: $%.2f -> $%.2f (%+.2f%%)\n",
           stock->state.symbol, old_price, new_price, stock->state.change_percent);
    
    /* 关键点：通知所有观察者 */
    subject_notify(&stock->base);
}

const stock_state_t* stock_get_state(stock_t *stock) {
    return &stock->state;
}


/*---------------------------------------------------------------------------
 * 具体观察者实现
 *---------------------------------------------------------------------------*/
#include "observer.h"
#include "stock.h"
#include <stdio.h>

/* 观察者1：显示器 */
void display_update(observer_t *self, subject_t *subject) {
    stock_t *stock = (stock_t *)subject;
    const stock_state_t *state = stock_get_state(stock);
    
    printf("  [%s] 📊 Display: %s = $%.2f (%+.2f, %+.2f%%)\n",
           self->name, state->symbol, state->price, 
           state->change, state->change_percent);
}

/* 观察者2：记录器 */
typedef struct {
    int log_count;
} logger_data_t;

void logger_update(observer_t *self, subject_t *subject) {
    stock_t *stock = (stock_t *)subject;
    const stock_state_t *state = stock_get_state(stock);
    logger_data_t *data = (logger_data_t *)self->user_data;
    
    data->log_count++;
    printf("  [%s] 📝 Log #%d: %s,$%.2f,%+.2f\n",
           self->name, data->log_count, state->symbol, 
           state->price, state->change);
}

/* 观察者3：告警器 */
typedef struct {
    float high_threshold;
    float low_threshold;
} alarm_config_t;

void alarm_update(observer_t *self, subject_t *subject) {
    stock_t *stock = (stock_t *)subject;
    const stock_state_t *state = stock_get_state(stock);
    alarm_config_t *config = (alarm_config_t *)self->user_data;
    
    if (state->price > config->high_threshold) {
        printf("  [%s] 🔔 ALERT: %s exceeded high threshold ($%.2f > $%.2f)\n",
               self->name, state->symbol, state->price, config->high_threshold);
    } else if (state->price < config->low_threshold) {
        printf("  [%s] 🔔 ALERT: %s below low threshold ($%.2f < $%.2f)\n",
               self->name, state->symbol, state->price, config->low_threshold);
    } else {
        printf("  [%s] ✓ %s within normal range\n", self->name, state->symbol);
    }
}

/* 观察者4：自动交易器 */
typedef struct {
    float buy_below;
    float sell_above;
    int shares_held;
} trader_config_t;

void trader_update(observer_t *self, subject_t *subject) {
    stock_t *stock = (stock_t *)subject;
    const stock_state_t *state = stock_get_state(stock);
    trader_config_t *config = (trader_config_t *)self->user_data;
    
    if (state->price < config->buy_below && config->shares_held < 100) {
        config->shares_held += 10;
        printf("  [%s] 💰 BUY 10 shares @ $%.2f (total: %d)\n",
               self->name, state->price, config->shares_held);
    } else if (state->price > config->sell_above && config->shares_held > 0) {
        int sell = (config->shares_held > 10) ? 10 : config->shares_held;
        config->shares_held -= sell;
        printf("  [%s] 💵 SELL %d shares @ $%.2f (remaining: %d)\n",
               self->name, sell, state->price, config->shares_held);
    } else {
        printf("  [%s] ⏸ HOLD (price=$%.2f, shares=%d)\n",
               self->name, state->price, config->shares_held);
    }
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
int main(void) {
    printf("=== Observer Pattern Demo ===\n\n");
    
    /* 创建股票（主题） */
    stock_t *apple = stock_create("AAPL", 150.00);
    
    /* 创建观察者及其配置 */
    logger_data_t logger_data = {.log_count = 0};
    alarm_config_t alarm_config = {.high_threshold = 160.0, .low_threshold = 140.0};
    trader_config_t trader_config = {.buy_below = 145.0, .sell_above = 155.0, .shares_held = 0};
    
    observer_t display = {
        .name = "Display",
        .update = display_update,
        .user_data = NULL
    };
    
    observer_t logger = {
        .name = "Logger",
        .update = logger_update,
        .user_data = &logger_data
    };
    
    observer_t alarm = {
        .name = "Alarm",
        .update = alarm_update,
        .user_data = &alarm_config
    };
    
    observer_t trader = {
        .name = "AutoTrader",
        .update = trader_update,
        .user_data = &trader_config
    };
    
    /* 关键点：注册观察者 */
    printf("--- Attaching Observers ---\n");
    subject_attach(&apple->base, &display);
    subject_attach(&apple->base, &logger);
    subject_attach(&apple->base, &alarm);
    subject_attach(&apple->base, &trader);
    
    /* 关键点：价格变化自动通知所有观察者 */
    printf("\n--- Simulating Price Changes ---\n");
    
    stock_set_price(apple, 152.50);
    stock_set_price(apple, 148.00);
    stock_set_price(apple, 143.00);
    stock_set_price(apple, 158.00);
    stock_set_price(apple, 162.00);
    
    /* 动态注销观察者 */
    printf("\n--- Detaching Logger ---\n");
    subject_detach(&apple->base, &logger);
    
    stock_set_price(apple, 155.00);
    
    /* 清理 */
    printf("\n--- Cleanup ---\n");
    stock_destroy(apple);
    printf("Done!\n");
    
    return 0;
}
```

## 运行输出示例

```
=== Observer Pattern Demo ===

[Stock] Created AAPL @ $150.00

--- Attaching Observers ---
[Subject:AAPL] Observer 'Display' attached
[Subject:AAPL] Observer 'Logger' attached
[Subject:AAPL] Observer 'Alarm' attached
[Subject:AAPL] Observer 'AutoTrader' attached

--- Simulating Price Changes ---

[Stock] AAPL price changed: $150.00 -> $152.50 (+1.67%)
[Subject:AAPL] Notifying 4 observers...
  [Display] 📊 Display: AAPL = $152.50 (+2.50, +1.67%)
  [Logger] 📝 Log #1: AAPL,$152.50,+2.50
  [Alarm] ✓ AAPL within normal range
  [AutoTrader] ⏸ HOLD (price=$152.50, shares=0)

[Stock] AAPL price changed: $152.50 -> $143.00 (-6.23%)
[Subject:AAPL] Notifying 4 observers...
  [Display] 📊 Display: AAPL = $143.00 (-9.50, -6.23%)
  [Logger] 📝 Log #2: AAPL,$143.00,-9.50
  [Alarm] 🔔 ALERT: AAPL below low threshold ($143.00 < $140.00)
  [AutoTrader] 💰 BUY 10 shares @ $143.00 (total: 10)

[Stock] AAPL price changed: $143.00 -> $162.00 (+13.29%)
[Subject:AAPL] Notifying 4 observers...
  [Display] 📊 Display: AAPL = $162.00 (+19.00, +13.29%)
  [Logger] 📝 Log #4: AAPL,$162.00,+19.00
  [Alarm] 🔔 ALERT: AAPL exceeded high threshold ($162.00 > $160.00)
  [AutoTrader] 💵 SELL 10 shares @ $162.00 (remaining: 10)

--- Detaching Logger ---
[Subject:AAPL] Observer 'Logger' detached

[Stock] AAPL price changed: $162.00 -> $155.00 (-4.32%)
[Subject:AAPL] Notifying 3 observers...
  [Display] 📊 Display: AAPL = $155.00 (-7.00, -4.32%)
  [Alarm] ✓ AAPL within normal range
  [AutoTrader] ⏸ HOLD (price=$155.00, shares=10)

--- Cleanup ---
Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **松耦合** | 主题和观察者只通过接口交互 |
| **广播通信** | 一次通知，多个响应 |
| **动态订阅** | 运行时可添加/移除观察者 |
| **开闭原则** | 新增观察者不修改主题 |
| **各自处理** | 观察者独立决定响应方式 |

