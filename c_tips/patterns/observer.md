# Observer Pattern (观察者模式)

## 1. Core Concept and Use Cases

### Core Concept
Define a **one-to-many** dependency between objects. When one object (Subject) changes state, all its dependents (Observers) are **notified and updated automatically**.

### Typical Use Cases
- Event notification systems
- GUI event handling
- State change broadcasting
- Message publish/subscribe systems
- Real-time data monitoring

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                     Observer Pattern                                              |
+--------------------------------------------------------------------------------------------------+

                                +---------------------------+
                                |         Subject           |
                                |      (Observable)         |
                                +---------------------------+
                                | - observers[]             |
                                | - state                   |
                                +---------------------------+
                                | + attach(observer)        |
                                | + detach(observer)        |
                                | + notify()                |
                                | + set_state(new_state)    |
                                +-------------+-------------+
                                              |
                                              | notify() calls
                                              | observer->update()
                                              |
                      +-----------------------+-----------------------+
                      |                       |                       |
                      v                       v                       v
               +-------------+         +-------------+         +-------------+
               | Observer A  |         | Observer B  |         | Observer C  |
               +-------------+         +-------------+         +-------------+
               | + update()  |         | + update()  |         | + update()  |
               +-------------+         +-------------+         +-------------+
                      |                       |                       |
                      v                       v                       v
               Handle event A          Handle event B          Handle event C
               (e.g., log)             (e.g., alert)           (e.g., update UI)


    +---------------------------+
    |     Event Flow            |
    +---------------------------+
    
    1. Subject state changes
           |
           v
    2. Subject calls notify()
           |
           v
    3. notify() iterates observers[]
           |
           +---> observer[0]->update(subject)
           |
           +---> observer[1]->update(subject)
           |
           +---> observer[2]->update(subject)
           |
           v
    4. Each observer handles event independently
```

**中文说明：**

观察者模式的核心流程：

1. **主题（Subject）**：
   - 维护一个观察者列表 `observers[]`
   - 持有状态数据 `state`
   - 提供注册/注销观察者的方法

2. **观察者（Observer）**：
   - 实现 `update()` 方法
   - 注册到主题后，状态变化时会被通知

3. **通知流程**：
   - 主题状态改变
   - 主题调用 `notify()`
   - 遍历所有观察者，调用其 `update()` 方法
   - 每个观察者独立处理事件

---

## 3. Code Skeleton

```c
/* Observer callback type */
typedef void (*observer_callback)(void* subject, void* data);

/* Observer structure */
typedef struct Observer {
    observer_callback callback;
    void* user_data;
} Observer;

/* Subject structure */
typedef struct Subject {
    Observer* observers[MAX_OBSERVERS];
    int observer_count;
    void* state;
} Subject;

/* Subject operations */
void subject_attach(Subject* s, Observer* obs);
void subject_detach(Subject* s, Observer* obs);
void subject_notify(Subject* s);
void subject_set_state(Subject* s, void* new_state);
```

**中文说明：**

代码骨架包含：
- `observer_callback`：观察者回调函数类型
- `Observer`：观察者结构体，包含回调和用户数据
- `Subject`：主题结构体，包含观察者列表和状态
- 核心操作：`attach`、`detach`、`notify`、`set_state`

---

## 4. Complete Example Code

```c
/*
 * Observer Pattern - Stock Price Monitor Example
 * 
 * This example demonstrates a stock price monitoring system
 * where multiple observers react to price changes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OBSERVERS 10
#define MAX_STOCK_NAME 32

/* ============================================
 * Forward Declarations
 * ============================================ */
typedef struct Subject Subject;
typedef struct Observer Observer;

/* ============================================
 * Observer Interface
 * ============================================ */
typedef void (*observer_update_fn)(Observer* self, Subject* subject);

struct Observer {
    char name[32];                      /* Observer name for identification */
    observer_update_fn update;          /* Callback function - called when subject changes */
    void* user_data;                    /* Observer-specific data */
};

/* ============================================
 * Subject (Observable) Structure
 * ============================================ */
typedef struct {
    char  stock_name[MAX_STOCK_NAME];   /* Stock symbol */
    float price;                        /* Current price */
    float change_percent;               /* Price change percentage */
} StockData;

struct Subject {
    Observer* observers[MAX_OBSERVERS]; /* List of registered observers */
    int observer_count;                 /* Number of observers */
    StockData stock;                    /* Stock data (the observable state) */
};

/* ============================================
 * Subject Operations
 * ============================================ */

/* Initialize subject */
Subject* subject_create(const char* stock_name, float initial_price)
{
    Subject* s = (Subject*)malloc(sizeof(Subject));
    if (s == NULL) return NULL;
    
    memset(s, 0, sizeof(Subject));
    strncpy(s->stock.stock_name, stock_name, MAX_STOCK_NAME - 1);
    s->stock.price = initial_price;
    s->stock.change_percent = 0.0f;
    s->observer_count = 0;
    
    printf("[Subject] Created stock monitor for %s at $%.2f\n", 
           stock_name, initial_price);
    return s;
}

/* Attach an observer to the subject */
int subject_attach(Subject* s, Observer* obs)
{
    if (s == NULL || obs == NULL) return -1;
    
    if (s->observer_count >= MAX_OBSERVERS) {
        printf("[Subject] Error: Maximum observers reached\n");
        return -1;
    }
    
    /* Add observer to the list */
    s->observers[s->observer_count++] = obs;
    printf("[Subject] Attached observer: %s (total: %d)\n", 
           obs->name, s->observer_count);
    return 0;
}

/* Detach an observer from the subject */
int subject_detach(Subject* s, Observer* obs)
{
    if (s == NULL || obs == NULL) return -1;
    
    for (int i = 0; i < s->observer_count; i++) {
        if (s->observers[i] == obs) {
            /* Shift remaining observers */
            for (int j = i; j < s->observer_count - 1; j++) {
                s->observers[j] = s->observers[j + 1];
            }
            s->observer_count--;
            printf("[Subject] Detached observer: %s (remaining: %d)\n", 
                   obs->name, s->observer_count);
            return 0;
        }
    }
    return -1;
}

/* Notify all observers - core of Observer pattern */
void subject_notify(Subject* s)
{
    if (s == NULL) return;
    
    printf("[Subject] Notifying %d observers...\n", s->observer_count);
    
    /* Iterate through all observers and call their update method */
    for (int i = 0; i < s->observer_count; i++) {
        if (s->observers[i] != NULL && s->observers[i]->update != NULL) {
            s->observers[i]->update(s->observers[i], s);  /* Call observer's update */
        }
    }
}

/* Update stock price and notify observers */
void subject_set_price(Subject* s, float new_price)
{
    if (s == NULL) return;
    
    float old_price = s->stock.price;
    s->stock.change_percent = ((new_price - old_price) / old_price) * 100.0f;
    s->stock.price = new_price;
    
    printf("\n[Subject] %s price changed: $%.2f -> $%.2f (%.2f%%)\n",
           s->stock.stock_name, old_price, new_price, s->stock.change_percent);
    
    /* Notify all observers about the change */
    subject_notify(s);
}

/* Destroy subject */
void subject_destroy(Subject* s)
{
    if (s != NULL) {
        printf("[Subject] Destroyed\n");
        free(s);
    }
}

/* ============================================
 * Concrete Observer 1: Price Logger
 * Logs all price changes to console
 * ============================================ */
typedef struct {
    int log_count;
} LoggerData;

void logger_update(Observer* self, Subject* subject)
{
    LoggerData* data = (LoggerData*)self->user_data;
    data->log_count++;
    
    printf("  [Logger] #%d: %s = $%.2f (%.2f%%)\n",
           data->log_count,
           subject->stock.stock_name,
           subject->stock.price,
           subject->stock.change_percent);
}

Observer* create_logger_observer(void)
{
    Observer* obs = (Observer*)malloc(sizeof(Observer));
    LoggerData* data = (LoggerData*)malloc(sizeof(LoggerData));
    
    if (obs == NULL || data == NULL) {
        free(obs);
        free(data);
        return NULL;
    }
    
    strncpy(obs->name, "PriceLogger", sizeof(obs->name) - 1);
    obs->update = logger_update;
    obs->user_data = data;
    data->log_count = 0;
    
    return obs;
}

/* ============================================
 * Concrete Observer 2: Alert Monitor
 * Alerts when price changes exceed threshold
 * ============================================ */
typedef struct {
    float threshold_percent;
    int alert_count;
} AlertData;

void alert_update(Observer* self, Subject* subject)
{
    AlertData* data = (AlertData*)self->user_data;
    float change = subject->stock.change_percent;
    
    /* Check if change exceeds threshold */
    if (change > data->threshold_percent || change < -data->threshold_percent) {
        data->alert_count++;
        printf("  [ALERT!] #%d: %s moved %.2f%% - SIGNIFICANT CHANGE!\n",
               data->alert_count,
               subject->stock.stock_name,
               change);
    } else {
        printf("  [Alert] %s: Change %.2f%% within normal range\n",
               subject->stock.stock_name, change);
    }
}

Observer* create_alert_observer(float threshold)
{
    Observer* obs = (Observer*)malloc(sizeof(Observer));
    AlertData* data = (AlertData*)malloc(sizeof(AlertData));
    
    if (obs == NULL || data == NULL) {
        free(obs);
        free(data);
        return NULL;
    }
    
    strncpy(obs->name, "AlertMonitor", sizeof(obs->name) - 1);
    obs->update = alert_update;
    obs->user_data = data;
    data->threshold_percent = threshold;
    data->alert_count = 0;
    
    return obs;
}

/* ============================================
 * Concrete Observer 3: Portfolio Tracker
 * Tracks portfolio value based on holdings
 * ============================================ */
typedef struct {
    int shares_owned;
    float total_value;
} PortfolioData;

void portfolio_update(Observer* self, Subject* subject)
{
    PortfolioData* data = (PortfolioData*)self->user_data;
    data->total_value = data->shares_owned * subject->stock.price;
    
    printf("  [Portfolio] %d shares of %s = $%.2f\n",
           data->shares_owned,
           subject->stock.stock_name,
           data->total_value);
}

Observer* create_portfolio_observer(int shares)
{
    Observer* obs = (Observer*)malloc(sizeof(Observer));
    PortfolioData* data = (PortfolioData*)malloc(sizeof(PortfolioData));
    
    if (obs == NULL || data == NULL) {
        free(obs);
        free(data);
        return NULL;
    }
    
    strncpy(obs->name, "PortfolioTracker", sizeof(obs->name) - 1);
    obs->update = portfolio_update;
    obs->user_data = data;
    data->shares_owned = shares;
    data->total_value = 0.0f;
    
    return obs;
}

/* Helper to destroy observer */
void observer_destroy(Observer* obs)
{
    if (obs != NULL) {
        if (obs->user_data != NULL) {
            free(obs->user_data);
        }
        free(obs);
    }
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Observer Pattern Demo ===\n\n");
    
    /* Create subject (stock to monitor) */
    Subject* stock = subject_create("AAPL", 150.00f);
    
    /* Create observers */
    printf("\n--- Creating Observers ---\n");
    Observer* logger = create_logger_observer();
    Observer* alert = create_alert_observer(2.0f);  /* Alert if change > 2% */
    Observer* portfolio = create_portfolio_observer(100);  /* Owns 100 shares */
    
    /* Attach observers to subject */
    printf("\n--- Attaching Observers ---\n");
    subject_attach(stock, logger);
    subject_attach(stock, alert);
    subject_attach(stock, portfolio);
    
    /* Simulate price changes - all observers will be notified */
    printf("\n--- Simulating Price Changes ---\n");
    
    subject_set_price(stock, 151.50f);  /* Small change */
    subject_set_price(stock, 155.00f);  /* Larger change - should trigger alert */
    subject_set_price(stock, 152.00f);  /* Price drop */
    subject_set_price(stock, 145.00f);  /* Big drop - should trigger alert */
    
    /* Detach one observer and continue */
    printf("\n--- Detaching Alert Observer ---\n");
    subject_detach(stock, alert);
    
    printf("\n--- More Price Changes (without alert) ---\n");
    subject_set_price(stock, 148.00f);
    
    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    observer_destroy(logger);
    observer_destroy(alert);
    observer_destroy(portfolio);
    subject_destroy(stock);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个股票价格监控系统：

1. **主题（Subject）**：
   - `Subject` 结构体包含股票数据和观察者列表
   - `subject_attach()`：注册观察者
   - `subject_detach()`：注销观察者
   - `subject_notify()`：通知所有观察者
   - `subject_set_price()`：更新价格并触发通知

2. **观察者实现**：
   - **PriceLogger**：记录所有价格变化
   - **AlertMonitor**：当价格变化超过阈值时发出警报
   - **PortfolioTracker**：跟踪投资组合价值

3. **核心机制**：
   - 价格变化时自动通知所有注册的观察者
   - 每个观察者独立处理通知
   - 可以动态添加/移除观察者

4. **松耦合优势**：
   - Subject 不需要知道具体的观察者类型
   - 观察者可以独立实现自己的逻辑
   - 新增观察者类型无需修改 Subject 代码

