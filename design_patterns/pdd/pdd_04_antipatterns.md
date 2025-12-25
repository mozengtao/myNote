# PDD Anti-Patterns & Failure Modes

## Introduction

```
+------------------------------------------------------------------+
|  RECOGNIZING PDD VIOLATIONS IS AS IMPORTANT AS KNOWING PDD       |
+------------------------------------------------------------------+

    Anti-patterns cause:
    
    1. CASCADING CHANGES - One change ripples everywhere
    2. TESTING DIFFICULTY - Can't test without full system
    3. TECHNOLOGY LOCK-IN - Can't swap implementations
    4. COGNITIVE OVERLOAD - Must understand everything at once
    5. BUG AMPLIFICATION - Bugs spread across layers
```

---

## Anti-Pattern 1: Anemic Domain Layer

### What It Is

```
+------------------------------------------------------------------+
|  ANEMIC DOMAIN: Logic Scattered, Domain Is Just Data             |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Domain layer contains only data structures                 │
    │  All logic lives in Presentation or Data layers            │
    │  Domain has no behavior, just getters/setters              │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Business rules duplicated everywhere                       │
    │  Presentation must understand storage details              │
    │  No single source of truth for policies                    │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* ANEMIC DOMAIN - Domain is just data */

/* domain/order.h - Anemic: only data, no behavior */
struct order {
    int id;
    double total;
    int status;  /* Just a value */
    char customer[64];
};

/* presentation/api.c - Logic lives HERE (WRONG!) */
int handle_submit_order(struct order *o)
{
    /* Business rule in presentation! */
    if (o->total < 10.0) {
        return -1;  /* Minimum order amount */
    }
    
    /* Business rule in presentation! */
    if (o->status != 0) {
        return -1;  /* Can only submit new orders */
    }
    
    /* Validation in presentation! */
    if (strlen(o->customer) == 0) {
        return -1;
    }
    
    /* State transition in presentation! */
    o->status = 1;  /* Magic number */
    
    return db_save_order(o);  /* Calls data directly */
}
```

### Correct Version

```c
/* RICH DOMAIN - Domain owns behavior */

/* domain/order.h */
enum order_status {
    ORDER_NEW = 0,
    ORDER_SUBMITTED,
    ORDER_APPROVED,
    ORDER_SHIPPED,
    ORDER_DELIVERED,
};

struct order {
    int id;
    double total;
    enum order_status status;
    char customer[64];
};

/* DOMAIN owns validation */
bool order_validate(const struct order *o);

/* DOMAIN owns state transitions */
enum order_result order_submit(struct order *o);
enum order_result order_approve(struct order *o);

/* DOMAIN owns business rules */
bool order_can_cancel(const struct order *o);
double order_calculate_tax(const struct order *o);

/* domain/order.c */
#define MIN_ORDER_AMOUNT 10.0

enum order_result order_submit(struct order *o)
{
    /* DOMAIN enforces rules */
    if (o->status != ORDER_NEW)
        return ORDER_INVALID_STATE;
    
    if (o->total < MIN_ORDER_AMOUNT)
        return ORDER_BELOW_MINIMUM;
    
    if (!order_validate(o))
        return ORDER_INVALID;
    
    /* DOMAIN controls state transition */
    o->status = ORDER_SUBMITTED;
    return ORDER_OK;
}

/* presentation/api.c - Now thin */
int handle_submit_order(struct order *o)
{
    enum order_result r = order_submit(o);  /* Delegate to domain */
    if (r != ORDER_OK)
        return api_error(r);
    
    return order_repo_save(o);  /* Use repository */
}
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Anemic Domain                              |
+------------------------------------------------------------------+

    □ Domain structs have only data fields, no functions
    □ Presentation layer has if/switch for business rules
    □ Same validation logic appears in multiple places
    □ Magic numbers (0, 1, 2) instead of enums in domain
    □ State changes happen outside domain layer
    □ Presentation directly calls data layer
    □ Domain functions are just simple getters/setters
```

---

## Anti-Pattern 2: Fat Presentation Layer

### What It Is

```
+------------------------------------------------------------------+
|  FAT PRESENTATION: Too Much Logic at the Boundary                |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Syscall handlers contain algorithms                        │
    │  CLI parsers make business decisions                        │
    │  Protocol handlers validate business rules                  │
    │  UI code calculates results                                 │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Adding new UI requires copying all logic                  │
    │  Can't test business logic without UI                      │
    │  Protocol bugs become business bugs                        │
    │  Different UIs behave differently                          │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* FAT PRESENTATION - Logic in syscall handler */

/* Kernel example: too much in syscall */
SYSCALL_DEFINE3(my_read, int, fd, char __user *, buf, size_t, count)
{
    struct file *f = fget(fd);
    if (!f)
        return -EBADF;
    
    /* Business logic in presentation! */
    if (f->f_mode & FMODE_SPECIAL) {
        /* Complex special handling */
        if (count > MAX_SPECIAL_SIZE) {
            count = MAX_SPECIAL_SIZE;
        }
        /* More special logic... */
    }
    
    /* Policy decision in presentation! */
    if (current->audit_enabled && is_sensitive_file(f)) {
        log_access(current, f);
    }
    
    /* Algorithm in presentation! */
    size_t chunk_size = calculate_optimal_chunk(f, count);
    char *kbuf = kmalloc(chunk_size, GFP_KERNEL);
    
    /* ... 100 more lines of logic ... */
}
```

### Correct Version

```c
/* THIN PRESENTATION - Just adapt and dispatch */

SYSCALL_DEFINE3(my_read, int, fd, char __user *, buf, size_t, count)
{
    struct file *f;
    ssize_t ret;
    
    /* PRESENTATION: Validate user parameters */
    if (!access_ok(VERIFY_WRITE, buf, count))
        return -EFAULT;
    
    /* PRESENTATION: Translate fd to internal object */
    f = fget(fd);
    if (!f)
        return -EBADF;
    
    /* DISPATCH TO DOMAIN */
    ret = vfs_read(f, buf, count, &f->f_pos);
    
    fput(f);
    return ret;
}

/* DOMAIN: vfs_read handles all policy */
ssize_t vfs_read(struct file *f, char __user *buf, 
                 size_t count, loff_t *pos)
{
    /* Domain policy: audit if needed */
    if (f->f_flags & F_AUDIT)
        audit_file_access(f, AUDIT_READ);
    
    /* Domain policy: adjust count */
    count = file_adjust_read_count(f, count);
    
    /* Domain: dispatch to file operations */
    return f->f_op->read(f, buf, count, pos);
}
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Fat Presentation                           |
+------------------------------------------------------------------+

    □ Syscall/handler functions are > 50 lines
    □ Business conditionals (if special_case) in handlers
    □ Algorithms implemented in presentation code
    □ Policy decisions made in protocol parsing
    □ Multiple presentation paths with duplicated logic
    □ Hard to add new presentation (REST, gRPC, etc.)
```

---

## Anti-Pattern 3: Business Logic in Data Layer

### What It Is

```
+------------------------------------------------------------------+
|  LOGIC IN DATA: Storage Knows Too Much                           |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Database stored procedures contain business rules          │
    │  Driver decides when to accept/reject operations           │
    │  File format encodes business policies                      │
    │  Cache invalidation depends on business state              │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Can't change storage without breaking business            │
    │  Business rules scattered across data implementations      │
    │  Testing requires real storage                             │
    │  Storage performance affects business logic                │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* LOGIC IN DATA LAYER - Storage makes decisions */

/* data/user_storage.c - BAD: business rules here */
int storage_create_user(const char *username, const char *password)
{
    /* Business rule in data layer! */
    if (strlen(username) < 3 || strlen(username) > 20) {
        return -EINVAL;  /* Validation here */
    }
    
    /* Business rule in data layer! */
    if (!is_valid_email(username)) {
        return -EINVAL;  /* Business format check */
    }
    
    /* Business rule in data layer! */
    char hashed[64];
    if (!hash_password(password, hashed)) {  /* Security logic */
        return -EINVAL;
    }
    
    /* Business rule in data layer! */
    if (count_users() >= MAX_USERS) {
        return -ENOSPC;  /* Quota enforcement */
    }
    
    /* Finally, actual storage */
    return db_insert("users", username, hashed);
}
```

### Correct Version

```c
/* DOMAIN owns business logic */

/* domain/user.c - Domain validates and decides */
struct user {
    char username[64];
    char password_hash[64];
    enum user_status status;
};

enum user_result user_create(struct user *u, 
                             const char *username,
                             const char *password)
{
    /* DOMAIN: All validation here */
    if (!validate_username(username))
        return USER_INVALID_NAME;
    
    if (!validate_email_format(username))
        return USER_INVALID_EMAIL;
    
    if (!validate_password_strength(password))
        return USER_WEAK_PASSWORD;
    
    /* DOMAIN: Check quota */
    if (user_count >= user_quota)
        return USER_QUOTA_EXCEEDED;
    
    /* DOMAIN: Prepare user object */
    strncpy(u->username, username, sizeof(u->username));
    hash_password(password, u->password_hash);
    u->status = USER_PENDING;
    
    return USER_OK;
}

/* data/user_storage.c - Pure storage */
int user_storage_save(void *ctx, const struct user *u)
{
    /* DATA: Only storage concerns */
    struct db_conn *db = ctx;
    
    return db_insert(db, "users",
                     u->username,
                     u->password_hash,
                     u->status);
}
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Logic in Data Layer                        |
+------------------------------------------------------------------+

    □ Storage functions validate business rules
    □ Database has complex stored procedures
    □ Driver rejects operations based on business state
    □ Can't change storage backend without changing behavior
    □ Unit tests require database/hardware
    □ Storage layer imports domain headers
```

---

## Anti-Pattern 4: Bidirectional Dependencies

### What It Is

```
+------------------------------------------------------------------+
|  BIDIRECTIONAL: Layers Depend on Each Other                      |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Domain includes presentation headers                       │
    │  Data layer calls back into domain                          │
    │  Circular includes between layers                           │
    │  Changes in any layer affect all layers                     │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Compile-time dependency cycles                             │
    │  Cannot test layers independently                          │
    │  Change amplification                                       │
    │  Architectural erosion over time                           │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* BIDIRECTIONAL DEPENDENCIES */

/* domain/engine.h */
#include "presentation/display.h"  /* BAD: Domain knows presentation */

struct engine {
    struct display *display;  /* BAD: Domain holds presentation */
    /* ... */
};

void engine_process(struct engine *e)
{
    /* ... processing ... */
    display_update(e->display, result);  /* BAD: Domain calls presentation */
}

/* data/storage.h */
#include "domain/validator.h"  /* BAD: Data knows domain */

int storage_save(struct record *r)
{
    /* BAD: Data validates using domain */
    if (!domain_validate(r))
        return -1;
    
    /* ... save ... */
}
```

### Correct Version

```c
/* UNIDIRECTIONAL: Callbacks for upward communication */

/* domain/engine.h - Domain defines callback interface */
typedef void (*engine_result_cb)(void *ctx, const struct result *r);

struct engine {
    /* No presentation dependency */
    engine_result_cb on_result;
    void *callback_ctx;
};

void engine_set_callback(struct engine *e, 
                         engine_result_cb cb, void *ctx)
{
    e->on_result = cb;
    e->callback_ctx = ctx;
}

void engine_process(struct engine *e)
{
    struct result r;
    /* ... processing ... */
    
    /* Notify via callback - domain doesn't know who handles it */
    if (e->on_result)
        e->on_result(e->callback_ctx, &r);
}

/* presentation/display.c - Presentation registers callback */
static void on_engine_result(void *ctx, const struct result *r)
{
    struct display *d = ctx;
    display_update(d, r);  /* Presentation knows domain types */
}

void display_connect_engine(struct display *d, struct engine *e)
{
    engine_set_callback(e, on_engine_result, d);
}
```

### Dependency Injection Pattern

```c
/* DEPENDENCY INJECTION: Domain depends on interface, not implementation */

/* domain/storage_if.h - Domain defines what it needs */
struct storage_interface {
    int (*save)(void *ctx, const void *data, size_t size);
    int (*load)(void *ctx, void *data, size_t size);
};

/* domain/service.c - Domain uses interface */
struct service {
    const struct storage_interface *storage;
    void *storage_ctx;
};

int service_do_work(struct service *s)
{
    /* Domain uses storage via interface */
    return s->storage->save(s->storage_ctx, &result, sizeof(result));
}

/* data/file_storage.c - Data implements interface */
static int file_save(void *ctx, const void *data, size_t size) { ... }
static int file_load(void *ctx, void *data, size_t size) { ... }

const struct storage_interface file_storage = {
    .save = file_save,
    .load = file_load,
};

/* main.c - Wire up at initialization */
struct service svc;
svc.storage = &file_storage;  /* Inject data layer */
svc.storage_ctx = file_ctx;
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Bidirectional Dependencies                 |
+------------------------------------------------------------------+

    □ Domain .c files include presentation .h files
    □ Data .c files include domain .h files (except interfaces)
    □ Circular #include guards needed
    □ Compilation order matters
    □ Changing one layer requires recompiling all layers
    □ Cannot link domain code without presentation code
```

---

## Anti-Pattern 5: Interface Explosion

### What It Is

```
+------------------------------------------------------------------+
|  INTERFACE EXPLOSION: Too Many Tiny Interfaces                   |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Every function has its own interface struct                │
    │  Hundreds of single-method ops tables                       │
    │  Layers communicate through many narrow contracts           │
    │  More interface code than implementation                    │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Cognitive overload - too many abstractions                │
    │  Boilerplate dominates codebase                            │
    │  Hard to understand data flow                              │
    │  Performance overhead from indirection                     │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* INTERFACE EXPLOSION - Every operation is an interface */

/* Too many interfaces! */
struct user_validator_ops {
    bool (*validate)(void *ctx, const char *s);
};

struct user_hasher_ops {
    int (*hash)(void *ctx, const char *in, char *out);
};

struct user_counter_ops {
    int (*count)(void *ctx);
};

struct user_max_getter_ops {
    int (*get_max)(void *ctx);
};

struct user_saver_ops {
    int (*save)(void *ctx, const struct user *u);
};

/* ... 20 more single-method interfaces ... */

/* Using them is painful */
int create_user(struct user_service *svc, ...)
{
    if (!svc->validator_ops->validate(svc->validator_ctx, username))
        return -1;
    svc->hasher_ops->hash(svc->hasher_ctx, password, hash);
    if (svc->counter_ops->count(svc->counter_ctx) >= 
        svc->max_getter_ops->get_max(svc->max_ctx))
        return -1;
    return svc->saver_ops->save(svc->saver_ctx, &user);
}
```

### Correct Version

```c
/* COHESIVE INTERFACES - Group related operations */

/* One interface for user storage concerns */
struct user_storage_ops {
    int (*save)(void *ctx, const struct user *u);
    int (*load)(void *ctx, int id, struct user *u);
    int (*delete)(void *ctx, int id);
    int (*count)(void *ctx);
    int (*list)(void *ctx, struct user *out, int max);
};

/* One interface for crypto concerns */
struct crypto_ops {
    int (*hash_password)(void *ctx, const char *in, char *out);
    bool (*verify_password)(void *ctx, const char *plain, const char *hash);
    int (*generate_token)(void *ctx, char *out, size_t size);
};

/* Clean usage */
int create_user(struct user_service *svc, ...)
{
    if (!validate_username(username))  /* Domain function */
        return -1;
    svc->crypto->hash_password(svc->crypto_ctx, password, hash);
    if (svc->storage->count(svc->storage_ctx) >= svc->max_users)
        return -1;
    return svc->storage->save(svc->storage_ctx, &user);
}
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Interface Explosion                        |
+------------------------------------------------------------------+

    □ More interface structs than implementation files
    □ Single-method ops tables everywhere
    □ Context passing requires many pointers
    □ Simple operations need multiple interface calls
    □ Interface definitions longer than implementations
    □ Developers confused about which interface to use
```

---

## Anti-Pattern 6: Over-Layering

### What It Is

```
+------------------------------------------------------------------+
|  OVER-LAYERING: Layers for the Sake of Layers                    |
+------------------------------------------------------------------+

    SYMPTOM:
    ┌─────────────────────────────────────────────────────────────┐
    │  5+ layers when 3 would suffice                             │
    │  Layers that just forward calls                             │
    │  "Manager" layers that manage nothing                       │
    │  Every struct wrapped in another struct                     │
    └─────────────────────────────────────────────────────────────┘

    RESULT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Hard to trace execution                                    │
    │  Performance death by indirection                          │
    │  Simple changes touch many files                           │
    │  Developers lose sight of actual work                      │
    └─────────────────────────────────────────────────────────────┘
```

### Code Example (BAD)

```c
/* OVER-LAYERING - Useless intermediate layers */

/* Layer 1: Presentation */
int api_create_user(struct request *r) {
    return user_controller_create(r);  /* Just forwards */
}

/* Layer 2: Controller (useless) */
int user_controller_create(struct request *r) {
    return user_service_create(r);  /* Just forwards */
}

/* Layer 3: Service (useless) */
int user_service_create(struct request *r) {
    return user_manager_create(r);  /* Just forwards */
}

/* Layer 4: Manager (useless) */
int user_manager_create(struct request *r) {
    return user_repository_create(r);  /* Just forwards */
}

/* Layer 5: Repository (useless wrapper) */
int user_repository_create(struct request *r) {
    return user_dao_create(r);  /* Just forwards */
}

/* Layer 6: DAO - Finally does something */
int user_dao_create(struct request *r) {
    /* Actual database work */
}
```

### Correct Version

```c
/* MINIMAL LAYERING - Only layers that add value */

/* Presentation: Parse input, format output */
int api_create_user(const char *json, char *response, size_t size)
{
    struct user_request req;
    if (parse_user_request(json, &req) < 0)
        return format_error(response, size, "invalid request");
    
    struct user u;
    enum user_result r = user_create(&u, &req);  /* Domain */
    if (r != USER_OK)
        return format_error(response, size, user_result_str(r));
    
    if (user_storage_save(&u) < 0)  /* Data */
        return format_error(response, size, "storage failed");
    
    return format_user(response, size, &u);
}

/* Domain: Business logic and validation */
enum user_result user_create(struct user *u, const struct user_request *req)
{
    /* Validation, business rules, state management */
}

/* Data: Storage */
int user_storage_save(const struct user *u)
{
    /* Database/file operations */
}

/* Three layers. Done. */
```

### How to Recognize

```
+------------------------------------------------------------------+
|  DETECTION CHECKLIST: Over-Layering                              |
+------------------------------------------------------------------+

    □ Functions that just call another function
    □ Classes/structs that wrap single other class/struct
    □ "Manager", "Handler", "Helper" suffixes everywhere
    □ More than 5 layers in call stack for simple operations
    □ Can't explain what each layer adds
    □ Removing a layer wouldn't break anything
```

---

## Summary: Anti-Pattern Quick Reference

```
+------------------------------------------------------------------+
|  ANTI-PATTERN SUMMARY                                            |
+------------------------------------------------------------------+

    ANEMIC DOMAIN
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Domain is just data, no behavior                 │
    │  Symptom:  Logic in presentation/data layers               │
    │  Fix:      Move business rules into domain                  │
    └─────────────────────────────────────────────────────────────┘

    FAT PRESENTATION
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Too much logic at system boundary               │
    │  Symptom:  50+ line handlers, business rules in parsing    │
    │  Fix:      Thin presentation, delegate to domain           │
    └─────────────────────────────────────────────────────────────┘

    LOGIC IN DATA
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Storage makes business decisions                │
    │  Symptom:  Validation in drivers, rules in stored procs    │
    │  Fix:      Data only stores, domain decides                │
    └─────────────────────────────────────────────────────────────┘

    BIDIRECTIONAL DEPENDENCIES
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Layers depend on each other                     │
    │  Symptom:  Circular includes, can't compile separately     │
    │  Fix:      Callbacks, dependency injection                 │
    └─────────────────────────────────────────────────────────────┘

    INTERFACE EXPLOSION
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Too many small interfaces                       │
    │  Symptom:  Single-method ops tables everywhere             │
    │  Fix:      Cohesive interfaces grouping related ops       │
    └─────────────────────────────────────────────────────────────┘

    OVER-LAYERING
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Unnecessary intermediate layers                 │
    │  Symptom:  Functions that just forward calls               │
    │  Fix:      Minimal layering - only if value added         │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **贫血领域**：领域层只有数据没有行为，业务逻辑散落在其他层
- **臃肿展示层**：系统边界代码包含太多业务逻辑
- **数据层包含逻辑**：存储层做业务决策（验证、规则）
- **双向依赖**：层之间相互依赖，无法独立编译测试
- **接口爆炸**：太多细粒度接口，认知负担过重
- **过度分层**：不必要的中间层，只是转发调用

