# Practical C Systems Programming Guidelines

A battle-tested guide for writing maintainable, portable, and robust C code in kernels, libraries, and long-lived products.

---

## Table of Contents

1. [Reasonability](#1-reasonability)
2. [Maintainability](#2-maintainability)
3. [Collaboration](#3-collaboration)
4. [Reusability](#4-reusability)
5. [Portability](#5-portability)
6. [Testability](#6-testability)
7. [Project Structure & Organization](#7-project-structure--organization)
8. [Header File Rules](#8-header-file-rules)
9. [Build System Guidelines](#9-build-system-guidelines)
10. [Documentation Standards](#10-documentation-standards)

---

## 1. Reasonability

**Principle**: Code should be obvious, unsurprising, and match the mental model of someone reading it for the first time.

### 1.1 Prefer Explicit Over Clever

**❌ Bad: Clever bit manipulation without context**
```c
/* What does this do? */
int f(int n) {
    return n & (n - 1) ? 0 : 1;
}
```

**✅ Reasonable: Self-documenting with clear intent**
```c
/**
 * is_power_of_two - Check if n is a power of two
 * @n: Value to check (must be > 0)
 *
 * Returns: 1 if n is a power of two, 0 otherwise
 */
static inline int is_power_of_two(unsigned int n)
{
    return n != 0 && (n & (n - 1)) == 0;
}
```

**Benefits**:
- New team members understand code in seconds, not minutes
- Reduces bugs from misunderstanding
- Self-documents edge cases (n must be > 0)

---

### 1.2 Single Responsibility Per Function

**❌ Bad: Function doing too many things**
```c
int process_user_data(const char *input, char *output, size_t outlen,
                      FILE *log, int *error_count)
{
    /* Parse input */
    /* Validate parsed data */
    /* Transform data */
    /* Write to output */
    /* Log results */
    /* Update error count */
    return status;
}
```

**✅ Reasonable: Decomposed into focused functions**
```c
struct user_data {
    char name[64];
    int age;
    /* ... */
};

static int parse_user_input(const char *input, struct user_data *data);
static int validate_user_data(const struct user_data *data);
static int transform_user_data(struct user_data *data);
static int serialize_user_data(const struct user_data *data, 
                               char *buf, size_t len);

int process_user_data(const char *input, char *output, size_t outlen)
{
    struct user_data data;
    int ret;

    ret = parse_user_input(input, &data);
    if (ret < 0)
        return ret;

    ret = validate_user_data(&data);
    if (ret < 0)
        return ret;

    ret = transform_user_data(&data);
    if (ret < 0)
        return ret;

    return serialize_user_data(&data, output, outlen);
}
```

**Benefits**:
- Each function is testable in isolation
- Failures are easier to locate
- Functions can be reused independently

---

### 1.3 Avoid Magic Numbers

**❌ Bad: Unexplained constants**
```c
if (retry_count > 3)
    return -1;

char buf[4096];
usleep(100000);
```

**✅ Reasonable: Named constants with rationale**
```c
/*
 * MAX_RETRIES: 3 attempts balances reliability vs. latency.
 * Empirically, transient failures resolve within 2 retries.
 */
#define MAX_RETRIES     3

/* Page-aligned buffer for efficient DMA transfers */
#define IO_BUFFER_SIZE  4096

/* 100ms backoff per RFC 7323 recommendation */
#define RETRY_DELAY_US  (100 * 1000)

if (retry_count > MAX_RETRIES)
    return -EAGAIN;

char buf[IO_BUFFER_SIZE];
usleep(RETRY_DELAY_US);
```

**Benefits**:
- Rationale is preserved for future maintainers
- Single point of change for tuning
- grep-able symbols for analysis

---

### 1.4 Fail Fast and Explicitly

**❌ Bad: Silent failure, unclear state**
```c
void *get_resource(int id)
{
    if (id < 0 || id >= MAX_ID)
        return NULL;  /* Caller might ignore this */
    
    struct resource *r = lookup(id);
    if (!r)
        return NULL;  /* Same return for different errors */
    
    return r->data;
}
```

**✅ Reasonable: Distinct error codes, defensive checks**
```c
/**
 * get_resource - Retrieve resource by ID
 * @id: Resource identifier (0 to MAX_ID-1)
 * @out: Pointer to store result
 *
 * Returns: 0 on success, negative errno on failure
 *   -EINVAL: Invalid ID
 *   -ENOENT: Resource not found
 *   -EBUSY:  Resource temporarily unavailable
 */
int get_resource(int id, void **out)
{
    struct resource *r;

    if (!out)
        return -EINVAL;

    if (id < 0 || id >= MAX_ID) {
        pr_warn("get_resource: invalid id %d\n", id);
        return -EINVAL;
    }

    r = lookup(id);
    if (!r)
        return -ENOENT;

    if (!r->available)
        return -EBUSY;

    *out = r->data;
    return 0;
}
```

**Benefits**:
- Callers can handle specific errors appropriately
- Debugging is straightforward
- Follows POSIX/kernel convention (0 = success, negative = error)

---

## 2. Maintainability

**Principle**: Code will be read and modified far more often than written. Optimize for the reader and future maintainer.

### 2.1 Consistent Error Handling Pattern

**❌ Bad: Mixed error handling styles**
```c
int init_subsystem(void)
{
    a = alloc_a();
    if (!a)
        return -ENOMEM;
    
    b = alloc_b();
    if (!b) {
        free_a(a);
        return -ENOMEM;
    }
    
    c = alloc_c();
    if (!c) {
        free_b(b);
        free_a(a);
        return -ENOMEM;
    }
    
    /* More allocations... cleanup becomes exponential */
    return 0;
}
```

**✅ Reasonable: Goto-based cleanup (Linux kernel style)**
```c
int init_subsystem(void)
{
    int ret;

    a = alloc_a();
    if (!a) {
        ret = -ENOMEM;
        goto fail_a;
    }

    b = alloc_b();
    if (!b) {
        ret = -ENOMEM;
        goto fail_b;
    }

    c = alloc_c();
    if (!c) {
        ret = -ENOMEM;
        goto fail_c;
    }

    d = alloc_d();
    if (!d) {
        ret = -ENOMEM;
        goto fail_d;
    }

    return 0;

fail_d:
    free_c(c);
fail_c:
    free_b(b);
fail_b:
    free_a(a);
fail_a:
    return ret;
}
```

**Benefits**:
- Adding new resources is O(1) change
- Cleanup order is visually obvious (reverse of allocation)
- Single exit path simplifies reasoning

---

### 2.2 Defensive Initialization

**❌ Bad: Uninitialized or partially initialized structures**
```c
struct connection conn;
conn.fd = socket(AF_INET, SOCK_STREAM, 0);
conn.state = STATE_NEW;
/* conn.timeout is uninitialized! */
/* conn.buffer is uninitialized! */
```

**✅ Reasonable: Designated initializers + init functions**
```c
/* Zero-initialize, then set specific fields */
struct connection conn = {
    .fd = -1,
    .state = STATE_INVALID,
    .timeout_ms = DEFAULT_TIMEOUT_MS,
    .buffer = NULL,
};

/* Or use an init function for complex setup */
static int connection_init(struct connection *conn)
{
    memset(conn, 0, sizeof(*conn));
    
    conn->fd = -1;  /* Invalid FD sentinel */
    conn->state = STATE_NEW;
    conn->timeout_ms = DEFAULT_TIMEOUT_MS;
    
    conn->buffer = malloc(BUFFER_SIZE);
    if (!conn->buffer)
        return -ENOMEM;
    
    return 0;
}

static void connection_cleanup(struct connection *conn)
{
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    free(conn->buffer);
    conn->buffer = NULL;
}
```

**Benefits**:
- Valgrind/sanitizers can catch actual bugs, not false positives
- Cleanup functions can safely be called multiple times
- Sentinel values (-1 for fd) make state obvious

---

### 2.3 Limit Scope and Lifetime

**❌ Bad: Variables with unnecessarily wide scope**
```c
int process_items(struct item *items, int count)
{
    int i, j, ret, temp;
    char buffer[256];
    struct context ctx;
    
    /* 200 lines of code using various subsets of these */
}
```

**✅ Reasonable: Declare at point of use, minimal scope**
```c
int process_items(struct item *items, int count)
{
    for (int i = 0; i < count; i++) {
        struct context ctx;
        int ret;
        
        ret = init_context(&ctx, &items[i]);
        if (ret < 0)
            return ret;
        
        /* ctx and ret only exist within this iteration */
        
        cleanup_context(&ctx);
    }
    
    return 0;
}
```

**Benefits**:
- Reduced cognitive load—fewer variables to track
- Compiler can optimize register allocation
- Prevents accidental reuse of stale values

---

### 2.4 Prefer Composition Over Deep Nesting

**❌ Bad: Arrow code (deep nesting)**
```c
int handle_request(struct request *req)
{
    if (req) {
        if (req->valid) {
            if (req->auth) {
                if (check_permission(req)) {
                    if (load_data(req)) {
                        if (process(req)) {
                            return send_response(req);
                        }
                    }
                }
            }
        }
    }
    return -1;
}
```

**✅ Reasonable: Early return, flat structure**
```c
int handle_request(struct request *req)
{
    int ret;

    if (!req)
        return -EINVAL;

    if (!req->valid)
        return -EBADMSG;

    if (!req->auth)
        return -EACCES;

    ret = check_permission(req);
    if (ret < 0)
        return ret;

    ret = load_data(req);
    if (ret < 0)
        return ret;

    ret = process(req);
    if (ret < 0)
        return ret;

    return send_response(req);
}
```

**Benefits**:
- Linear reading, no mental stack to maintain
- Each validation stands alone
- Easy to add new checks

---

## 3. Collaboration

**Principle**: Code is written by teams over years. Make it easy for others to understand, modify, and review your code.

### 3.1 Consistent Naming Conventions

**❌ Bad: Inconsistent naming**
```c
int getCount();
void set_value(int val);
int ProcessData();
struct myStruct { int numItems; int item_count; };
```

**✅ Reasonable: Project-wide consistent style**
```c
/*
 * Linux kernel style: lowercase_with_underscores
 * Prefix functions with subsystem name
 */

/* Public API */
int netdev_get_count(struct net_device *dev);
void netdev_set_mtu(struct net_device *dev, int mtu);
int netdev_process_packet(struct net_device *dev, struct sk_buff *skb);

/* Structures */
struct net_device {
    int packet_count;
    int mtu;
    /* ... */
};

/* Internal/static functions: shorter names OK */
static int validate_mtu(int mtu);
static void update_stats(struct net_device *dev);
```

**Naming Convention Summary**:
| Element | Convention | Example |
|---------|------------|---------|
| Functions | `subsystem_verb_noun` | `socket_create_stream` |
| Structs | `subsystem_noun` | `struct socket_buffer` |
| Macros | `SUBSYSTEM_NAME` | `SOCKET_MAX_BACKLOG` |
| Enums | `SUBSYSTEM_STATE_VALUE` | `SOCKET_STATE_CONNECTED` |
| Locals | short, contextual | `int i, ret, len;` |
| Globals | descriptive | `unsigned long nr_active_sockets;` |

---

### 3.2 Document the "Why", Not the "What"

**❌ Bad: Comments that repeat the code**
```c
/* Increment counter */
counter++;

/* Check if buffer is null */
if (buffer == NULL)
    return -1;

/* Loop through all items */
for (int i = 0; i < count; i++) {
```

**✅ Reasonable: Comments explain non-obvious decisions**
```c
/*
 * Use atomic increment here because interrupt handlers
 * may also update this counter. See irq_handler() in irq.c.
 */
atomic_inc(&counter);

/*
 * Buffer can be NULL during early boot before memory
 * allocator is initialized. In that case, fall back to
 * static buffer (see early_printk.c).
 */
if (!buffer)
    return use_static_buffer(data, len);

/*
 * Process items in reverse order to maintain dependency
 * invariant: children must be processed before parents.
 * Forward iteration causes use-after-free (see bug #4521).
 */
for (int i = count - 1; i >= 0; i--) {
```

**Benefits**:
- Preserves institutional knowledge
- Prevents regressions when someone "cleans up" code
- Saves hours of archaeology in git blame

---

### 3.3 Keep Related Code Together

**❌ Bad: Related code scattered across file**
```c
/* Line 50 */
struct foo { ... };

/* Line 200 */
static void foo_helper1(struct foo *f) { ... }

/* Line 500 */
int foo_create(struct foo **out) { ... }

/* Line 150 */
static void foo_helper2(struct foo *f) { ... }

/* Line 700 */
void foo_destroy(struct foo *f) { ... }
```

**✅ Reasonable: Cohesive grouping**
```c
/*
 * =============================================================
 * Foo Subsystem
 * =============================================================
 */

/* --- Data Structures --- */

struct foo {
    int refcount;
    void *data;
};

/* --- Internal Helpers --- */

static void foo_helper1(struct foo *f)
{
    /* ... */
}

static void foo_helper2(struct foo *f)
{
    /* ... */
}

/* --- Public API --- */

int foo_create(struct foo **out)
{
    /* ... */
}

void foo_destroy(struct foo *f)
{
    /* ... */
}
```

**Benefits**:
- Readers find all related code in one scroll
- Modifications less likely to miss related changes
- Natural structure for code review

---

### 3.4 Prefer Small, Focused Commits

**Commit message structure** (widely used convention):

```
subsystem: short summary (50 chars max)

Longer explanation of what and why (not how—the code shows how).
Wrap at 72 characters.

- Bullet points for multiple changes
- Reference issues: Fixes #1234
- Explain non-obvious decisions

Signed-off-by: Your Name <email@example.com>
```

**Example good commit**:
```
netdev: fix race condition in packet receive path

The receive handler could access freed memory when the device
was unregistered during packet processing. Add RCU protection
around the critical section.

The bug was introduced in commit abc123 ("netdev: add multiqueue
support") when the locking was restructured.

Fixes: abc123def456 ("netdev: add multiqueue support")
Reported-by: User Name <user@example.com>
Signed-off-by: Developer <dev@example.com>
```

---

## 4. Reusability

**Principle**: Write code that can be used in multiple contexts without modification.

### 4.1 Separate Policy from Mechanism

**❌ Bad: Policy embedded in mechanism**
```c
int create_log_file(void)
{
    /* Hardcoded policy: where, when, how */
    char *path = "/var/log/myapp.log";
    int fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (fd < 0) {
        fprintf(stderr, "Failed to open log\n");  /* Hardcoded error output */
        exit(1);  /* Hardcoded error policy */
    }
    return fd;
}
```

**✅ Reasonable: Mechanism takes policy as parameters**
```c
struct log_config {
    const char *path;
    int flags;
    mode_t mode;
    void (*error_handler)(const char *msg, int err);
};

static const struct log_config default_log_config = {
    .path = "/var/log/app.log",
    .flags = O_CREAT | O_APPEND | O_WRONLY,
    .mode = 0644,
    .error_handler = NULL,
};

int create_log_file(const struct log_config *config)
{
    const struct log_config *cfg = config ? config : &default_log_config;
    int fd;

    fd = open(cfg->path, cfg->flags, cfg->mode);
    if (fd < 0) {
        if (cfg->error_handler)
            cfg->error_handler(cfg->path, errno);
        return -errno;
    }
    return fd;
}
```

**Benefits**:
- Same code works in different environments (embedded, server, testing)
- Caller controls error handling strategy
- Defaults make simple cases simple

---

### 4.2 Use Opaque Types for Encapsulation

**❌ Bad: Exposed internals that callers depend on**
```c
/* In header file—exposes all internals */
struct database {
    int fd;
    char *buffer;
    size_t buf_size;
    pthread_mutex_t lock;
    struct { /* internal cache */ } cache;
};

/* Callers can (and will) access internals */
db->buffer[0] = 'x';  /* Bypass API! */
```

**✅ Reasonable: Opaque handle with accessor functions**
```c
/* --- public header: database.h --- */

/* Forward declaration only—internals hidden */
struct database;

/* Lifecycle */
int database_open(const char *path, struct database **db);
void database_close(struct database *db);

/* Operations */
int database_get(struct database *db, const char *key, char *value, size_t len);
int database_put(struct database *db, const char *key, const char *value);

/* --- private implementation: database.c --- */

struct database {
    int fd;
    char *buffer;
    size_t buf_size;
    pthread_mutex_t lock;
    /* Free to change without breaking ABI */
};

int database_open(const char *path, struct database **db)
{
    struct database *d = calloc(1, sizeof(*d));
    if (!d)
        return -ENOMEM;
    
    /* ... initialization ... */
    
    *db = d;
    return 0;
}
```

**Benefits**:
- Internal changes don't break callers
- API is explicit—no "backdoor" access
- Enables different implementations (mocking, debug versions)

---

### 4.3 Design for Composition

**❌ Bad: Monolithic function with many responsibilities**
```c
int send_http_request_and_parse_json(const char *url, 
                                      struct json_object **result)
{
    /* HTTP connection, SSL, headers, body, JSON parsing all mixed */
}
```

**✅ Reasonable: Composable building blocks**
```c
/* Layer 1: Transport */
struct connection *conn_open(const char *host, int port);
int conn_write(struct connection *conn, const void *buf, size_t len);
int conn_read(struct connection *conn, void *buf, size_t len);
void conn_close(struct connection *conn);

/* Layer 2: HTTP (uses transport) */
struct http_response *http_request(struct connection *conn,
                                   const char *method,
                                   const char *path,
                                   const char *body);
void http_response_free(struct http_response *resp);

/* Layer 3: JSON (independent) */
int json_parse(const char *text, size_t len, struct json_object **out);
void json_free(struct json_object *obj);

/* Composed usage */
int fetch_api_data(const char *host, const char *endpoint,
                   struct json_object **result)
{
    struct connection *conn;
    struct http_response *resp;
    int ret;

    conn = conn_open(host, 443);
    if (!conn)
        return -EIO;

    resp = http_request(conn, "GET", endpoint, NULL);
    conn_close(conn);
    
    if (!resp)
        return -EIO;

    if (resp->status != 200) {
        ret = -EPROTO;
        goto out;
    }

    ret = json_parse(resp->body, resp->body_len, result);

out:
    http_response_free(resp);
    return ret;
}
```

**Benefits**:
- Each layer is independently testable
- Can swap implementations (e.g., mock connection for testing)
- Reuse pieces in different combinations

---

### 4.4 Callback-Based Extension Points

**❌ Bad: Hardcoded behavior**
```c
void process_events(struct event_loop *loop)
{
    while (!loop->stop) {
        struct event *ev = get_next_event(loop);
        
        /* Fixed set of event types */
        switch (ev->type) {
        case EVENT_READ:
            handle_read(ev);
            break;
        case EVENT_WRITE:
            handle_write(ev);
            break;
        /* Cannot add new types without modifying this code */
        }
    }
}
```

**✅ Reasonable: User-supplied callbacks**
```c
typedef void (*event_handler_t)(struct event *ev, void *user_data);

struct event_source {
    int fd;
    uint32_t events;          /* EPOLLIN, EPOLLOUT, etc. */
    event_handler_t handler;
    void *user_data;
};

int event_loop_add(struct event_loop *loop, struct event_source *src);
int event_loop_remove(struct event_loop *loop, int fd);
void event_loop_run(struct event_loop *loop);

/* Usage */
static void on_client_readable(struct event *ev, void *user_data)
{
    struct client *client = user_data;
    /* Handle read */
}

struct event_source src = {
    .fd = client_fd,
    .events = EPOLLIN,
    .handler = on_client_readable,
    .user_data = client,
};
event_loop_add(loop, &src);
```

**Benefits**:
- Event loop code never needs modification
- Users handle domain-specific logic
- Core remains stable and well-tested

---

## 5. Portability

**Principle**: Code should work correctly across different compilers, architectures, and operating systems.

### 5.1 Use Fixed-Width Integer Types

**❌ Bad: Platform-dependent sizes**
```c
int counter;              /* 16, 32, or 64 bits? */
long offset;              /* 32 bits on 32-bit, 64 on 64-bit */
unsigned long flags;      /* Size varies by platform */
```

**✅ Reasonable: Explicit sizes where it matters**
```c
#include <stdint.h>
#include <stddef.h>

uint32_t counter;         /* Always 32 bits */
int64_t offset;           /* Always 64 bits, signed */
uint64_t flags;           /* Always 64 bits */
size_t length;            /* Appropriate for array indexing */
ssize_t result;           /* Signed version for error returns */
uintptr_t addr;           /* Integer that can hold a pointer */

/* For on-disk/network formats, be explicit */
struct __attribute__((packed)) file_header {
    uint32_t magic;
    uint32_t version;
    uint64_t file_size;
    uint32_t checksum;
};
```

**When to use what**:
| Type | Use Case |
|------|----------|
| `int` | Local loop counters, status returns |
| `size_t` | Array sizes, memory sizes |
| `uint32_t` | Explicit 32-bit values (files, network) |
| `uint64_t` | Large counters, addresses |
| `intptr_t` | Pointers stored as integers |

---

### 5.2 Handle Endianness Explicitly

**❌ Bad: Assumes host byte order**
```c
uint32_t read_header(FILE *f)
{
    uint32_t value;
    fread(&value, sizeof(value), 1, f);
    return value;  /* Wrong on opposite-endian machines! */
}
```

**✅ Reasonable: Explicit byte order conversion**
```c
#include <endian.h>  /* Linux */
/* or define your own for portability */

#ifndef htole32
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define htole32(x) (x)
  #define le32toh(x) (x)
  #define htobe32(x) __builtin_bswap32(x)
  #define be32toh(x) __builtin_bswap32(x)
#else
  #define htole32(x) __builtin_bswap32(x)
  #define le32toh(x) __builtin_bswap32(x)
  #define htobe32(x) (x)
  #define be32toh(x) (x)
#endif
#endif

/* Reading little-endian data from file */
uint32_t read_header_le(FILE *f)
{
    uint32_t value;
    fread(&value, sizeof(value), 1, f);
    return le32toh(value);
}

/* Writing network byte order (big-endian) */
void write_network_int(int fd, uint32_t value)
{
    uint32_t net_value = htobe32(value);
    write(fd, &net_value, sizeof(net_value));
}

/* Portable byte-by-byte approach (always works) */
static inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
```

---

### 5.3 Avoid Compiler/Platform Extensions (or Isolate Them)

**❌ Bad: Unguarded compiler extensions**
```c
void log_message(char *fmt, ...) __attribute__((format(printf, 1, 2)));
int count = __builtin_popcount(flags);
typeof(x) temp = x;
```

**✅ Reasonable: Abstracted platform differences**
```c
/* --- compat.h --- */

/* Compiler detection */
#if defined(__GNUC__)
  #define COMPILER_GCC 1
#elif defined(_MSC_VER)
  #define COMPILER_MSVC 1
#elif defined(__clang__)
  #define COMPILER_CLANG 1
#endif

/* Printf format checking */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
  #define PRINTF_FMT(fmt_idx, arg_idx) \
      __attribute__((format(printf, fmt_idx, arg_idx)))
#else
  #define PRINTF_FMT(fmt_idx, arg_idx)
#endif

/* Likely/unlikely branch hints */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
  #define likely(x)   __builtin_expect(!!(x), 1)
  #define unlikely(x) __builtin_expect(!!(x), 0)
#else
  #define likely(x)   (x)
  #define unlikely(x) (x)
#endif

/* Unused parameter marker */
#define UNUSED(x) ((void)(x))

/* Static assert (C11 has _Static_assert) */
#if __STDC_VERSION__ >= 201112L
  #define STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
  #define STATIC_ASSERT(expr, msg) \
      typedef char static_assert_##__LINE__[(expr) ? 1 : -1]
#endif

/* --- Usage --- */
void log_message(const char *fmt, ...) PRINTF_FMT(1, 2);

if (unlikely(error_condition)) {
    handle_error();
}
```

---

### 5.4 Use Standard Library Safely

**❌ Bad: Unsafe string functions**
```c
char buffer[64];
strcpy(buffer, user_input);        /* Buffer overflow */
sprintf(buffer, "Hello %s", name); /* Buffer overflow */
gets(buffer);                      /* Never use gets() */
```

**✅ Reasonable: Bounded operations**
```c
#include <string.h>
#include <stdio.h>

char buffer[64];

/* Use strncpy with explicit null termination */
strncpy(buffer, user_input, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';

/* Better: use snprintf (returns required length) */
int needed = snprintf(buffer, sizeof(buffer), "Hello %s", name);
if (needed >= sizeof(buffer)) {
    /* Truncation occurred */
    handle_truncation();
}

/* Even better: strlcpy if available (returns required length) */
#ifdef HAVE_STRLCPY
size_t needed = strlcpy(buffer, user_input, sizeof(buffer));
#endif

/* Or define your own safe_strcpy */
static inline size_t safe_strcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (len >= size) {
        if (size > 0) {
            memcpy(dst, src, size - 1);
            dst[size - 1] = '\0';
        }
        return len;  /* Would have copied */
    }
    memcpy(dst, src, len + 1);
    return len;
}
```

---

## 6. Testability

**Principle**: Code should be designed to be easily tested in isolation.

### 6.1 Dependency Injection

**❌ Bad: Hard dependencies that can't be mocked**
```c
int process_file(const char *path)
{
    FILE *f = fopen(path, "r");  /* Real filesystem */
    time_t now = time(NULL);     /* Real clock */
    
    /* Cannot test without real files and time sensitivity */
}
```

**✅ Reasonable: Injectable dependencies**
```c
/* Abstract interfaces */
struct file_ops {
    void *(*open)(const char *path, const char *mode);
    size_t (*read)(void *handle, void *buf, size_t size);
    int (*close)(void *handle);
};

struct time_ops {
    time_t (*now)(void);
};

/* Production implementations */
static const struct file_ops real_file_ops = {
    .open = (void *)fopen,
    .read = (void *)fread,
    .close = (void *)fclose,
};

static time_t real_time_now(void) { return time(NULL); }
static const struct time_ops real_time_ops = { .now = real_time_now };

/* Function accepts operations structs */
int process_file(const char *path,
                 const struct file_ops *fops,
                 const struct time_ops *tops)
{
    if (!fops) fops = &real_file_ops;
    if (!tops) tops = &real_time_ops;
    
    void *f = fops->open(path, "r");
    time_t now = tops->now();
    /* ... */
}

/* Test can inject mocks */
static void *mock_open(const char *path, const char *mode)
{
    return (void *)&test_data;  /* Return test data */
}
static time_t mock_now(void) { return 1234567890; }

void test_process_file(void)
{
    struct file_ops mock_fops = { .open = mock_open, /* ... */ };
    struct time_ops mock_tops = { .now = mock_now };
    
    int result = process_file("test.txt", &mock_fops, &mock_tops);
    assert(result == expected);
}
```

---

### 6.2 Pure Functions Where Possible

**❌ Bad: Functions with hidden state**
```c
static int counter = 0;

int get_next_id(void)
{
    return ++counter;  /* Hidden state, not deterministic */
}
```

**✅ Reasonable: Explicit state, pure computation**
```c
/* Pure function: same inputs always produce same outputs */
int compute_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return sum & 0xFFFF;
}

/* State passed explicitly */
struct id_generator {
    uint64_t next_id;
};

void id_gen_init(struct id_generator *gen, uint64_t start)
{
    gen->next_id = start;
}

uint64_t id_gen_next(struct id_generator *gen)
{
    return gen->next_id++;
}

/* Test can control state */
void test_id_generator(void)
{
    struct id_generator gen;
    id_gen_init(&gen, 100);
    assert(id_gen_next(&gen) == 100);
    assert(id_gen_next(&gen) == 101);
}
```

---

### 6.3 Design for Observability

**❌ Bad: No way to observe internal state**
```c
void connection_manager_run(struct conn_mgr *mgr)
{
    while (!mgr->stop) {
        /* Internal state not visible */
        /* Hard to verify correct behavior in tests */
    }
}
```

**✅ Reasonable: Query methods and statistics**
```c
struct conn_stats {
    uint64_t connections_total;
    uint64_t connections_active;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t errors;
};

/* Query internal state for testing/monitoring */
int conn_mgr_get_stats(struct conn_mgr *mgr, struct conn_stats *stats);
int conn_mgr_get_connection_count(struct conn_mgr *mgr);
int conn_mgr_is_connection_active(struct conn_mgr *mgr, int conn_id);

/* Test can verify behavior */
void test_connection_lifecycle(void)
{
    struct conn_mgr *mgr = conn_mgr_create();
    
    assert(conn_mgr_get_connection_count(mgr) == 0);
    
    int conn = conn_mgr_connect(mgr, "localhost", 8080);
    assert(conn_mgr_get_connection_count(mgr) == 1);
    assert(conn_mgr_is_connection_active(mgr, conn));
    
    conn_mgr_disconnect(mgr, conn);
    assert(conn_mgr_get_connection_count(mgr) == 0);
    
    conn_mgr_destroy(mgr);
}
```

---

### 6.4 Minimal Test Framework Pattern

A simple, portable test framework that works anywhere:

```c
/* --- test_framework.h --- */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

#define TEST(name) static void test_##name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        test_failures++; \
    } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

#define RUN_TEST(name) do { \
    test_count++; \
    printf("Running %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while (0)

static int test_count = 0;
static int test_failures = 0;

#define TEST_MAIN() \
    int main(void) { \
        run_tests(); \
        printf("\n%d tests, %d failures\n", test_count, test_failures); \
        return test_failures > 0 ? 1 : 0; \
    }

#endif

/* --- test_example.c --- */
#include "test_framework.h"
#include "mylib.h"

TEST(buffer_create)
{
    struct buffer *buf = buffer_create(64);
    ASSERT(buf != NULL);
    ASSERT_EQ(buffer_size(buf), 64);
    ASSERT_EQ(buffer_length(buf), 0);
    buffer_destroy(buf);
}

TEST(buffer_append)
{
    struct buffer *buf = buffer_create(64);
    int ret = buffer_append(buf, "hello", 5);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(buffer_length(buf), 5);
    ASSERT_STR_EQ(buffer_data(buf), "hello");
    buffer_destroy(buf);
}

static void run_tests(void)
{
    RUN_TEST(buffer_create);
    RUN_TEST(buffer_append);
}

TEST_MAIN()
```

---

## 7. Project Structure & Organization

### 7.1 Recommended Directory Layout

```
project/
├── README.md              # Project overview, build instructions
├── LICENSE
├── Makefile               # Or CMakeLists.txt, meson.build
├── .gitignore
│
├── include/               # Public headers (API)
│   └── project/
│       ├── core.h
│       ├── utils.h
│       └── types.h
│
├── src/                   # Implementation files
│   ├── core/
│   │   ├── core.c
│   │   └── core_internal.h   # Private headers
│   ├── utils/
│   │   └── utils.c
│   └── main.c             # Entry point (for executables)
│
├── lib/                   # Third-party libraries (vendored)
│   └── thirdparty/
│
├── tests/                 # Test files
│   ├── test_core.c
│   ├── test_utils.c
│   └── test_main.c
│
├── tools/                 # Build/development scripts
│   ├── check_style.sh
│   └── run_tests.sh
│
├── docs/                  # Documentation
│   ├── api/
│   └── design/
│
├── examples/              # Example programs
│   └── basic_usage.c
│
└── build/                 # Build output (gitignored)
    ├── obj/
    ├── lib/
    └── bin/
```

### 7.2 Key Principles

| Guideline | Rationale |
|-----------|-----------|
| Separate public/private headers | Clear API boundary, stable ABI |
| One module = one directory | Easy to find related code |
| Tests mirror src structure | Easy to locate tests |
| Build output isolated | Clean gitignore, easy clean |

---

## 8. Header File Rules

### 8.1 Include Guard Convention

```c
/* include/project/buffer.h */

#ifndef PROJECT_BUFFER_H
#define PROJECT_BUFFER_H

/* 
 * Include guard naming: PROJECT_FILENAME_H
 * Use the full path for uniqueness
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Content here */

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_BUFFER_H */
```

### 8.2 Include Order

```c
/*
 * Include order (each group separated by blank line):
 * 1. Corresponding header (for .c files)
 * 2. System headers
 * 3. Third-party library headers
 * 4. Project headers
 */

/* src/buffer.c */
#include "project/buffer.h"      /* 1. Own header first */

#include <stddef.h>              /* 2. System headers */
#include <stdint.h>
#include <string.h>

#include <zlib.h>                /* 3. Third-party (if any) */

#include "project/memory.h"      /* 4. Project headers */
#include "buffer_internal.h"     /* 5. Module-private headers */
```

### 8.3 Header Self-Containment

Every header must compile independently:

```c
/* ❌ Bad: Requires prior includes */
/* Assumes <stdint.h> was included by caller */
struct buffer {
    uint8_t *data;
    size_t len;
};

/* ✅ Reasonable: Self-contained */
#ifndef PROJECT_BUFFER_H
#define PROJECT_BUFFER_H

#include <stddef.h>
#include <stdint.h>

struct buffer {
    uint8_t *data;
    size_t len;
};

#endif
```

### 8.4 What Goes in Headers vs. Source Files

| Header (.h) | Source (.c) |
|-------------|-------------|
| Type definitions | Implementation |
| Function declarations | Static functions |
| Macros (public) | Macros (private) |
| Extern variable declarations | Variable definitions |
| Inline functions (small) | Large function bodies |
| Documentation | Implementation details |

---

## 9. Build System Guidelines

### 9.1 Makefile Template (Simple)

```makefile
# Project configuration
PROJECT := mylib
VERSION := 1.0.0

# Directories
SRCDIR  := src
INCDIR  := include
OBJDIR  := build/obj
LIBDIR  := build/lib
BINDIR  := build/bin
TESTDIR := tests

# Compiler configuration
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11
CFLAGS  += -I$(INCDIR)
CFLAGS  += -fPIC

# Debug/Release modes
ifdef DEBUG
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

# Source files
SRCS    := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/**/*.c)
OBJS    := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Library output
LIB_STATIC := $(LIBDIR)/lib$(PROJECT).a
LIB_SHARED := $(LIBDIR)/lib$(PROJECT).so

# Default target
all: $(LIB_STATIC) $(LIB_SHARED)

# Create directories
$(OBJDIR) $(LIBDIR) $(BINDIR):
	mkdir -p $@

# Compile objects
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Static library
$(LIB_STATIC): $(OBJS) | $(LIBDIR)
	$(AR) rcs $@ $^

# Shared library
$(LIB_SHARED): $(OBJS) | $(LIBDIR)
	$(CC) -shared -o $@ $^

# Tests
TEST_SRCS := $(wildcard $(TESTDIR)/*.c)
TEST_OBJS := $(TEST_SRCS:$(TESTDIR)/%.c=$(OBJDIR)/test/%.o)
TEST_BIN  := $(BINDIR)/run_tests

$(OBJDIR)/test/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(LIB_STATIC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) -L$(LIBDIR) -l$(PROJECT)

# Clean
clean:
	rm -rf build/

# Install (example)
PREFIX  ?= /usr/local
install: $(LIB_STATIC) $(LIB_SHARED)
	install -d $(PREFIX)/lib $(PREFIX)/include/$(PROJECT)
	install -m 644 $(LIB_STATIC) $(LIB_SHARED) $(PREFIX)/lib/
	install -m 644 $(INCDIR)/$(PROJECT)/*.h $(PREFIX)/include/$(PROJECT)/

.PHONY: all test clean install
```

### 9.2 Compiler Warning Flags

Recommended flags for different levels:

```makefile
# Minimum (always use)
CFLAGS_WARN := -Wall -Wextra

# Recommended for production code
CFLAGS_WARN += -Werror              # Treat warnings as errors
CFLAGS_WARN += -Wpedantic           # ISO C compliance
CFLAGS_WARN += -Wshadow             # Variable shadowing
CFLAGS_WARN += -Wconversion         # Implicit conversions
CFLAGS_WARN += -Wstrict-prototypes  # Function prototypes

# Extra paranoid (good for libraries)
CFLAGS_PARANOID := $(CFLAGS_WARN)
CFLAGS_PARANOID += -Wformat=2       # Printf format checks
CFLAGS_PARANOID += -Wundef          # Undefined macros in #if
CFLAGS_PARANOID += -Wcast-align     # Pointer alignment
CFLAGS_PARANOID += -Wwrite-strings  # Const string literals
```

---

## 10. Documentation Standards

### 10.1 Function Documentation (Kernel-doc Style)

```c
/**
 * buffer_append - Append data to buffer
 * @buf: Target buffer (must not be NULL)
 * @data: Data to append
 * @len: Length of data in bytes
 *
 * Appends @len bytes from @data to @buf, growing the buffer if necessary.
 * If the buffer cannot grow (memory allocation failure), the buffer is
 * left unchanged.
 *
 * Context: May sleep (calls kmalloc with GFP_KERNEL).
 *
 * Return:
 *   0 on success
 *   -ENOMEM if memory allocation fails
 *   -EINVAL if @buf is NULL
 */
int buffer_append(struct buffer *buf, const void *data, size_t len);
```

### 10.2 File Header

```c
// SPDX-License-Identifier: MIT
/*
 * buffer.c - Dynamic buffer implementation
 *
 * Copyright (C) 2024 Author Name <email@example.com>
 *
 * This file implements a growable byte buffer with support for
 * append, prepend, and slice operations. Thread-safe if compiled
 * with -DBUFFER_THREAD_SAFE.
 *
 * See include/project/buffer.h for the public API.
 */
```

### 10.3 Inline Comments

```c
/*
 * Multi-line comment for explaining complex logic.
 * Use complete sentences and explain WHY, not WHAT.
 */

int result = complex_calculation();  /* Single-line: end-of-line OK */

/* 
 * Block comment before a section of related statements.
 * Preferred over end-of-line comments for explanations.
 */
```

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    C Systems Programming Quick Reference                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  NAMING                          │  ERROR HANDLING                          │
│  ─────────                       │  ───────────────                         │
│  • functions: subsystem_verb()   │  • Return 0 success, negative errno      │
│  • structs: struct subsystem_x   │  • Use goto for cleanup                  │
│  • macros: SUBSYSTEM_NAME        │  • Check NULL early, return fast         │
│  • locals: short, contextual     │  • Document error codes                  │
│                                  │                                          │
│  HEADERS                         │  INTEGERS                                │
│  ────────                        │  ─────────                               │
│  • Guard: PROJECT_FILENAME_H     │  • size_t for sizes                      │
│  • Self-contained                │  • int32_t/uint32_t for explicit size    │
│  • Own header first in .c        │  • int for loop counters, returns        │
│  • C++ extern "C" guards         │  • Avoid bare int for data structures    │
│                                  │                                          │
│  MEMORY                          │  STRINGS                                 │
│  ───────                         │  ────────                                │
│  • Check all allocations         │  • Use snprintf, not sprintf             │
│  • Pair alloc/free in same file  │  • Always null-terminate                 │
│  • Zero-initialize structs       │  • strncpy + explicit '\0'               │
│  • Set pointers to NULL after    │  • Check return value for truncation     │
│    free                          │                                          │
│                                  │                                          │
│  PORTABILITY                     │  TESTING                                 │
│  ────────────                    │  ────────                                │
│  • Endian: use le32toh, etc.     │  • Dependency injection                  │
│  • Wrap compiler extensions      │  • Pure functions where possible         │
│  • Use stdint.h types            │  • Query methods for state               │
│  • -Wall -Wextra -Werror         │  • Small, focused test cases             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

**Document Version**: 1.0  
**Target Audience**: Experienced C/C++ engineers working on systems software  
**Influences**: Linux kernel coding style, GNU coding standards, CERT C, Google C++ Style Guide (C-applicable parts)

