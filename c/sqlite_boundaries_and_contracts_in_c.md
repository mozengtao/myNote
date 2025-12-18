# Boundaries and Contracts in C Architectures

A professional-level guide for systems architects on designing, identifying,
and enforcing architectural boundaries in large, long-lived C codebases.

---

## Table of Contents

1. [What Is a Boundary in C](#step-1--what-is-a-boundary-in-c)
2. [Common Architectural Layers](#step-2--common-architectural-layers-in-c)
3. [Boundary Types and Contracts](#step-3--boundary-types-and-contracts)
4. [Contracts as C Interfaces](#step-4--contracts-as-c-interfaces)
5. [Allowed Interaction Patterns](#step-5--allowed-interaction-patterns-between-layers)
6. [Forbidden Interactions (Anti-Patterns)](#step-6--forbidden-interactions-anti-patterns)
7. [Contracts Over Time](#step-7--contracts-over-time-evolution)
8. [Mapping Boundaries to Real Systems: SQLite](#step-8--mapping-boundaries-to-real-systems)
9. [Boundary Review Checklist](#step-9--boundary-review-checklist)

---

## Step 1 — What Is a Boundary in C

### Definition

A **boundary** in C is a deliberately chosen line of separation between
components that defines:

- What knowledge each side may have of the other
- What operations are permitted across the line
- What invariants must hold when crossing the line

Unlike languages with modules, packages, or visibility keywords (`private`,
`protected`, `internal`), C provides **no compiler-enforced encapsulation**.
Every symbol is potentially visible. Every struct field is accessible if
the definition is available. This makes boundaries a matter of
**discipline, convention, and tooling**—not language enforcement.

### Why Boundaries Exist

```
+---------------------------------------------------------------+
|                    WHY BOUNDARIES MATTER                      |
+---------------------------------------------------------------+
|                                                               |
|  Without Boundaries          With Boundaries                  |
|  ------------------          ---------------                  |
|                                                               |
|   +---+    +---+              +---+     +---+                  |
|   | A |<-->| B |              | A |---->| B |                  |
|   +---+    +---+              +---+     +---+                  |
|     ^        ^                  |         |                   |
|     |        |                  v         v                   |
|     v        v              [CONTRACT] [CONTRACT]             |
|   +---+    +---+                |         |                   |
|   | C |<-->| D |              +---+     +---+                  |
|   +---+    +---+              | C |     | D |                  |
|                               +---+     +---+                  |
|   All components know                                         |
|   about all other            Dependencies flow                |
|   components                 in one direction                 |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- 左侧：无边界的系统，所有组件双向依赖，形成紧耦合
- 右侧：有边界的系统，依赖单向流动，通过契约交互
- CONTRACT 表示明确定义的接口，限制跨边界的交互

Boundaries exist to solve these problems:

| Problem | Without Boundaries | With Boundaries |
|---------|-------------------|-----------------|
| **Change Propagation** | One change ripples through entire codebase | Changes contained within component |
| **Cognitive Load** | Must understand everything to change anything | Can reason about isolated components |
| **Testing** | Integration tests required everywhere | Unit tests possible at each layer |
| **Team Coordination** | Merge conflicts, stepping on toes | Clear ownership, parallel development |
| **Bug Isolation** | Defects can originate anywhere | Invariants narrow the search space |

### What Problems Boundaries Prevent Over Time

1. **Architectural Erosion**: Without explicit boundaries, expedient shortcuts
   accumulate. A "quick fix" that bypasses the proper layer becomes permanent.
   After years, the original architecture is unrecognizable.

2. **Dependency Tangles**: When any module can call any other module,
   dependency graphs become cyclic. Build times explode. Incremental
   compilation becomes impossible.

3. **Untestable Code**: If low-level code depends on high-level policy,
   testing requires the entire system. Mocking becomes impractical.

4. **Knowledge Diffusion**: Without boundaries, implementation details leak.
   Callers start depending on internal behavior. Changing that behavior
   breaks unknown dependents.

### Conceptual vs. Code-Level Boundaries

| Aspect | Conceptual Boundary | Code-Level Boundary |
|--------|--------------------|--------------------|
| **Definition** | A logical separation understood by developers | A separation enforced through code organization |
| **Expression** | Documentation, diagrams, team agreements | Header files, directory structure, naming conventions |
| **Enforcement** | Code review, architectural guidelines | Compiler visibility, linker controls, static analysis |
| **Durability** | Erodes without vigilance | Survives developer turnover |
| **Examples** | "The pager owns page caching" | `pager.h` exposes only `Pager*` opaque type |

**Key insight**: Conceptual boundaries define the intent; code-level boundaries
implement that intent in a way that survives human fallibility.

---

## Step 2 — Common Architectural Layers in C

Large C systems typically organize into horizontal layers, where each layer
has a single responsibility and strict rules about what it may depend on.

```
+================================================================+
|                    LAYER ARCHITECTURE                          |
+================================================================+
|                                                                |
|   +--------------------------------------------------------+   |
|   |          APPLICATION / POLICY LAYER                    |   |
|   |   (SQL parser, query planner, user configuration)      |   |
|   +--------------------------------------------------------+   |
|                            |                                   |
|                            | calls down                        |
|                            v                                   |
|   +--------------------------------------------------------+   |
|   |           DOMAIN / SERVICE LAYER                       |   |
|   |   (Virtual machine, expression evaluator, triggers)    |   |
|   +--------------------------------------------------------+   |
|                            |                                   |
|                            | calls down                        |
|                            v                                   |
|   +--------------------------------------------------------+   |
|   |            CORE / MECHANISM LAYER                      |   |
|   |   (B-tree, pager, page cache, transaction manager)     |   |
|   +--------------------------------------------------------+   |
|                            |                                   |
|                            | calls down                        |
|                            v                                   |
|   +--------------------------------------------------------+   |
|   |         INFRASTRUCTURE / OS LAYER                      |   |
|   |   (VFS, memory allocator, mutex, file I/O)             |   |
|   +--------------------------------------------------------+   |
|                                                                |
+================================================================+
```

**图解说明 (Diagram Explanation):**
- APPLICATION/POLICY: 最高层，处理用户策略和高级逻辑
- DOMAIN/SERVICE: 业务逻辑层，执行核心算法
- CORE/MECHANISM: 底层机制，提供数据结构和存储
- INFRASTRUCTURE/OS: 最底层，抽象操作系统接口
- 箭头表示允许的调用方向：只能向下调用

### Layer Details

#### 1. Application / Policy Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Interpret user intent, make policy decisions, coordinate high-level workflows |
| **Allowed Dependencies** | Domain layer, Core layer (for initialization), Infrastructure (for configuration) |
| **Forbidden Dependencies** | Nothing—this is the top layer |
| **Typical Volatility** | HIGH — changes with new features, user requirements |
| **Examples** | SQL parser, command-line interface, configuration management, query planner |

The policy layer answers questions like:
- Should this query use an index?
- What isolation level applies?
- How should conflicts be resolved?

#### 2. Domain / Service Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Implement business logic, execute operations, coordinate core mechanisms |
| **Allowed Dependencies** | Core layer, Infrastructure layer |
| **Forbidden Dependencies** | Application layer (no upward calls) |
| **Typical Volatility** | MEDIUM — changes with algorithm improvements, new operations |
| **Examples** | Virtual machine (VDBE), expression evaluator, trigger executor, backup system |

The domain layer knows *how* to do things, but not *whether* to do them.
It receives commands from above and coordinates the mechanisms below.

#### 3. Core / Mechanism Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Provide fundamental data structures and storage mechanisms |
| **Allowed Dependencies** | Infrastructure layer only |
| **Forbidden Dependencies** | Application layer, Domain layer |
| **Typical Volatility** | LOW — changes only for bug fixes, performance, or fundamental redesign |
| **Examples** | B-tree implementation, pager, page cache, write-ahead log, record codec |

The core layer provides *mechanisms* without *policy*. It does not decide
when to commit a transaction—it provides the capability to commit.

#### 4. Infrastructure / OS Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Abstract operating system services, provide portable interfaces |
| **Allowed Dependencies** | Operating system APIs only |
| **Forbidden Dependencies** | All higher layers |
| **Typical Volatility** | VERY LOW — changes only for new platform support or OS API changes |
| **Examples** | VFS (Virtual File System), memory allocator, mutex implementation, I/O wrappers |

This layer isolates the codebase from OS-specific details. Porting to a
new platform should require changes only here.

### Dependency Rules Summary

```
+---------------------------------------------------------------+
|              DEPENDENCY FLOW RULES                            |
+---------------------------------------------------------------+
|                                                               |
|    Layer N may depend on Layer N-1, N-2, ..., 0               |
|    Layer N must NOT depend on Layer N+1, N+2, ...             |
|                                                               |
|    Application  -->  Domain  -->  Core  -->  Infrastructure   |
|         |              |           |              |           |
|         |              |           |              v           |
|         |              |           |         [OS APIs]        |
|         |              |           |                          |
|         v              v           v                          |
|    [Higher layers call lower layers, never the reverse]       |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- 依赖方向：从上层指向下层，禁止反向
- 每层只能调用比它更低的层
- Infrastructure 层调用操作系统 API
- 这种单向流动使得每层可以独立测试和替换

---

## Step 3 — Boundary Types and Contracts

A boundary is not useful without a **contract**—an explicit or implicit
agreement about what behavior is guaranteed and what behavior is forbidden.

### 3.1 API Boundaries

**What it is**: The set of functions, types, and constants that one module
exposes to another.

**Contract enforced**:
- Only declared symbols may be used
- Function preconditions must be met by caller
- Function postconditions are guaranteed by implementation
- Return value semantics are fixed

**What is allowed**:
- Calling any function declared in the public header
- Passing arguments that satisfy documented preconditions
- Depending on documented return value meanings

**What is forbidden**:
- Calling internal/static functions
- Depending on undocumented behavior
- Passing arguments that violate preconditions

**What breaks when violated**:
- Undefined behavior (crashes, corruption)
- Silent data corruption
- Broken assumptions in future versions

```c
/* GOOD: Clear API boundary with documented contract */
/* btree.h */
int sqlite3BtreeBeginTrans(Btree *p, int wrflag, int *pSchemaVersion);
/*
** Contract:
** - p must be a valid, open Btree handle
** - wrflag: 0 for read transaction, 1 for write transaction
** - pSchemaVersion: OUT param, may be NULL
** - Returns SQLITE_OK on success, error code otherwise
** - If returns SQLITE_OK, transaction is active until commit/rollback
*/
```

### 3.2 Data Ownership Boundaries

**What it is**: Rules about which component is responsible for allocating,
modifying, and freeing data.

**Contract enforced**:
- Single owner for each piece of data at any time
- Transfer of ownership is explicit
- Borrowers may not outlive owners

**What is allowed**:
- Owner allocates and frees
- Owner may transfer ownership via explicit API
- Non-owners may read (if contract permits)

**What is forbidden**:
- Non-owner freeing data
- Non-owner modifying data (unless explicitly borrowed as mutable)
- Retaining references after ownership transferred

**What breaks when violated**:
- Double-free
- Use-after-free
- Data corruption from unsynchronized writes

```
+---------------------------------------------------------------+
|                 DATA OWNERSHIP MODELS                         |
+---------------------------------------------------------------+
|                                                               |
|   EXCLUSIVE OWNERSHIP     BORROWED REFERENCE    SHARED OWNER  |
|   ------------------      ------------------    ------------  |
|                                                               |
|   +--------+              +--------+            +--------+    |
|   | Owner  |              | Owner  |            | Ref    |    |
|   |  [*p]  |              |  [*p]  |<--,        | Count  |    |
|   +--------+              +--------+   |        |  [3]   |    |
|       |                       |        |        +--------+    |
|       | alloc/free            |        |            ^         |
|       v                       v        |       +----+----+    |
|   [memory]               [memory]      |       |    |    |    |
|                               ^        |      Ref  Ref  Ref   |
|                               |        |                      |
|                           +--------+   |                      |
|                           |Borrower|---'                      |
|                           | [*p]   |  (read only,             |
|                           +--------+   limited lifetime)      |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- EXCLUSIVE OWNERSHIP: 单一所有者负责分配和释放内存
- BORROWED REFERENCE: 借用者持有临时引用，不能释放，不能超出生命周期
- SHARED OWNER: 引用计数，多个持有者共享所有权

### 3.3 Control Flow Boundaries

**What it is**: Rules about how execution may flow between components.

**Contract enforced**:
- Synchronous calls return to caller
- Callbacks invoked in defined contexts
- No unexpected re-entrancy

**What is allowed**:
- Direct calls down the layer stack
- Callbacks to registered functions
- Return values and error codes

**What is forbidden**:
- Calling up the layer stack (except via registered callback)
- Blocking indefinitely in lower layers
- Recursive calls that break stack invariants

**What breaks when violated**:
- Deadlocks
- Stack overflow
- State corruption from unexpected re-entrancy

### 3.4 Error Propagation Boundaries

**What it is**: Rules about how errors are communicated across boundaries.

**Contract enforced**:
- Errors are reported through defined mechanisms (return codes, errno, etc.)
- Partial operations are either completed or rolled back
- Resources are cleaned up on error paths

**What is allowed**:
- Returning documented error codes
- Setting thread-local error state (errno pattern)
- Logging errors for diagnostics

**What is forbidden**:
- Swallowing errors silently
- Using undefined error codes
- Leaking resources on error paths

**What breaks when violated**:
- Silent failures leading to data corruption
- Resource leaks (memory, file handles, locks)
- Misleading diagnostics

```c
/* Error propagation contract */
#define SQLITE_OK           0   /* Successful result */
#define SQLITE_ERROR        1   /* Generic error */
#define SQLITE_BUSY         5   /* Database is locked */
#define SQLITE_NOMEM        7   /* Out of memory */

/* Contract: All functions return SQLITE_OK on success.
** On failure, return appropriate error code AND ensure
** no partial state changes are visible to caller. */
```

### 3.5 Configuration / Policy Boundaries

**What it is**: Separation between what behavior is performed and
how that behavior is configured.

**Contract enforced**:
- Configuration is set at defined points (initialization, runtime)
- Configuration does not change during critical operations
- Defaults are documented and sensible

**What is allowed**:
- Setting configuration before operations begin
- Querying current configuration
- Changing configuration when safe (between operations)

**What is forbidden**:
- Changing configuration during in-progress operations
- Configuration that makes invariants impossible
- Undocumented configuration coupling

**What breaks when violated**:
- Race conditions
- Impossible states
- Unpredictable behavior

### 3.6 Visibility / Symbol Boundaries

**What it is**: Rules about which symbols (functions, variables, types)
are visible outside a compilation unit.

**Contract enforced**:
- Only intentionally public symbols are exported
- Internal symbols are hidden (static, or linker scripts)
- Symbol names follow conventions indicating scope

**What is allowed**:
- Declaring public API in headers
- Using `static` for file-local functions
- Using naming conventions (underscore prefix for internal)

**What is forbidden**:
- Exposing internal implementation details
- Name collisions with other modules
- Depending on internal symbols from other modules

**What breaks when violated**:
- Linker errors (duplicate symbols)
- Accidental coupling to internals
- Breaking changes when internals change

---

## Step 4 — Contracts as C Interfaces

C provides several mechanisms to express and partially enforce contracts.

### 4.1 Header Files as Contract Documents

The header file is the primary contract document in C. It declares:
- What types exist (opaque or transparent)
- What functions are available
- What constants and macros are defined

```c
/* pager.h - Public interface to the page cache subsystem */

#ifndef SQLITE_PAGER_H
#define SQLITE_PAGER_H

/*
** The Pager handle type. The internals are private.
** Clients may only use Pager* as an opaque pointer.
*/
typedef struct Pager Pager;

/*
** Page handle type. Represents a cached database page.
*/
typedef struct PgHdr DbPage;

/*
** Page number type. Page 1 is the first page. 0 means "no page".
*/
typedef u32 Pgno;

/*
** Open and close a Pager connection.
**
** sqlite3PagerOpen() creates a new Pager object.
** Returns SQLITE_OK on success, stores handle in *ppPager.
** Caller must eventually call sqlite3PagerClose().
**
** sqlite3PagerClose() releases all resources.
** The Pager handle is invalid after this call.
*/
int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,    /* Virtual file system to use */
  Pager **ppPager,      /* OUT: Pager handle */
  const char *zPath,    /* Database file path */
  int nExtra,           /* Extra bytes per page */
  int flags,            /* PAGER_* flags */
  int vfsFlags,         /* Flags passed to VFS */
  void(*xReinit)(DbPage*) /* Page reinitializer callback */
);
int sqlite3PagerClose(Pager *pPager, sqlite3 *db);

/*
** Acquire and release page references.
**
** sqlite3PagerGet() obtains a reference to page pgno.
** The page is loaded from disk if not cached.
** Caller must call sqlite3PagerUnref() when done.
**
** sqlite3PagerUnref() releases a page reference.
** Page may be evicted from cache if refcount drops to zero.
*/
int sqlite3PagerGet(Pager *pPager, Pgno pgno, DbPage **ppPage, int flags);
void sqlite3PagerUnref(DbPage *pPage);

#endif /* SQLITE_PAGER_H */
```

### 4.2 Opaque Structs for Information Hiding

The opaque pointer pattern hides implementation details completely.

```c
/* PUBLIC HEADER: btree.h */

/* Forward declaration only - no definition visible to clients */
typedef struct Btree Btree;
typedef struct BtCursor BtCursor;

/* Clients can only use pointers, never access fields */
int sqlite3BtreeOpen(
  sqlite3_vfs *pVfs,
  const char *zFilename,
  sqlite3 *db,
  Btree **ppBtree,      /* OUT: opaque handle */
  int flags,
  int vfsFlags
);

int sqlite3BtreeClose(Btree*);

/* ------------------------------------------------------------ */

/* PRIVATE HEADER: btreeInt.h - Only included by btree.c */

struct Btree {
  sqlite3 *db;           /* Associated database connection */
  BtShared *pBt;         /* Shared btree state */
  u8 inTrans;            /* Transaction state */
  u8 sharable;           /* True if sharable */
  u8 locked;             /* True if locked */
  u8 hasIncrblobCur;     /* Has incremental blob cursor */
  int wantToLock;        /* Pending lock count */
  int nBackup;           /* Number of active backups */
  u32 iBDataVersion;     /* Combined with pBt->pPager for txn ID */
  Btree *pNext;          /* Next in linked list */
  Btree *pPrev;          /* Previous in linked list */
};
```

**图解说明 (Diagram Explanation):**

```
+---------------------------------------------------------------+
|                 OPAQUE POINTER PATTERN                        |
+---------------------------------------------------------------+
|                                                               |
|   CLIENT CODE                    IMPLEMENTATION               |
|   -----------                    --------------               |
|                                                               |
|   #include "btree.h"             #include "btreeInt.h"        |
|                                                               |
|   Btree *pBt;                    struct Btree {               |
|   sqlite3BtreeOpen(..., &pBt);     sqlite3 *db;               |
|                                    BtShared *pBt;             |
|   // Cannot access pBt->db         u8 inTrans;                |
|   // Cannot access pBt->pBt        ...                        |
|   // Can only call functions     };                           |
|                                                               |
|   sqlite3BtreeClose(pBt);        // Full access to fields     |
|                                                               |
+---------------------------------------------------------------+
```

- 客户端代码只能看到类型声明，无法访问内部字段
- 实现代码包含完整的结构体定义
- 这种分离确保了封装性，客户端只能通过函数操作数据

### 4.3 Function Signatures as Contracts

Function signatures encode several contract elements:

```c
/*
** Contract elements in a function signature:
**
** 1. Ownership: const indicates no modification
** 2. Nullability: documented, not in signature
** 3. Output parameters: pointer-to-pointer pattern
** 4. Error reporting: return type
*/

/* Input ownership: caller retains ownership, function reads only */
int sqlite3BtreeSetCacheSize(Btree *p, int mxPage);

/* Output ownership: function allocates, caller must free */
int sqlite3_mprintf_result(char **pzResult, const char *zFormat, ...);

/* Transfer ownership: caller gives up ownership of zFilename */
int sqlite3BtreeOpen(
  sqlite3_vfs *pVfs,       /* borrowed: caller retains */
  const char *zFilename,   /* borrowed: caller retains */
  sqlite3 *db,             /* borrowed: caller retains */
  Btree **ppBtree,         /* out: new ownership to caller */
  int flags,
  int vfsFlags
);
```

### 4.4 Naming Conventions

Naming conventions signal intent and scope:

```c
/* SQLite naming conventions */

/* Public API: sqlite3_ prefix */
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_close(sqlite3 *db);

/* Internal but shared across modules: sqlite3 prefix (no underscore) */
int sqlite3BtreeOpen(...);
void sqlite3PagerUnref(DbPage*);

/* Module-private: static keyword */
static int btreePagecount(BtShared *pBt);
static void pageReinit(DbPage *pData);

/* Internal constants: SQLITE_ prefix, all caps */
#define SQLITE_MAX_PAGE_SIZE 65536
#define SQLITE_DEFAULT_CACHE_SIZE -2000

/* Private constants: module prefix */
#define BTREE_INTKEY  1
#define PAGER_SYNCHRONOUS_OFF 0x01
```

### 4.5 Documentation Invariants

Comments document what the compiler cannot express:

```c
/*
** sqlite3BtreeBeginTrans - Start a transaction
**
** PRECONDITIONS:
**   - p is a valid Btree handle returned by sqlite3BtreeOpen()
**   - No transaction is currently active on p (p->inTrans == TRANS_NONE)
**   - wrflag is 0 or 1
**
** POSTCONDITIONS:
**   - If returns SQLITE_OK:
**       - A transaction is active (p->inTrans == TRANS_READ or TRANS_WRITE)
**       - If wrflag==1, the transaction allows writes
**       - *pSchemaVersion (if non-NULL) contains schema version
**   - If returns error:
**       - No transaction is active
**       - Database state unchanged
**
** THREAD SAFETY:
**   - Must hold the database mutex (automatically acquired if needed)
**
** LOCKING:
**   - Acquires SHARED or RESERVED lock depending on wrflag
**   - May block if another connection holds conflicting lock
*/
int sqlite3BtreeBeginTrans(Btree *p, int wrflag, int *pSchemaVersion);
```

---

## Step 5 — Allowed Interaction Patterns Between Layers

### 5.1 Direct Downward Calls

The simplest and most common pattern: a higher layer calls a lower layer.

```c
/* VDBE (Domain layer) calls Btree (Core layer) */

int sqlite3VdbeExec(Vdbe *p) {
  /* ... */
  
  /* Direct call down to core layer */
  rc = sqlite3BtreeBeginTrans(pBt, 1, &iMeta);
  if (rc != SQLITE_OK) {
    return rc;
  }
  
  /* More direct calls */
  rc = sqlite3BtreeCursor(pBt, iTable, wrFlag, pKeyInfo, pCur);
  
  /* ... */
}
```

**Why allowed**: This follows the dependency flow. The VDBE knows it
needs a transaction but delegates the mechanism to Btree.

**Rules**:
- Caller is responsible for meeting preconditions
- Caller handles errors returned by callee
- Caller does not assume implementation details

### 5.2 Dependency Injection via Function Pointers

When a lower layer needs to invoke behavior defined by a higher layer,
use dependency injection instead of direct upward calls.

```c
/* INFRASTRUCTURE LAYER: VFS abstraction */

struct sqlite3_vfs {
  int iVersion;
  int szOsFile;
  int mxPathname;
  sqlite3_vfs *pNext;
  const char *zName;
  void *pAppData;
  
  /* Function pointers - "injected" by platform-specific code */
  int (*xOpen)(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
  int (*xDelete)(sqlite3_vfs*, const char*, int);
  int (*xAccess)(sqlite3_vfs*, const char*, int, int*);
  int (*xFullPathname)(sqlite3_vfs*, const char*, int, char*);
  /* ... */
};

/* Registration: higher layer injects implementation */
int sqlite3_vfs_register(sqlite3_vfs *pVfs, int makeDflt);

/* Core layer uses VFS without knowing concrete implementation */
int sqlite3OsOpen(sqlite3_vfs *pVfs, const char *zPath, 
                  sqlite3_file *pFile, int flags, int *pFlagsOut) {
  return pVfs->xOpen(pVfs, zPath, pFile, flags, pFlagsOut);
}
```

```
+---------------------------------------------------------------+
|            DEPENDENCY INJECTION PATTERN                       |
+---------------------------------------------------------------+
|                                                               |
|   CORE LAYER                 INFRASTRUCTURE LAYER             |
|   ----------                 --------------------             |
|                                                               |
|   +------------+             +------------------+              |
|   |   Pager    |             |   sqlite3_vfs    |             |
|   |            |             |                  |             |
|   | pVfs ------+------------>| xOpen()  -------+---> Unix     |
|   |            |             | xDelete() ------+---> Win32    |
|   +------------+             | xAccess() ------+---> Custom   |
|                              +------------------+             |
|                                                               |
|   Pager calls pVfs->xOpen()                                   |
|   Does NOT know if Unix, Win32, or custom                     |
|   Concrete implementation injected at registration            |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- Pager 持有 VFS 接口的指针
- VFS 结构体中包含函数指针
- 具体实现（Unix/Win32/Custom）在注册时注入
- Pager 不需要知道具体使用哪种实现

**Why allowed**: The core layer depends on an abstraction (the VFS interface),
not a concrete implementation. The dependency points downward toward the
interface; the implementation is provided at runtime.

**Rules**:
- Interface (struct with function pointers) defined by lower layer
- Concrete implementation provided by higher or peer layer
- Lower layer calls through interface, never directly

### 5.3 Callbacks Without Layer Inversion

Callbacks let lower layers notify higher layers of events without
creating upward dependencies.

```c
/* Core layer defines callback type */
typedef void (*PagerReinitCallback)(DbPage*);

/* Core layer accepts callback at initialization */
int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,
  Pager **ppPager,
  const char *zFilename,
  int nExtra,
  int flags,
  int vfsFlags,
  PagerReinitCallback xReinit   /* Callback provided by caller */
);

/* Domain layer provides callback implementation */
static void pageReinit(DbPage *pData) {
  /* Reinitialize page-specific data */
}

/* Domain layer passes callback when opening pager */
rc = sqlite3PagerOpen(pVfs, &pPager, zFilename, 
                      sizeof(MemPage), flags, vfsFlags,
                      pageReinit);
```

**Why allowed**: The Pager (core) does not depend on the callback's
implementation. It only knows the signature. The callback is provided
by the domain layer, so the dependency points downward.

**Rules**:
- Callback signature defined by lower layer
- Callback implementation provided by higher layer
- Lower layer invokes callback at defined points
- Callback must not violate lower layer's invariants

### 5.4 Data Passed Across Layers Safely

When data crosses layer boundaries, ownership must be clear.

```c
/* Pattern 1: Caller allocates, callee fills */
int sqlite3PagerGet(
  Pager *pPager, 
  Pgno pgno,
  DbPage **ppPage,  /* OUT: caller receives reference */
  int flags
);
/* Contract: On success, caller owns a reference to *ppPage.
** Caller MUST call sqlite3PagerUnref() to release. */

/* Pattern 2: Callee allocates, caller frees */
char *sqlite3_mprintf(const char *zFormat, ...);
/* Contract: Returns newly allocated string.
** Caller MUST call sqlite3_free() on result. */

/* Pattern 3: Borrowed reference (caller retains ownership) */
const char *sqlite3PagerFilename(const Pager *pPager, int nullIfMemDb);
/* Contract: Returns pointer to internal string.
** Caller MUST NOT free. Valid only while Pager is open. */

/* Pattern 4: Transfer ownership */
void sqlite3BtreeEnterCursor(BtCursor *pCur);
/* Contract: Cursor mutex ownership transfers to caller.
** Caller MUST call sqlite3BtreeLeaveCursor() to release. */
```

---

## Step 6 — Forbidden Interactions (Anti-Patterns)

### 6.1 Upward Dependencies

**Problem**: Lower layer directly calls or depends on higher layer.

```c
/* BAD: Core layer (btree.c) directly calls domain layer (vdbe.c) */

/* btree.c */
#include "vdbeInt.h"  /* VIOLATION: Core includes Domain header */

int btreeCommit(Btree *p) {
  /* ... */
  
  /* VIOLATION: Core layer calling domain layer function */
  sqlite3VdbeHalt(p->db->pVdbe);  /* Direct upward call */
  
  /* ... */
}
```

**Why harmful**:
- Creates circular dependencies
- Makes core layer untestable without domain layer
- Changes in domain layer can break core layer

**Correct approach**: Use callback or event mechanism:

```c
/* GOOD: Core layer notifies via callback */

/* btree.h */
typedef void (*BtreeCommitCallback)(void*);

int sqlite3BtreeSetCommitCallback(Btree *p, 
                                   BtreeCommitCallback xCommit,
                                   void *pArg);

/* btree.c */
int btreeCommit(Btree *p) {
  /* ... */
  
  /* Notify via registered callback - no upward dependency */
  if (p->xCommit) {
    p->xCommit(p->pCommitArg);
  }
  
  /* ... */
}
```

### 6.2 Leaking Internal Structs

**Problem**: Exposing implementation details through public headers.

```c
/* BAD: Public header exposes internal structure */

/* btree.h (PUBLIC) */
struct Btree {
  sqlite3 *db;
  BtShared *pBt;        /* Internal detail exposed! */
  u8 inTrans;
  u8 sharable;
  int wantToLock;
  /* ... all fields visible to all callers ... */
};

/* Callers can now do this: */
void badClientCode(Btree *p) {
  /* Direct field access - bypasses all invariants */
  p->inTrans = 2;        /* Corrupt transaction state */
  p->pBt->nPage = 0;     /* Corrupt page count */
}
```

**Why harmful**:
- Callers depend on exact field layout
- Cannot change structure without breaking callers
- No enforcement of invariants

**Correct approach**: Opaque types with accessor functions:

```c
/* GOOD: Opaque type with accessors */

/* btree.h (PUBLIC) */
typedef struct Btree Btree;  /* Opaque - no definition */

/* Accessor function for needed information */
int sqlite3BtreeTxnState(Btree *p);

/* btreeInt.h (PRIVATE) */
struct Btree {
  /* Full definition - only visible to btree.c */
};
```

### 6.3 Shared Global State

**Problem**: Multiple modules communicate through global variables.

```c
/* BAD: Global state for inter-module communication */

/* globals.c */
int g_lastError = 0;
char g_errorMessage[256] = "";
int g_transactionActive = 0;
sqlite3 *g_currentDb = NULL;

/* btree.c */
extern int g_lastError;
extern int g_transactionActive;

int btreeBeginTrans(Btree *p) {
  if (g_transactionActive) {     /* Check global */
    g_lastError = SQLITE_BUSY;   /* Set global */
    return SQLITE_BUSY;
  }
  g_transactionActive = 1;       /* Modify global */
  /* ... */
}

/* vdbe.c */
extern int g_transactionActive;
extern char g_errorMessage[];

void vdbeReportError() {
  if (g_transactionActive) {     /* Read global */
    sprintf(g_errorMessage, "Transaction in progress");
  }
}
```

**Why harmful**:
- Hidden coupling between modules
- Not thread-safe
- Order of operations matters but isn't enforced
- Testing requires manipulating globals

**Correct approach**: Explicit context passing:

```c
/* GOOD: Explicit context */

/* btree.c */
int sqlite3BtreeBeginTrans(Btree *p, int wrflag, int *pSchemaVersion) {
  /* State is in explicit parameter */
  if (p->inTrans != TRANS_NONE) {
    return SQLITE_BUSY;
  }
  p->inTrans = (wrflag ? TRANS_WRITE : TRANS_READ);
  /* ... */
}

/* All state is explicit, passed through call chain */
```

### 6.4 Policy Decisions in Low Layers

**Problem**: Low-level mechanism code makes high-level policy decisions.

```c
/* BAD: Core layer making policy decisions */

/* pager.c */
int pagerWrite(Pager *pPager, PgHdr *pPg) {
  /* VIOLATION: Core layer deciding policy */
  if (pPager->nPage > 10000) {
    /* "Smart" optimization - policy decision! */
    sqlite3PagerSetCacheSize(pPager, 5000);
  }
  
  /* VIOLATION: Core layer interpreting user intent */
  if (strcmp(pPager->zFilename, ":memory:") == 0) {
    /* Skip fsync for in-memory - policy decision! */
    return SQLITE_OK;
  }
  
  /* ... actual mechanism ... */
}
```

**Why harmful**:
- Policy cannot be changed without modifying mechanism
- Policy is scattered across codebase
- Mechanism is harder to test (depends on policy context)

**Correct approach**: Mechanism receives policy through configuration:

```c
/* GOOD: Mechanism controlled by explicit configuration */

/* pager.h */
#define PAGER_SYNCHRONOUS_OFF    0x01
#define PAGER_SYNCHRONOUS_NORMAL 0x02
#define PAGER_SYNCHRONOUS_FULL   0x03

void sqlite3PagerSetFlags(Pager *pPager, unsigned flags);

/* pager.c */
int pagerWrite(Pager *pPager, PgHdr *pPg) {
  /* Mechanism only - follows configured policy */
  if (pPager->syncFlags & PAGER_SYNCHRONOUS_OFF) {
    /* Skip sync - but decision was made by policy layer */
    return SQLITE_OK;
  }
  
  rc = sqlite3OsSync(pPager->fd, pPager->syncFlags);
  return rc;
}

/* Policy layer (main.c / pragma.c) */
void applyPragmaSynchronous(sqlite3 *db, int level) {
  /* Policy decision made here, in policy layer */
  sqlite3PagerSetFlags(db->pBt->pPager, level);
}
```

---

## Step 7 — Contracts Over Time (Evolution)

### 7.1 Adding Features

**With clear boundaries**:

```
+---------------------------------------------------------------+
|              ADDING A NEW FEATURE                             |
+---------------------------------------------------------------+
|                                                               |
|   1. Identify which layer owns the feature                    |
|   2. Add new functions to that layer's header                 |
|   3. Implement using existing lower-layer APIs                |
|   4. Update higher layers to use new feature                  |
|                                                               |
|   Example: Adding WAL (Write-Ahead Logging)                   |
|                                                               |
|   POLICY LAYER:  Add "PRAGMA journal_mode=WAL"                |
|                  (pragma.c - interprets user request)         |
|                          |                                    |
|                          v                                    |
|   DOMAIN LAYER:  Add sqlite3PagerSetJournalMode()             |
|                  (pager.h - new interface)                    |
|                          |                                    |
|                          v                                    |
|   CORE LAYER:    Add wal.c, wal.h                             |
|                  (new module, implements WAL mechanism)       |
|                          |                                    |
|                          v                                    |
|   INFRA LAYER:   Use existing VFS (xShmMap, xShmLock)         |
|                  (no changes needed - abstractions hold)      |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- 新功能（WAL）需要在每层添加相应的组件
- 每层只处理自己的职责：策略层解释用户请求，核心层实现机制
- 基础设施层无需修改，因为抽象设计得当

**Without clear boundaries**: Features require changes scattered across
many files. Each change risks breaking unrelated functionality.

### 7.2 Optimizing Performance

**With clear boundaries**:

```c
/* Optimization contained within core layer */

/* btree.c - internal optimization */
/* Changed algorithm for page splits - no API change */

/* BEFORE: Linear scan for split point */
static int findSplitPoint_v1(MemPage *pPage) {
  int i;
  for (i = 0; i < pPage->nCell; i++) {
    if (cellSize(pPage, i) > threshold) break;
  }
  return i;
}

/* AFTER: Binary search for split point */
static int findSplitPoint_v2(MemPage *pPage) {
  int lo = 0, hi = pPage->nCell;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (cellSize(pPage, mid) > threshold) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  return lo;
}

/* API unchanged. Callers unaffected. */
```

**Without clear boundaries**: Optimization requires understanding all callers,
because implementation details have leaked.

### 7.3 Refactoring

**With clear boundaries**:

- Split one module into two? Update internal headers, keep public API stable.
- Merge two modules? Same principle.
- Change data structures? Internal change, API unchanged.

**Without clear boundaries**: Every refactoring is a project-wide change.
Risk of regression is high. Developers avoid refactoring, leading to
accumulated technical debt.

### 7.4 Team Changes

**With clear boundaries**:

```
+---------------------------------------------------------------+
|                 TEAM BOUNDARIES                               |
+---------------------------------------------------------------+
|                                                               |
|   +-----------+     +-----------+     +-----------+           |
|   | SQL Team  |     | Storage   |     | Platform  |           |
|   |           |     | Team      |     | Team      |           |
|   | Policy &  |     | Btree &   |     | VFS &     |           |
|   | Domain    |     | Pager     |     | OS        |           |
|   +-----------+     +-----------+     +-----------+           |
|         |                 |                 |                 |
|         v                 v                 v                 |
|   +-----------+     +-----------+     +-----------+           |
|   |  sql*.h   |     | btree.h   |     |   os.h    |           |
|   | vdbe*.h   |     | pager.h   |     |  vfs.h    |           |
|   +-----------+     +-----------+     +-----------+           |
|                                                               |
|   Teams can work in parallel                                  |
|   Contracts at boundaries prevent conflicts                   |
|   New team members learn one layer first                      |
|                                                               |
+---------------------------------------------------------------+
```

**图解说明 (Diagram Explanation):**
- 每个团队负责一组相关的层
- 团队之间通过公开的头文件（契约）交互
- 新成员可以先学习一个层，逐步扩展
- 并行开发成为可能，减少冲突

**Without clear boundaries**: New developers must understand the entire
codebase before making changes. Knowledge is concentrated in senior
developers who become bottlenecks.

### 7.5 Consequences of Vague Contracts

| Symptom | Cause | Long-term Effect |
|---------|-------|------------------|
| "It worked before" bugs | Undocumented behavior changed | Fear of change, frozen codebase |
| Ownership disputes | Unclear who maintains what | Gaps in maintenance, duplicated effort |
| Integration test only | Dependencies make unit tests impossible | Slow feedback, bugs found late |
| Onboarding takes months | No clear learning path | High turnover, knowledge loss |
| "Rewrite from scratch" proposals | Technical debt too high to address | Project risk, wasted investment |

---

## Step 8 — Mapping Boundaries to Real Systems

### SQLite Architecture Analysis

SQLite is an excellent example of disciplined boundary management in C.

```
+================================================================+
|                    SQLITE LAYER ARCHITECTURE                   |
+================================================================+
|                                                                |
|   +--------------------------------------------------------+   |
|   |                  PUBLIC API (sqlite3.h)                |   |
|   |  sqlite3_open, sqlite3_exec, sqlite3_prepare, ...      |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |                TOKENIZER & PARSER                      |   |
|   |  tokenize.c, parse.y -> parse.c                        |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |               CODE GENERATOR                           |   |
|   |  select.c, where.c, expr.c, insert.c, update.c, ...    |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |           VIRTUAL MACHINE (VDBE)                       |   |
|   |  vdbe.c, vdbeaux.c, vdbeapi.c                          |   |
|   |  Interface: vdbe.h (public), vdbeInt.h (private)       |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |              B-TREE ENGINE                             |   |
|   |  btree.c                                               |   |
|   |  Interface: btree.h (public), btreeInt.h (private)     |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |                   PAGER                                |   |
|   |  pager.c                                               |   |
|   |  Interface: pager.h                                    |   |
|   +--------------------------------------------------------+   |
|                              |                                 |
|   +--------------------------------------------------------+   |
|   |              OS INTERFACE (VFS)                        |   |
|   |  os.c, os_unix.c, os_win.c                             |   |
|   |  Interface: os.h, sqlite3.h (sqlite3_vfs)              |   |
|   +--------------------------------------------------------+   |
|                                                                |
+================================================================+
```

**图解说明 (Diagram Explanation):**
- 最顶层：公共 API，用户直接调用
- TOKENIZER & PARSER：SQL 文本解析
- CODE GENERATOR：生成虚拟机字节码
- VDBE：执行字节码
- B-TREE：数据存储结构
- PAGER：页面缓存管理
- VFS：操作系统抽象

### Major Boundaries Identified

#### Boundary 1: Public API (sqlite3.h)

```c
/* sqlite3.h - THE public contract */

/* Opaque types */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

/* Lifetime management */
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_close(sqlite3*);

/* Contract: 
** - sqlite3_open creates handle, caller must close
** - After sqlite3_close, handle is invalid
** - All statements must be finalized before close
*/
```

**Strictness**: Very strict. Public API is stable. Changes are additive only.
Backward compatibility is paramount.

#### Boundary 2: VDBE Interface (vdbe.h vs vdbeInt.h)

```c
/* vdbe.h - Interface to VDBE for other internal modules */

typedef struct Vdbe Vdbe;  /* Opaque to callers */

Vdbe *sqlite3VdbeCreate(Parse*);
int sqlite3VdbeAddOp3(Vdbe*, int, int, int, int);
void sqlite3VdbeDelete(Vdbe*);

/* vdbeInt.h - Internal to VDBE implementation */

struct Vdbe {
  sqlite3 *db;            /* Database connection */
  Vdbe *pPrev, *pNext;    /* Linked list */
  Parse *pParse;          /* Parser context */
  /* ... many more internal fields ... */
};
```

**Strictness**: Moderately strict. Internal modules use vdbe.h.
Only vdbe*.c files include vdbeInt.h.

#### Boundary 3: B-tree Interface (btree.h vs btreeInt.h)

```c
/* btree.h - Interface used by VDBE */

typedef struct Btree Btree;
typedef struct BtCursor BtCursor;

int sqlite3BtreeOpen(...);
int sqlite3BtreeCursor(...);
int sqlite3BtreeNext(BtCursor*, int flags);

/* btreeInt.h - Implementation details */

struct Btree { /* full definition */ };
struct BtCursor { /* full definition */ };
struct MemPage { /* page structure */ };
```

**Strictness**: Very strict. The B-tree is a well-defined abstraction.
The VDBE interacts only through btree.h.

#### Boundary 4: Pager Interface (pager.h)

```c
/* pager.h - Interface used by B-tree */

typedef struct Pager Pager;
typedef struct PgHdr DbPage;

int sqlite3PagerOpen(...);
int sqlite3PagerGet(Pager*, Pgno, DbPage**, int);
void sqlite3PagerUnref(DbPage*);
int sqlite3PagerWrite(DbPage*);
```

**Strictness**: Very strict. Pager manages pages; B-tree uses pages.
Clean separation.

#### Boundary 5: VFS Interface (os.h, sqlite3.h)

```c
/* sqlite3.h - VFS structure definition (public, extensible) */

struct sqlite3_vfs {
  int iVersion;
  int szOsFile;
  int mxPathname;
  sqlite3_vfs *pNext;
  const char *zName;
  void *pAppData;
  int (*xOpen)(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
  int (*xDelete)(sqlite3_vfs*, const char*, int);
  /* ... more function pointers ... */
};

/* os.h - Internal wrappers */

int sqlite3OsOpen(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
int sqlite3OsDelete(sqlite3_vfs*, const char*, int);
```

**Strictness**: Strict interface, flexible implementation. Users can
provide custom VFS implementations. SQLite provides os_unix.c, os_win.c.

### Where SQLite is Pragmatic

1. **Single-file distribution**: SQLite is often distributed as an
   amalgamation (sqlite3.c, sqlite3.h). This violates the "separate files
   for separate concerns" principle but serves deployment simplicity.

2. **Internal header sharing**: Some internal types are shared via
   sqliteInt.h rather than strictly layered headers. This is pragmatic
   for compilation efficiency.

3. **Global configuration**: Some configuration (sqlite3_config) uses
   global state. This is documented and constrained (must be called
   before any other API).

---

## Step 9 — Boundary Review Checklist

Use this checklist during code reviews to evaluate architectural health.

### Header File Review

```
[ ] Does the header declare only what external callers need?
[ ] Are implementation details hidden (opaque types, no struct definitions)?
[ ] Are all public functions documented with preconditions/postconditions?
[ ] Does the header include only what IT needs (not transitive dependencies)?
[ ] Are include guards present and unique?
[ ] Does the naming convention indicate scope (public vs internal)?
```

### Dependency Review

```
[ ] Does this code depend only on layers below it?
[ ] Are there any #include of headers from higher layers?
[ ] Are there any extern declarations reaching into other modules?
[ ] If callbacks are used, is the callback signature owned by the lower layer?
[ ] Are function pointers used instead of direct upward calls?
```

### Data Ownership Review

```
[ ] Is it clear who owns (allocates/frees) each piece of data?
[ ] Are ownership transfers explicit and documented?
[ ] Are borrowed references documented with lifetime constraints?
[ ] Are const qualifiers used correctly to indicate read-only access?
[ ] Is there any shared mutable state without synchronization?
```

### Contract Clarity Review

```
[ ] Are preconditions documented (what must be true before call)?
[ ] Are postconditions documented (what is guaranteed after call)?
[ ] Are error conditions documented (which errors, what state on error)?
[ ] Is thread safety documented (which locks must be held)?
[ ] Are any invariants documented (what must always be true)?
```

### Control Flow Review

```
[ ] Are there any calls that could cause unexpected re-entrancy?
[ ] Are callbacks invoked only at documented points?
[ ] Are there any blocking operations in low layers?
[ ] Is error propagation consistent (always return codes, or always errno)?
```

### Symbol Visibility Review

```
[ ] Are internal functions marked static?
[ ] Do internal symbols have a distinguishing prefix/naming convention?
[ ] Are there any global variables? If so, are they justified?
[ ] Could any extern declarations be replaced with function calls?
```

### Evolution Risk Review

```
[ ] If this implementation changes, will callers break?
[ ] Are there any dependencies on undocumented behavior?
[ ] Is the interface stable enough for the expected lifetime?
[ ] Are deprecated interfaces marked and documented?
[ ] Is there a migration path if the interface changes?
```

### Quick Reference: Red Flags

| Red Flag | What It Indicates |
|----------|-------------------|
| Header includes `*Int.h` from another module | Boundary violation |
| Struct definition in public header | Information leakage |
| `extern` variable declarations | Global state coupling |
| Cast away `const` | Ownership confusion |
| Upward `#include` | Dependency inversion |
| Undocumented function | Contract uncertainty |
| Magic numbers without constants | Policy in mechanism |
| Module A calling B calling A | Circular dependency |

---

## Summary

Boundaries and contracts are not academic exercises—they are survival
mechanisms for long-lived C codebases. The C language provides no
enforcement, which means architects must be deliberate:

1. **Define boundaries explicitly** through header files and naming conventions
2. **Express contracts clearly** through documentation, types, and assertions
3. **Enforce boundaries through discipline** via code review and static analysis
4. **Respect the dependency rule**: always point downward
5. **Keep mechanism and policy separate**: low layers provide capabilities,
   high layers make decisions

The investment in architectural discipline pays dividends in:
- Faster development (clear ownership, parallel work)
- Lower defect rates (invariants catch bugs early)
- Easier onboarding (clear learning path)
- Sustainable evolution (changes don't ripple everywhere)

A well-boundaried C codebase can remain maintainable for decades.
An undisciplined codebase becomes a liability within years.

---

## Further Reading

- SQLite source code: https://sqlite.org/src
- "A Philosophy of Software Design" by John Ousterhout
- "Clean Architecture" by Robert C. Martin (principles apply to C)
- Linux kernel coding style (Documentation/process/coding-style.rst)

---

*Document version: 1.0*
*Generated for systems architecture training*

