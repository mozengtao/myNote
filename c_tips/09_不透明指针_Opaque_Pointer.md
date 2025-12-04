# 不透明指针 (Opaque Pointer)

## 定义

不透明指针是一种信息隐藏技术，通过在头文件中只声明结构体类型名称而不定义其内容，将结构体的实现细节完全隐藏在源文件中。外部代码只能通过指针和API函数访问数据，无法直接访问结构体成员。

## 适用场景

- 库的API设计，隐藏内部实现细节
- 需要保持ABI（应用二进制接口）稳定性的场景
- 封装第三方库，提供简洁的接口
- 驱动程序和硬件抽象层
- 需要严格控制数据访问权限的模块
- 大型项目中减少编译依赖

## ASCII 图解

```
+------------------------------------------------------------------------+
|                      OPAQUE POINTER PATTERN                             |
+------------------------------------------------------------------------+
|                                                                         |
|   PUBLIC HEADER FILE (database.h)                                       |
|   +-------------------------------------------+                         |
|   |  // Only type declaration, no definition  |                         |
|   |  typedef struct Database Database;        |                         |
|   |                                           |                         |
|   |  // Public API functions                  |                         |
|   |  Database* db_create(const char* path);   |                         |
|   |  int db_query(Database* db, ...);         |                         |
|   |  void db_destroy(Database* db);           |                         |
|   +-------------------------------------------+                         |
|                         |                                               |
|                         | Clients include this                          |
|                         v                                               |
|   +-------------------+   +-------------------+   +-------------------+ |
|   |    Client A       |   |    Client B       |   |    Client C       | |
|   |  Database* db;    |   |  Database* db;    |   |  Database* db;    | |
|   |  db = db_create();|   |  db_query(db,...);|   |  db_destroy(db);  | |
|   +-------------------+   +-------------------+   +-------------------+ |
|           |                       |                       |             |
|           | Cannot access db->xxx (compile error!)        |             |
|           +---------------------------+-------------------+             |
|                                       |                                 |
+------------------------------------------------------------------------+
|                                                                         |
|   PRIVATE SOURCE FILE (database.c)                                      |
|   +-------------------------------------------+                         |
|   |  struct Database {                        |                         |
|   |      char connection_string[256];         |  <-- Hidden from        |
|   |      void* internal_handle;               |      clients            |
|   |      int is_connected;                    |                         |
|   |      int query_count;                     |                         |
|   |      char last_error[512];                |                         |
|   |  };                                       |                         |
|   |                                           |                         |
|   |  Database* db_create(...) { ... }         |                         |
|   |  int db_query(...) { ... }                |                         |
|   |  void db_destroy(...) { ... }             |                         |
|   +-------------------------------------------+                         |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   BENEFIT: ABI Stability                                                |
|                                                                         |
|   Version 1.0              Version 2.0 (struct changed)                |
|   +-----------------+      +-----------------+                          |
|   | struct Database |      | struct Database |                          |
|   | - field_a       |      | - field_a       |                          |
|   | - field_b       |      | - field_b       |                          |
|   +-----------------+      | - field_c (NEW) |                          |
|                            | - field_d (NEW) |                          |
|                            +-----------------+                          |
|                                                                         |
|   Client code does NOT need recompilation!                              |
|   Only re-link with new library.                                        |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了不透明指针的核心机制。公开头文件只包含类型声明（`typedef struct Database Database;`），不包含结构体定义，因此客户端代码只能看到指针类型。所有结构体成员的定义都在私有的`.c`源文件中，客户端无法直接访问`db->xxx`，必须通过API函数操作。这种设计的最大优势是ABI稳定性：即使库的内部结构发生变化（如添加新字段），客户端代码也不需要重新编译，只需重新链接即可。

## 实现方法

1. 在头文件中使用前向声明：`typedef struct TypeName TypeName;`
2. 在源文件中定义完整的结构体
3. 所有对结构体的操作都通过函数API进行
4. 使用malloc/free在堆上创建和销毁对象

## C语言代码示例

### 头文件（公开接口）

```c
// config_manager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stddef.h>

// 不透明类型声明 - 外部只知道这是一个类型，不知道内部结构
typedef struct ConfigManager ConfigManager;

// 配置值类型
typedef enum {
    CONFIG_TYPE_INT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_BOOL
} ConfigType;

// 公开API
ConfigManager* config_create(void);
int config_load_file(ConfigManager* cfg, const char* filepath);
int config_save_file(ConfigManager* cfg, const char* filepath);

// 获取配置值
int config_get_int(ConfigManager* cfg, const char* key, int default_value);
double config_get_float(ConfigManager* cfg, const char* key, double default_value);
const char* config_get_string(ConfigManager* cfg, const char* key, const char* default_value);
int config_get_bool(ConfigManager* cfg, const char* key, int default_value);

// 设置配置值
int config_set_int(ConfigManager* cfg, const char* key, int value);
int config_set_float(ConfigManager* cfg, const char* key, double value);
int config_set_string(ConfigManager* cfg, const char* key, const char* value);
int config_set_bool(ConfigManager* cfg, const char* key, int value);

// 查询
int config_has_key(ConfigManager* cfg, const char* key);
size_t config_count(ConfigManager* cfg);
void config_print_all(ConfigManager* cfg);

// 销毁
void config_destroy(ConfigManager* cfg);

#endif // CONFIG_MANAGER_H
```

### 源文件（私有实现）

```c
// config_manager.c
#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CONFIGS 256
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 256

// 私有结构体定义 - 完全对外部隐藏
typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    ConfigType type;
} ConfigEntry;

// 主结构体的完整定义
struct ConfigManager {
    ConfigEntry entries[MAX_CONFIGS];
    size_t count;
    char filepath[256];
    int is_modified;
    
    // 内部统计（可随时添加，不影响客户端）
    int read_count;
    int write_count;
};

// ==================== 内部辅助函数 ====================

static ConfigEntry* find_entry(ConfigManager* cfg, const char* key) {
    for (size_t i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            return &cfg->entries[i];
        }
    }
    return NULL;
}

static ConfigEntry* create_entry(ConfigManager* cfg, const char* key) {
    if (cfg->count >= MAX_CONFIGS) {
        return NULL;
    }
    
    ConfigEntry* entry = &cfg->entries[cfg->count++];
    strncpy(entry->key, key, MAX_KEY_LEN - 1);
    entry->key[MAX_KEY_LEN - 1] = '\0';
    entry->value[0] = '\0';
    
    return entry;
}

// ==================== 公开API实现 ====================

ConfigManager* config_create(void) {
    ConfigManager* cfg = (ConfigManager*)calloc(1, sizeof(ConfigManager));
    if (cfg) {
        cfg->count = 0;
        cfg->is_modified = 0;
        cfg->read_count = 0;
        cfg->write_count = 0;
        printf("[ConfigManager] Created new instance\n");
    }
    return cfg;
}

int config_load_file(ConfigManager* cfg, const char* filepath) {
    if (!cfg || !filepath) return -1;
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("[ConfigManager] Cannot open file: %s\n", filepath);
        return -1;
    }
    
    strncpy(cfg->filepath, filepath, sizeof(cfg->filepath) - 1);
    cfg->count = 0;
    
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char key[MAX_KEY_LEN];
        char value[MAX_VALUE_LEN];
        
        if (sscanf(line, "%63[^=]=%255[^\n]", key, value) == 2) {
            // 去除首尾空格
            char* k = key;
            while (*k == ' ') k++;
            char* v = value;
            while (*v == ' ') v++;
            
            ConfigEntry* entry = create_entry(cfg, k);
            if (entry) {
                strncpy(entry->value, v, MAX_VALUE_LEN - 1);
                entry->type = CONFIG_TYPE_STRING;
            }
        }
    }
    
    fclose(file);
    printf("[ConfigManager] Loaded %zu entries from %s\n", cfg->count, filepath);
    return 0;
}

int config_save_file(ConfigManager* cfg, const char* filepath) {
    if (!cfg) return -1;
    
    const char* path = filepath ? filepath : cfg->filepath;
    if (!path[0]) return -1;
    
    FILE* file = fopen(path, "w");
    if (!file) return -1;
    
    fprintf(file, "# Configuration file\n");
    fprintf(file, "# Generated by ConfigManager\n\n");
    
    for (size_t i = 0; i < cfg->count; i++) {
        fprintf(file, "%s=%s\n", cfg->entries[i].key, cfg->entries[i].value);
    }
    
    fclose(file);
    cfg->is_modified = 0;
    printf("[ConfigManager] Saved %zu entries to %s\n", cfg->count, path);
    return 0;
}

int config_get_int(ConfigManager* cfg, const char* key, int default_value) {
    if (!cfg || !key) return default_value;
    
    cfg->read_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) return default_value;
    
    return atoi(entry->value);
}

double config_get_float(ConfigManager* cfg, const char* key, double default_value) {
    if (!cfg || !key) return default_value;
    
    cfg->read_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) return default_value;
    
    return atof(entry->value);
}

const char* config_get_string(ConfigManager* cfg, const char* key, const char* default_value) {
    if (!cfg || !key) return default_value;
    
    cfg->read_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) return default_value;
    
    return entry->value;
}

int config_get_bool(ConfigManager* cfg, const char* key, int default_value) {
    if (!cfg || !key) return default_value;
    
    cfg->read_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) return default_value;
    
    return (strcmp(entry->value, "true") == 0 || 
            strcmp(entry->value, "1") == 0 ||
            strcmp(entry->value, "yes") == 0);
}

int config_set_int(ConfigManager* cfg, const char* key, int value) {
    if (!cfg || !key) return -1;
    
    cfg->write_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) entry = create_entry(cfg, key);
    if (!entry) return -1;
    
    snprintf(entry->value, MAX_VALUE_LEN, "%d", value);
    entry->type = CONFIG_TYPE_INT;
    cfg->is_modified = 1;
    return 0;
}

int config_set_float(ConfigManager* cfg, const char* key, double value) {
    if (!cfg || !key) return -1;
    
    cfg->write_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) entry = create_entry(cfg, key);
    if (!entry) return -1;
    
    snprintf(entry->value, MAX_VALUE_LEN, "%f", value);
    entry->type = CONFIG_TYPE_FLOAT;
    cfg->is_modified = 1;
    return 0;
}

int config_set_string(ConfigManager* cfg, const char* key, const char* value) {
    if (!cfg || !key || !value) return -1;
    
    cfg->write_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) entry = create_entry(cfg, key);
    if (!entry) return -1;
    
    strncpy(entry->value, value, MAX_VALUE_LEN - 1);
    entry->type = CONFIG_TYPE_STRING;
    cfg->is_modified = 1;
    return 0;
}

int config_set_bool(ConfigManager* cfg, const char* key, int value) {
    if (!cfg || !key) return -1;
    
    cfg->write_count++;
    ConfigEntry* entry = find_entry(cfg, key);
    if (!entry) entry = create_entry(cfg, key);
    if (!entry) return -1;
    
    strncpy(entry->value, value ? "true" : "false", MAX_VALUE_LEN - 1);
    entry->type = CONFIG_TYPE_BOOL;
    cfg->is_modified = 1;
    return 0;
}

int config_has_key(ConfigManager* cfg, const char* key) {
    return find_entry(cfg, key) != NULL;
}

size_t config_count(ConfigManager* cfg) {
    return cfg ? cfg->count : 0;
}

void config_print_all(ConfigManager* cfg) {
    if (!cfg) return;
    
    printf("\n+------ Configuration ------+\n");
    printf("| Entries: %-16zu |\n", cfg->count);
    printf("+---------------------------+\n");
    
    for (size_t i = 0; i < cfg->count; i++) {
        printf("| %-12s = %-10s |\n", 
               cfg->entries[i].key, cfg->entries[i].value);
    }
    printf("+---------------------------+\n");
    printf("| Reads: %-4d  Writes: %-4d |\n", cfg->read_count, cfg->write_count);
    printf("+---------------------------+\n\n");
}

void config_destroy(ConfigManager* cfg) {
    if (cfg) {
        printf("[ConfigManager] Destroyed (R:%d W:%d)\n", 
               cfg->read_count, cfg->write_count);
        free(cfg);
    }
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "config_manager.h"

int main() {
    printf("=== Opaque Pointer Demo ===\n\n");
    
    // 创建配置管理器
    ConfigManager* cfg = config_create();
    
    // 注意：无法直接访问内部成员
    // cfg->count = 10;        // 编译错误！
    // printf("%d", cfg->count); // 编译错误！
    
    // 只能通过API操作
    config_set_string(cfg, "app_name", "MyApp");
    config_set_int(cfg, "max_users", 100);
    config_set_float(cfg, "version", 1.5);
    config_set_bool(cfg, "debug_mode", 1);
    
    // 读取配置
    printf("App Name: %s\n", config_get_string(cfg, "app_name", "Unknown"));
    printf("Max Users: %d\n", config_get_int(cfg, "max_users", 0));
    printf("Version: %.1f\n", config_get_float(cfg, "version", 0.0));
    printf("Debug: %s\n", config_get_bool(cfg, "debug_mode", 0) ? "Yes" : "No");
    
    // 检查键是否存在
    printf("Has 'timeout': %s\n", config_has_key(cfg, "timeout") ? "Yes" : "No");
    
    // 打印所有配置
    config_print_all(cfg);
    
    // 保存到文件
    config_save_file(cfg, "app.conf");
    
    // 销毁
    config_destroy(cfg);
    
    // 重新加载测试
    printf("\n--- Reload Test ---\n");
    ConfigManager* cfg2 = config_create();
    config_load_file(cfg2, "app.conf");
    config_print_all(cfg2);
    config_destroy(cfg2);
    
    return 0;
}

/* 输出示例:
=== Opaque Pointer Demo ===

[ConfigManager] Created new instance
App Name: MyApp
Max Users: 100
Version: 1.5
Debug: Yes
Has 'timeout': No

+------ Configuration ------+
| Entries: 4                |
+---------------------------+
| app_name     = MyApp      |
| max_users    = 100        |
| version      = 1.500000   |
| debug_mode   = true       |
+---------------------------+
| Reads: 5     Writes: 4    |
+---------------------------+

[ConfigManager] Saved 4 entries to app.conf
[ConfigManager] Destroyed (R:5 W:4)

--- Reload Test ---
[ConfigManager] Created new instance
[ConfigManager] Loaded 4 entries from app.conf

+------ Configuration ------+
| Entries: 4                |
+---------------------------+
| app_name     = MyApp      |
| max_users    = 100        |
| version      = 1.500000   |
| debug_mode   = true       |
+---------------------------+
| Reads: 0     Writes: 0    |
+---------------------------+

[ConfigManager] Destroyed (R:0 W:0)
*/
```

## 优缺点

### 优点
- **完全封装**：结构体成员对外完全不可见
- **ABI稳定**：修改内部结构不需要重新编译客户端代码
- **减少编译依赖**：头文件简洁，减少编译时间
- **易于维护**：可以自由修改内部实现
- **强制接口使用**：防止绕过API直接操作数据

### 缺点
- 必须在堆上分配内存（无法栈上创建）
- 每次访问数据都需要函数调用开销
- 增加代码量（需要为每个操作提供函数）
- 调试时不易查看内部状态

