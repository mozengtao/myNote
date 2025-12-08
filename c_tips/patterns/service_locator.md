# Service Locator Pattern (服务定位器模式)

## 1. Core Concept and Use Cases

### Core Concept
Provide a **centralized registry** that knows how to get hold of all the services that an application might need. It acts as a single point to **locate and retrieve services** by name or type.

### Typical Use Cases
- Plugin systems
- Middleware management
- Driver registration
- Component lookup in frameworks
- Dynamic service discovery

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                   Service Locator Pattern                                         |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |     Service Locator       |
                              |       (Registry)          |
                              +---------------------------+
                              | - services[name -> impl]  |
                              | - service_count           |
                              +---------------------------+
                              | + register(name, service) |
                              | + get(name)               |
                              | + unregister(name)        |
                              +-------------+-------------+
                                            |
                        +-------------------+-------------------+
                        |                   |                   |
                        v                   v                   v
                 +-----------+       +-----------+       +-----------+
                 | Service A |       | Service B |       | Service C |
                 | "logger"  |       | "database"|       | "cache"   |
                 +-----------+       +-----------+       +-----------+


    Registration Phase:
    
    +-------------+                  +------------------+
    | Service     |  register()      | Service Locator  |
    | Provider    |----------------->| Registry         |
    +-------------+                  +------------------+
                                     | "logger" -> LoggerService
                                     | "database" -> DbService
                                     | "cache" -> CacheService
                                     +------------------+

    Lookup Phase:
    
    +-------------+                  +------------------+                  +-------------+
    |   Client    |  get("logger")   | Service Locator  |   return         |   Logger    |
    |             |----------------->|                  |----------------->|   Service   |
    +-------------+                  +------------------+                  +-------------+
```

**中文说明：**

服务定位器模式的核心流程：

1. **注册阶段**：
   - 服务提供者向定位器注册服务
   - 每个服务有唯一的名称标识

2. **查找阶段**：
   - 客户端通过名称请求服务
   - 定位器返回对应的服务实例

3. **集中管理**：
   - 所有服务统一管理
   - 支持动态添加/移除服务

---

## 3. Code Skeleton

```c
/* Service interface */
typedef struct Service {
    char name[32];
    void* instance;
    void (*init)(struct Service* self);
    void (*destroy)(struct Service* self);
} Service;

/* Service locator */
typedef struct {
    Service* services[MAX_SERVICES];
    int service_count;
} ServiceLocator;

/* Locator operations */
void locator_register(ServiceLocator* loc, Service* service);
Service* locator_get(ServiceLocator* loc, const char* name);
void locator_unregister(ServiceLocator* loc, const char* name);
```

**中文说明：**

代码骨架包含：
- `Service`：服务接口，包含名称和实例
- `ServiceLocator`：服务定位器，管理所有服务
- 核心操作：`register`、`get`、`unregister`

---

## 4. Complete Example Code

```c
/*
 * Service Locator Pattern - Application Services Example
 * 
 * This example demonstrates a service locator that manages
 * various application services like logging, caching, and database.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SERVICES 20
#define MAX_NAME_LEN 32

/* ============================================
 * Service Interface
 * ============================================ */
typedef struct Service Service;

typedef void (*service_init_fn)(Service* self);
typedef void (*service_destroy_fn)(Service* self);
typedef void* (*service_get_instance_fn)(Service* self);

struct Service {
    char name[MAX_NAME_LEN];              /* Service name (key) */
    void* instance;                       /* Service instance */
    service_init_fn init;                 /* Initialization function */
    service_destroy_fn destroy;           /* Cleanup function */
    service_get_instance_fn get_instance; /* Get instance function */
    int is_initialized;                   /* Initialization state */
};

/* ============================================
 * Service Locator (Registry)
 * ============================================ */
typedef struct {
    Service* services[MAX_SERVICES];      /* Registered services */
    int service_count;                    /* Number of services */
} ServiceLocator;

/* Global service locator instance (singleton) */
static ServiceLocator* g_locator = NULL;

/* Initialize the service locator */
ServiceLocator* locator_create(void)
{
    ServiceLocator* loc = (ServiceLocator*)malloc(sizeof(ServiceLocator));
    if (loc) {
        memset(loc->services, 0, sizeof(loc->services));
        loc->service_count = 0;
        printf("[Locator] Service locator created\n");
    }
    return loc;
}

/* Get global locator instance */
ServiceLocator* locator_get_instance(void)
{
    if (g_locator == NULL) {
        g_locator = locator_create();
    }
    return g_locator;
}

/* Register a service */
int locator_register(ServiceLocator* loc, Service* service)
{
    if (loc == NULL || service == NULL) return -1;
    
    /* Check for duplicate */
    for (int i = 0; i < loc->service_count; i++) {
        if (strcmp(loc->services[i]->name, service->name) == 0) {
            printf("[Locator] ERROR: Service '%s' already registered\n", service->name);
            return -1;
        }
    }
    
    if (loc->service_count >= MAX_SERVICES) {
        printf("[Locator] ERROR: Maximum services reached\n");
        return -1;
    }
    
    loc->services[loc->service_count++] = service;
    printf("[Locator] Registered service: '%s'\n", service->name);
    return 0;
}

/* Get a service by name */
Service* locator_get(ServiceLocator* loc, const char* name)
{
    if (loc == NULL || name == NULL) return NULL;
    
    for (int i = 0; i < loc->service_count; i++) {
        if (strcmp(loc->services[i]->name, name) == 0) {
            Service* svc = loc->services[i];
            
            /* Lazy initialization */
            if (!svc->is_initialized && svc->init != NULL) {
                printf("[Locator] Initializing service: '%s'\n", name);
                svc->init(svc);
                svc->is_initialized = 1;
            }
            
            return svc;
        }
    }
    
    printf("[Locator] Service not found: '%s'\n", name);
    return NULL;
}

/* Unregister a service */
int locator_unregister(ServiceLocator* loc, const char* name)
{
    if (loc == NULL || name == NULL) return -1;
    
    for (int i = 0; i < loc->service_count; i++) {
        if (strcmp(loc->services[i]->name, name) == 0) {
            Service* svc = loc->services[i];
            
            /* Destroy if initialized */
            if (svc->is_initialized && svc->destroy != NULL) {
                svc->destroy(svc);
            }
            
            /* Remove from array */
            for (int j = i; j < loc->service_count - 1; j++) {
                loc->services[j] = loc->services[j + 1];
            }
            loc->service_count--;
            
            printf("[Locator] Unregistered service: '%s'\n", name);
            return 0;
        }
    }
    return -1;
}

/* List all services */
void locator_list(ServiceLocator* loc)
{
    printf("\n=== Registered Services ===\n");
    for (int i = 0; i < loc->service_count; i++) {
        printf("  [%d] %s (initialized: %s)\n", 
               i, loc->services[i]->name,
               loc->services[i]->is_initialized ? "yes" : "no");
    }
    printf("===========================\n\n");
}

/* Destroy locator and all services */
void locator_destroy(ServiceLocator* loc)
{
    if (loc == NULL) return;
    
    printf("[Locator] Destroying all services...\n");
    for (int i = 0; i < loc->service_count; i++) {
        if (loc->services[i]->is_initialized && loc->services[i]->destroy) {
            loc->services[i]->destroy(loc->services[i]);
        }
        free(loc->services[i]);
    }
    free(loc);
    
    if (loc == g_locator) {
        g_locator = NULL;
    }
}

/* ============================================
 * Concrete Service 1: Logger Service
 * ============================================ */
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

typedef struct {
    LogLevel level;
    int log_count;
} LoggerInstance;

void logger_init(Service* self)
{
    LoggerInstance* logger = (LoggerInstance*)malloc(sizeof(LoggerInstance));
    logger->level = LOG_INFO;
    logger->log_count = 0;
    self->instance = logger;
    printf("    [Logger] Initialized with level INFO\n");
}

void logger_destroy(Service* self)
{
    LoggerInstance* logger = (LoggerInstance*)self->instance;
    printf("    [Logger] Destroyed (logged %d messages)\n", logger->log_count);
    free(logger);
}

void logger_log(Service* self, LogLevel level, const char* message)
{
    LoggerInstance* logger = (LoggerInstance*)self->instance;
    if (level >= logger->level) {
        const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        printf("    [LOG %s] %s\n", level_str[level], message);
        logger->log_count++;
    }
}

Service* create_logger_service(void)
{
    Service* svc = (Service*)malloc(sizeof(Service));
    strncpy(svc->name, "logger", MAX_NAME_LEN - 1);
    svc->instance = NULL;
    svc->init = logger_init;
    svc->destroy = logger_destroy;
    svc->is_initialized = 0;
    return svc;
}

/* ============================================
 * Concrete Service 2: Cache Service
 * ============================================ */
#define CACHE_SIZE 10

typedef struct {
    char keys[CACHE_SIZE][32];
    char values[CACHE_SIZE][256];
    int count;
} CacheInstance;

void cache_init(Service* self)
{
    CacheInstance* cache = (CacheInstance*)malloc(sizeof(CacheInstance));
    cache->count = 0;
    self->instance = cache;
    printf("    [Cache] Initialized with capacity %d\n", CACHE_SIZE);
}

void cache_destroy(Service* self)
{
    CacheInstance* cache = (CacheInstance*)self->instance;
    printf("    [Cache] Destroyed (had %d entries)\n", cache->count);
    free(cache);
}

void cache_put(Service* self, const char* key, const char* value)
{
    CacheInstance* cache = (CacheInstance*)self->instance;
    if (cache->count < CACHE_SIZE) {
        strncpy(cache->keys[cache->count], key, 31);
        strncpy(cache->values[cache->count], value, 255);
        cache->count++;
        printf("    [Cache] PUT '%s' = '%s'\n", key, value);
    }
}

const char* cache_get(Service* self, const char* key)
{
    CacheInstance* cache = (CacheInstance*)self->instance;
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->keys[i], key) == 0) {
            printf("    [Cache] GET '%s' = '%s' (HIT)\n", key, cache->values[i]);
            return cache->values[i];
        }
    }
    printf("    [Cache] GET '%s' (MISS)\n", key);
    return NULL;
}

Service* create_cache_service(void)
{
    Service* svc = (Service*)malloc(sizeof(Service));
    strncpy(svc->name, "cache", MAX_NAME_LEN - 1);
    svc->instance = NULL;
    svc->init = cache_init;
    svc->destroy = cache_destroy;
    svc->is_initialized = 0;
    return svc;
}

/* ============================================
 * Concrete Service 3: Database Service
 * ============================================ */
typedef struct {
    char connection_string[128];
    int is_connected;
    int query_count;
} DbInstance;

void db_init(Service* self)
{
    DbInstance* db = (DbInstance*)malloc(sizeof(DbInstance));
    strncpy(db->connection_string, "postgres://localhost:5432/mydb", 127);
    db->is_connected = 1;
    db->query_count = 0;
    self->instance = db;
    printf("    [Database] Connected to %s\n", db->connection_string);
}

void db_destroy(Service* self)
{
    DbInstance* db = (DbInstance*)self->instance;
    printf("    [Database] Disconnected (executed %d queries)\n", db->query_count);
    free(db);
}

int db_execute(Service* self, const char* query)
{
    DbInstance* db = (DbInstance*)self->instance;
    if (db->is_connected) {
        db->query_count++;
        printf("    [Database] Execute: %s\n", query);
        return 0;
    }
    return -1;
}

Service* create_database_service(void)
{
    Service* svc = (Service*)malloc(sizeof(Service));
    strncpy(svc->name, "database", MAX_NAME_LEN - 1);
    svc->instance = NULL;
    svc->init = db_init;
    svc->destroy = db_destroy;
    svc->is_initialized = 0;
    return svc;
}

/* ============================================
 * Client Code - Using Service Locator
 * ============================================ */
void application_logic(ServiceLocator* locator)
{
    printf("\n=== Application Logic ===\n\n");
    
    /* Get logger service */
    Service* logger = locator_get(locator, "logger");
    if (logger) {
        logger_log(logger, LOG_INFO, "Application started");
    }
    
    /* Get cache service */
    Service* cache = locator_get(locator, "cache");
    if (cache) {
        cache_put(cache, "user:1", "Alice");
        cache_put(cache, "user:2", "Bob");
        cache_get(cache, "user:1");
        cache_get(cache, "user:3");  /* Miss */
    }
    
    /* Get database service */
    Service* db = locator_get(locator, "database");
    if (db) {
        db_execute(db, "SELECT * FROM users");
        db_execute(db, "INSERT INTO logs VALUES ('event')");
    }
    
    /* Log completion */
    if (logger) {
        logger_log(logger, LOG_INFO, "Application logic completed");
    }
    
    printf("\n=========================\n");
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Service Locator Pattern Demo ===\n\n");
    
    /* Create service locator */
    ServiceLocator* locator = locator_get_instance();
    
    /* Register services */
    printf("--- Registering Services ---\n");
    locator_register(locator, create_logger_service());
    locator_register(locator, create_cache_service());
    locator_register(locator, create_database_service());
    
    locator_list(locator);
    
    /* Run application (services initialized on first use) */
    application_logic(locator);
    
    locator_list(locator);
    
    /* Try to register duplicate */
    printf("\n--- Try Duplicate Registration ---\n");
    locator_register(locator, create_logger_service());
    
    /* Try to get non-existent service */
    printf("\n--- Try Non-existent Service ---\n");
    locator_get(locator, "email");
    
    /* Unregister a service */
    printf("\n--- Unregister Cache Service ---\n");
    locator_unregister(locator, "cache");
    locator_list(locator);
    
    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    locator_destroy(locator);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了应用服务定位器：

1. **服务定位器（ServiceLocator）**：
   - 管理所有注册的服务
   - `register()`：注册服务
   - `get()`：按名称查找服务（懒初始化）
   - `unregister()`：注销服务

2. **具体服务**：
   - **LoggerService**：日志服务，支持不同级别
   - **CacheService**：缓存服务，key-value 存储
   - **DatabaseService**：数据库服务，模拟查询

3. **懒初始化**：
   - 服务首次获取时才初始化
   - 避免不必要的资源分配

4. **全局单例**：
   - `locator_get_instance()` 返回全局唯一实例
   - 应用任何地方都可访问

