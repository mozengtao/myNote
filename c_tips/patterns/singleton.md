# Singleton Pattern (单例模式)

## 1. Core Concept and Use Cases

### Core Concept
Ensure a class/module has **only one instance** and provide a global point of access to it.

### Typical Use Cases
- Configuration management
- Logger system
- Hardware resource management
- Database connection pool
- Cache management

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                    Singleton Pattern                                              |
+--------------------------------------------------------------------------------------------------+

                                First Call                    Subsequent Calls
                                    |                              |
                                    v                              v
                        +---------------------+        +---------------------+
                        | get_instance()      |        | get_instance()      |
                        +---------------------+        +---------------------+
                                    |                              |
                                    v                              v
                        +---------------------+        +---------------------+
                        | instance == NULL?   |        | instance == NULL?   |
                        +---------------------+        +---------------------+
                              |           |                  |           |
                            YES          NO                YES          NO
                              |           |                  |           |
                              v           |                  |           v
                    +------------------+  |                  |    +------------------+
                    | Create instance  |  |                  |    | Return existing  |
                    | Allocate memory  |  |                  |    | instance         |
                    | Initialize data  |  |                  |    +------------------+
                    +------------------+  |                  |
                              |           |                  |
                              v           v                  v
                        +------------------------------------------+
                        |           Global Instance                 |
                        |   +----------------------------------+   |
                        |   |  static Singleton* g_instance    |   |
                        |   |  - config_data                   |   |
                        |   |  - state_info                    |   |
                        |   +----------------------------------+   |
                        +------------------------------------------+
                                          ^
                                          |
                    +---------------------+---------------------+
                    |                     |                     |
              +-----------+         +-----------+         +-----------+
              | Module A  |         | Module B  |         | Module C  |
              | Uses same |         | Uses same |         | Uses same |
              | instance  |         | instance  |         | instance  |
              +-----------+         +-----------+         +-----------+
```

**中文说明：**

单例模式的核心流程：

1. **首次调用**：
   - 调用 `get_instance()` 获取实例
   - 检查全局实例是否为 NULL
   - 如果为 NULL，创建新实例并初始化
   - 返回新创建的实例

2. **后续调用**：
   - 调用 `get_instance()` 获取实例
   - 检查全局实例是否为 NULL
   - 实例已存在，直接返回现有实例

3. **全局唯一性**：
   - 所有模块（A、B、C）都访问同一个实例
   - 保证数据一致性和资源共享

---

## 3. Code Skeleton

```c
// Singleton structure definition
typedef struct {
    // Data members
} Singleton;

// Global instance pointer (static)
static Singleton* g_instance = NULL;

// Get instance function
Singleton* singleton_get_instance(void);

// Initialize function (private)
static void singleton_init(Singleton* s);

// Destroy function
void singleton_destroy(void);
```

**中文说明：**

代码骨架包含：
- `Singleton` 结构体：定义单例的数据成员
- `g_instance`：静态全局指针，存储唯一实例
- `singleton_get_instance()`：获取实例的公共接口
- `singleton_init()`：内部初始化函数
- `singleton_destroy()`：销毁实例，释放资源

---

## 4. Complete Example Code

```c
/*
 * Singleton Pattern - Configuration Manager Example
 * 
 * This example demonstrates a configuration manager that
 * should only have one instance throughout the application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ============================================
 * Singleton Structure Definition
 * ============================================ */
typedef struct {
    char   app_name[64];        /* Application name */
    int    log_level;           /* Log level: 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG */
    int    max_connections;     /* Maximum connections allowed */
    char   db_host[128];        /* Database host */
    int    db_port;             /* Database port */
    int    is_initialized;      /* Initialization flag */
} ConfigManager;

/* ============================================
 * Global Instance (Static - Private)
 * ============================================ */
static ConfigManager* g_config_instance = NULL;  /* Global singleton instance */
static pthread_mutex_t g_config_mutex = PTHREAD_MUTEX_INITIALIZER;  /* Thread safety */

/* ============================================
 * Private: Initialize Configuration
 * ============================================ */
static void config_init(ConfigManager* config)
{
    /* Set default values */
    strncpy(config->app_name, "MyApplication", sizeof(config->app_name) - 1);
    config->log_level = 2;  /* INFO level */
    config->max_connections = 100;
    strncpy(config->db_host, "localhost", sizeof(config->db_host) - 1);
    config->db_port = 5432;
    config->is_initialized = 1;
    
    printf("[ConfigManager] Initialized with default values\n");
}

/* ============================================
 * Public: Get Singleton Instance
 * This is the core of Singleton pattern
 * ============================================ */
ConfigManager* config_get_instance(void)
{
    /* Double-checked locking for thread safety */
    if (g_config_instance == NULL) {                    /* First check (without lock) */
        pthread_mutex_lock(&g_config_mutex);            /* Acquire lock */
        
        if (g_config_instance == NULL) {                /* Second check (with lock) */
            /* Allocate memory for singleton instance */
            g_config_instance = (ConfigManager*)malloc(sizeof(ConfigManager));
            
            if (g_config_instance != NULL) {
                memset(g_config_instance, 0, sizeof(ConfigManager));
                config_init(g_config_instance);         /* Initialize the instance */
            }
        }
        
        pthread_mutex_unlock(&g_config_mutex);          /* Release lock */
    }
    
    return g_config_instance;                           /* Return the singleton instance */
}

/* ============================================
 * Public: Destroy Singleton Instance
 * ============================================ */
void config_destroy(void)
{
    pthread_mutex_lock(&g_config_mutex);
    
    if (g_config_instance != NULL) {
        free(g_config_instance);
        g_config_instance = NULL;
        printf("[ConfigManager] Instance destroyed\n");
    }
    
    pthread_mutex_unlock(&g_config_mutex);
}

/* ============================================
 * Public: Setter Functions
 * ============================================ */
void config_set_log_level(int level)
{
    ConfigManager* config = config_get_instance();
    if (config != NULL) {
        config->log_level = level;
        printf("[ConfigManager] Log level set to %d\n", level);
    }
}

void config_set_max_connections(int max_conn)
{
    ConfigManager* config = config_get_instance();
    if (config != NULL) {
        config->max_connections = max_conn;
        printf("[ConfigManager] Max connections set to %d\n", max_conn);
    }
}

/* ============================================
 * Public: Getter Functions
 * ============================================ */
int config_get_log_level(void)
{
    ConfigManager* config = config_get_instance();
    return (config != NULL) ? config->log_level : -1;
}

int config_get_max_connections(void)
{
    ConfigManager* config = config_get_instance();
    return (config != NULL) ? config->max_connections : -1;
}

void config_print(void)
{
    ConfigManager* config = config_get_instance();
    if (config != NULL && config->is_initialized) {
        printf("\n=== Configuration ===\n");
        printf("App Name: %s\n", config->app_name);
        printf("Log Level: %d\n", config->log_level);
        printf("Max Connections: %d\n", config->max_connections);
        printf("DB Host: %s:%d\n", config->db_host, config->db_port);
        printf("=====================\n\n");
    }
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Singleton Pattern Demo ===\n\n");
    
    /* First access - instance will be created */
    printf("1. First access to ConfigManager:\n");
    ConfigManager* config1 = config_get_instance();
    printf("   Instance address: %p\n\n", (void*)config1);
    
    /* Second access - same instance returned */
    printf("2. Second access to ConfigManager:\n");
    ConfigManager* config2 = config_get_instance();
    printf("   Instance address: %p\n\n", (void*)config2);
    
    /* Verify both pointers point to same instance */
    printf("3. Verify singleton:\n");
    printf("   config1 == config2 ? %s\n\n", 
           (config1 == config2) ? "YES (Same instance)" : "NO (Different instances)");
    
    /* Use the configuration */
    printf("4. Print default configuration:\n");
    config_print();
    
    /* Modify configuration */
    printf("5. Modify configuration:\n");
    config_set_log_level(3);
    config_set_max_connections(200);
    
    /* Print modified configuration */
    printf("\n6. Print modified configuration:\n");
    config_print();
    
    /* Access from "another module" - still same instance */
    printf("7. Access from 'another module':\n");
    ConfigManager* config3 = config_get_instance();
    printf("   Log level from config3: %d\n", config3->log_level);
    printf("   Proves all modules share the same instance\n\n");
    
    /* Cleanup */
    config_destroy();
    
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个配置管理器单例：

1. **结构体定义**：
   - `ConfigManager` 包含应用配置数据
   - 包括日志级别、最大连接数、数据库信息等

2. **核心实现**：
   - `g_config_instance`：静态全局指针，存储唯一实例
   - `config_get_instance()`：使用双重检查锁定保证线程安全
   - 首次调用时创建实例，后续调用返回已存在的实例

3. **线程安全**：
   - 使用 `pthread_mutex_t` 互斥锁
   - 双重检查锁定避免不必要的锁竞争

4. **使用演示**：
   - 多次获取实例，验证返回同一个对象
   - 修改配置后，所有模块看到相同的修改

