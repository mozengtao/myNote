# 观察者模式 (Observer Pattern)

## 定义

观察者模式是一种行为型设计模式，它定义了对象之间的一对多依赖关系。当一个对象（被观察者/主题）的状态发生变化时，所有依赖于它的对象（观察者）都会自动收到通知并更新。

## 适用场景

- 事件处理系统
- GUI框架中的按钮点击、窗口事件等
- 消息队列和发布/订阅系统
- 数据变化通知（如MVC架构中Model通知View）
- 传感器数据监控
- 股票价格变动通知
- 配置文件变更通知

## ASCII 图解

```
+------------------------------------------------------------------------+
|                        OBSERVER PATTERN                                 |
+------------------------------------------------------------------------+
|                                                                         |
|                      +-------------------+                              |
|                      |      Subject      |                              |
|                      | (Observable)      |                              |
|                      +-------------------+                              |
|                      | - observers[]     |                              |
|                      | - state           |                              |
|                      +-------------------+                              |
|                      | + attach()        |                              |
|                      | + detach()        |                              |
|                      | + notify()        |                              |
|                      | + setState()      |                              |
|                      +---------+---------+                              |
|                                |                                        |
|                                | notify()                               |
|                                |                                        |
|          +---------------------+---------------------+                  |
|          |                     |                     |                  |
|          v                     v                     v                  |
|   +-------------+       +-------------+       +-------------+           |
|   | Observer A  |       | Observer B  |       | Observer C  |           |
|   +-------------+       +-------------+       +-------------+           |
|   | + update()  |       | + update()  |       | + update()  |           |
|   +-------------+       +-------------+       +-------------+           |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Event Flow:                                                           |
|                                                                         |
|   +--------+    setState()    +---------+    notify()    +----------+  |
|   | Client |----------------->| Subject |--------------->| Observer |  |
|   +--------+                  +---------+                | List     |  |
|                                    |                     +----------+  |
|                                    |                          |        |
|                               +----+----+                     |        |
|                               | State   |                     |        |
|                               | Changed |                     |        |
|                               +---------+                     |        |
|                                                               |        |
|                                    For each observer:         |        |
|                                    +------------------------+ |        |
|                                    | observer->update(data) |<+        |
|                                    +------------------------+          |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Observer Registration:                                                |
|                                                                         |
|   +----------+  attach(obs)  +-----------------------------------+     |
|   | Observer |-------------->| Subject's Observer List           |     |
|   +----------+               | +------+------+------+------+     |     |
|                              | | Obs1 | Obs2 | Obs3 | .... |     |     |
|   +----------+  detach(obs)  | +------+------+------+------+     |     |
|   | Observer |-------------->|      ^                            |     |
|   +----------+               |      | (removed)                  |     |
|                              +-----------------------------------+     |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了观察者模式的核心结构。Subject（被观察者）维护一个观察者列表，当其状态改变时，会遍历列表通知所有已注册的观察者。每个观察者实现 `update()` 方法来响应状态变化。中间的事件流程图显示了完整的通知过程：客户端调用 `setState()` → Subject 检测到状态变化 → 调用 `notify()` → 依次调用每个观察者的 `update()` 方法。底部展示了观察者的注册（attach）和注销（detach）机制。

## 实现方法

在C语言中实现观察者模式：

1. 定义观察者接口（包含update函数指针）
2. 定义被观察者（Subject）结构，包含观察者列表
3. 实现attach/detach函数管理观察者
4. 实现notify函数遍历通知所有观察者
5. 各观察者实现自己的update函数

## C语言代码示例

### 观察者接口定义

```c
// observer.h
#ifndef OBSERVER_H
#define OBSERVER_H

// 前向声明
typedef struct Subject Subject;
typedef struct Observer Observer;

// 事件数据结构
typedef struct {
    int event_type;
    void* data;
    const char* message;
} EventData;

// 观察者接口
typedef struct Observer {
    void (*update)(Observer* self, Subject* subject, EventData* event);
    void (*destroy)(Observer* self);
    const char* name;
} Observer;

// 观察者辅助函数
void observer_update(Observer* obs, Subject* subject, EventData* event);
void observer_destroy(Observer* obs);

#endif // OBSERVER_H
```

### 被观察者（Subject）实现

```c
// subject.h
#ifndef SUBJECT_H
#define SUBJECT_H

#include "observer.h"

#define MAX_OBSERVERS 32

// 被观察者结构
typedef struct Subject {
    Observer* observers[MAX_OBSERVERS];
    int observer_count;
    void* state;
    const char* name;
} Subject;

// Subject操作函数
void subject_init(Subject* subject, const char* name);
int subject_attach(Subject* subject, Observer* observer);
int subject_detach(Subject* subject, Observer* observer);
void subject_notify(Subject* subject, EventData* event);
void subject_set_state(Subject* subject, void* state);
void* subject_get_state(Subject* subject);
void subject_cleanup(Subject* subject);

#endif // SUBJECT_H
```

```c
// subject.c
#include "subject.h"
#include <stdio.h>
#include <string.h>

void subject_init(Subject* subject, const char* name) {
    if (subject) {
        memset(subject->observers, 0, sizeof(subject->observers));
        subject->observer_count = 0;
        subject->state = NULL;
        subject->name = name;
        printf("[Subject:%s] Initialized\n", name);
    }
}

int subject_attach(Subject* subject, Observer* observer) {
    if (!subject || !observer) return -1;
    
    if (subject->observer_count >= MAX_OBSERVERS) {
        printf("[Subject:%s] Cannot attach: max observers reached\n", subject->name);
        return -1;
    }
    
    // 检查是否已存在
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            printf("[Subject:%s] Observer '%s' already attached\n", 
                   subject->name, observer->name);
            return 0;
        }
    }
    
    subject->observers[subject->observer_count++] = observer;
    printf("[Subject:%s] Attached observer '%s' (total: %d)\n", 
           subject->name, observer->name, subject->observer_count);
    return 0;
}

int subject_detach(Subject* subject, Observer* observer) {
    if (!subject || !observer) return -1;
    
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            // 移动后面的元素
            for (int j = i; j < subject->observer_count - 1; j++) {
                subject->observers[j] = subject->observers[j + 1];
            }
            subject->observer_count--;
            subject->observers[subject->observer_count] = NULL;
            printf("[Subject:%s] Detached observer '%s' (remaining: %d)\n",
                   subject->name, observer->name, subject->observer_count);
            return 0;
        }
    }
    
    printf("[Subject:%s] Observer '%s' not found\n", subject->name, observer->name);
    return -1;
}

void subject_notify(Subject* subject, EventData* event) {
    if (!subject) return;
    
    printf("[Subject:%s] Notifying %d observers...\n", 
           subject->name, subject->observer_count);
    
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] && subject->observers[i]->update) {
            subject->observers[i]->update(subject->observers[i], subject, event);
        }
    }
}

void subject_set_state(Subject* subject, void* state) {
    if (subject) {
        subject->state = state;
    }
}

void* subject_get_state(Subject* subject) {
    return subject ? subject->state : NULL;
}

void subject_cleanup(Subject* subject) {
    if (subject) {
        subject->observer_count = 0;
        printf("[Subject:%s] Cleaned up\n", subject->name);
    }
}
```

### 具体观察者实现

```c
// concrete_observers.h
#ifndef CONCRETE_OBSERVERS_H
#define CONCRETE_OBSERVERS_H

#include "observer.h"

// 创建不同类型的观察者
Observer* create_logger_observer(const char* name);
Observer* create_email_observer(const char* name, const char* email);
Observer* create_sms_observer(const char* name, const char* phone);
Observer* create_dashboard_observer(const char* name);

#endif
```

```c
// concrete_observers.c
#include "concrete_observers.h"
#include "subject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== Logger Observer ====================

typedef struct {
    Observer base;
    char log_prefix[32];
} LoggerObserver;

static void logger_update(Observer* self, Subject* subject, EventData* event) {
    LoggerObserver* logger = (LoggerObserver*)self;
    printf("  [LOG][%s] Event from '%s': type=%d, msg='%s'\n",
           logger->log_prefix, subject->name, 
           event->event_type, event->message ? event->message : "(null)");
}

static void logger_destroy(Observer* self) {
    printf("  [LOG] Observer '%s' destroyed\n", self->name);
    free(self);
}

Observer* create_logger_observer(const char* name) {
    LoggerObserver* obs = (LoggerObserver*)malloc(sizeof(LoggerObserver));
    if (obs) {
        obs->base.update = logger_update;
        obs->base.destroy = logger_destroy;
        obs->base.name = name;
        strncpy(obs->log_prefix, name, sizeof(obs->log_prefix) - 1);
    }
    return (Observer*)obs;
}

// ==================== Email Observer ====================

typedef struct {
    Observer base;
    char email[64];
} EmailObserver;

static void email_update(Observer* self, Subject* subject, EventData* event) {
    EmailObserver* email_obs = (EmailObserver*)self;
    printf("  [EMAIL] Sending to '%s': Subject changed - %s\n",
           email_obs->email, event->message ? event->message : "(no message)");
}

static void email_destroy(Observer* self) {
    printf("  [EMAIL] Observer '%s' destroyed\n", self->name);
    free(self);
}

Observer* create_email_observer(const char* name, const char* email) {
    EmailObserver* obs = (EmailObserver*)malloc(sizeof(EmailObserver));
    if (obs) {
        obs->base.update = email_update;
        obs->base.destroy = email_destroy;
        obs->base.name = name;
        strncpy(obs->email, email, sizeof(obs->email) - 1);
    }
    return (Observer*)obs;
}

// ==================== SMS Observer ====================

typedef struct {
    Observer base;
    char phone[20];
} SMSObserver;

static void sms_update(Observer* self, Subject* subject, EventData* event) {
    SMSObserver* sms_obs = (SMSObserver*)self;
    printf("  [SMS] Sending to '%s': Alert! %s\n",
           sms_obs->phone, event->message ? event->message : "(no message)");
}

static void sms_destroy(Observer* self) {
    printf("  [SMS] Observer '%s' destroyed\n", self->name);
    free(self);
}

Observer* create_sms_observer(const char* name, const char* phone) {
    SMSObserver* obs = (SMSObserver*)malloc(sizeof(SMSObserver));
    if (obs) {
        obs->base.update = sms_update;
        obs->base.destroy = sms_destroy;
        obs->base.name = name;
        strncpy(obs->phone, phone, sizeof(obs->phone) - 1);
    }
    return (Observer*)obs;
}

// ==================== Dashboard Observer ====================

typedef struct {
    Observer base;
    int update_count;
} DashboardObserver;

static void dashboard_update(Observer* self, Subject* subject, EventData* event) {
    DashboardObserver* dash = (DashboardObserver*)self;
    dash->update_count++;
    
    printf("  [DASHBOARD] Update #%d\n", dash->update_count);
    printf("    +----------------------------------+\n");
    printf("    | Source: %-23s |\n", subject->name);
    printf("    | Event Type: %-19d |\n", event->event_type);
    printf("    | Message: %-21s |\n", 
           event->message ? event->message : "(none)");
    printf("    +----------------------------------+\n");
}

static void dashboard_destroy(Observer* self) {
    DashboardObserver* dash = (DashboardObserver*)self;
    printf("  [DASHBOARD] Observer '%s' destroyed (handled %d updates)\n", 
           self->name, dash->update_count);
    free(self);
}

Observer* create_dashboard_observer(const char* name) {
    DashboardObserver* obs = (DashboardObserver*)malloc(sizeof(DashboardObserver));
    if (obs) {
        obs->base.update = dashboard_update;
        obs->base.destroy = dashboard_destroy;
        obs->base.name = name;
        obs->update_count = 0;
    }
    return (Observer*)obs;
}
```

### 实际应用示例：股票价格监控

```c
// stock_monitor.h
#ifndef STOCK_MONITOR_H
#define STOCK_MONITOR_H

#include "subject.h"

// 股票数据
typedef struct {
    char symbol[10];
    double price;
    double change;
    double change_percent;
} StockData;

// 股票监控器
typedef struct {
    Subject base;
    StockData current_data;
} StockMonitor;

StockMonitor* stock_monitor_create(const char* symbol);
void stock_monitor_update_price(StockMonitor* monitor, double new_price);
void stock_monitor_destroy(StockMonitor* monitor);

#endif
```

```c
// stock_monitor.c
#include "stock_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StockMonitor* stock_monitor_create(const char* symbol) {
    StockMonitor* monitor = (StockMonitor*)malloc(sizeof(StockMonitor));
    if (monitor) {
        subject_init(&monitor->base, symbol);
        strncpy(monitor->current_data.symbol, symbol, 
                sizeof(monitor->current_data.symbol) - 1);
        monitor->current_data.price = 0.0;
        monitor->current_data.change = 0.0;
        monitor->current_data.change_percent = 0.0;
    }
    return monitor;
}

void stock_monitor_update_price(StockMonitor* monitor, double new_price) {
    if (!monitor) return;
    
    double old_price = monitor->current_data.price;
    monitor->current_data.price = new_price;
    monitor->current_data.change = new_price - old_price;
    
    if (old_price > 0) {
        monitor->current_data.change_percent = 
            (monitor->current_data.change / old_price) * 100.0;
    }
    
    // 设置状态并通知
    subject_set_state(&monitor->base, &monitor->current_data);
    
    // 创建事件数据
    char msg[128];
    snprintf(msg, sizeof(msg), "%s: $%.2f (%+.2f%%)", 
             monitor->current_data.symbol,
             monitor->current_data.price,
             monitor->current_data.change_percent);
    
    EventData event = {
        .event_type = (monitor->current_data.change >= 0) ? 1 : -1,
        .data = &monitor->current_data,
        .message = msg
    };
    
    subject_notify(&monitor->base, &event);
}

void stock_monitor_destroy(StockMonitor* monitor) {
    if (monitor) {
        subject_cleanup(&monitor->base);
        free(monitor);
    }
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "stock_monitor.h"
#include "concrete_observers.h"

int main() {
    printf("=== Observer Pattern Demo - Stock Monitor ===\n\n");
    
    // 创建股票监控器
    StockMonitor* apple = stock_monitor_create("AAPL");
    StockMonitor* google = stock_monitor_create("GOOGL");
    
    // 创建观察者
    Observer* logger = create_logger_observer("MainLogger");
    Observer* email = create_email_observer("EmailAlert", "trader@example.com");
    Observer* sms = create_sms_observer("SMSAlert", "+1-555-1234");
    Observer* dashboard = create_dashboard_observer("TradingDashboard");
    
    // 注册观察者到AAPL
    printf("\n--- Registering observers for AAPL ---\n");
    subject_attach(&apple->base, logger);
    subject_attach(&apple->base, email);
    subject_attach(&apple->base, dashboard);
    
    // 注册观察者到GOOGL
    printf("\n--- Registering observers for GOOGL ---\n");
    subject_attach(&google->base, logger);
    subject_attach(&google->base, sms);
    subject_attach(&google->base, dashboard);
    
    // 模拟股票价格变化
    printf("\n--- Initial prices ---\n");
    stock_monitor_update_price(apple, 150.00);
    stock_monitor_update_price(google, 2800.00);
    
    printf("\n--- Price changes ---\n");
    stock_monitor_update_price(apple, 152.50);
    stock_monitor_update_price(google, 2750.00);
    
    printf("\n--- More changes ---\n");
    stock_monitor_update_price(apple, 148.00);
    
    // 取消一个观察者
    printf("\n--- Detaching email observer from AAPL ---\n");
    subject_detach(&apple->base, email);
    
    printf("\n--- After detach ---\n");
    stock_monitor_update_price(apple, 155.00);
    
    // 清理
    printf("\n--- Cleanup ---\n");
    observer_destroy(logger);
    observer_destroy(email);
    observer_destroy(sms);
    observer_destroy(dashboard);
    
    stock_monitor_destroy(apple);
    stock_monitor_destroy(google);
    
    return 0;
}

/* 输出示例:
=== Observer Pattern Demo - Stock Monitor ===

[Subject:AAPL] Initialized
[Subject:GOOGL] Initialized

--- Registering observers for AAPL ---
[Subject:AAPL] Attached observer 'MainLogger' (total: 1)
[Subject:AAPL] Attached observer 'EmailAlert' (total: 2)
[Subject:AAPL] Attached observer 'TradingDashboard' (total: 3)

--- Registering observers for GOOGL ---
[Subject:GOOGL] Attached observer 'MainLogger' (total: 1)
[Subject:GOOGL] Attached observer 'SMSAlert' (total: 2)
[Subject:GOOGL] Attached observer 'TradingDashboard' (total: 3)

--- Initial prices ---
[Subject:AAPL] Notifying 3 observers...
  [LOG][MainLogger] Event from 'AAPL': type=1, msg='AAPL: $150.00 (+0.00%)'
  [EMAIL] Sending to 'trader@example.com': Subject changed - AAPL: $150.00 (+0.00%)
  [DASHBOARD] Update #1
    +----------------------------------+
    | Source: AAPL                    |
    | Event Type: 1                   |
    | Message: AAPL: $150.00 (+0.00%) |
    +----------------------------------+
...
*/
```

## 优缺点

### 优点
- 实现了对象之间的松耦合
- 支持广播通信
- 符合开闭原则（可以随时添加新观察者）
- 可以在运行时建立对象之间的关系

### 缺点
- 如果观察者很多，通知所有观察者可能耗时
- 观察者之间有循环依赖可能导致系统崩溃
- 观察者不知道彼此的存在，可能导致意外的更新
- 内存泄漏风险（忘记取消注册）

