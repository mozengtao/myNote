# Dependency Injection Pattern (依赖注入模式)

## 1. Core Concept and Use Cases

### Core Concept
A technique where an object **receives its dependencies from external sources** rather than creating them internally. The dependencies are "injected" into the object, promoting loose coupling and easier testing.

### Typical Use Cases
- Unit testing with mock objects
- Plugin architectures
- Configuration-based behavior
- Inversion of Control (IoC) containers
- Modular system design

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                 Dependency Injection Pattern                                      |
+--------------------------------------------------------------------------------------------------+

    WITHOUT Dependency Injection:
    
    +------------------+                  +------------------+
    |    Consumer      |    creates       |   Dependency     |
    +------------------+----------------->+------------------+
    | - dependency     |                  |                  |
    | Consumer() {     |                  |                  |
    |   dep = new Dep()|                  |                  |
    | }                |                  |                  |
    +------------------+                  +------------------+
    
    Problem: Consumer is tightly coupled to concrete Dependency


    WITH Dependency Injection:
    
    +------------------+                  +------------------+
    |    Injector      |    creates       |   Dependency     |
    | (Factory/Config) |----------------->+------------------+
    +--------+---------+                  
             |
             | injects
             v
    +------------------+
    |    Consumer      |
    +------------------+
    | - dependency     |  <-- Received from outside
    | Consumer(dep) {  |
    |   this.dep = dep |
    | }                |
    +------------------+
    
    Benefit: Consumer only depends on interface, not implementation


    Injection Types:
    
    1. Constructor Injection:
       consumer = Consumer(dependency)
       
    2. Setter Injection:
       consumer.set_dependency(dependency)
       
    3. Interface Injection:
       consumer.inject(dependency)
```

**中文说明：**

依赖注入模式的核心流程：

1. **无依赖注入**：
   - 消费者内部创建依赖
   - 紧耦合，难以测试和替换

2. **有依赖注入**：
   - 依赖由外部创建
   - 通过构造函数、setter 或接口注入
   - 松耦合，易于测试和替换

3. **注入方式**：
   - **构造函数注入**：创建时传入依赖
   - **Setter 注入**：通过方法设置依赖
   - **接口注入**：实现注入接口

---

## 3. Code Skeleton

```c
/* Dependency interface */
typedef struct Dependency {
    void (*operation)(struct Dependency* self);
    void* impl;
} Dependency;

/* Consumer that receives dependency */
typedef struct {
    Dependency* dep;  /* Injected dependency */
    void* data;
} Consumer;

/* Constructor injection */
Consumer* consumer_create(Dependency* dep);

/* Setter injection */
void consumer_set_dependency(Consumer* c, Dependency* dep);

/* Factory that creates and wires dependencies */
Consumer* factory_create_consumer(const char* config);
```

**中文说明：**

代码骨架包含：
- `Dependency`：依赖接口
- `Consumer`：消费者，接收注入的依赖
- 构造函数注入和 setter 注入
- 工厂负责创建和组装

---

## 4. Complete Example Code

```c
/*
 * Dependency Injection Pattern - Logger and Data Service Example
 * 
 * This example demonstrates how to inject dependencies
 * into a service, enabling easy testing and configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================
 * Dependency Interface 1: Logger
 * ============================================ */
typedef struct Logger Logger;

typedef void (*log_fn)(Logger* self, const char* level, const char* message);
typedef void (*close_fn)(Logger* self);

struct Logger {
    char name[32];
    log_fn log;
    close_fn close;
    void* config;
};

/* Concrete Logger 1: Console Logger */
void console_log(Logger* self, const char* level, const char* message)
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    printf("[%s][%s][%s] %s\n", time_str, self->name, level, message);
}

void console_close(Logger* self)
{
    printf("[%s] Console logger closed\n", self->name);
    free(self);
}

Logger* create_console_logger(void)
{
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    strncpy(logger->name, "CONSOLE", sizeof(logger->name) - 1);
    logger->log = console_log;
    logger->close = console_close;
    logger->config = NULL;
    return logger;
}

/* Concrete Logger 2: File Logger (simulated) */
typedef struct {
    char filename[64];
    int log_count;
} FileLoggerConfig;

void file_log(Logger* self, const char* level, const char* message)
{
    FileLoggerConfig* cfg = (FileLoggerConfig*)self->config;
    cfg->log_count++;
    printf("[FILE:%s] #%d [%s] %s\n", cfg->filename, cfg->log_count, level, message);
}

void file_close(Logger* self)
{
    FileLoggerConfig* cfg = (FileLoggerConfig*)self->config;
    printf("[%s] File logger closed (wrote %d entries to %s)\n",
           self->name, cfg->log_count, cfg->filename);
    free(cfg);
    free(self);
}

Logger* create_file_logger(const char* filename)
{
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    FileLoggerConfig* cfg = (FileLoggerConfig*)malloc(sizeof(FileLoggerConfig));
    
    strncpy(logger->name, "FILE", sizeof(logger->name) - 1);
    logger->log = file_log;
    logger->close = file_close;
    
    strncpy(cfg->filename, filename, sizeof(cfg->filename) - 1);
    cfg->log_count = 0;
    logger->config = cfg;
    
    return logger;
}

/* Concrete Logger 3: Mock Logger (for testing) */
typedef struct {
    char last_level[16];
    char last_message[256];
    int call_count;
} MockLoggerData;

void mock_log(Logger* self, const char* level, const char* message)
{
    MockLoggerData* data = (MockLoggerData*)self->config;
    strncpy(data->last_level, level, sizeof(data->last_level) - 1);
    strncpy(data->last_message, message, sizeof(data->last_message) - 1);
    data->call_count++;
    /* Silent - doesn't actually output anything */
}

void mock_close(Logger* self)
{
    free(self->config);
    free(self);
}

Logger* create_mock_logger(void)
{
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    MockLoggerData* data = (MockLoggerData*)malloc(sizeof(MockLoggerData));
    
    strncpy(logger->name, "MOCK", sizeof(logger->name) - 1);
    logger->log = mock_log;
    logger->close = mock_close;
    
    memset(data, 0, sizeof(MockLoggerData));
    logger->config = data;
    
    return logger;
}

/* Helper to verify mock logger (for testing) */
int mock_logger_verify(Logger* logger, const char* expected_level, const char* expected_msg)
{
    MockLoggerData* data = (MockLoggerData*)logger->config;
    int level_match = strcmp(data->last_level, expected_level) == 0;
    int msg_match = strstr(data->last_message, expected_msg) != NULL;
    return level_match && msg_match;
}

int mock_logger_get_call_count(Logger* logger)
{
    MockLoggerData* data = (MockLoggerData*)logger->config;
    return data->call_count;
}

/* ============================================
 * Dependency Interface 2: Data Repository
 * ============================================ */
typedef struct Repository Repository;

typedef int (*repo_save_fn)(Repository* self, const char* key, const char* value);
typedef const char* (*repo_get_fn)(Repository* self, const char* key);
typedef void (*repo_close_fn)(Repository* self);

struct Repository {
    char name[32];
    repo_save_fn save;
    repo_get_fn get;
    repo_close_fn close;
    void* storage;
};

/* Concrete Repository 1: In-Memory Repository */
#define MAX_ENTRIES 100

typedef struct {
    char keys[MAX_ENTRIES][32];
    char values[MAX_ENTRIES][256];
    int count;
} MemoryStorage;

int memory_save(Repository* self, const char* key, const char* value)
{
    MemoryStorage* storage = (MemoryStorage*)self->storage;
    
    /* Check for existing key */
    for (int i = 0; i < storage->count; i++) {
        if (strcmp(storage->keys[i], key) == 0) {
            strncpy(storage->values[i], value, 255);
            return 0;
        }
    }
    
    /* Add new entry */
    if (storage->count < MAX_ENTRIES) {
        strncpy(storage->keys[storage->count], key, 31);
        strncpy(storage->values[storage->count], value, 255);
        storage->count++;
        return 0;
    }
    return -1;
}

const char* memory_get(Repository* self, const char* key)
{
    MemoryStorage* storage = (MemoryStorage*)self->storage;
    
    for (int i = 0; i < storage->count; i++) {
        if (strcmp(storage->keys[i], key) == 0) {
            return storage->values[i];
        }
    }
    return NULL;
}

void memory_close(Repository* self)
{
    MemoryStorage* storage = (MemoryStorage*)self->storage;
    printf("[%s] Repository closed (%d entries)\n", self->name, storage->count);
    free(storage);
    free(self);
}

Repository* create_memory_repository(void)
{
    Repository* repo = (Repository*)malloc(sizeof(Repository));
    MemoryStorage* storage = (MemoryStorage*)malloc(sizeof(MemoryStorage));
    
    strncpy(repo->name, "MEMORY", sizeof(repo->name) - 1);
    repo->save = memory_save;
    repo->get = memory_get;
    repo->close = memory_close;
    
    memset(storage, 0, sizeof(MemoryStorage));
    repo->storage = storage;
    
    return repo;
}

/* ============================================
 * Consumer: User Service
 * This class receives its dependencies via injection
 * ============================================ */
typedef struct {
    Logger* logger;           /* Injected dependency */
    Repository* repository;   /* Injected dependency */
    int operation_count;
} UserService;

/* Constructor Injection - dependencies passed at creation */
UserService* userservice_create(Logger* logger, Repository* repo)
{
    UserService* service = (UserService*)malloc(sizeof(UserService));
    
    /* Store injected dependencies */
    service->logger = logger;
    service->repository = repo;
    service->operation_count = 0;
    
    service->logger->log(service->logger, "INFO", "UserService created with injected dependencies");
    return service;
}

/* Setter Injection - dependencies can be changed later */
void userservice_set_logger(UserService* service, Logger* logger)
{
    service->logger = logger;
}

void userservice_set_repository(UserService* service, Repository* repo)
{
    service->repository = repo;
}

/* Business logic - uses injected dependencies */
int userservice_create_user(UserService* service, const char* username, const char* email)
{
    char log_msg[256];
    char key[64];
    
    service->operation_count++;
    
    snprintf(log_msg, sizeof(log_msg), "Creating user: %s", username);
    service->logger->log(service->logger, "INFO", log_msg);
    
    /* Save to repository */
    snprintf(key, sizeof(key), "user:%s:email", username);
    if (service->repository->save(service->repository, key, email) != 0) {
        service->logger->log(service->logger, "ERROR", "Failed to save user");
        return -1;
    }
    
    snprintf(log_msg, sizeof(log_msg), "User %s created successfully", username);
    service->logger->log(service->logger, "INFO", log_msg);
    return 0;
}

const char* userservice_get_email(UserService* service, const char* username)
{
    char key[64];
    char log_msg[128];
    
    snprintf(key, sizeof(key), "user:%s:email", username);
    const char* email = service->repository->get(service->repository, key);
    
    if (email) {
        snprintf(log_msg, sizeof(log_msg), "Found email for user %s", username);
        service->logger->log(service->logger, "DEBUG", log_msg);
    } else {
        snprintf(log_msg, sizeof(log_msg), "User %s not found", username);
        service->logger->log(service->logger, "WARN", log_msg);
    }
    
    return email;
}

void userservice_destroy(UserService* service)
{
    service->logger->log(service->logger, "INFO", "UserService destroyed");
    free(service);
}

/* ============================================
 * Dependency Injection Container / Factory
 * ============================================ */
typedef struct {
    const char* logger_type;    /* "console", "file", "mock" */
    const char* repo_type;      /* "memory" */
    const char* log_file;       /* For file logger */
} DIConfig;

UserService* di_container_create_service(DIConfig* config)
{
    Logger* logger = NULL;
    Repository* repo = NULL;
    
    printf("\n[DI Container] Creating service with config:\n");
    printf("  Logger: %s\n", config->logger_type);
    printf("  Repository: %s\n", config->repo_type);
    
    /* Create logger based on configuration */
    if (strcmp(config->logger_type, "console") == 0) {
        logger = create_console_logger();
    } else if (strcmp(config->logger_type, "file") == 0) {
        logger = create_file_logger(config->log_file);
    } else if (strcmp(config->logger_type, "mock") == 0) {
        logger = create_mock_logger();
    }
    
    /* Create repository based on configuration */
    if (strcmp(config->repo_type, "memory") == 0) {
        repo = create_memory_repository();
    }
    
    /* Inject dependencies and create service */
    return userservice_create(logger, repo);
}

/* ============================================
 * Unit Test Example (using Mock dependencies)
 * ============================================ */
void run_unit_tests(void)
{
    printf("\n===== UNIT TESTS =====\n\n");
    
    /* Create mock dependencies for testing */
    Logger* mock_logger = create_mock_logger();
    Repository* mock_repo = create_memory_repository();
    
    /* Inject mocks into service */
    UserService* service = userservice_create(mock_logger, mock_repo);
    
    /* Test 1: Create user */
    printf("Test 1: Create user\n");
    int result = userservice_create_user(service, "testuser", "test@example.com");
    
    if (result == 0) {
        printf("  PASS: User created successfully\n");
    } else {
        printf("  FAIL: User creation failed\n");
    }
    
    /* Verify mock logger was called */
    int call_count = mock_logger_get_call_count(mock_logger);
    printf("  Logger was called %d times\n", call_count);
    
    /* Test 2: Get user email */
    printf("\nTest 2: Get user email\n");
    const char* email = userservice_get_email(service, "testuser");
    
    if (email && strcmp(email, "test@example.com") == 0) {
        printf("  PASS: Email retrieved correctly: %s\n", email);
    } else {
        printf("  FAIL: Email retrieval failed\n");
    }
    
    /* Test 3: Get non-existent user */
    printf("\nTest 3: Get non-existent user\n");
    email = userservice_get_email(service, "nobody");
    
    if (email == NULL) {
        printf("  PASS: Correctly returned NULL for non-existent user\n");
    } else {
        printf("  FAIL: Should have returned NULL\n");
    }
    
    /* Cleanup */
    userservice_destroy(service);
    mock_logger->close(mock_logger);
    mock_repo->close(mock_repo);
    
    printf("\n===== TESTS COMPLETE =====\n");
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Dependency Injection Pattern Demo ===\n");
    
    /* Production configuration */
    printf("\n--- Production Mode (Console Logger) ---\n");
    DIConfig prod_config = {
        .logger_type = "console",
        .repo_type = "memory",
        .log_file = NULL
    };
    
    UserService* prod_service = di_container_create_service(&prod_config);
    
    userservice_create_user(prod_service, "alice", "alice@example.com");
    userservice_create_user(prod_service, "bob", "bob@example.com");
    
    printf("\nLooking up users:\n");
    printf("Alice's email: %s\n", userservice_get_email(prod_service, "alice"));
    printf("Bob's email: %s\n", userservice_get_email(prod_service, "bob"));
    
    /* Cleanup production service */
    Logger* prod_logger = prod_service->logger;
    Repository* prod_repo = prod_service->repository;
    userservice_destroy(prod_service);
    prod_logger->close(prod_logger);
    prod_repo->close(prod_repo);
    
    /* File logging configuration */
    printf("\n--- File Logging Mode ---\n");
    DIConfig file_config = {
        .logger_type = "file",
        .repo_type = "memory",
        .log_file = "/var/log/app.log"
    };
    
    UserService* file_service = di_container_create_service(&file_config);
    userservice_create_user(file_service, "charlie", "charlie@example.com");
    
    Logger* file_logger = file_service->logger;
    Repository* file_repo = file_service->repository;
    userservice_destroy(file_service);
    file_logger->close(file_logger);
    file_repo->close(file_repo);
    
    /* Run unit tests with mock dependencies */
    run_unit_tests();
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了依赖注入系统：

1. **依赖接口**：
   - **Logger**：日志接口，有 Console、File、Mock 三种实现
   - **Repository**：数据存储接口，有 Memory 实现

2. **消费者（UserService）**：
   - 通过构造函数接收 Logger 和 Repository 依赖
   - 不创建自己的依赖，完全依赖注入
   - 业务逻辑使用注入的依赖

3. **依赖注入容器**：
   - `di_container_create_service()`：根据配置创建和注入依赖
   - 支持不同环境使用不同实现

4. **单元测试**：
   - 使用 Mock Logger 进行测试
   - 可以验证方法调用次数
   - 不会产生实际输出

5. **核心优势**：
   - **可测试性**：可以注入 Mock 对象
   - **灵活性**：可以通过配置切换实现
   - **松耦合**：消费者只依赖接口

