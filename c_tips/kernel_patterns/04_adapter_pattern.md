# Adapter Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                      ADAPTER PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    Client                Target Interface                         |
|    +--------+           +------------------+                      |
|    | Client |---------->|  Target          |                      |
|    +--------+           +------------------+                      |
|                         | + request()      |                      |
|                         +--------+---------+                      |
|                                  ^                                |
|                                  |                                |
|                         +--------+---------+                      |
|                         |     Adapter      |  (Wrapper)           |
|                         +------------------+                      |
|                         | + request()      |                      |
|                         |   {              |                      |
|                         |     adaptee->    |                      |
|                         |     specific_req |                      |
|                         |   }              |                      |
|                         +--------+---------+                      |
|                                  |                                |
|                                  | wraps                          |
|                                  v                                |
|                         +--------+---------+                      |
|                         |    Adaptee       |                      |
|                         +------------------+                      |
|                         | + specific_req() |                      |
|                         +------------------+                      |
|                                                                   |
|    Adapter translates client calls to adaptee's interface         |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 适配器模式通过中间层（适配器）转换接口，让不兼容的模块协同工作。客户端调用目标接口，适配器将调用转换为被适配者能理解的形式。Linux内核广泛使用这种模式来连接不同的子系统，如VFS层适配各种文件系统，I2C核心适配不同的I2C控制器。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: I2C SMBus Adapter

```c
/* From: drivers/i2c/i2c-core.c */

/**
 * i2c_smbus_xfer - Execute SMBus protocol operations
 * @adapter: I2C adapter
 * @addr: Device address
 * @flags: Protocol flags
 * @read_write: Direction
 * @command: Command byte
 * @protocol: SMBus protocol type
 * @data: Data buffer
 *
 * Adapter pattern: Converts SMBus calls to I2C transfers
 * if the hardware doesn't support native SMBus.
 */
s32 i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr, unsigned short flags,
                   char read_write, u8 command, int protocol,
                   union i2c_smbus_data *data)
{
    s32 res;

    /* Try native SMBus implementation first */
    if (adapter->algo->smbus_xfer) {
        /* Direct call - adapter supports SMBus natively */
        res = adapter->algo->smbus_xfer(adapter, addr, flags,
                                        read_write, command, protocol, data);
    } else {
        /* ADAPTATION: Emulate SMBus using I2C master_xfer() */
        res = i2c_smbus_xfer_emulated(adapter, addr, flags,
                                      read_write, command, protocol, data);
    }
    
    return res;
}

/**
 * i2c_smbus_xfer_emulated - SMBus emulation using I2C
 *
 * This is the ADAPTER: converts SMBus protocol to I2C messages.
 */
static s32 i2c_smbus_xfer_emulated(struct i2c_adapter *adapter, u16 addr,
                                   unsigned short flags, char read_write,
                                   u8 command, int size,
                                   union i2c_smbus_data *data)
{
    struct i2c_msg msg[2];  /* I2C message format */
    unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+3];
    unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX+2];
    
    /* Convert SMBus protocol to I2C messages */
    msg[0].addr = addr;
    msg[0].flags = flags;
    msg[0].len = 1;  /* Command byte */
    msg[0].buf = msgbuf0;
    msgbuf0[0] = command;
    
    switch (size) {
    case I2C_SMBUS_BYTE_DATA:
        if (read_write == I2C_SMBUS_WRITE) {
            msg[0].len = 2;
            msgbuf0[1] = data->byte;
        } else {
            msg[1].addr = addr;
            msg[1].flags = flags | I2C_M_RD;
            msg[1].len = 1;
            msg[1].buf = msgbuf1;
        }
        break;
    /* ... other SMBus protocol conversions ... */
    }
    
    /* Execute using I2C interface (the adaptee) */
    status = i2c_transfer(adapter, msg, num);
    
    /* Convert results back to SMBus format */
    if (read_write == I2C_SMBUS_READ) {
        switch (size) {
        case I2C_SMBUS_BYTE_DATA:
            data->byte = msgbuf1[0];
            break;
        /* ... other conversions ... */
        }
    }
    
    return status;
}
```

### 2.2 Kernel Example: VFS to Filesystem Adapter

```c
/* From: fs/read_write.c */

/**
 * vfs_read - VFS layer read operation
 * @file: File to read
 * @buf: User buffer
 * @count: Bytes to read
 * @pos: File position
 *
 * VFS acts as an ADAPTER between user system calls
 * and various filesystem implementations.
 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    /* Validate request */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    
    /* Security check */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        /* ADAPTATION: Call filesystem-specific read */
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
    }

    return ret;
}

/**
 * do_sync_read - Synchronous read adapter
 *
 * Adapts async aio_read to sync read interface.
 */
ssize_t do_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct iovec iov = { .iov_base = buf, .iov_len = len };
    struct kiocb kiocb;
    ssize_t ret;

    /* Set up async IO control block */
    init_sync_kiocb(&kiocb, filp);
    kiocb.ki_pos = *ppos;
    kiocb.ki_left = len;

    /* ADAPTATION: Convert sync call to async interface */
    ret = filp->f_op->aio_read(&kiocb, &iov, 1, kiocb.ki_pos);
    
    /* Wait for completion */
    if (-EIOCBQUEUED == ret)
        ret = wait_on_sync_kiocb(&kiocb);
    *ppos = kiocb.ki_pos;
    return ret;
}
```

### 2.3 Kernel Example: System Call Wrappers

```c
/* From: arch/sh/kernel/sys_sh32.c */

/**
 * sys_pread_wrapper - Adapter for pread system call
 *
 * Adapts architecture-specific calling convention to generic syscall.
 * SH32 needs special handling for 64-bit arguments.
 */
asmlinkage ssize_t sys_pread_wrapper(unsigned int fd, char __user *buf,
                                     size_t count, long dummy, loff_t pos)
{
    /* ADAPTATION: Convert SH32 argument layout to generic interface */
    return sys_pread64(fd, buf, count, pos);
}

/**
 * sys_fadvise64_64_wrapper - Adapter for fadvise system call
 *
 * Handles endianness differences in 64-bit argument passing.
 */
asmlinkage int sys_fadvise64_64_wrapper(int fd, u32 offset0, u32 offset1,
                                        u32 len0, u32 len1, int advice)
{
#ifdef __LITTLE_ENDIAN__
    /* ADAPTATION: Reconstruct 64-bit values from two 32-bit parts */
    return sys_fadvise64_64(fd, (u64)offset1 << 32 | offset0,
                            (u64)len1 << 32 | len0, advice);
#else
    return sys_fadvise64_64(fd, (u64)offset0 << 32 | offset1,
                            (u64)len0 << 32 | len1, advice);
#endif
}
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|                 LINUX VFS ADAPTER ARCHITECTURE                    |
+------------------------------------------------------------------+
|                                                                   |
|    User Space                                                     |
|    +------------------+                                           |
|    | Application      |                                           |
|    +--------+---------+                                           |
|             |                                                     |
|             | read(), write(), open()                             |
|             v                                                     |
|    +--------+------------------+                                  |
|    |    System Call Layer      |                                  |
|    +---------------------------+                                  |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    |         VFS LAYER         |  <-- ADAPTER                     |
|    +---------------------------+                                  |
|    | vfs_read()               |                                   |
|    | vfs_write()              |                                   |
|    | vfs_open()               |                                   |
|    +--------+------------------+                                  |
|             |                                                     |
|             | Calls file->f_op->xxx()                             |
|             |                                                     |
|    +--------+--------+--------+--------+                          |
|    |        |        |        |        |                          |
|    v        v        v        v        v                          |
| +------+ +------+ +------+ +------+ +------+                      |
| | ext4 | | xfs  | | nfs  | | fat  | |procfs|  (Adaptees)          |
| +------+ +------+ +------+ +------+ +------+                      |
| |.read | |.read | |.read | |.read | |.read |                      |
| |.write| |.write| |.write| |.write| |.write|                      |
| +------+ +------+ +------+ +------+ +------+                      |
|                                                                   |
|    Each filesystem implements file_operations differently         |
|    VFS adapts uniform interface to specific implementations       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux的VFS（虚拟文件系统）是适配器模式的典型应用。用户程序通过统一的系统调用（read/write/open）访问文件，VFS层将这些调用适配到不同文件系统的具体实现。每个文件系统（ext4、xfs、nfs等）实现相同的file_operations接口，但内部实现完全不同。VFS作为适配器，屏蔽了底层差异。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Interface Compatibility** | Makes incompatible interfaces work together |
| **Code Reuse** | Existing code can be used without modification |
| **Separation of Concerns** | Adapter handles interface translation logic |
| **Flexibility** | Easy to add new adapters for new adaptees |
| **Legacy Support** | Old implementations work with new interfaces |
| **Testing** | Adapters can be tested independently |

**中文说明：** 适配器模式的优势包括：接口兼容（让不兼容的接口协同工作）、代码重用（现有代码无需修改）、关注点分离（适配器处理接口转换逻辑）、灵活性（容易为新的被适配者添加适配器）、遗留支持（旧实现可以与新接口配合）、可测试性（适配器可以独立测试）。

---

## 4. User-Space Implementation Example

```c
/*
 * Adapter Pattern - User Space Implementation
 * Mimics Linux Kernel's I2C/SMBus adapter mechanism
 * 
 * Compile: gcc -o adapter adapter.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Target Interface - What the client expects
 * Similar to SMBus interface in kernel
 * ============================================================ */

/* High-level database operations (Target Interface) */
struct database_ops {
    int (*connect)(void *db, const char *host, int port);
    int (*query)(void *db, const char *sql, char *result, int len);
    int (*insert)(void *db, const char *table, const char *data);
    int (*update)(void *db, const char *table, const char *key, const char *data);
    void (*disconnect)(void *db);
    const char *name;
};

/* Database handle */
struct database {
    const struct database_ops *ops;
    void *private_data;
    char connection_string[256];
    int connected;
};

/* ============================================================
 * Adaptee 1: Legacy File-Based Storage
 * This is an old interface that doesn't match our target
 * ============================================================ */

/* Legacy file storage interface */
struct file_storage {
    char base_path[256];
    FILE *current_file;
    int is_open;
};

/* Legacy file storage functions */
int file_storage_open_path(struct file_storage *fs, const char *path)
{
    snprintf(fs->base_path, sizeof(fs->base_path), "%s", path);
    fs->is_open = 1;
    printf("[FileStorage] Opened path: %s\n", path);
    return 0;
}

int file_storage_read_record(struct file_storage *fs, const char *key, 
                             char *buf, int len)
{
    snprintf(buf, len, "FILE_RECORD[%s]={data_from_file}", key);
    printf("[FileStorage] Read record: %s\n", key);
    return 0;
}

int file_storage_write_record(struct file_storage *fs, const char *key,
                              const char *data)
{
    printf("[FileStorage] Write record: %s = %s\n", key, data);
    return 0;
}

void file_storage_close(struct file_storage *fs)
{
    fs->is_open = 0;
    printf("[FileStorage] Closed\n");
}

/* ============================================================
 * Adaptee 2: Key-Value Store
 * Another incompatible interface
 * ============================================================ */

struct kv_store {
    char server[128];
    int port;
    int socket_fd;
};

int kv_connect_server(struct kv_store *kv, const char *server, int port)
{
    snprintf(kv->server, sizeof(kv->server), "%s", server);
    kv->port = port;
    kv->socket_fd = 100;  /* Simulated */
    printf("[KVStore] Connected to %s:%d\n", server, port);
    return 0;
}

int kv_get(struct kv_store *kv, const char *key, char *value, int len)
{
    snprintf(value, len, "KV_VALUE[%s]={data_from_kv}", key);
    printf("[KVStore] GET %s\n", key);
    return 0;
}

int kv_set(struct kv_store *kv, const char *key, const char *value)
{
    printf("[KVStore] SET %s = %s\n", key, value);
    return 0;
}

void kv_disconnect(struct kv_store *kv)
{
    kv->socket_fd = -1;
    printf("[KVStore] Disconnected\n");
}

/* ============================================================
 * Adapter 1: File Storage to Database Adapter
 * Adapts legacy file storage to database interface
 * ============================================================ */

/* Adapter-specific data */
struct file_db_adapter {
    struct file_storage fs;
};

/**
 * file_db_connect - ADAPTER: Connect via file path
 *
 * Converts database connect(host, port) to file open(path).
 */
static int file_db_connect(void *db, const char *host, int port)
{
    struct database *database = db;
    struct file_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting DB connect to file open\n");
    
    /* ADAPTATION: Use "host" as file path, ignore port */
    int ret = file_storage_open_path(&adapter->fs, host);
    if (ret == 0) {
        database->connected = 1;
        snprintf(database->connection_string, 
                 sizeof(database->connection_string),
                 "file://%s", host);
    }
    return ret;
}

/**
 * file_db_query - ADAPTER: Convert SQL to file read
 *
 * Extracts key from SQL and reads from file.
 */
static int file_db_query(void *db, const char *sql, char *result, int len)
{
    struct database *database = db;
    struct file_db_adapter *adapter = database->private_data;
    char key[64];
    
    printf("[Adapter] Converting SQL query to file read\n");
    
    /* ADAPTATION: Extract key from simple SQL (SELECT * FROM t WHERE id='key') */
    /* Simplified: just use SQL as key */
    snprintf(key, sizeof(key), "%s", sql);
    
    return file_storage_read_record(&adapter->fs, key, result, len);
}

/**
 * file_db_insert - ADAPTER: Convert INSERT to file write
 */
static int file_db_insert(void *db, const char *table, const char *data)
{
    struct database *database = db;
    struct file_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting INSERT to file write\n");
    
    /* ADAPTATION: Use table as key */
    return file_storage_write_record(&adapter->fs, table, data);
}

/**
 * file_db_update - ADAPTER: Convert UPDATE to file write
 */
static int file_db_update(void *db, const char *table, const char *key, 
                          const char *data)
{
    struct database *database = db;
    struct file_db_adapter *adapter = database->private_data;
    char full_key[128];
    
    printf("[Adapter] Converting UPDATE to file write\n");
    
    /* ADAPTATION: Combine table and key */
    snprintf(full_key, sizeof(full_key), "%s/%s", table, key);
    return file_storage_write_record(&adapter->fs, full_key, data);
}

/**
 * file_db_disconnect - ADAPTER: Convert disconnect to file close
 */
static void file_db_disconnect(void *db)
{
    struct database *database = db;
    struct file_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting disconnect to file close\n");
    
    file_storage_close(&adapter->fs);
    database->connected = 0;
}

/* File storage adapter operations */
static const struct database_ops file_adapter_ops = {
    .connect = file_db_connect,
    .query = file_db_query,
    .insert = file_db_insert,
    .update = file_db_update,
    .disconnect = file_db_disconnect,
    .name = "FileStorageAdapter"
};

/* ============================================================
 * Adapter 2: Key-Value Store to Database Adapter
 * Adapts KV store to database interface
 * ============================================================ */

struct kv_db_adapter {
    struct kv_store kv;
};

static int kv_db_connect(void *db, const char *host, int port)
{
    struct database *database = db;
    struct kv_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting DB connect to KV connect\n");
    
    int ret = kv_connect_server(&adapter->kv, host, port);
    if (ret == 0) {
        database->connected = 1;
        snprintf(database->connection_string,
                 sizeof(database->connection_string),
                 "kv://%s:%d", host, port);
    }
    return ret;
}

static int kv_db_query(void *db, const char *sql, char *result, int len)
{
    struct database *database = db;
    struct kv_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting SQL query to KV GET\n");
    
    /* ADAPTATION: Use SQL as key for simplicity */
    return kv_get(&adapter->kv, sql, result, len);
}

static int kv_db_insert(void *db, const char *table, const char *data)
{
    struct database *database = db;
    struct kv_db_adapter *adapter = database->private_data;
    char key[128];
    
    printf("[Adapter] Converting INSERT to KV SET\n");
    
    /* ADAPTATION: Generate key from table and timestamp */
    snprintf(key, sizeof(key), "%s:new_entry", table);
    return kv_set(&adapter->kv, key, data);
}

static int kv_db_update(void *db, const char *table, const char *key,
                        const char *data)
{
    struct database *database = db;
    struct kv_db_adapter *adapter = database->private_data;
    char full_key[128];
    
    printf("[Adapter] Converting UPDATE to KV SET\n");
    
    snprintf(full_key, sizeof(full_key), "%s:%s", table, key);
    return kv_set(&adapter->kv, full_key, data);
}

static void kv_db_disconnect(void *db)
{
    struct database *database = db;
    struct kv_db_adapter *adapter = database->private_data;
    
    printf("[Adapter] Converting disconnect to KV disconnect\n");
    
    kv_disconnect(&adapter->kv);
    database->connected = 0;
}

static const struct database_ops kv_adapter_ops = {
    .connect = kv_db_connect,
    .query = kv_db_query,
    .insert = kv_db_insert,
    .update = kv_db_update,
    .disconnect = kv_db_disconnect,
    .name = "KVStoreAdapter"
};

/* ============================================================
 * Database Factory - Creates adapted database instances
 * ============================================================ */

enum db_type {
    DB_TYPE_FILE,
    DB_TYPE_KV
};

struct database *create_database(enum db_type type)
{
    struct database *db = malloc(sizeof(struct database));
    if (!db) return NULL;
    
    db->connected = 0;
    memset(db->connection_string, 0, sizeof(db->connection_string));
    
    switch (type) {
    case DB_TYPE_FILE:
        db->ops = &file_adapter_ops;
        db->private_data = malloc(sizeof(struct file_db_adapter));
        printf("[Factory] Created database with FileStorageAdapter\n");
        break;
        
    case DB_TYPE_KV:
        db->ops = &kv_adapter_ops;
        db->private_data = malloc(sizeof(struct kv_db_adapter));
        printf("[Factory] Created database with KVStoreAdapter\n");
        break;
        
    default:
        free(db);
        return NULL;
    }
    
    return db;
}

void destroy_database(struct database *db)
{
    if (db) {
        if (db->connected) {
            db->ops->disconnect(db);
        }
        free(db->private_data);
        free(db);
    }
}

/* ============================================================
 * Client Code - Uses uniform database interface
 * Doesn't know about underlying storage mechanism
 * ============================================================ */

void test_database(struct database *db, const char *host, int port)
{
    char result[256];
    
    printf("\n========================================\n");
    printf("Testing Database: %s\n", db->ops->name);
    printf("========================================\n");
    
    /* Connect using uniform interface */
    db->ops->connect(db, host, port);
    printf("Connected: %s\n\n", db->connection_string);
    
    /* Perform operations - same code works for any backend */
    db->ops->insert(db, "users", "name=John,age=30");
    db->ops->update(db, "users", "1", "name=John,age=31");
    db->ops->query(db, "SELECT * FROM users WHERE id=1", result, sizeof(result));
    printf("Query result: %s\n\n", result);
    
    db->ops->disconnect(db);
    printf("========================================\n\n");
}

int main(void)
{
    struct database *file_db;
    struct database *kv_db;

    printf("=== Adapter Pattern Demo ===\n\n");

    /* Create database with file storage adapter */
    file_db = create_database(DB_TYPE_FILE);
    test_database(file_db, "/var/data/mydb", 0);
    destroy_database(file_db);
    
    /* Create database with KV store adapter */
    kv_db = create_database(DB_TYPE_KV);
    test_database(kv_db, "redis.example.com", 6379);
    destroy_database(kv_db);

    printf("=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Adapter Pattern Flow

```
+------------------------------------------------------------------+
|                    ADAPTER PATTERN FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|    Client Code (Application)                                      |
|    +---------------------------+                                  |
|    | db->ops->query(db,        |                                  |
|    |   "SELECT * FROM users",  |                                  |
|    |   result, len);           |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | Calls target interface                         |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |    Adapter (kv_db_query)  |                                  |
|    +---------------------------+                                  |
|    | 1. Parse SQL query        |                                  |
|    | 2. Extract key            |                                  |
|    | 3. Convert to KV format   |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | Calls adaptee interface                        |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |   Adaptee (kv_get)        |                                  |
|    +---------------------------+                                  |
|    | - Connects to KV server   |                                  |
|    | - Retrieves value by key  |                                  |
|    | - Returns raw data        |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | Returns adaptee result                         |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |    Adapter (continued)    |                                  |
|    +---------------------------+                                  |
|    | 4. Convert result format  |                                  |
|    | 5. Return to client       |                                  |
|    +---------------------------+                                  |
|                                                                   |
|    Result: "KV_VALUE[SELECT * FROM users]={data_from_kv}"         |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 适配器模式的执行流程：客户端调用目标接口（如SQL查询），适配器接收调用并解析请求，将其转换为被适配者能理解的格式（如KV键），调用被适配者的方法获取数据，然后将结果转换回目标接口的格式返回给客户端。整个过程对客户端透明。

---

## 6. Key Implementation Points

1. **Wrapper Structure**: Adapter contains reference to adaptee
2. **Interface Translation**: Convert parameters and return values
3. **Protocol Mapping**: Map operations from target to adaptee semantics
4. **Error Handling**: Translate error codes between interfaces
5. **Bidirectional Conversion**: Handle both input and output adaptation
6. **Composition over Inheritance**: Prefer wrapping over extending

**中文说明：** 实现适配器模式的关键点：适配器包含对被适配者的引用（包装结构）、转换参数和返回值（接口翻译）、映射操作语义（协议映射）、翻译错误码（错误处理）、处理输入输出双向转换、优先使用组合而非继承。

