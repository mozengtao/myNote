# VFS Architecture Study: Extracting Reusable Patterns for User Space

## 1. Ops-Based Interfaces

```
+------------------------------------------------------------------+
|  PATTERN: OPS-BASED INTERFACES                                   |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Multiple implementations of same interface               │
    │  • Framework code should not know concrete types            │
    │  • Runtime selection of behavior                            │
    │  • Extension without modifying framework                    │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES REQUIRED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Struct with function pointers                            │
    │  • Pointer to ops table in object                           │
    │  • Dispatch via obj->ops->method(obj, ...)                  │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Only one implementation exists                           │
    │  • Performance critical inner loop (indirect call cost)     │
    │  • Simple operations with no variation                      │
    └─────────────────────────────────────────────────────────────┘
```

### Complete User-Space Example

```c
/* ops_interface.c - Ops-based storage backend */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration */
struct storage;

/* Operations table (the "interface") */
struct storage_ops {
    int (*open)(struct storage *s, const char *path);
    int (*read)(struct storage *s, void *buf, size_t size);
    int (*write)(struct storage *s, const void *buf, size_t size);
    void (*close)(struct storage *s);
};

/* Base object (holds ops pointer + common state) */
struct storage {
    const struct storage_ops *ops;   /* Polymorphism via this pointer */
    const char *name;
    int error_count;
};

/* Wrapper functions (framework API) */
static inline int storage_open(struct storage *s, const char *path) {
    return s->ops->open(s, path);
}
static inline int storage_read(struct storage *s, void *buf, size_t size) {
    return s->ops->read(s, buf, size);
}
static inline int storage_write(struct storage *s, const void *buf, size_t size) {
    return s->ops->write(s, buf, size);
}
static inline void storage_close(struct storage *s) {
    s->ops->close(s);
}

/* ============ FILE BACKEND ============ */
struct file_storage {
    struct storage base;   /* Embedded base for container_of */
    FILE *fp;
};

static int file_open(struct storage *s, const char *path) {
    struct file_storage *fs = (struct file_storage *)s;
    fs->fp = fopen(path, "r+");
    return fs->fp ? 0 : -1;
}

static int file_read(struct storage *s, void *buf, size_t size) {
    struct file_storage *fs = (struct file_storage *)s;
    return fread(buf, 1, size, fs->fp);
}

static int file_write(struct storage *s, const void *buf, size_t size) {
    struct file_storage *fs = (struct file_storage *)s;
    return fwrite(buf, 1, size, fs->fp);
}

static void file_close(struct storage *s) {
    struct file_storage *fs = (struct file_storage *)s;
    if (fs->fp) fclose(fs->fp);
    fs->fp = NULL;
}

static const struct storage_ops file_ops = {
    .open  = file_open,
    .read  = file_read,
    .write = file_write,
    .close = file_close,
};

struct file_storage *file_storage_create(void) {
    struct file_storage *fs = malloc(sizeof(*fs));
    if (fs) {
        fs->base.ops = &file_ops;
        fs->base.name = "file";
        fs->base.error_count = 0;
        fs->fp = NULL;
    }
    return fs;
}

/* ============ MEMORY BACKEND ============ */
struct mem_storage {
    struct storage base;
    char *buffer;
    size_t size;
    size_t pos;
};

static int mem_open(struct storage *s, const char *path) {
    struct mem_storage *ms = (struct mem_storage *)s;
    ms->size = 4096;
    ms->buffer = malloc(ms->size);
    ms->pos = 0;
    return ms->buffer ? 0 : -1;
}

static int mem_read(struct storage *s, void *buf, size_t size) {
    struct mem_storage *ms = (struct mem_storage *)s;
    size_t avail = ms->size - ms->pos;
    size_t to_read = size < avail ? size : avail;
    memcpy(buf, ms->buffer + ms->pos, to_read);
    ms->pos += to_read;
    return to_read;
}

static int mem_write(struct storage *s, const void *buf, size_t size) {
    struct mem_storage *ms = (struct mem_storage *)s;
    size_t avail = ms->size - ms->pos;
    size_t to_write = size < avail ? size : avail;
    memcpy(ms->buffer + ms->pos, buf, to_write);
    ms->pos += to_write;
    return to_write;
}

static void mem_close(struct storage *s) {
    struct mem_storage *ms = (struct mem_storage *)s;
    free(ms->buffer);
    ms->buffer = NULL;
}

static const struct storage_ops mem_ops = {
    .open  = mem_open,
    .read  = mem_read,
    .write = mem_write,
    .close = mem_close,
};

struct mem_storage *mem_storage_create(void) {
    struct mem_storage *ms = malloc(sizeof(*ms));
    if (ms) {
        ms->base.ops = &mem_ops;
        ms->base.name = "memory";
        ms->base.error_count = 0;
        ms->buffer = NULL;
    }
    return ms;
}

/* ============ GENERIC USAGE ============ */
void process_data(struct storage *s) {
    char buf[256];
    /* Framework code doesn't know if this is file or memory */
    storage_open(s, "/tmp/test");
    storage_write(s, "hello", 5);
    storage_close(s);
}

int main(void) {
    struct file_storage *fs = file_storage_create();
    struct mem_storage *ms = mem_storage_create();
    
    /* Same function works with both backends */
    process_data(&fs->base);
    process_data(&ms->base);
    
    free(fs);
    free(ms);
    return 0;
}
```

---

## 2. Opaque Handles

```
+------------------------------------------------------------------+
|  PATTERN: OPAQUE HANDLES                                         |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Hide internal structure from users                       │
    │  • Allow internal changes without breaking API              │
    │  • Prevent direct field access/manipulation                 │
    │  • Clear ownership: library owns object, user owns handle   │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES REQUIRED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Forward declaration in header (struct foo;)             │
    │  • Full definition only in .c file                         │
    │  • Pointer-based API                                       │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • User needs to embed object in their structure           │
    │  • Stack allocation is required                            │
    │  • Maximum performance (extra indirection)                 │
    └─────────────────────────────────────────────────────────────┘
```

### Complete User-Space Example

```c
/* ======== connection.h (PUBLIC HEADER) ======== */
#ifndef CONNECTION_H
#define CONNECTION_H

#include <stddef.h>

/* Opaque handle - user cannot see internals */
typedef struct connection connection_t;

/* Public API */
connection_t *connection_create(const char *host, int port);
int connection_send(connection_t *conn, const void *data, size_t len);
int connection_recv(connection_t *conn, void *buf, size_t len);
void connection_destroy(connection_t *conn);

/* Accessor for read-only data */
const char *connection_get_host(connection_t *conn);
int connection_get_port(connection_t *conn);

#endif /* CONNECTION_H */

/* ======== connection.c (PRIVATE IMPLEMENTATION) ======== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include "connection.h" */

/* Full definition - ONLY visible in this file */
struct connection {
    char host[256];
    int port;
    int socket_fd;
    int connected;
    size_t bytes_sent;
    size_t bytes_recv;
    /* Internal state user never sees */
    int retry_count;
    int timeout_ms;
};

connection_t *connection_create(const char *host, int port) {
    connection_t *conn = malloc(sizeof(*conn));
    if (!conn)
        return NULL;
    
    strncpy(conn->host, host, sizeof(conn->host) - 1);
    conn->host[sizeof(conn->host) - 1] = '\0';
    conn->port = port;
    conn->socket_fd = -1;
    conn->connected = 0;
    conn->bytes_sent = 0;
    conn->bytes_recv = 0;
    conn->retry_count = 3;
    conn->timeout_ms = 5000;
    
    /* Simulate connection */
    conn->connected = 1;
    conn->socket_fd = 42;  /* Fake FD */
    
    return conn;
}

int connection_send(connection_t *conn, const void *data, size_t len) {
    if (!conn || !conn->connected)
        return -1;
    /* Simulate send */
    conn->bytes_sent += len;
    return len;
}

int connection_recv(connection_t *conn, void *buf, size_t len) {
    if (!conn || !conn->connected)
        return -1;
    /* Simulate recv */
    memset(buf, 0, len);
    conn->bytes_recv += len;
    return len;
}

void connection_destroy(connection_t *conn) {
    if (!conn)
        return;
    /* Close socket, cleanup */
    conn->connected = 0;
    conn->socket_fd = -1;
    free(conn);
}

const char *connection_get_host(connection_t *conn) {
    return conn ? conn->host : NULL;
}

int connection_get_port(connection_t *conn) {
    return conn ? conn->port : -1;
}

/* ======== main.c (USER CODE) ======== */
int main(void) {
    connection_t *conn = connection_create("localhost", 8080);
    if (!conn) {
        fprintf(stderr, "Failed to create connection\n");
        return 1;
    }
    
    printf("Connected to %s:%d\n", 
           connection_get_host(conn), 
           connection_get_port(conn));
    
    /* User CANNOT access conn->bytes_sent directly */
    /* User CANNOT modify conn->timeout_ms */
    /* This is enforced at compile time */
    
    connection_send(conn, "hello", 5);
    connection_destroy(conn);
    
    return 0;
}
```

---

## 3. Layered APIs

```
+------------------------------------------------------------------+
|  PATTERN: LAYERED APIs                                           |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Different users need different abstraction levels       │
    │  • Common operations should be easy                        │
    │  • Advanced users need access to lower layers              │
    │  • Implementation can change without affecting high layer  │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES REQUIRED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Separate header files per layer                         │
    │  • Higher layer calls lower layer (not vice versa)         │
    │  • Clear naming convention (prefix per layer)              │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Only one use case exists                                │
    │  • Layers add overhead without value                       │
    │  • System is too simple for layers                         │
    └─────────────────────────────────────────────────────────────┘
```

### Complete User-Space Example

```c
/* Layered logging system: HIGH → MID → LOW */

/* ============ LOW LAYER: log_raw.h ============ */
#ifndef LOG_RAW_H
#define LOG_RAW_H

#include <stdio.h>
#include <time.h>

/* Lowest layer: just writes bytes */
typedef struct {
    FILE *output;
    size_t bytes_written;
} log_raw_t;

static inline int log_raw_init(log_raw_t *raw, const char *path) {
    raw->output = fopen(path, "a");
    raw->bytes_written = 0;
    return raw->output ? 0 : -1;
}

static inline int log_raw_write(log_raw_t *raw, const char *data, size_t len) {
    size_t written = fwrite(data, 1, len, raw->output);
    raw->bytes_written += written;
    fflush(raw->output);
    return written == len ? 0 : -1;
}

static inline void log_raw_close(log_raw_t *raw) {
    if (raw->output) fclose(raw->output);
    raw->output = NULL;
}

#endif /* LOG_RAW_H */

/* ============ MID LAYER: log_format.h ============ */
#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H

#include <stdarg.h>
#include <string.h>
/* #include "log_raw.h" */

/* Middle layer: adds formatting and timestamps */
typedef struct {
    log_raw_t raw;  /* Contains lower layer */
    char prefix[64];
} log_format_t;

static inline int log_format_init(log_format_t *fmt, const char *path, 
                                  const char *prefix) {
    strncpy(fmt->prefix, prefix, sizeof(fmt->prefix) - 1);
    fmt->prefix[sizeof(fmt->prefix) - 1] = '\0';
    return log_raw_init(&fmt->raw, path);
}

static inline int log_format_write(log_format_t *fmt, const char *level,
                                   const char *msg) {
    char buffer[1024];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    
    int len = snprintf(buffer, sizeof(buffer), 
                       "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s: %s\n",
                       tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                       tm->tm_hour, tm->tm_min, tm->tm_sec,
                       level, fmt->prefix, msg);
    
    return log_raw_write(&fmt->raw, buffer, len);
}

static inline void log_format_close(log_format_t *fmt) {
    log_raw_close(&fmt->raw);
}

#endif /* LOG_FORMAT_H */

/* ============ HIGH LAYER: log_easy.h ============ */
#ifndef LOG_EASY_H
#define LOG_EASY_H

/* #include "log_format.h" */

/* Highest layer: simple API for common use */
typedef struct {
    log_format_t fmt;  /* Contains middle layer */
    int min_level;     /* Filter: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR */
} log_easy_t;

enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };

static inline int log_easy_init(log_easy_t *log, const char *path) {
    log->min_level = LOG_INFO;
    return log_format_init(&log->fmt, path, "APP");
}

static inline void log_easy_set_level(log_easy_t *log, int level) {
    log->min_level = level;
}

static inline void log_debug(log_easy_t *log, const char *msg) {
    if (log->min_level <= LOG_DEBUG)
        log_format_write(&log->fmt, "DEBUG", msg);
}

static inline void log_info(log_easy_t *log, const char *msg) {
    if (log->min_level <= LOG_INFO)
        log_format_write(&log->fmt, "INFO", msg);
}

static inline void log_warn(log_easy_t *log, const char *msg) {
    if (log->min_level <= LOG_WARN)
        log_format_write(&log->fmt, "WARN", msg);
}

static inline void log_error(log_easy_t *log, const char *msg) {
    if (log->min_level <= LOG_ERROR)
        log_format_write(&log->fmt, "ERROR", msg);
}

static inline void log_easy_close(log_easy_t *log) {
    log_format_close(&log->fmt);
}

#endif /* LOG_EASY_H */

/* ============ USAGE ============ */
int main(void) {
    /* HIGH LAYER: Simple API for most users */
    log_easy_t app_log;
    log_easy_init(&app_log, "/tmp/app.log");
    log_info(&app_log, "Application started");
    log_error(&app_log, "Something went wrong");
    log_easy_close(&app_log);
    
    /* MID LAYER: Custom formatting if needed */
    log_format_t custom_log;
    log_format_init(&custom_log, "/tmp/custom.log", "CUSTOM");
    log_format_write(&custom_log, "SPECIAL", "Custom message");
    log_format_close(&custom_log);
    
    /* LOW LAYER: Direct byte writing if needed */
    log_raw_t raw_log;
    log_raw_init(&raw_log, "/tmp/raw.log");
    log_raw_write(&raw_log, "raw bytes\n", 10);
    log_raw_close(&raw_log);
    
    return 0;
}
```

---

## 4. Registration Systems

```
+------------------------------------------------------------------+
|  PATTERN: REGISTRATION SYSTEMS                                   |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Plugins/backends discovered at runtime                  │
    │  • Framework doesn't know concrete types at compile time   │
    │  • Dynamic extension without recompiling framework         │
    │  • Central registry for lookup by name/ID                  │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES REQUIRED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Linked list or array for registry                       │
    │  • Registration function (add to list)                     │
    │  • Lookup function (search by key)                         │
    │  • (Optional) constructor attribute for auto-registration  │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • All implementations known at compile time               │
    │  • Only 2-3 implementations (direct switch is simpler)     │
    │  • No runtime discovery needed                             │
    └─────────────────────────────────────────────────────────────┘
```

### Complete User-Space Example

```c
/* registration_system.c - Plugin registration pattern */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============ REGISTRY FRAMEWORK ============ */

#define MAX_PLUGINS 32

/* Plugin descriptor (what gets registered) */
struct plugin_type {
    const char *name;
    int (*init)(void *config);
    int (*process)(void *data, size_t len);
    void (*cleanup)(void);
};

/* Global registry */
static struct {
    const struct plugin_type *plugins[MAX_PLUGINS];
    int count;
} registry = { .count = 0 };

/* Registration function */
int register_plugin(const struct plugin_type *plugin) {
    if (registry.count >= MAX_PLUGINS)
        return -1;
    if (!plugin || !plugin->name)
        return -1;
    
    /* Check for duplicate */
    for (int i = 0; i < registry.count; i++) {
        if (strcmp(registry.plugins[i]->name, plugin->name) == 0)
            return -1;  /* Already registered */
    }
    
    registry.plugins[registry.count++] = plugin;
    printf("Registered plugin: %s\n", plugin->name);
    return 0;
}

/* Lookup function */
const struct plugin_type *find_plugin(const char *name) {
    for (int i = 0; i < registry.count; i++) {
        if (strcmp(registry.plugins[i]->name, name) == 0)
            return registry.plugins[i];
    }
    return NULL;
}

/* List all plugins */
void list_plugins(void) {
    printf("Registered plugins (%d):\n", registry.count);
    for (int i = 0; i < registry.count; i++) {
        printf("  - %s\n", registry.plugins[i]->name);
    }
}

/* ============ PLUGIN: JSON ============ */
static int json_init(void *config) {
    printf("JSON plugin initialized\n");
    return 0;
}

static int json_process(void *data, size_t len) {
    printf("JSON processing %zu bytes\n", len);
    return 0;
}

static void json_cleanup(void) {
    printf("JSON plugin cleanup\n");
}

static const struct plugin_type json_plugin = {
    .name    = "json",
    .init    = json_init,
    .process = json_process,
    .cleanup = json_cleanup,
};

/* ============ PLUGIN: XML ============ */
static int xml_init(void *config) {
    printf("XML plugin initialized\n");
    return 0;
}

static int xml_process(void *data, size_t len) {
    printf("XML processing %zu bytes\n", len);
    return 0;
}

static void xml_cleanup(void) {
    printf("XML plugin cleanup\n");
}

static const struct plugin_type xml_plugin = {
    .name    = "xml",
    .init    = xml_init,
    .process = xml_process,
    .cleanup = xml_cleanup,
};

/* ============ PLUGIN: CSV ============ */
static int csv_init(void *config) {
    printf("CSV plugin initialized\n");
    return 0;
}

static int csv_process(void *data, size_t len) {
    printf("CSV processing %zu bytes\n", len);
    return 0;
}

static void csv_cleanup(void) {
    printf("CSV plugin cleanup\n");
}

static const struct plugin_type csv_plugin = {
    .name    = "csv",
    .init    = csv_init,
    .process = csv_process,
    .cleanup = csv_cleanup,
};

/* ============ FRAMEWORK USAGE ============ */
void init_all_plugins(void) {
    /* Plugins register themselves */
    register_plugin(&json_plugin);
    register_plugin(&xml_plugin);
    register_plugin(&csv_plugin);
}

int main(int argc, char *argv[]) {
    /* Phase 1: Registration */
    init_all_plugins();
    list_plugins();
    
    /* Phase 2: Lookup by name (could be from config file) */
    const char *plugin_name = (argc > 1) ? argv[1] : "json";
    const struct plugin_type *plugin = find_plugin(plugin_name);
    
    if (!plugin) {
        fprintf(stderr, "Unknown plugin: %s\n", plugin_name);
        return 1;
    }
    
    /* Phase 3: Use the plugin */
    plugin->init(NULL);
    plugin->process("test data", 9);
    plugin->cleanup();
    
    return 0;
}
```

---

## 5. Explicit Ownership Rules

```
+------------------------------------------------------------------+
|  PATTERN: EXPLICIT OWNERSHIP RULES                               |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Who allocates? Who frees?                               │
    │  • Who holds reference? Who releases?                      │
    │  • When is pointer valid? When is it stale?                │
    │  • Clear contracts prevent use-after-free, leaks           │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES REQUIRED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Documentation (comments, naming conventions)            │
    │  • Reference counting for shared ownership                 │
    │  • "transfer" vs "borrow" semantics in API                │
    │  • Consistent patterns across codebase                     │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Garbage collected language                              │
    │  • Trivial programs with simple lifetimes                  │
    │  • Single-owner objects (transfer is obvious)              │
    └─────────────────────────────────────────────────────────────┘
```

### Complete User-Space Example

```c
/* ownership.c - Explicit ownership patterns */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ============ REFERENCE-COUNTED OBJECT ============ */

struct buffer {
    atomic_int refcount;  /* Reference count */
    size_t size;
    char data[];          /* Flexible array member */
};

/*
 * OWNERSHIP RULES FOR struct buffer:
 *
 * 1. buffer_create() returns OWNED reference (refcount = 1)
 *    - Caller MUST call buffer_put() when done
 *
 * 2. buffer_get() BORROWS reference, increments refcount
 *    - Caller MUST call buffer_put() for each buffer_get()
 *
 * 3. buffer_put() RELEASES reference, decrements refcount
 *    - When refcount reaches 0, buffer is freed
 *
 * 4. Pointers are valid as long as you hold a reference
 *    - NEVER use buffer after buffer_put() without buffer_get()
 */

/* Create: returns owned reference */
struct buffer *buffer_create(size_t size) {
    struct buffer *buf = malloc(sizeof(*buf) + size);
    if (!buf)
        return NULL;
    
    atomic_init(&buf->refcount, 1);  /* Initial owner */
    buf->size = size;
    memset(buf->data, 0, size);
    
    return buf;  /* Transfers ownership to caller */
}

/* Get: borrow reference (increment refcount) */
struct buffer *buffer_get(struct buffer *buf) {
    if (buf)
        atomic_fetch_add(&buf->refcount, 1);
    return buf;
}

/* Put: release reference (decrement refcount) */
void buffer_put(struct buffer *buf) {
    if (!buf)
        return;
    
    if (atomic_fetch_sub(&buf->refcount, 1) == 1) {
        /* We were the last reference */
        printf("Freeing buffer at %p\n", (void *)buf);
        free(buf);
    }
}

/* ============ TRANSFER vs BORROW SEMANTICS ============ */

/*
 * NAMING CONVENTION:
 *   _take suffix: ownership transferred TO function
 *   _give suffix: ownership transferred FROM function  
 *   no suffix: borrowed (function doesn't change ownership)
 */

/* Borrows buffer (caller still owns) */
void process_buffer(struct buffer *buf) {
    printf("Processing buffer: %zu bytes\n", buf->size);
    /* We borrowed buf, we don't call buffer_put() */
}

/* Takes ownership (caller gives up reference) */
void queue_buffer_take(struct buffer *buf) {
    printf("Queuing buffer (taking ownership)\n");
    /* We now own buf, we will call buffer_put() when done */
    /* Simulate: queue processes and releases later */
    buffer_put(buf);  /* Queue releases when done */
}

/* Gives ownership (returns owned reference) */
struct buffer *queue_buffer_give(void) {
    struct buffer *buf = buffer_create(256);
    printf("Creating and giving buffer\n");
    return buf;  /* Caller now owns this */
}

/* ============ COPY vs REFERENCE ============ */

/* Returns copy (caller owns the copy) */
char *buffer_copy_data(struct buffer *buf) {
    char *copy = malloc(buf->size);
    if (copy)
        memcpy(copy, buf->data, buf->size);
    return copy;  /* Caller must free() */
}

/* Returns pointer (valid only while buffer is alive) */
const char *buffer_borrow_data(struct buffer *buf) {
    return buf->data;  /* Caller must NOT free() */
    /* Pointer invalid after buffer_put() */
}

/* ============ SHARED OWNERSHIP EXAMPLE ============ */

struct cache_entry {
    char key[64];
    struct buffer *buf;  /* Shared reference */
};

void cache_store(struct cache_entry *entry, const char *key, 
                 struct buffer *buf) {
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->buf = buffer_get(buf);  /* Cache takes its own reference */
}

void cache_clear(struct cache_entry *entry) {
    buffer_put(entry->buf);  /* Release cache's reference */
    entry->buf = NULL;
}

/* ============ USAGE ============ */

int main(void) {
    printf("=== Ownership Example ===\n\n");
    
    /* 1. Create buffer (we own it, refcount = 1) */
    struct buffer *buf = buffer_create(100);
    printf("Created buffer, refcount = %d\n", 
           atomic_load(&buf->refcount));
    
    /* 2. Share with cache (refcount = 2) */
    struct cache_entry cache = {0};
    cache_store(&cache, "mykey", buf);
    printf("After cache_store, refcount = %d\n",
           atomic_load(&buf->refcount));
    
    /* 3. Borrow for processing (refcount unchanged) */
    process_buffer(buf);
    
    /* 4. We're done, release our reference (refcount = 1) */
    buffer_put(buf);
    printf("After our buffer_put, refcount = %d\n",
           atomic_load(&cache.buf->refcount));
    
    /* 5. Buffer still alive in cache */
    printf("Cache still has buffer: %s\n", 
           cache.buf ? "yes" : "no");
    
    /* 6. Clear cache (refcount = 0, freed) */
    cache_clear(&cache);
    
    /* buf is now INVALID - do not use! */
    
    printf("\n=== Transfer Example ===\n\n");
    
    /* Create and immediately transfer ownership */
    struct buffer *buf2 = buffer_create(50);
    queue_buffer_take(buf2);  /* We no longer own buf2 */
    /* buf2 is now INVALID - do not use! */
    
    /* Receive ownership from function */
    struct buffer *buf3 = queue_buffer_give();
    printf("Received buffer, refcount = %d\n",
           atomic_load(&buf3->refcount));
    buffer_put(buf3);  /* We must release what we received */
    
    return 0;
}
```

---

## Summary

```
+------------------------------------------------------------------+
|  REUSABLE PATTERNS SUMMARY                                       |
+------------------------------------------------------------------+

    OPS-BASED INTERFACES:
    • Problem: Multiple implementations, runtime polymorphism
    • Solution: Function pointer table + dispatch
    • Key: Base object contains ops pointer

    OPAQUE HANDLES:
    • Problem: Hide internals, allow change
    • Solution: Forward declaration + pointer API
    • Key: Full struct only in .c file

    LAYERED APIs:
    • Problem: Different abstraction levels needed
    • Solution: Separate modules, high calls low
    • Key: Clear naming, dependency direction

    REGISTRATION SYSTEMS:
    • Problem: Runtime discovery, plugins
    • Solution: Central registry + lookup
    • Key: Descriptor struct, register function

    EXPLICIT OWNERSHIP:
    • Problem: Who allocates/frees, when valid
    • Solution: Refcounting + naming conventions
    • Key: _take/_give suffixes, consistent patterns
```

**中文总结：**
- **Ops接口模式**：函数指针表实现运行时多态，基础对象包含ops指针
- **不透明句柄**：头文件前向声明，完整定义仅在.c文件中
- **分层API**：高层调用低层，清晰命名和依赖方向
- **注册系统**：中央注册表，描述符结构体，运行时查找
- **显式所有权**：引用计数，_take/_give命名约定，成对操作

