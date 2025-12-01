# 领域驱动设计 (Domain-Driven Design, DDD) - C语言示例详解

## 目录

1. [什么是 DDD](#1-什么是-ddd)
2. [DDD 核心概念](#2-ddd-核心概念)
3. [传统方式 vs DDD 方式对比](#3-传统方式-vs-ddd-方式对比)
4. [完整 C 语言示例：订单管理系统](#4-完整-c-语言示例订单管理系统)
5. [DDD 的好处总结](#5-ddd-的好处总结)

---

## 1. 什么是 DDD

**领域驱动设计 (Domain-Driven Design, DDD)** 是一种软件开发方法论，其核心思想是：

> **将业务领域的概念和规则直接映射到代码中，让代码"说业务语言"。**

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         DDD 核心理念                                                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    传统开发思维:                              DDD 开发思维:
    
    "我要操作数据库"                           "我要处理业务"
    "我要调用 API"                             "我要执行业务规则"
    "我要更新字段"                             "我要完成业务操作"
    
         ↓                                          ↓
    
    ┌─────────────────┐                        ┌─────────────────┐
    │   技术驱动      │                         │   业务驱动      │
    │                 │                        │                 │
    │  UPDATE orders  │                        │  order.confirm()│
    │  SET status=1   │                        │                 │
    │  WHERE id=123   │                        │  // 内部处理:   │
    │                 │                        │  // - 检查库存  │
    │                 │                        │  // - 扣减库存  │
    │                 │                        │  // - 更新状态  │
    │                 │                        │  // - 发送通知  │
    └─────────────────┘                        └─────────────────┘
    
    代码反映"怎么做"                           代码反映"做什么"
    (技术实现细节)                             (业务意图)
```

**说明**：
- 传统方式：代码围绕数据库表、API 接口等技术概念组织
- DDD 方式：代码围绕业务概念（订单、客户、库存）组织，技术细节被封装

---

## 2. DDD 核心概念

### 2.1 概念总览

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         DDD 核心概念图                                                   │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                              Bounded Context (限界上下文)                            │
    │                              "订单管理上下文"                                        │
    │                                                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         Aggregate (聚合)                                    │   │
    │   │                         "订单聚合"                                           │   │
    │   │                                                                             │   │
    │   │   ┌───────────────────────────────────────────────────────────────────┐     │   │
    │   │   │              Aggregate Root (聚合根)                               │     │   │
    │   │   │              Order (订单)                                          │     │   │
    │   │   │                                                                   │     │   │
    │   │   │   - order_id      (唯一标识)                                       │     │   │
    │   │   │   - status        (状态)                                          │     │   │
    │   │   │   - total_amount  (总金额)                                        │     │   │
    │   │   │   - items[]       (订单项)                                        │     │   │
    │   │   │                                                                   │     │   │
    │   │   │   + confirm()     (确认订单 - 业务方法)                            │     │   │
    │   │   │   + cancel()      (取消订单 - 业务方法)                            │     │   │
    │   │   │   + add_item()    (添加商品 - 业务方法)                            │     │   │
    │   │   │                                                                   │     │   │
    │   │   └───────────────────────────────────────────────────────────────────┘     │   │
    │   │                              │                                              │   │
    │   │                              │ 包含                                         │   │
    │   │                              ▼                                              │   │
    │   │   ┌───────────────────────────────────────────────────────────────────┐     │   │
    │   │   │              Entity (实体)                                        │     │   │
    │   │   │              OrderItem (订单项)                                   │     │   │
    │   │   │                                                                   │     │   │
    │   │   │   - item_id       (唯一标识)                                      │     │   │
    │   │   │   - product_id    (商品ID)                                        │     │   │
    │   │   │   - quantity      (数量)                                          │     │   │
    │   │   │   - unit_price    (单价)                                          │     │   │
    │   │   │                                                                   │     │   │
    │   │   └───────────────────────────────────────────────────────────────────┘     │   │
    │   │                                                                             │   │
    │   │   ┌───────────────────────────────────────────────────────────────────┐     │   │
    │   │   │              Value Object (值对象)                                │     │   │
    │   │   │              Money (金额)                                         │     │   │
    │   │   │                                                                   │     │   │
    │   │   │   - amount        (数值)                                          │     │   │
    │   │   │   - currency      (货币类型)                                      │     │   │
    │   │   │                                                                   │     │   │
    │   │   │   (无唯一标识，通过值比较相等性)                                  │     │   │
    │   │   │                                                                   │     │   │
    │   │   └───────────────────────────────────────────────────────────────────┘     │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │              Domain Service (领域服务)                                      │   │
    │   │              OrderService                                                   │   │
    │   │                                                                             │   │
    │   │   - 处理跨聚合的业务逻辑                                                    │   │
    │   │   - 例如: 订单支付涉及订单+支付+库存多个聚合                                │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │              Repository (仓储)                                              │   │
    │   │              OrderRepository                                                │   │
    │   │                                                                             │   │
    │   │   - 负责聚合的持久化                                                        │   │
    │   │   - 隐藏存储细节 (数据库/文件/内存)                                         │   │
    │   │                                                                             │   │
    │   │   + save(order)                                                             │   │
    │   │   + find_by_id(order_id)                                                    │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 核心概念详解

| 概念 | 英文 | 说明 | C语言对应 |
|------|------|------|-----------|
| **实体** | Entity | 有唯一标识，可变状态 | `struct` + ID 字段 |
| **值对象** | Value Object | 无唯一标识，不可变，通过值比较 | `const struct` |
| **聚合** | Aggregate | 一组相关对象的集合，保证一致性 | 嵌套 `struct` |
| **聚合根** | Aggregate Root | 聚合的入口点，外部只能通过它访问聚合 | 主 `struct` |
| **仓储** | Repository | 聚合的持久化接口 | 函数指针结构体 |
| **领域服务** | Domain Service | 不属于任何实体的业务逻辑 | 独立函数 |
| **领域事件** | Domain Event | 业务发生的重要事件 | 事件 `struct` + 回调 |

---

## 3. 传统方式 vs DDD 方式对比

### 3.1 传统方式（以数据为中心）

```c
/* 传统方式：代码围绕数据库操作 */

/* 数据结构只是数据容器，没有行为 */
struct order_row {
    int id;
    int customer_id;
    int status;        /* 0=pending, 1=confirmed, 2=cancelled */
    double total;
};

/* 业务逻辑散落在各处 */
void confirm_order(int order_id) {
    /* 直接操作数据库 */
    struct order_row order;
    db_query("SELECT * FROM orders WHERE id = ?", order_id, &order);
    
    /* 业务规则硬编码在这里 */
    if (order.status != 0) {
        printf("Error: Order already processed\n");
        return;
    }
    
    /* 检查库存 - 业务规则分散 */
    struct order_item_row items[100];
    int item_count;
    db_query("SELECT * FROM order_items WHERE order_id = ?", order_id, items, &item_count);
    
    for (int i = 0; i < item_count; i++) {
        int stock;
        db_query("SELECT stock FROM products WHERE id = ?", items[i].product_id, &stock);
        if (stock < items[i].quantity) {
            printf("Error: Insufficient stock\n");
            return;
        }
    }
    
    /* 扣减库存 */
    for (int i = 0; i < item_count; i++) {
        db_execute("UPDATE products SET stock = stock - ? WHERE id = ?",
                   items[i].quantity, items[i].product_id);
    }
    
    /* 更新订单状态 */
    db_execute("UPDATE orders SET status = 1 WHERE id = ?", order_id);
    
    /* 发送通知 - 又是一个分散的业务逻辑 */
    send_email(order.customer_id, "Your order is confirmed");
}
```

**问题**：
- 业务规则散落在 SQL 和代码各处
- 数据结构只是"哑数据"，没有行为
- 难以理解完整的业务流程
- 修改业务规则需要到处找代码
- 难以测试（依赖数据库）

### 3.2 DDD 方式（以领域为中心）

```c
/* DDD 方式：代码反映业务概念 */

/* 订单有自己的行为，封装业务规则 */
typedef struct Order Order;

/* 业务方法名称直接表达意图 */
OrderResult order_confirm(Order *order);
OrderResult order_cancel(Order *order);
OrderResult order_add_item(Order *order, const Product *product, int quantity);

/* 使用时代码读起来像业务流程 */
void process_order(OrderRepository *repo, int order_id) {
    /* 获取订单（通过仓储，隐藏存储细节） */
    Order *order = repo->find_by_id(repo, order_id);
    if (!order) {
        printf("Order not found\n");
        return;
    }
    
    /* 确认订单 - 一个方法调用包含所有业务规则 */
    OrderResult result = order_confirm(order);
    
    if (result.success) {
        /* 保存变更 */
        repo->save(repo, order);
        printf("Order confirmed successfully\n");
    } else {
        /* 业务规则验证失败 */
        printf("Cannot confirm order: %s\n", result.error_message);
    }
}
```

**优点**：
- 业务规则集中在领域对象中
- 代码直接表达业务意图
- 容易理解业务流程
- 修改业务规则只需改一处
- 容易测试（不依赖数据库）

---

## 4. 完整 C 语言示例：订单管理系统

### 4.1 项目结构

```
order_system/
├── domain/                 # 领域层 - 核心业务逻辑
│   ├── order.h            # 订单聚合根
│   ├── order.c
│   ├── order_item.h       # 订单项实体
│   ├── order_item.c
│   ├── money.h            # 金额值对象
│   ├── money.c
│   ├── order_status.h     # 订单状态枚举
│   └── order_events.h     # 领域事件
│
├── repository/            # 仓储层 - 持久化
│   ├── order_repository.h
│   └── order_repository.c
│
├── service/               # 应用服务层
│   ├── order_service.h
│   └── order_service.c
│
└── main.c                 # 应用入口
```

### 4.2 值对象：Money（金额）

```c
/* ============================================================================
 * 文件: domain/money.h
 * 说明: 金额值对象 - 不可变，通过值比较
 * ============================================================================ */

#ifndef MONEY_H
#define MONEY_H

#include <stdbool.h>

/* 货币类型 */
typedef enum {
    CURRENCY_CNY,   /* 人民币 */
    CURRENCY_USD,   /* 美元 */
    CURRENCY_EUR    /* 欧元 */
} Currency;

/* 
 * 值对象: Money (金额)
 * 
 * 特点:
 * 1. 不可变 - 一旦创建不能修改
 * 2. 无唯一标识 - 两个 Money 如果 amount 和 currency 相同，就是相等的
 * 3. 整体替换 - 要改变金额，创建新的 Money 对象
 */
typedef struct {
    const long cents;        /* 金额（分），避免浮点精度问题 */
    const Currency currency; /* 货币类型 */
} Money;

/* 创建金额（工厂方法） */
Money money_create(long cents, Currency currency);
Money money_from_yuan(double yuan);

/* 金额运算（返回新对象，不修改原对象） */
Money money_add(Money a, Money b);
Money money_subtract(Money a, Money b);
Money money_multiply(Money m, int times);

/* 值比较 */
bool money_equals(Money a, Money b);
bool money_is_greater_than(Money a, Money b);
bool money_is_zero(Money m);

/* 格式化输出 */
void money_to_string(Money m, char *buffer, int size);

#endif /* MONEY_H */
```

```c
/* ============================================================================
 * 文件: domain/money.c
 * ============================================================================ */

#include "money.h"
#include <stdio.h>
#include <string.h>

Money money_create(long cents, Currency currency) {
    Money m = { .cents = cents, .currency = currency };
    return m;
}

Money money_from_yuan(double yuan) {
    return money_create((long)(yuan * 100), CURRENCY_CNY);
}

Money money_add(Money a, Money b) {
    /* 业务规则：不同货币不能直接相加 */
    if (a.currency != b.currency) {
        /* 在实际项目中，这里可能抛出异常或返回错误 */
        return money_create(0, a.currency);
    }
    return money_create(a.cents + b.cents, a.currency);
}

Money money_subtract(Money a, Money b) {
    if (a.currency != b.currency) {
        return money_create(0, a.currency);
    }
    return money_create(a.cents - b.cents, a.currency);
}

Money money_multiply(Money m, int times) {
    return money_create(m.cents * times, m.currency);
}

bool money_equals(Money a, Money b) {
    /* 值对象通过值比较相等性 */
    return a.cents == b.cents && a.currency == b.currency;
}

bool money_is_greater_than(Money a, Money b) {
    if (a.currency != b.currency) return false;
    return a.cents > b.cents;
}

bool money_is_zero(Money m) {
    return m.cents == 0;
}

void money_to_string(Money m, char *buffer, int size) {
    const char *symbol = "";
    switch (m.currency) {
        case CURRENCY_CNY: symbol = "¥"; break;
        case CURRENCY_USD: symbol = "$"; break;
        case CURRENCY_EUR: symbol = "€"; break;
    }
    snprintf(buffer, size, "%s%.2f", symbol, m.cents / 100.0);
}
```

### 4.3 订单状态枚举与领域事件

```c
/* ============================================================================
 * 文件: domain/order_status.h
 * 说明: 订单状态 - 使用枚举表达业务状态
 * ============================================================================ */

#ifndef ORDER_STATUS_H
#define ORDER_STATUS_H

/*
 * 订单状态枚举
 * 
 * 状态转换规则（业务规则）:
 *   PENDING -> CONFIRMED (确认订单)
 *   PENDING -> CANCELLED (取消订单)
 *   CONFIRMED -> SHIPPED (发货)
 *   CONFIRMED -> CANCELLED (取消订单)
 *   SHIPPED -> DELIVERED (送达)
 */
typedef enum {
    ORDER_STATUS_PENDING,    /* 待确认 */
    ORDER_STATUS_CONFIRMED,  /* 已确认 */
    ORDER_STATUS_CANCELLED,  /* 已取消 */
    ORDER_STATUS_SHIPPED,    /* 已发货 */
    ORDER_STATUS_DELIVERED   /* 已送达 */
} OrderStatus;

/* 状态名称（用于日志和显示） */
static inline const char* order_status_name(OrderStatus status) {
    switch (status) {
        case ORDER_STATUS_PENDING:   return "待确认";
        case ORDER_STATUS_CONFIRMED: return "已确认";
        case ORDER_STATUS_CANCELLED: return "已取消";
        case ORDER_STATUS_SHIPPED:   return "已发货";
        case ORDER_STATUS_DELIVERED: return "已送达";
        default: return "未知";
    }
}

#endif /* ORDER_STATUS_H */
```

```c
/* ============================================================================
 * 文件: domain/order_events.h
 * 说明: 领域事件 - 记录业务中发生的重要事件
 * ============================================================================ */

#ifndef ORDER_EVENTS_H
#define ORDER_EVENTS_H

#include <time.h>
#include "money.h"

/*
 * 领域事件: 订单已确认
 * 
 * 用途:
 * 1. 通知其他系统（如库存系统扣减库存）
 * 2. 发送通知给用户
 * 3. 记录审计日志
 */
typedef struct {
    int order_id;
    int customer_id;
    Money total_amount;
    time_t confirmed_at;
} OrderConfirmedEvent;

/*
 * 领域事件: 订单已取消
 */
typedef struct {
    int order_id;
    int customer_id;
    const char *reason;
    time_t cancelled_at;
} OrderCancelledEvent;

/*
 * 领域事件: 订单项已添加
 */
typedef struct {
    int order_id;
    int product_id;
    int quantity;
    Money unit_price;
    time_t added_at;
} OrderItemAddedEvent;

/* 事件处理器类型 */
typedef void (*OrderConfirmedHandler)(const OrderConfirmedEvent *event);
typedef void (*OrderCancelledHandler)(const OrderCancelledEvent *event);
typedef void (*OrderItemAddedHandler)(const OrderItemAddedEvent *event);

#endif /* ORDER_EVENTS_H */
```

### 4.4 实体：OrderItem（订单项）

```c
/* ============================================================================
 * 文件: domain/order_item.h
 * 说明: 订单项实体 - 有唯一标识，属于订单聚合
 * ============================================================================ */

#ifndef ORDER_ITEM_H
#define ORDER_ITEM_H

#include "money.h"

/*
 * 实体: OrderItem (订单项)
 * 
 * 特点:
 * 1. 有唯一标识 (item_id)
 * 2. 可变状态 (quantity 可以修改)
 * 3. 生命周期依赖于 Order（聚合根）
 */
typedef struct {
    int item_id;          /* 唯一标识 */
    int product_id;       /* 商品ID */
    char product_name[64];/* 商品名称（快照，下单时的名称） */
    int quantity;         /* 数量 */
    Money unit_price;     /* 单价（快照，下单时的价格） */
} OrderItem;

/* 创建订单项 */
OrderItem order_item_create(int item_id, int product_id, 
                            const char *product_name,
                            int quantity, Money unit_price);

/* 计算小计 */
Money order_item_subtotal(const OrderItem *item);

/* 修改数量（业务方法） */
typedef struct {
    bool success;
    const char *error_message;
} OrderItemResult;

OrderItemResult order_item_change_quantity(OrderItem *item, int new_quantity);

#endif /* ORDER_ITEM_H */
```

```c
/* ============================================================================
 * 文件: domain/order_item.c
 * ============================================================================ */

#include "order_item.h"
#include <string.h>

OrderItem order_item_create(int item_id, int product_id,
                            const char *product_name,
                            int quantity, Money unit_price) {
    OrderItem item;
    item.item_id = item_id;
    item.product_id = product_id;
    strncpy(item.product_name, product_name, sizeof(item.product_name) - 1);
    item.product_name[sizeof(item.product_name) - 1] = '\0';
    item.quantity = quantity;
    item.unit_price = unit_price;
    return item;
}

Money order_item_subtotal(const OrderItem *item) {
    return money_multiply(item->unit_price, item->quantity);
}

OrderItemResult order_item_change_quantity(OrderItem *item, int new_quantity) {
    OrderItemResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则：数量必须大于 0 */
    if (new_quantity <= 0) {
        result.error_message = "数量必须大于0";
        return result;
    }
    
    /* 业务规则：单次购买不能超过 99 件 */
    if (new_quantity > 99) {
        result.error_message = "单次购买不能超过99件";
        return result;
    }
    
    item->quantity = new_quantity;
    result.success = true;
    return result;
}
```

### 4.5 聚合根：Order（订单）

```c
/* ============================================================================
 * 文件: domain/order.h
 * 说明: 订单聚合根 - 聚合的入口点，封装所有业务规则
 * ============================================================================ */

#ifndef ORDER_H
#define ORDER_H

#include <stdbool.h>
#include <time.h>
#include "order_status.h"
#include "order_item.h"
#include "order_events.h"
#include "money.h"

#define MAX_ORDER_ITEMS 50

/*
 * 聚合根: Order (订单)
 * 
 * 特点:
 * 1. 聚合的入口点 - 外部只能通过 Order 访问 OrderItem
 * 2. 保证聚合内的一致性 - 所有业务规则在这里检查
 * 3. 有唯一标识 (order_id)
 * 4. 封装业务行为 - confirm(), cancel(), add_item() 等
 */
typedef struct Order {
    /* 标识 */
    int order_id;
    int customer_id;
    
    /* 状态 */
    OrderStatus status;
    
    /* 包含的实体 */
    OrderItem items[MAX_ORDER_ITEMS];
    int item_count;
    
    /* 时间戳 */
    time_t created_at;
    time_t updated_at;
    
    /* 领域事件（待发布） */
    void *pending_events[10];
    int event_count;
    
} Order;

/* 操作结果 */
typedef struct {
    bool success;
    const char *error_message;
} OrderResult;

/* ========== 工厂方法 ========== */

/* 创建新订单 */
Order* order_create(int order_id, int customer_id);

/* 释放订单 */
void order_destroy(Order *order);

/* ========== 业务方法（体现 DDD 的核心）========== */

/*
 * 确认订单
 * 
 * 业务规则:
 * 1. 只有待确认状态的订单可以确认
 * 2. 订单必须至少有一个商品
 * 3. 确认后状态变为"已确认"
 * 4. 发布 OrderConfirmedEvent 事件
 */
OrderResult order_confirm(Order *order);

/*
 * 取消订单
 * 
 * 业务规则:
 * 1. 已发货的订单不能取消
 * 2. 已送达的订单不能取消
 * 3. 取消后状态变为"已取消"
 * 4. 发布 OrderCancelledEvent 事件
 */
OrderResult order_cancel(Order *order, const char *reason);

/*
 * 添加商品
 * 
 * 业务规则:
 * 1. 只有待确认状态的订单可以添加商品
 * 2. 同一商品会合并数量
 * 3. 订单最多 50 种商品
 * 4. 发布 OrderItemAddedEvent 事件
 */
OrderResult order_add_item(Order *order, int product_id, 
                           const char *product_name,
                           int quantity, Money unit_price);

/*
 * 移除商品
 * 
 * 业务规则:
 * 1. 只有待确认状态的订单可以移除商品
 * 2. 商品必须存在于订单中
 */
OrderResult order_remove_item(Order *order, int product_id);

/*
 * 修改商品数量
 * 
 * 业务规则:
 * 1. 只有待确认状态的订单可以修改
 * 2. 数量必须大于 0 且不超过 99
 */
OrderResult order_change_item_quantity(Order *order, int product_id, int new_quantity);

/* ========== 查询方法 ========== */

/* 计算订单总金额 */
Money order_total_amount(const Order *order);

/* 获取订单项数量 */
int order_item_count(const Order *order);

/* 根据商品ID查找订单项 */
const OrderItem* order_find_item(const Order *order, int product_id);

/* 检查订单是否可以修改 */
bool order_is_modifiable(const Order *order);

/* 打印订单信息 */
void order_print(const Order *order);

#endif /* ORDER_H */
```

```c
/* ============================================================================
 * 文件: domain/order.c
 * 说明: 订单聚合根实现 - 所有业务规则集中在这里
 * ============================================================================ */

#include "order.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ========== 工厂方法 ========== */

Order* order_create(int order_id, int customer_id) {
    Order *order = (Order*)malloc(sizeof(Order));
    if (!order) return NULL;
    
    order->order_id = order_id;
    order->customer_id = customer_id;
    order->status = ORDER_STATUS_PENDING;  /* 初始状态：待确认 */
    order->item_count = 0;
    order->created_at = time(NULL);
    order->updated_at = order->created_at;
    order->event_count = 0;
    
    return order;
}

void order_destroy(Order *order) {
    if (order) {
        /* 清理待发布的事件 */
        for (int i = 0; i < order->event_count; i++) {
            free(order->pending_events[i]);
        }
        free(order);
    }
}

/* ========== 私有辅助函数 ========== */

static int find_item_index(const Order *order, int product_id) {
    for (int i = 0; i < order->item_count; i++) {
        if (order->items[i].product_id == product_id) {
            return i;
        }
    }
    return -1;
}

static int generate_item_id(const Order *order) {
    /* 简单的ID生成策略 */
    return order->order_id * 1000 + order->item_count + 1;
}

static void add_event(Order *order, void *event) {
    if (order->event_count < 10) {
        order->pending_events[order->event_count++] = event;
    }
}

/* ========== 业务方法实现 ========== */

OrderResult order_confirm(Order *order) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则 1：只有待确认状态的订单可以确认 */
    if (order->status != ORDER_STATUS_PENDING) {
        result.error_message = "只有待确认状态的订单可以确认";
        return result;
    }
    
    /* 业务规则 2：订单必须至少有一个商品 */
    if (order->item_count == 0) {
        result.error_message = "订单中没有商品，无法确认";
        return result;
    }
    
    /* 执行状态转换 */
    order->status = ORDER_STATUS_CONFIRMED;
    order->updated_at = time(NULL);
    
    /* 发布领域事件 */
    OrderConfirmedEvent *event = (OrderConfirmedEvent*)malloc(sizeof(OrderConfirmedEvent));
    if (event) {
        event->order_id = order->order_id;
        event->customer_id = order->customer_id;
        event->total_amount = order_total_amount(order);
        event->confirmed_at = order->updated_at;
        add_event(order, event);
    }
    
    result.success = true;
    return result;
}

OrderResult order_cancel(Order *order, const char *reason) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则：已发货或已送达的订单不能取消 */
    if (order->status == ORDER_STATUS_SHIPPED) {
        result.error_message = "已发货的订单不能取消";
        return result;
    }
    
    if (order->status == ORDER_STATUS_DELIVERED) {
        result.error_message = "已送达的订单不能取消";
        return result;
    }
    
    if (order->status == ORDER_STATUS_CANCELLED) {
        result.error_message = "订单已经被取消";
        return result;
    }
    
    /* 执行状态转换 */
    order->status = ORDER_STATUS_CANCELLED;
    order->updated_at = time(NULL);
    
    /* 发布领域事件 */
    OrderCancelledEvent *event = (OrderCancelledEvent*)malloc(sizeof(OrderCancelledEvent));
    if (event) {
        event->order_id = order->order_id;
        event->customer_id = order->customer_id;
        event->reason = reason;
        event->cancelled_at = order->updated_at;
        add_event(order, event);
    }
    
    result.success = true;
    return result;
}

OrderResult order_add_item(Order *order, int product_id,
                           const char *product_name,
                           int quantity, Money unit_price) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则 1：只有待确认状态的订单可以添加商品 */
    if (order->status != ORDER_STATUS_PENDING) {
        result.error_message = "只有待确认状态的订单可以添加商品";
        return result;
    }
    
    /* 业务规则 2：数量必须大于 0 */
    if (quantity <= 0) {
        result.error_message = "商品数量必须大于0";
        return result;
    }
    
    /* 检查是否已存在该商品 */
    int existing_index = find_item_index(order, product_id);
    if (existing_index >= 0) {
        /* 业务规则 3：同一商品合并数量 */
        int new_quantity = order->items[existing_index].quantity + quantity;
        if (new_quantity > 99) {
            result.error_message = "同一商品数量不能超过99件";
            return result;
        }
        order->items[existing_index].quantity = new_quantity;
    } else {
        /* 业务规则 4：订单最多 50 种商品 */
        if (order->item_count >= MAX_ORDER_ITEMS) {
            result.error_message = "订单商品种类已达上限";
            return result;
        }
        
        /* 添加新商品 */
        int item_id = generate_item_id(order);
        order->items[order->item_count] = order_item_create(
            item_id, product_id, product_name, quantity, unit_price);
        order->item_count++;
    }
    
    order->updated_at = time(NULL);
    
    /* 发布领域事件 */
    OrderItemAddedEvent *event = (OrderItemAddedEvent*)malloc(sizeof(OrderItemAddedEvent));
    if (event) {
        event->order_id = order->order_id;
        event->product_id = product_id;
        event->quantity = quantity;
        event->unit_price = unit_price;
        event->added_at = order->updated_at;
        add_event(order, event);
    }
    
    result.success = true;
    return result;
}

OrderResult order_remove_item(Order *order, int product_id) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则：只有待确认状态的订单可以移除商品 */
    if (order->status != ORDER_STATUS_PENDING) {
        result.error_message = "只有待确认状态的订单可以移除商品";
        return result;
    }
    
    int index = find_item_index(order, product_id);
    if (index < 0) {
        result.error_message = "订单中不存在该商品";
        return result;
    }
    
    /* 移除商品（将后面的元素前移） */
    for (int i = index; i < order->item_count - 1; i++) {
        order->items[i] = order->items[i + 1];
    }
    order->item_count--;
    order->updated_at = time(NULL);
    
    result.success = true;
    return result;
}

OrderResult order_change_item_quantity(Order *order, int product_id, int new_quantity) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 业务规则：只有待确认状态的订单可以修改 */
    if (order->status != ORDER_STATUS_PENDING) {
        result.error_message = "只有待确认状态的订单可以修改";
        return result;
    }
    
    int index = find_item_index(order, product_id);
    if (index < 0) {
        result.error_message = "订单中不存在该商品";
        return result;
    }
    
    /* 委托给 OrderItem 处理（实体有自己的业务规则） */
    OrderItemResult item_result = order_item_change_quantity(
        &order->items[index], new_quantity);
    
    if (!item_result.success) {
        result.error_message = item_result.error_message;
        return result;
    }
    
    order->updated_at = time(NULL);
    result.success = true;
    return result;
}

/* ========== 查询方法实现 ========== */

Money order_total_amount(const Order *order) {
    Money total = money_create(0, CURRENCY_CNY);
    for (int i = 0; i < order->item_count; i++) {
        Money subtotal = order_item_subtotal(&order->items[i]);
        total = money_add(total, subtotal);
    }
    return total;
}

int order_item_count(const Order *order) {
    return order->item_count;
}

const OrderItem* order_find_item(const Order *order, int product_id) {
    int index = find_item_index(order, product_id);
    if (index < 0) return NULL;
    return &order->items[index];
}

bool order_is_modifiable(const Order *order) {
    return order->status == ORDER_STATUS_PENDING;
}

void order_print(const Order *order) {
    char money_str[32];
    
    printf("\n========================================\n");
    printf("订单号: %d\n", order->order_id);
    printf("客户ID: %d\n", order->customer_id);
    printf("状态: %s\n", order_status_name(order->status));
    printf("----------------------------------------\n");
    printf("商品列表:\n");
    
    for (int i = 0; i < order->item_count; i++) {
        const OrderItem *item = &order->items[i];
        Money subtotal = order_item_subtotal(item);
        money_to_string(item->unit_price, money_str, sizeof(money_str));
        printf("  [%d] %s x %d @ %s", 
               item->product_id, item->product_name, 
               item->quantity, money_str);
        money_to_string(subtotal, money_str, sizeof(money_str));
        printf(" = %s\n", money_str);
    }
    
    printf("----------------------------------------\n");
    Money total = order_total_amount(order);
    money_to_string(total, money_str, sizeof(money_str));
    printf("订单总额: %s\n", money_str);
    printf("========================================\n");
}
```

### 4.6 仓储：OrderRepository

```c
/* ============================================================================
 * 文件: repository/order_repository.h
 * 说明: 订单仓储接口 - 隐藏持久化细节
 * ============================================================================ */

#ifndef ORDER_REPOSITORY_H
#define ORDER_REPOSITORY_H

#include "../domain/order.h"

/*
 * 仓储: OrderRepository
 * 
 * 作用:
 * 1. 提供聚合的持久化接口
 * 2. 隐藏存储细节（可以是数据库、文件、内存等）
 * 3. 领域层只依赖接口，不依赖具体实现
 * 
 * 使用函数指针实现接口，类似于 OOP 中的虚函数
 */
typedef struct OrderRepository {
    /* 保存订单 */
    bool (*save)(struct OrderRepository *repo, Order *order);
    
    /* 根据ID查找订单 */
    Order* (*find_by_id)(struct OrderRepository *repo, int order_id);
    
    /* 根据客户ID查找订单列表 */
    int (*find_by_customer)(struct OrderRepository *repo, int customer_id, 
                            Order **orders, int max_count);
    
    /* 删除订单 */
    bool (*delete)(struct OrderRepository *repo, int order_id);
    
    /* 私有数据（实现相关） */
    void *private_data;
    
} OrderRepository;

/* 创建内存仓储（用于测试） */
OrderRepository* order_repository_create_in_memory(void);

/* 销毁仓储 */
void order_repository_destroy(OrderRepository *repo);

#endif /* ORDER_REPOSITORY_H */
```

```c
/* ============================================================================
 * 文件: repository/order_repository.c
 * 说明: 订单仓储的内存实现（用于演示和测试）
 * ============================================================================ */

#include "order_repository.h"
#include <stdlib.h>
#include <string.h>

#define MAX_ORDERS 1000

/* 内存存储的私有数据 */
typedef struct {
    Order *orders[MAX_ORDERS];
    int count;
} InMemoryStore;

/* ========== 接口实现 ========== */

static bool in_memory_save(OrderRepository *repo, Order *order) {
    InMemoryStore *store = (InMemoryStore*)repo->private_data;
    
    /* 检查是否已存在（更新） */
    for (int i = 0; i < store->count; i++) {
        if (store->orders[i]->order_id == order->order_id) {
            /* 已存在，不需要额外操作（内存中是同一个对象） */
            return true;
        }
    }
    
    /* 新增 */
    if (store->count >= MAX_ORDERS) {
        return false;
    }
    
    store->orders[store->count++] = order;
    return true;
}

static Order* in_memory_find_by_id(OrderRepository *repo, int order_id) {
    InMemoryStore *store = (InMemoryStore*)repo->private_data;
    
    for (int i = 0; i < store->count; i++) {
        if (store->orders[i]->order_id == order_id) {
            return store->orders[i];
        }
    }
    return NULL;
}

static int in_memory_find_by_customer(OrderRepository *repo, int customer_id,
                                      Order **orders, int max_count) {
    InMemoryStore *store = (InMemoryStore*)repo->private_data;
    int found = 0;
    
    for (int i = 0; i < store->count && found < max_count; i++) {
        if (store->orders[i]->customer_id == customer_id) {
            orders[found++] = store->orders[i];
        }
    }
    return found;
}

static bool in_memory_delete(OrderRepository *repo, int order_id) {
    InMemoryStore *store = (InMemoryStore*)repo->private_data;
    
    for (int i = 0; i < store->count; i++) {
        if (store->orders[i]->order_id == order_id) {
            order_destroy(store->orders[i]);
            /* 将后面的元素前移 */
            for (int j = i; j < store->count - 1; j++) {
                store->orders[j] = store->orders[j + 1];
            }
            store->count--;
            return true;
        }
    }
    return false;
}

/* ========== 工厂方法 ========== */

OrderRepository* order_repository_create_in_memory(void) {
    OrderRepository *repo = (OrderRepository*)malloc(sizeof(OrderRepository));
    if (!repo) return NULL;
    
    InMemoryStore *store = (InMemoryStore*)malloc(sizeof(InMemoryStore));
    if (!store) {
        free(repo);
        return NULL;
    }
    
    store->count = 0;
    memset(store->orders, 0, sizeof(store->orders));
    
    repo->save = in_memory_save;
    repo->find_by_id = in_memory_find_by_id;
    repo->find_by_customer = in_memory_find_by_customer;
    repo->delete = in_memory_delete;
    repo->private_data = store;
    
    return repo;
}

void order_repository_destroy(OrderRepository *repo) {
    if (repo) {
        InMemoryStore *store = (InMemoryStore*)repo->private_data;
        if (store) {
            for (int i = 0; i < store->count; i++) {
                order_destroy(store->orders[i]);
            }
            free(store);
        }
        free(repo);
    }
}
```

### 4.7 应用服务：OrderService

```c
/* ============================================================================
 * 文件: service/order_service.h
 * 说明: 订单应用服务 - 编排领域对象完成用例
 * ============================================================================ */

#ifndef ORDER_SERVICE_H
#define ORDER_SERVICE_H

#include "../domain/order.h"
#include "../repository/order_repository.h"

/*
 * 应用服务: OrderService
 * 
 * 职责:
 * 1. 编排领域对象完成用例
 * 2. 处理事务边界
 * 3. 调用仓储进行持久化
 * 4. 发布领域事件
 * 
 * 注意: 应用服务不包含业务逻辑，业务逻辑在领域对象中
 */
typedef struct {
    OrderRepository *order_repo;
    /* 可以注入其他依赖，如事件发布器、通知服务等 */
} OrderService;

/* 创建服务 */
OrderService* order_service_create(OrderRepository *order_repo);

/* 销毁服务 */
void order_service_destroy(OrderService *service);

/* ========== 用例方法 ========== */

/* 创建订单 */
Order* order_service_create_order(OrderService *service, int customer_id);

/* 添加商品到订单 */
OrderResult order_service_add_item(OrderService *service, int order_id,
                                   int product_id, const char *product_name,
                                   int quantity, double price);

/* 确认订单 */
OrderResult order_service_confirm_order(OrderService *service, int order_id);

/* 取消订单 */
OrderResult order_service_cancel_order(OrderService *service, int order_id, 
                                       const char *reason);

/* 查询订单 */
Order* order_service_get_order(OrderService *service, int order_id);

#endif /* ORDER_SERVICE_H */
```

```c
/* ============================================================================
 * 文件: service/order_service.c
 * ============================================================================ */

#include "order_service.h"
#include <stdlib.h>
#include <stdio.h>

static int next_order_id = 1000;

OrderService* order_service_create(OrderRepository *order_repo) {
    OrderService *service = (OrderService*)malloc(sizeof(OrderService));
    if (!service) return NULL;
    
    service->order_repo = order_repo;
    return service;
}

void order_service_destroy(OrderService *service) {
    if (service) {
        free(service);
    }
}

Order* order_service_create_order(OrderService *service, int customer_id) {
    /* 生成订单ID */
    int order_id = next_order_id++;
    
    /* 调用领域对象的工厂方法 */
    Order *order = order_create(order_id, customer_id);
    if (!order) return NULL;
    
    /* 持久化 */
    if (!service->order_repo->save(service->order_repo, order)) {
        order_destroy(order);
        return NULL;
    }
    
    printf("[OrderService] 订单创建成功，订单号: %d\n", order_id);
    return order;
}

OrderResult order_service_add_item(OrderService *service, int order_id,
                                   int product_id, const char *product_name,
                                   int quantity, double price) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 获取订单 */
    Order *order = service->order_repo->find_by_id(service->order_repo, order_id);
    if (!order) {
        result.error_message = "订单不存在";
        return result;
    }
    
    /* 调用领域对象的业务方法 */
    Money unit_price = money_from_yuan(price);
    result = order_add_item(order, product_id, product_name, quantity, unit_price);
    
    if (result.success) {
        /* 持久化 */
        service->order_repo->save(service->order_repo, order);
        printf("[OrderService] 商品添加成功: %s x %d\n", product_name, quantity);
    }
    
    return result;
}

OrderResult order_service_confirm_order(OrderService *service, int order_id) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    /* 获取订单 */
    Order *order = service->order_repo->find_by_id(service->order_repo, order_id);
    if (!order) {
        result.error_message = "订单不存在";
        return result;
    }
    
    /* 调用领域对象的业务方法 - 所有业务规则在这里检查 */
    result = order_confirm(order);
    
    if (result.success) {
        /* 持久化 */
        service->order_repo->save(service->order_repo, order);
        
        /* 这里可以发布领域事件，通知其他系统 */
        printf("[OrderService] 订单确认成功，订单号: %d\n", order_id);
        
        /* 示例：发送通知 */
        printf("[OrderService] 向客户 %d 发送订单确认通知\n", order->customer_id);
    }
    
    return result;
}

OrderResult order_service_cancel_order(OrderService *service, int order_id,
                                       const char *reason) {
    OrderResult result = { .success = false, .error_message = NULL };
    
    Order *order = service->order_repo->find_by_id(service->order_repo, order_id);
    if (!order) {
        result.error_message = "订单不存在";
        return result;
    }
    
    /* 调用领域对象的业务方法 */
    result = order_cancel(order, reason);
    
    if (result.success) {
        service->order_repo->save(service->order_repo, order);
        printf("[OrderService] 订单取消成功，订单号: %d，原因: %s\n", order_id, reason);
    }
    
    return result;
}

Order* order_service_get_order(OrderService *service, int order_id) {
    return service->order_repo->find_by_id(service->order_repo, order_id);
}
```

### 4.8 主程序：演示完整流程

```c
/* ============================================================================
 * 文件: main.c
 * 说明: 演示 DDD 的完整使用流程
 * ============================================================================ */

#include <stdio.h>
#include "domain/order.h"
#include "repository/order_repository.h"
#include "service/order_service.h"

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║       领域驱动设计 (DDD) - C语言示例演示                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* ========== 初始化基础设施 ========== */
    printf("\n[初始化] 创建仓储和服务...\n");
    
    OrderRepository *repo = order_repository_create_in_memory();
    OrderService *service = order_service_create(repo);
    
    /* ========== 用例 1: 创建订单并添加商品 ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("用例 1: 创建订单并添加商品\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    /* 创建订单 */
    Order *order1 = order_service_create_order(service, 10001);
    if (!order1) {
        printf("创建订单失败\n");
        return 1;
    }
    
    /* 添加商品 */
    order_service_add_item(service, order1->order_id, 
                           1, "iPhone 15 Pro", 1, 8999.00);
    order_service_add_item(service, order1->order_id,
                           2, "AirPods Pro", 2, 1899.00);
    order_service_add_item(service, order1->order_id,
                           3, "MacBook Pro 14", 1, 16999.00);
    
    /* 打印订单 */
    order_print(order1);
    
    /* ========== 用例 2: 确认订单 ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("用例 2: 确认订单\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    OrderResult result = order_service_confirm_order(service, order1->order_id);
    if (result.success) {
        order_print(order1);
    } else {
        printf("确认订单失败: %s\n", result.error_message);
    }
    
    /* ========== 用例 3: 尝试修改已确认的订单（应该失败） ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("用例 3: 尝试修改已确认的订单（业务规则验证）\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    result = order_service_add_item(service, order1->order_id,
                                    4, "iPad Pro", 1, 6999.00);
    if (!result.success) {
        printf("预期的失败: %s\n", result.error_message);
    }
    
    /* ========== 用例 4: 创建另一个订单并取消 ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("用例 4: 创建订单并取消\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    Order *order2 = order_service_create_order(service, 10002);
    order_service_add_item(service, order2->order_id,
                           5, "Apple Watch", 1, 3299.00);
    
    order_print(order2);
    
    result = order_service_cancel_order(service, order2->order_id, "用户主动取消");
    if (result.success) {
        order_print(order2);
    }
    
    /* ========== 用例 5: 尝试确认空订单（应该失败） ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("用例 5: 尝试确认空订单（业务规则验证）\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    Order *order3 = order_service_create_order(service, 10003);
    result = order_service_confirm_order(service, order3->order_id);
    if (!result.success) {
        printf("预期的失败: %s\n", result.error_message);
    }
    
    /* ========== 清理资源 ========== */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("演示结束，清理资源...\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    order_service_destroy(service);
    order_repository_destroy(repo);
    
    printf("\n完成!\n\n");
    return 0;
}
```

### 4.9 Makefile

```makefile
# Makefile for DDD Order System Example

CC = gcc
CFLAGS = -Wall -Wextra -g -I.

# 源文件
SRCS = main.c \
       domain/order.c \
       domain/order_item.c \
       domain/money.c \
       repository/order_repository.c \
       service/order_service.c

# 目标文件
OBJS = $(SRCS:.c=.o)

# 可执行文件
TARGET = order_system

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

### 4.10 预期输出

```
╔══════════════════════════════════════════════════════════════╗
║       领域驱动设计 (DDD) - C语言示例演示                     ║
╚══════════════════════════════════════════════════════════════╝

[初始化] 创建仓储和服务...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
用例 1: 创建订单并添加商品
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[OrderService] 订单创建成功，订单号: 1000
[OrderService] 商品添加成功: iPhone 15 Pro x 1
[OrderService] 商品添加成功: AirPods Pro x 2
[OrderService] 商品添加成功: MacBook Pro 14 x 1

========================================
订单号: 1000
客户ID: 10001
状态: 待确认
----------------------------------------
商品列表:
  [1] iPhone 15 Pro x 1 @ ¥8999.00 = ¥8999.00
  [2] AirPods Pro x 2 @ ¥1899.00 = ¥3798.00
  [3] MacBook Pro 14 x 1 @ ¥16999.00 = ¥16999.00
----------------------------------------
订单总额: ¥29796.00
========================================

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
用例 2: 确认订单
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[OrderService] 订单确认成功，订单号: 1000
[OrderService] 向客户 10001 发送订单确认通知

========================================
订单号: 1000
客户ID: 10001
状态: 已确认
...
========================================

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
用例 3: 尝试修改已确认的订单（业务规则验证）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
预期的失败: 只有待确认状态的订单可以添加商品

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
用例 5: 尝试确认空订单（业务规则验证）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
预期的失败: 订单中没有商品，无法确认

完成!
```

---

## 5. DDD 的好处总结

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         DDD 的核心好处                                                  │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   1. 业务逻辑集中，易于理解和维护                                                   │
    │                                                                                     │
    │      传统方式:                          DDD 方式:                                   │
    │      ┌─────────────────────────┐        ┌─────────────────────────┐                 │
    │      │ 业务规则散落在:         │        │ 业务规则集中在:         │                 │
    │      │ - Controller            │        │ - Order.confirm()       │                 │
    │      │ - Service               │   →    │ - Order.cancel()        │                 │
    │      │ - DAO                   │        │ - Order.add_item()      │                 │
    │      │ - 存储过程              │        │                         │                 │
    │      └─────────────────────────┘        └─────────────────────────┘                 │
    │                                                                                     │
    │      修改业务规则只需要改一个地方!                                                  │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   2. 代码即文档，表达业务意图                                                       │
    │                                                                                     │
    │      传统代码:                          DDD 代码:                                   │
    │      ┌─────────────────────────┐        ┌─────────────────────────┐                 │
    │      │ UPDATE orders           │        │ order_confirm(order);   │                 │
    │      │ SET status = 1          │   →    │                         │                 │
    │      │ WHERE id = 123          │        │ // 读代码就知道在做什么 │                 │
    │      │ AND status = 0;         │        │ // 不需要额外注释       │                 │
    │      └─────────────────────────┘        └─────────────────────────┘                 │
    │                                                                                     │
    │      新人看代码就能理解业务流程!                                                    │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   3. 易于测试，不依赖基础设施                                                       │
    │                                                                                     │
    │      传统测试:                          DDD 测试:                                   │
    │      ┌─────────────────────────┐        ┌─────────────────────────┐                 │
    │      │ 需要:                   │        │ 只需要:                 │                 │
    │      │ - 启动数据库            │        │ - 创建领域对象          │                 │
    │      │ - 准备测试数据          │   →    │ - 调用业务方法          │                 │
    │      │ - 清理测试数据          │        │ - 验证结果              │                 │
    │      │ - 测试很慢              │        │ - 毫秒级完成            │                 │
    │      └─────────────────────────┘        └─────────────────────────┘                 │
    │                                                                                     │
    │      单元测试可以覆盖所有业务规则!                                                  │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   4. 技术与业务解耦                                                                 │
    │                                                                                     │
    │      ┌─────────────────────────────────────────────────────────────────────────┐    │
    │      │                                                                         │    │
    │      │   领域层 (Order, OrderItem, Money)                                      │    │
    │      │   - 纯业务逻辑                                                          │    │
    │      │   - 不依赖任何框架                                                      │    │
    │      │   - 不依赖数据库                                                        │    │
    │      │                                                                         │    │
    │      └─────────────────────────────────────────────────────────────────────────┘    │
    │                              ▲                                                      │
    │                              │ 依赖倒置                                             │
    │                              │                                                      │
    │      ┌─────────────────────────────────────────────────────────────────────────┐    │
    │      │                                                                         │    │
    │      │   基础设施层 (Repository 实现)                                          │    │
    │      │   - 可以换 MySQL → PostgreSQL                                           │    │
    │      │   - 可以换 文件 → Redis                                                 │    │
    │      │   - 领域层代码不用改                                                    │    │
    │      │                                                                         │    │
    │      └─────────────────────────────────────────────────────────────────────────┘    │
    │                                                                                     │
    │      更换技术栈不影响业务代码!                                                      │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   5. 统一语言，减少沟通成本                                                         │
    │                                                                                     │
    │      ┌─────────────────────────────────────────────────────────────────────────┐    │
    │      │                                                                         │    │
    │      │   业务人员说:  "确认订单"                                               │    │
    │      │   代码里写:    order_confirm(order)                                     │    │
    │      │                                                                         │    │
    │      │   业务人员说:  "订单总金额"                                             │    │
    │      │   代码里写:    order_total_amount(order)                                │    │
    │      │                                                                         │    │
    │      │   业务人员说:  "只有待确认的订单可以添加商品"                           │    │
    │      │   代码里写:    if (order->status != ORDER_STATUS_PENDING) return error; │    │
    │      │                                                                         │    │
    │      └─────────────────────────────────────────────────────────────────────────┘    │
    │                                                                                     │
    │      业务人员和开发人员使用相同的语言!                                              │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

### 好处总结表

| 好处 | 说明 | 示例体现 |
|------|------|----------|
| **业务逻辑集中** | 所有业务规则在领域对象中 | `order_confirm()` 包含所有确认规则 |
| **代码即文档** | 方法名直接表达业务意图 | `order_add_item()` 而非 `insertOrderItem()` |
| **易于测试** | 领域对象不依赖外部 | 可以直接 `new Order()` 测试 |
| **技术解耦** | 通过仓储接口隔离存储 | 换数据库不改业务代码 |
| **统一语言** | 代码术语与业务术语一致 | 业务说"确认"，代码写 `confirm` |
| **高内聚低耦合** | 聚合保证一致性边界 | 通过 Order 访问 OrderItem |
| **可扩展性** | 新增业务只需扩展领域对象 | 添加新状态只改 Order |

---

## 附录：DDD 核心原则速查

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         DDD 核心原则速查表                                              │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   原则 1: 领域对象有行为，不是贫血模型                                              │
    │                                                                                     │
    │   ✗ 贫血模型:                           ✓ 充血模型:                                 │
    │   struct Order {                        struct Order {                              │
    │       int status;  // 只有数据              int status;                             │
    │   };                                        OrderResult confirm();  // 有行为       │
    │   void confirm(Order *o) {                  OrderResult cancel();                   │
    │       o->status = 1;  // 外部修改       };                                          │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   原则 2: 通过聚合根访问聚合内的对象                                                │
    │                                                                                     │
    │   ✗ 直接访问:                           ✓ 通过聚合根:                               │
    │   order_item_set_qty(item, 5);          order_change_item_qty(order, pid, 5);       │
    │                                                                                     │
    │   外部不应该直接操作 OrderItem，而应该通过 Order                                    │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   原则 3: 值对象不可变                                                              │
    │                                                                                     │
    │   ✗ 可变:                               ✓ 不可变:                                   │
    │   money->amount = 100;                  Money new_money = money_add(m1, m2);        │
    │                                                                                     │
    │   值对象的修改应该返回新对象，而不是修改原对象                                      │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   原则 4: 仓储只操作聚合根                                                          │
    │                                                                                     │
    │   ✗ 错误:                               ✓ 正确:                                     │
    │   order_item_repo->save(item);          order_repo->save(order);                    │
    │                                                                                     │
    │   OrderItem 的持久化应该随 Order 一起，不应该有独立的仓储                           │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   原则 5: 领域层不依赖基础设施                                                      │
    │                                                                                     │
    │   ✗ 错误:                               ✓ 正确:                                     │
    │   #include <mysql.h>                    // 领域层只依赖自己的头文件                 │
    │   void order_save(Order *o) {           // 通过仓储接口隔离                         │
    │       mysql_query(...);                 repo->save(repo, order);                    │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

---

**总结**：DDD 的核心思想是让代码反映业务，而不是反映技术实现。通过将业务规则封装在领域对象中，使用统一语言，以及通过仓储隔离技术细节，可以构建出易于理解、测试和维护的软件系统。

