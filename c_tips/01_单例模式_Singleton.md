# 单例模式 (Singleton Pattern)

## 定义

单例模式是一种创建型设计模式，确保一个类只有一个实例，并提供一个全局访问点来获取该实例。

## 适用场景

- 需要严格控制全局变量的场景
- 系统中只需要一个实例的对象（如配置管理器、日志记录器、硬件驱动接口）
- 需要频繁创建和销毁的对象，但创建或销毁时消耗资源过多
- 数据库连接池、线程池等资源池的实现

## ASCII 图解

```
+------------------------------------------------------------------+
|                      SINGLETON PATTERN                            |
+------------------------------------------------------------------+
|                                                                   |
|    +-------------------+                                          |
|    |     Client A      |----+                                     |
|    +-------------------+    |                                     |
|                             |      +------------------------+     |
|    +-------------------+    |      |      Singleton         |     |
|    |     Client B      |----+----->|------------------------|     |
|    +-------------------+    |      | - static instance      |     |
|                             |      | - private data         |     |
|    +-------------------+    |      |------------------------|     |
|    |     Client C      |----+      | + getInstance()        |     |
|    +-------------------+           | + doSomething()        |     |
|                                    +------------------------+     |
|                                              ^                    |
|                                              |                    |
|                                    Only ONE instance exists       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|   getInstance() Flow:                                             |
|   +-----------------+    +------------------+    +--------------+ |
|   | Check instance  |--->| instance == NULL?|--->| Create new   | |
|   | exists?         |    |       YES        |    | instance     | |
|   +-----------------+    +------------------+    +--------------+ |
|          |                       |                     |          |
|          |                       NO                    |          |
|          v                       v                     v          |
|   +--------------------------------------------------+           |
|   |          Return the SAME instance pointer         |           |
|   +--------------------------------------------------+           |
|                                                                   |
+------------------------------------------------------------------+
```

**图解说明：**

上图展示了单例模式的核心结构。多个客户端（Client A、B、C）都通过 `getInstance()` 方法访问同一个 Singleton 实例。无论有多少个客户端请求，系统中始终只存在一个 Singleton 对象。下方的流程图说明了 `getInstance()` 的工作原理：首先检查实例是否存在，如果不存在则创建新实例，最终所有请求都返回同一个实例指针。

## 实现方法

在C语言中，由于没有类的概念，我们通过以下方式实现单例模式：

1. 使用静态全局变量存储唯一实例
2. 提供一个获取实例的函数
3. 将实例的创建逻辑封装在获取函数中
4. 对于多线程环境，需要添加互斥锁保护

## C语言代码示例

### 基础单例实现

```c
// singleton.h
#ifndef SINGLETON_H
#define SINGLETON_H

// 单例结构体定义
typedef struct {
    int value;
    char name[64];
} Singleton;

// 获取单例实例的函数
Singleton* singleton_get_instance(void);

// 单例的操作方法
void singleton_set_value(Singleton* instance, int value);
int singleton_get_value(Singleton* instance);
void singleton_set_name(Singleton* instance, const char* name);
const char* singleton_get_name(Singleton* instance);

// 销毁单例（程序退出时调用）
void singleton_destroy(void);

#endif // SINGLETON_H
```

```c
// singleton.c
#include "singleton.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 静态变量存储唯一实例
static Singleton* g_instance = NULL;

// 获取单例实例
Singleton* singleton_get_instance(void) {
    if (g_instance == NULL) {
        g_instance = (Singleton*)malloc(sizeof(Singleton));
        if (g_instance != NULL) {
            // 初始化默认值
            g_instance->value = 0;
            memset(g_instance->name, 0, sizeof(g_instance->name));
            printf("[Singleton] Instance created at address: %p\n", (void*)g_instance);
        }
    }
    return g_instance;
}

// 设置值
void singleton_set_value(Singleton* instance, int value) {
    if (instance != NULL) {
        instance->value = value;
    }
}

// 获取值
int singleton_get_value(Singleton* instance) {
    return (instance != NULL) ? instance->value : 0;
}

// 设置名称
void singleton_set_name(Singleton* instance, const char* name) {
    if (instance != NULL && name != NULL) {
        strncpy(instance->name, name, sizeof(instance->name) - 1);
        instance->name[sizeof(instance->name) - 1] = '\0';
    }
}

// 获取名称
const char* singleton_get_name(Singleton* instance) {
    return (instance != NULL) ? instance->name : "";
}

// 销毁单例
void singleton_destroy(void) {
    if (g_instance != NULL) {
        printf("[Singleton] Instance destroyed at address: %p\n", (void*)g_instance);
        free(g_instance);
        g_instance = NULL;
    }
}
```

### 线程安全的单例实现

```c
// singleton_threadsafe.h
#ifndef SINGLETON_THREADSAFE_H
#define SINGLETON_THREADSAFE_H

#include <pthread.h>

typedef struct {
    int counter;
    pthread_mutex_t mutex;
} ThreadSafeSingleton;

ThreadSafeSingleton* ts_singleton_get_instance(void);
void ts_singleton_increment(ThreadSafeSingleton* instance);
int ts_singleton_get_counter(ThreadSafeSingleton* instance);
void ts_singleton_destroy(void);

#endif
```

```c
// singleton_threadsafe.c
#include "singleton_threadsafe.h"
#include <stdio.h>
#include <stdlib.h>

static ThreadSafeSingleton* g_ts_instance = NULL;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

// 双重检查锁定（Double-Checked Locking）
ThreadSafeSingleton* ts_singleton_get_instance(void) {
    if (g_ts_instance == NULL) {
        pthread_mutex_lock(&g_init_mutex);
        if (g_ts_instance == NULL) {
            g_ts_instance = (ThreadSafeSingleton*)malloc(sizeof(ThreadSafeSingleton));
            if (g_ts_instance != NULL) {
                g_ts_instance->counter = 0;
                pthread_mutex_init(&g_ts_instance->mutex, NULL);
                printf("[ThreadSafeSingleton] Instance created\n");
            }
        }
        pthread_mutex_unlock(&g_init_mutex);
    }
    return g_ts_instance;
}

void ts_singleton_increment(ThreadSafeSingleton* instance) {
    if (instance != NULL) {
        pthread_mutex_lock(&instance->mutex);
        instance->counter++;
        pthread_mutex_unlock(&instance->mutex);
    }
}

int ts_singleton_get_counter(ThreadSafeSingleton* instance) {
    int value = 0;
    if (instance != NULL) {
        pthread_mutex_lock(&instance->mutex);
        value = instance->counter;
        pthread_mutex_unlock(&instance->mutex);
    }
    return value;
}

void ts_singleton_destroy(void) {
    pthread_mutex_lock(&g_init_mutex);
    if (g_ts_instance != NULL) {
        pthread_mutex_destroy(&g_ts_instance->mutex);
        free(g_ts_instance);
        g_ts_instance = NULL;
        printf("[ThreadSafeSingleton] Instance destroyed\n");
    }
    pthread_mutex_unlock(&g_init_mutex);
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "singleton.h"

int main() {
    printf("=== Singleton Pattern Demo ===\n\n");
    
    // 第一次获取实例
    Singleton* s1 = singleton_get_instance();
    singleton_set_value(s1, 100);
    singleton_set_name(s1, "ConfigManager");
    
    // 第二次获取实例
    Singleton* s2 = singleton_get_instance();
    
    // 验证是同一个实例
    printf("s1 address: %p\n", (void*)s1);
    printf("s2 address: %p\n", (void*)s2);
    printf("s1 == s2: %s\n", (s1 == s2) ? "TRUE" : "FALSE");
    
    // 通过s2访问s1设置的值
    printf("Value via s2: %d\n", singleton_get_value(s2));
    printf("Name via s2: %s\n", singleton_get_name(s2));
    
    // 清理
    singleton_destroy();
    
    return 0;
}

/* 输出示例:
=== Singleton Pattern Demo ===

[Singleton] Instance created at address: 0x55a1b2c3d4e0
s1 address: 0x55a1b2c3d4e0
s2 address: 0x55a1b2c3d4e0
s1 == s2: TRUE
Value via s2: 100
Name via s2: ConfigManager
[Singleton] Instance destroyed at address: 0x55a1b2c3d4e0
*/
```

## 优缺点

### 优点
- 确保全局只有一个实例，节省系统资源
- 提供对唯一实例的全局访问点
- 实例在第一次使用时才创建（懒加载）

### 缺点
- 违背单一职责原则（同时负责创建和业务逻辑）
- 可能隐藏类之间的依赖关系
- 在多线程环境下需要特殊处理
- 单元测试困难（全局状态难以隔离）

