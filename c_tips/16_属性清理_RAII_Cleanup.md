# 属性清理 / RAII风格 (Cleanup Attribute)

## 定义

属性清理是GCC和Clang提供的`__attribute__((cleanup))`扩展，它允许在变量离开作用域时自动调用指定的清理函数。这模拟了C++的RAII（资源获取即初始化）模式，确保资源在任何退出路径上都能被正确释放。

## 适用场景

- 自动文件句柄关闭
- 自动内存释放
- 互斥锁的自动解锁
- 临时资源的自动清理
- 数据库连接/事务的自动处理
- 需要保证清理的异常敏感代码
- 减少资源泄漏风险的关键代码路径

## ASCII 图解

```
+------------------------------------------------------------------------+
|                    CLEANUP ATTRIBUTE (RAII-style)                       |
+------------------------------------------------------------------------+
|                                                                         |
|   TRADITIONAL MANUAL CLEANUP:                                           |
|   +------------------------------------------+                          |
|   | void func() {                            |                          |
|   |     FILE* f = fopen("file", "r");        |                          |
|   |     if (!f) return;                      |                          |
|   |                                          |                          |
|   |     char* buf = malloc(1024);            |                          |
|   |     if (!buf) {                          |                          |
|   |         fclose(f);   // Don't forget!    |                          |
|   |         return;                          |                          |
|   |     }                                    |                          |
|   |                                          |                          |
|   |     if (error1) {                        |                          |
|   |         free(buf);   // Must remember!   |                          |
|   |         fclose(f);   // Easy to forget!  |                          |
|   |         return;                          |                          |
|   |     }                                    |                          |
|   |                                          |                          |
|   |     // ... more error paths ...          |                          |
|   |                                          |                          |
|   |     free(buf);                           |                          |
|   |     fclose(f);                           |                          |
|   | }                                        |                          |
|   +------------------------------------------+                          |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   WITH CLEANUP ATTRIBUTE:                                               |
|   +------------------------------------------+                          |
|   | void func() {                            |                          |
|   |     __cleanup(auto_close) FILE* f =      |                          |
|   |         fopen("file", "r");              |                          |
|   |     if (!f) return;  // No cleanup needed|                          |
|   |                                          |                          |
|   |     __cleanup(auto_free) char* buf =     |                          |
|   |         malloc(1024);                    |                          |
|   |     if (!buf) return; // f auto-closed!  |                          |
|   |                                          |                          |
|   |     if (error1) return; // ALL auto!     |                          |
|   |     if (error2) return; // ALL auto!     |                          |
|   |                                          |                          |
|   |     // At scope end: buf freed, f closed |                          |
|   | }  // <-- Cleanup happens here           |                          |
|   +------------------------------------------+                          |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   HOW IT WORKS:                                                         |
|                                                                         |
|       Variable Declaration                                              |
|   +-------------------------+                                           |
|   | __cleanup(func) T* var  |                                           |
|   +------------+------------+                                           |
|                |                                                        |
|   +------------v------------+                                           |
|   |   ... code execution ...|                                           |
|   +------------+------------+                                           |
|                |                                                        |
|   +------------v------------+                                           |
|   | Variable goes out       |                                           |
|   | of scope (any path)     |                                           |
|   +------------+------------+                                           |
|                |                                                        |
|   +------------v------------+                                           |
|   | Compiler auto-inserts:  |                                           |
|   | func(&var);             |  <-- Cleanup function called              |
|   +-------------------------+      with POINTER to variable             |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图对比了传统手动清理和使用cleanup属性的区别。传统方式需要在每个返回路径手动释放资源，容易遗漏。使用cleanup属性后，编译器会在变量离开作用域时自动插入清理函数调用，无论是正常返回、提前返回还是跳转，都会触发清理。注意清理函数接收的是变量的**指针**，而不是变量本身。

## 实现方法

1. 定义清理函数，参数为指向变量的指针
2. 使用`__attribute__((cleanup(func)))`修饰变量
3. 封装成便捷宏提高可读性
4. 确保清理函数能处理NULL等边界情况

## C语言代码示例

### 基础清理函数

```c
// cleanup.h
#ifndef CLEANUP_H
#define CLEANUP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== 清理属性宏 ====================

#define __cleanup(func) __attribute__((cleanup(func)))

// ==================== 内存清理 ====================

// 自动释放malloc分配的内存
static inline void auto_free(void* ptr) {
    void** p = (void**)ptr;
    if (p && *p) {
        free(*p);
        *p = NULL;
    }
}

// 智能指针风格宏
#define AUTO_FREE __cleanup(auto_free)
#define scoped_ptr AUTO_FREE void*
#define scoped_str AUTO_FREE char*

// ==================== 文件清理 ====================

// 自动关闭文件
static inline void auto_fclose(FILE** fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

#define AUTO_CLOSE __cleanup(auto_fclose)
#define scoped_file AUTO_CLOSE FILE*

// ==================== 通用清理宏 ====================

// 用于自定义类型
#define DEFINE_AUTO_CLEANUP(type, cleanup_fn)           \
    static inline void auto_cleanup_##type(type** ptr) {\
        if (ptr && *ptr) {                              \
            cleanup_fn(*ptr);                           \
            *ptr = NULL;                                \
        }                                               \
    }                                                   \
    typedef __cleanup(auto_cleanup_##type) type* scoped_##type

#endif // CLEANUP_H
```

### 互斥锁清理

```c
// mutex_cleanup.h
#ifndef MUTEX_CLEANUP_H
#define MUTEX_CLEANUP_H

#include <pthread.h>
#include "cleanup.h"

// ==================== 互斥锁清理 ====================

typedef struct {
    pthread_mutex_t* mutex;
    int locked;
} MutexGuard;

static inline void mutex_guard_release(MutexGuard* guard) {
    if (guard && guard->mutex && guard->locked) {
        pthread_mutex_unlock(guard->mutex);
        guard->locked = 0;
    }
}

#define MUTEX_GUARD __cleanup(mutex_guard_release)

// 锁保护宏 - 在作用域内自动持有和释放锁
#define SCOPED_LOCK(mutex_ptr)                          \
    MUTEX_GUARD MutexGuard _guard_##__LINE__ = {        \
        .mutex = (mutex_ptr),                           \
        .locked = (pthread_mutex_lock(mutex_ptr), 1)    \
    }

// 读写锁清理
typedef struct {
    pthread_rwlock_t* rwlock;
    int locked;
    int is_write;
} RWLockGuard;

static inline void rwlock_guard_release(RWLockGuard* guard) {
    if (guard && guard->rwlock && guard->locked) {
        pthread_rwlock_unlock(guard->rwlock);
        guard->locked = 0;
    }
}

#define RWLOCK_GUARD __cleanup(rwlock_guard_release)

#define SCOPED_READ_LOCK(rwlock_ptr)                    \
    RWLOCK_GUARD RWLockGuard _rguard_##__LINE__ = {     \
        .rwlock = (rwlock_ptr),                         \
        .locked = (pthread_rwlock_rdlock(rwlock_ptr), 1),\
        .is_write = 0                                   \
    }

#define SCOPED_WRITE_LOCK(rwlock_ptr)                   \
    RWLOCK_GUARD RWLockGuard _wguard_##__LINE__ = {     \
        .rwlock = (rwlock_ptr),                         \
        .locked = (pthread_rwlock_wrlock(rwlock_ptr), 1),\
        .is_write = 1                                   \
    }

#endif // MUTEX_CLEANUP_H
```

### 自定义资源清理

```c
// resource_cleanup.h
#ifndef RESOURCE_CLEANUP_H
#define RESOURCE_CLEANUP_H

#include "cleanup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== 数据库连接模拟 ====================

typedef struct {
    int id;
    char connection_string[256];
    int is_connected;
} DBConnection;

DBConnection* db_connect(const char* conn_str) {
    DBConnection* conn = (DBConnection*)malloc(sizeof(DBConnection));
    if (conn) {
        static int next_id = 1;
        conn->id = next_id++;
        strncpy(conn->connection_string, conn_str, 255);
        conn->is_connected = 1;
        printf("[DB] Connected #%d to: %s\n", conn->id, conn_str);
    }
    return conn;
}

void db_disconnect(DBConnection* conn) {
    if (conn && conn->is_connected) {
        printf("[DB] Disconnected #%d\n", conn->id);
        conn->is_connected = 0;
        free(conn);
    }
}

// 定义自动清理类型
static inline void auto_cleanup_DBConnection(DBConnection** ptr) {
    if (ptr && *ptr) {
        db_disconnect(*ptr);
        *ptr = NULL;
    }
}
#define scoped_db_conn __cleanup(auto_cleanup_DBConnection) DBConnection*

// ==================== 临时文件清理 ====================

typedef struct {
    char path[256];
    FILE* file;
} TempFile;

TempFile* tempfile_create(const char* prefix) {
    TempFile* tf = (TempFile*)malloc(sizeof(TempFile));
    if (tf) {
        snprintf(tf->path, sizeof(tf->path), "%s_temp_%d.tmp", prefix, rand());
        tf->file = fopen(tf->path, "w+");
        if (tf->file) {
            printf("[TempFile] Created: %s\n", tf->path);
        } else {
            free(tf);
            return NULL;
        }
    }
    return tf;
}

void tempfile_destroy(TempFile* tf) {
    if (tf) {
        if (tf->file) {
            fclose(tf->file);
        }
        printf("[TempFile] Removing: %s\n", tf->path);
        remove(tf->path);
        free(tf);
    }
}

static inline void auto_cleanup_TempFile(TempFile** ptr) {
    if (ptr && *ptr) {
        tempfile_destroy(*ptr);
        *ptr = NULL;
    }
}
#define scoped_tempfile __cleanup(auto_cleanup_TempFile) TempFile*

// ==================== 字符串构建器清理 ====================

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} StringBuilder;

StringBuilder* sb_create(size_t initial_capacity) {
    StringBuilder* sb = (StringBuilder*)malloc(sizeof(StringBuilder));
    if (sb) {
        sb->buffer = (char*)malloc(initial_capacity);
        if (!sb->buffer) {
            free(sb);
            return NULL;
        }
        sb->buffer[0] = '\0';
        sb->length = 0;
        sb->capacity = initial_capacity;
    }
    return sb;
}

void sb_append(StringBuilder* sb, const char* str) {
    if (!sb || !str) return;
    size_t len = strlen(str);
    if (sb->length + len >= sb->capacity) {
        size_t new_cap = (sb->capacity + len) * 2;
        char* new_buf = (char*)realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    strcpy(sb->buffer + sb->length, str);
    sb->length += len;
}

void sb_destroy(StringBuilder* sb) {
    if (sb) {
        free(sb->buffer);
        free(sb);
    }
}

static inline void auto_cleanup_StringBuilder(StringBuilder** ptr) {
    if (ptr && *ptr) {
        sb_destroy(*ptr);
        *ptr = NULL;
    }
}
#define scoped_stringbuilder __cleanup(auto_cleanup_StringBuilder) StringBuilder*

#endif // RESOURCE_CLEANUP_H
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cleanup.h"
#include "mutex_cleanup.h"
#include "resource_cleanup.h"

// ==================== 示例函数 ====================

int process_file_safely(const char* filename) {
    printf("\n--- process_file_safely('%s') ---\n", filename);
    
    // 文件会在函数返回时自动关闭
    scoped_file f = fopen(filename, "r");
    if (!f) {
        printf("Cannot open file (no cleanup needed)\n");
        return -1;  // f是NULL，不需要清理
    }
    
    // 缓冲区会在函数返回时自动释放
    scoped_str buffer = (char*)malloc(1024);
    if (!buffer) {
        printf("Cannot allocate buffer\n");
        return -2;  // f会被自动关闭！
    }
    
    // 模拟处理
    if (fgets(buffer, 1024, f)) {
        printf("Read: %s", buffer);
    }
    
    // 模拟错误条件
    if (rand() % 3 == 0) {
        printf("Simulated error - early return\n");
        return -3;  // buffer释放，f关闭，全自动！
    }
    
    printf("Processing complete\n");
    return 0;  // 正常返回，资源也会自动清理
}

int work_with_database(void) {
    printf("\n--- work_with_database() ---\n");
    
    // 数据库连接会自动关闭
    scoped_db_conn conn = db_connect("mysql://localhost/test");
    if (!conn) {
        printf("Connection failed\n");
        return -1;
    }
    
    // 临时文件会自动删除
    scoped_tempfile tf = tempfile_create("query_result");
    if (!tf) {
        printf("Cannot create temp file\n");
        return -2;  // conn会自动断开
    }
    
    // 模拟工作
    fprintf(tf->file, "Query results here...\n");
    printf("Database operation complete\n");
    
    return 0;  // conn断开，tf删除，全自动
}

void demonstrate_string_builder(void) {
    printf("\n--- demonstrate_string_builder() ---\n");
    
    scoped_stringbuilder sb = sb_create(64);
    if (!sb) {
        printf("Cannot create StringBuilder\n");
        return;
    }
    
    sb_append(sb, "Hello, ");
    sb_append(sb, "World! ");
    sb_append(sb, "This is ");
    sb_append(sb, "a test.");
    
    printf("Built string: %s\n", sb->buffer);
    printf("Length: %zu, Capacity: %zu\n", sb->length, sb->capacity);
    
    // sb会在函数返回时自动销毁
}

// 演示互斥锁清理（需要多线程环境）
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_counter = 0;

void increment_counter(void) {
    SCOPED_LOCK(&g_mutex);
    // 在这个作用域内，锁是持有的
    
    g_counter++;
    printf("Counter: %d\n", g_counter);
    
    if (g_counter > 5) {
        printf("Counter exceeded 5, returning early\n");
        return;  // 锁自动释放！
    }
    
    // 锁在这里自动释放
}

// ==================== Main ====================

int main() {
    printf("=== RAII-style Cleanup Demo ===\n");
    
    // 创建测试文件
    FILE* f = fopen("test_cleanup.txt", "w");
    fprintf(f, "Hello, RAII!\nLine 2\nLine 3\n");
    fclose(f);
    
    // 测试文件处理
    process_file_safely("test_cleanup.txt");
    process_file_safely("nonexistent.txt");
    
    // 测试数据库和临时文件
    work_with_database();
    
    // 测试字符串构建器
    demonstrate_string_builder();
    
    // 测试互斥锁清理
    printf("\n--- Mutex cleanup demo ---\n");
    for (int i = 0; i < 8; i++) {
        increment_counter();
    }
    
    // 嵌套作用域演示
    printf("\n--- Nested scope demo ---\n");
    {
        printf("Outer scope start\n");
        scoped_str outer = strdup("Outer string");
        
        {
            printf("Inner scope start\n");
            scoped_str inner = strdup("Inner string");
            printf("Inner: %s\n", inner);
            printf("Inner scope end\n");
        }  // inner在这里释放
        
        printf("Outer: %s\n", outer);
        printf("Outer scope end\n");
    }  // outer在这里释放
    
    // 清理测试文件
    remove("test_cleanup.txt");
    
    printf("\n=== All tests complete ===\n");
    
    return 0;
}

/* 输出示例:
=== RAII-style Cleanup Demo ===

--- process_file_safely('test_cleanup.txt') ---
Read: Hello, RAII!
Processing complete

--- process_file_safely('nonexistent.txt') ---
Cannot open file (no cleanup needed)

--- work_with_database() ---
[DB] Connected #1 to: mysql://localhost/test
[TempFile] Created: query_result_temp_12345.tmp
Database operation complete
[TempFile] Removing: query_result_temp_12345.tmp
[DB] Disconnected #1

--- demonstrate_string_builder() ---
Built string: Hello, World! This is a test.
Length: 30, Capacity: 64

--- Mutex cleanup demo ---
Counter: 1
Counter: 2
...

--- Nested scope demo ---
Outer scope start
Inner scope start
Inner: Inner string
Inner scope end
Outer: Outer string
Outer scope end

=== All tests complete ===
*/
```

## 优缺点

### 优点
- **自动资源管理**：无需手动清理，减少泄漏风险
- **任何退出路径都有效**：return、break、goto都触发清理
- **代码简洁**：减少清理相关的重复代码
- **类似RAII**：C语言也能享受C++的资源管理便利
- **嵌套作用域支持**：内层变量先清理

### 缺点
- **非标准**：只有GCC和Clang支持
- **不可移植**：MSVC不支持
- **函数签名固定**：清理函数必须接受指针参数
- **调试复杂**：清理调用在代码中不可见
- **顺序问题**：同一作用域变量按声明逆序清理

