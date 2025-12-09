# 单例模式 (Singleton Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      SINGLETON PATTERN                            |
+------------------------------------------------------------------+

    Normal Creation (Multiple Instances):
    
    get_instance() --> [Instance A]
    get_instance() --> [Instance B]  <-- Problem: Multiple instances!
    get_instance() --> [Instance C]
    

    Singleton Creation (Single Instance):
    
    +------------------+
    |  Static Storage  |
    |  +------------+  |
    |  | instance   |--+-----> [Only One Instance]
    |  | (static)   |  |              ^
    |  +------------+  |              |
    +------------------+              |
           ^                          |
           |                          |
    get_instance() -------------------+
    get_instance() -------------------+
    get_instance() -------------------+
    
    All calls return the SAME instance!


    Thread-Safe Singleton:
    
    +------------------+         +------------------+
    |    Thread A      |         |    Thread B      |
    +--------+---------+         +--------+---------+
             |                            |
             | get_instance()             | get_instance()
             v                            v
    +--------------------------------------------------+
    |                  MUTEX LOCK                       |
    |  +--------------------------------------------+  |
    |  |  if (instance == NULL) {                   |  |
    |  |      instance = create_instance();         |  |
    |  |  }                                         |  |
    |  |  return instance;                          |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
```

**核心思想说明：**
- 确保一个类/模块在整个程序生命周期中只有一个实例
- 通过静态变量存储唯一实例
- 提供全局访问点（获取函数）
- 多线程环境需要加锁保护

## 实现思路

1. **静态变量**：用 `static` 变量存储唯一实例
2. **私有化创建**：不暴露创建函数，只暴露获取函数
3. **懒加载**：首次调用时才创建实例
4. **线程安全**：多线程环境使用互斥锁保护

## 典型应用场景

- 硬件驱动管理器（如 UART、SPI 控制器）
- 日志系统
- 配置管理器
- 资源池管理
- 系统状态管理

## 完整代码示例

```c
/*============================================================================
 * 单例模式示例 - 系统日志管理器
 *============================================================================*/

/*---------------------------------------------------------------------------
 * logger_singleton.h - 公开接口
 *---------------------------------------------------------------------------*/
#ifndef LOGGER_SINGLETON_H
#define LOGGER_SINGLETON_H

#include <stdint.h>
#include <stdbool.h>

/* 日志级别 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

/* 日志配置 */
typedef struct {
    log_level_t min_level;
    bool        enable_timestamp;
    bool        enable_color;
    uint32_t    buffer_size;
} logger_config_t;

/* 关键点：只暴露获取实例和操作函数，不暴露创建函数 */
typedef struct logger logger_t;

logger_t* logger_get_instance(void);
bool logger_init(const logger_config_t *config);
void logger_log(log_level_t level, const char *tag, const char *fmt, ...);
void logger_set_level(log_level_t level);
void logger_flush(void);
void logger_shutdown(void);

/* 便捷宏 */
#define LOG_D(tag, fmt, ...) logger_log(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) logger_log(LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) logger_log(LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) logger_log(LOG_ERROR, tag, fmt, ##__VA_ARGS__)

#endif /* LOGGER_SINGLETON_H */


/*---------------------------------------------------------------------------
 * logger_singleton.c - 单例实现
 *---------------------------------------------------------------------------*/
#include "logger_singleton.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* 日志结构体定义（对外隐藏） */
struct logger {
    log_level_t min_level;
    bool        enable_timestamp;
    bool        enable_color;
    char       *buffer;
    uint32_t    buffer_size;
    uint32_t    buffer_pos;
    bool        initialized;
};

/*---------------------------------------------------------------------------
 * 关键点：静态变量存储唯一实例
 *---------------------------------------------------------------------------*/
static struct logger *g_logger_instance = NULL;
static pthread_mutex_t g_logger_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 级别名称 */
static const char *level_names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
static const char *level_colors[] = {"\033[36m", "\033[32m", "\033[33m", "\033[31m"};
#define COLOR_RESET "\033[0m"

/*---------------------------------------------------------------------------
 * 关键点：获取单例实例（线程安全）
 *---------------------------------------------------------------------------*/
logger_t* logger_get_instance(void) {
    /* 第一次检查（无锁快速路径） */
    if (g_logger_instance != NULL) {
        return g_logger_instance;
    }
    
    /* 加锁保护创建过程 */
    pthread_mutex_lock(&g_logger_mutex);
    
    /* 第二次检查（双重检查锁定） */
    if (g_logger_instance == NULL) {
        /* 关键点：首次调用时创建实例 */
        g_logger_instance = (struct logger *)malloc(sizeof(struct logger));
        if (g_logger_instance != NULL) {
            memset(g_logger_instance, 0, sizeof(struct logger));
            g_logger_instance->min_level = LOG_INFO;
            g_logger_instance->enable_timestamp = true;
            g_logger_instance->enable_color = true;
            g_logger_instance->initialized = false;
        }
    }
    
    pthread_mutex_unlock(&g_logger_mutex);
    
    return g_logger_instance;
}

/*---------------------------------------------------------------------------
 * 初始化日志系统
 *---------------------------------------------------------------------------*/
bool logger_init(const logger_config_t *config) {
    logger_t *logger = logger_get_instance();
    
    if (logger == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&g_logger_mutex);
    
    if (logger->initialized) {
        pthread_mutex_unlock(&g_logger_mutex);
        return true;  /* 已初始化，直接返回 */
    }
    
    /* 应用配置 */
    if (config != NULL) {
        logger->min_level = config->min_level;
        logger->enable_timestamp = config->enable_timestamp;
        logger->enable_color = config->enable_color;
        logger->buffer_size = config->buffer_size;
    } else {
        logger->buffer_size = 4096;
    }
    
    /* 分配缓冲区 */
    if (logger->buffer_size > 0) {
        logger->buffer = (char *)malloc(logger->buffer_size);
        if (logger->buffer == NULL) {
            pthread_mutex_unlock(&g_logger_mutex);
            return false;
        }
    }
    
    logger->initialized = true;
    
    pthread_mutex_unlock(&g_logger_mutex);
    
    LOG_I("LOGGER", "Logger singleton initialized");
    return true;
}

/*---------------------------------------------------------------------------
 * 记录日志
 *---------------------------------------------------------------------------*/
void logger_log(log_level_t level, const char *tag, const char *fmt, ...) {
    logger_t *logger = logger_get_instance();
    
    if (logger == NULL || !logger->initialized) {
        return;
    }
    
    /* 级别过滤 */
    if (level < logger->min_level) {
        return;
    }
    
    pthread_mutex_lock(&g_logger_mutex);
    
    /* 时间戳 */
    if (logger->enable_timestamp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        printf("[%02d:%02d:%02d] ", 
               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
    
    /* 级别和标签 */
    if (logger->enable_color) {
        printf("%s[%s]%s [%s] ", 
               level_colors[level], level_names[level], COLOR_RESET, tag);
    } else {
        printf("[%s] [%s] ", level_names[level], tag);
    }
    
    /* 消息内容 */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
    
    pthread_mutex_unlock(&g_logger_mutex);
}

/*---------------------------------------------------------------------------
 * 设置日志级别
 *---------------------------------------------------------------------------*/
void logger_set_level(log_level_t level) {
    logger_t *logger = logger_get_instance();
    if (logger != NULL) {
        logger->min_level = level;
    }
}

/*---------------------------------------------------------------------------
 * 刷新缓冲区
 *---------------------------------------------------------------------------*/
void logger_flush(void) {
    fflush(stdout);
}

/*---------------------------------------------------------------------------
 * 关闭日志系统
 *---------------------------------------------------------------------------*/
void logger_shutdown(void) {
    pthread_mutex_lock(&g_logger_mutex);
    
    if (g_logger_instance != NULL) {
        if (g_logger_instance->buffer != NULL) {
            free(g_logger_instance->buffer);
        }
        free(g_logger_instance);
        g_logger_instance = NULL;
    }
    
    pthread_mutex_unlock(&g_logger_mutex);
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "logger_singleton.h"

void module_a_work(void) {
    /* 关键点：任何模块都获取同一个实例 */
    logger_t *logger = logger_get_instance();
    LOG_I("MODULE_A", "Doing some work...");
    LOG_D("MODULE_A", "Debug info: value=%d", 42);
}

void module_b_work(void) {
    /* 关键点：同一个实例，配置共享 */
    LOG_I("MODULE_B", "Processing data...");
    LOG_W("MODULE_B", "Memory usage high");
}

int main(void) {
    /* 初始化日志（只需一次） */
    logger_config_t config = {
        .min_level = LOG_DEBUG,
        .enable_timestamp = true,
        .enable_color = true,
        .buffer_size = 4096
    };
    
    if (!logger_init(&config)) {
        printf("Logger init failed!\n");
        return -1;
    }
    
    /* 验证单例：多次获取返回同一实例 */
    logger_t *inst1 = logger_get_instance();
    logger_t *inst2 = logger_get_instance();
    printf("Singleton test: inst1=%p, inst2=%p, same=%s\n",
           (void*)inst1, (void*)inst2, (inst1 == inst2) ? "YES" : "NO");
    
    /* 各模块使用日志 */
    module_a_work();
    module_b_work();
    
    /* 修改级别，所有模块生效 */
    LOG_I("MAIN", "Setting log level to WARN");
    logger_set_level(LOG_WARN);
    
    LOG_D("MAIN", "This DEBUG won't show");
    LOG_I("MAIN", "This INFO won't show");
    LOG_W("MAIN", "This WARN will show");
    LOG_E("MAIN", "This ERROR will show");
    
    /* 关闭 */
    logger_shutdown();
    
    return 0;
}
```

## 运行输出示例

```
Singleton test: inst1=0x55a8c8d012a0, inst2=0x55a8c8d012a0, same=YES
[10:30:45] [INFO ] [LOGGER] Logger singleton initialized
[10:30:45] [INFO ] [MODULE_A] Doing some work...
[10:30:45] [DEBUG] [MODULE_A] Debug info: value=42
[10:30:45] [INFO ] [MODULE_B] Processing data...
[10:30:45] [WARN ] [MODULE_B] Memory usage high
[10:30:45] [INFO ] [MAIN] Setting log level to WARN
[10:30:45] [WARN ] [MAIN] This WARN will show
[10:30:45] [ERROR] [MAIN] This ERROR will show
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **全局唯一** | 保证系统中只有一个日志实例，配置统一 |
| **资源节省** | 避免创建多个实例浪费内存 |
| **状态一致** | 所有模块共享同一配置和状态 |
| **线程安全** | 双重检查锁定保证多线程安全 |
| **懒加载** | 首次使用时才创建，节省启动时间 |

## 注意事项

| 问题 | 解决方案 |
|------|----------|
| 多线程竞争 | 使用互斥锁保护创建过程 |
| 内存泄漏 | 提供 `shutdown()` 函数释放资源 |
| 测试困难 | 可提供 `reset()` 函数用于测试 |
| 隐式依赖 | 文档明确说明是单例 |

