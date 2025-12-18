# Layered Software Architecture in C: SQLite Case Study

> **A systems-level architectural analysis treating SQLite strictly as an architectural artifact.**  
> Focus: Layer boundaries, dependency direction, responsibility separation, and long-term evolution.

---

## STEP 1 — Identify the Architectural Layers

SQLite employs a **strict hierarchical layered architecture** with seven primary layers. Each layer has well-defined responsibilities and communicates only with adjacent layers.

### Architectural Layer Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                      PUBLIC API (sqlite3.h)                       │
│              sqlite3_open(), sqlite3_prepare(), sqlite3_step()    │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                    SQL FRONTEND / COMPILER                        │
│         Tokenizer → Parser → Code Generator (Prepare)            │
│     tokenize.c, parse.y, build.c, select.c, insert.c, etc.       │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                      QUERY PLANNER                                │
│              Cost-based optimization, index selection             │
│              where.c, wherecode.c, whereexpr.c                    │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                VIRTUAL DATABASE ENGINE (VDBE)                     │
│         Bytecode interpreter, register machine                    │
│      vdbe.c, vdbeaux.c, vdbeapi.c, vdbemem.c, vdbesort.c         │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                       B-TREE ENGINE                               │
│         Ordered key-value store, cursor-based navigation          │
│                      btree.c, btreeInt.h                          │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                    PAGER / PAGE CACHE                             │
│       Page-level I/O, journaling, transaction atomicity           │
│         pager.c, pcache.c, pcache1.c, wal.c, memjournal.c        │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                  OS INTERFACE (VFS)                               │
│       File I/O, locking, memory mapping, threading                │
│                 os.c, os_unix.c, os_win.c, os_kv.c               │
└──────────────────────────────────────────────────────────────────┘
```

**图解说明:**  
上图展示了 SQLite 的七层架构。每一层仅与其相邻的上下层通信。公共 API 位于最顶层，OS 接口位于最底层。依赖方向严格向下：上层依赖下层，下层绝不依赖上层。

### Layer Specification Table

| Layer | Primary Responsibility | Key Source Files | Allowed Dependencies | Must NOT Depend On This |
|-------|------------------------|------------------|---------------------|------------------------|
| **Public API** | Expose stable C interface to applications | `sqlite.h.in`, `main.c`, `legacy.c` | SQL Frontend | Everything except calling applications |
| **SQL Frontend** | Lexing, parsing, semantic analysis, bytecode generation | `tokenize.c`, `parse.y`, `build.c`, `select.c`, `insert.c`, `update.c`, `delete.c`, `expr.c`, `resolve.c` | Query Planner, VDBE | B-Tree, Pager, OS Interface |
| **Query Planner** | Cost estimation, access path selection, join ordering | `where.c`, `wherecode.c`, `whereexpr.c`, `whereInt.h` | VDBE (indirectly via code generation) | B-Tree (except for statistics), Pager, OS |
| **VDBE** | Execute bytecode programs via register-based VM | `vdbe.c`, `vdbe.h`, `vdbeInt.h`, `vdbeaux.c`, `vdbeapi.c`, `vdbemem.c`, `vdbesort.c`, `vdbeblob.c` | B-Tree Engine | Pager (except indirectly), OS Interface |
| **B-Tree Engine** | Manage B+tree data structures, cursor navigation | `btree.c`, `btree.h`, `btreeInt.h` | Pager | OS Interface (must go through Pager) |
| **Pager** | Page-level caching, atomic commits, journaling, WAL | `pager.c`, `pager.h`, `pcache.c`, `pcache.h`, `pcache1.c`, `wal.c`, `wal.h` | OS Interface | Nothing (foundation layer for storage) |
| **OS Interface (VFS)** | Abstract platform-specific file/memory/locking operations | `os.c`, `os.h`, `os_unix.c`, `os_win.c`, `os_kv.c` | OS/Kernel only | Nothing (bottom layer) |

---

## STEP 2 — Dependency Direction & Invariants

### Strict Dependency Direction

SQLite enforces a **unidirectional dependency rule**: higher layers depend on lower layers, never the reverse.

```
┌─────────────────┐
│  SQL Frontend   │ ────────────────────────────────────────────┐
└────────┬────────┘                                             │
         │ calls                                                │
         ▼                                                      │
┌─────────────────┐                                             │
│  Query Planner  │ ──────────────────────────────────────┐     │
└────────┬────────┘                                       │     │
         │ calls (generates VDBE ops)                     │     │
         ▼                                                │     │
┌─────────────────┐                                       │     │
│     VDBE        │ ────────────────────────────────┐     │     │
└────────┬────────┘                                 │     │     │
         │ calls (cursor ops, data access)          │     │     │
         ▼                                          │     │     │  DEPENDENCY
┌─────────────────┐                                 │     │     │  DIRECTION
│    B-Tree       │ ──────────────────────────┐     │     │     │  (ONE-WAY)
└────────┬────────┘                           │     │     │     │
         │ calls (page fetch, write)          │     │     │     │
         ▼                                    ▼     ▼     ▼     ▼
┌─────────────────┐
│     Pager       │ ────────────────┐
└────────┬────────┘                 │
         │ calls (file I/O)         │
         ▼                          ▼
┌─────────────────┐
│  OS Interface   │
└─────────────────┘
```

**中文解释:**  
依赖方向严格单向向下。SQL 前端调用查询规划器，查询规划器生成 VDBE 操作码，VDBE 调用 B-Tree 进行数据访问，B-Tree 通过 Pager 进行页面 I/O，Pager 最终调用 OS 接口。任何反向调用都被禁止。

### One-Way Dependency Rules

| Caller Layer | Can Call | Cannot Call |
|--------------|----------|-------------|
| SQL Frontend | Planner, VDBE (add ops) | B-Tree, Pager, OS |
| Query Planner | VDBE (add ops), B-Tree (read stats only) | Pager, OS |
| VDBE | B-Tree (full API) | Pager directly, OS |
| B-Tree | Pager | OS directly |
| Pager | OS Interface | — |
| OS Interface | Operating System Kernel | — |

### Invariants Enforced by Convention

Since C lacks language-level module systems, SQLite enforces layering through:

1. **Header File Discipline**: Each layer has public (`btree.h`) and private (`btreeInt.h`) headers. Lower layers never include upper-layer headers.

2. **Naming Conventions**: 
   - `sqlite3Btree*` — B-Tree public API
   - `sqlite3Pager*` — Pager public API
   - `sqlite3Vdbe*` — VDBE public API
   - `sqlite3Os*` — OS wrapper functions

3. **Comment Invariants**: Critical invariants documented in comments (see `doc/pager-invariants.txt`).

4. **Static Function Scoping**: Internal functions are declared `static` to prevent cross-layer access.

### Why Reversing Any Arrow Would Be Catastrophic

| Violation | Consequence |
|-----------|-------------|
| Pager → VDBE | Pager would need to understand execution context; impossible to test I/O in isolation |
| B-Tree → SQL Frontend | B-Tree would become coupled to SQL semantics; could not substitute storage engines |
| VDBE → Query Planner | Would create circular dependency; planner changes would break execution |
| OS Interface → Pager | Platform abstraction would fail; every new OS would require pager changes |

**Key Insight**: The layering enables SQLite to be **compiled as a single amalgamation** where the compiler can aggressively inline and optimize across layer boundaries, while the source organization maintains conceptual integrity.

---

## STEP 3 — Interfaces Between Layers

### 3.1 Public API ↔ SQL Frontend

**Interface Location**: `sqlite.h.in`, `prepare.c`

```c
/* Primary interface function */
int sqlite3_prepare_v2(
  sqlite3 *db,            /* Database handle */
  const char *zSql,       /* SQL statement, UTF-8 encoded */
  int nByte,              /* Maximum length of zSql in bytes */
  sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  const char **pzTail     /* OUT: Pointer to unused portion of zSql */
);
```

**Ownership Rules**:
- `sqlite3*` owned by application (created by `sqlite3_open`, destroyed by `sqlite3_close`)
- `sqlite3_stmt*` (which is really `Vdbe*`) owned by application
- SQL string copied internally; caller retains ownership

**Error Propagation**: Returns `SQLITE_OK` or error code. Extended error via `sqlite3_errmsg()`.

### 3.2 SQL Frontend ↔ Query Planner

**Interface Location**: `where.c`, `whereInt.h`

```c
/* Initiate WHERE clause processing */
WhereInfo *sqlite3WhereBegin(
  Parse *pParse,        /* Parser context */
  SrcList *pTabList,    /* FROM clause */
  Expr *pWhere,         /* WHERE clause */
  ExprList *pOrderBy,   /* ORDER BY clause */
  ExprList *pResultSet, /* Result set expressions */
  Select *pSelect,      /* The SELECT statement */
  u16 wctrlFlags,       /* Control flags */
  int iAuxArg           /* Auxiliary argument */
);

/* Terminate WHERE clause code generation */
void sqlite3WhereEnd(WhereInfo *pWInfo);
```

**Ownership Rules**:
- `WhereInfo*` allocated by planner, freed by `sqlite3WhereEnd()`
- `Parse*` context passed through entire compilation pipeline
- Expression trees (`Expr*`) shared, not copied

**Error Propagation**: Errors stored in `Parse.rc` and `Parse.zErrMsg`.

### 3.3 SQL Frontend ↔ VDBE (Code Generation)

**Interface Location**: `vdbe.h`

```c
/* Create a new VDBE program */
Vdbe *sqlite3VdbeCreate(Parse*);

/* Add instructions to the VDBE program */
int sqlite3VdbeAddOp3(Vdbe*, int op, int p1, int p2, int p3);
int sqlite3VdbeAddOp4(Vdbe*, int op, int p1, int p2, int p3, 
                       const char *zP4, int p4type);

/* Resolve forward jumps */
void sqlite3VdbeJumpHere(Vdbe*, int addr);

/* Finalize the VDBE program */
void sqlite3VdbeMakeReady(Vdbe*, Parse*);
```

**Ownership Rules**:
- `Vdbe*` created during prepare, executed via `sqlite3_step()`, destroyed via `sqlite3_finalize()`
- P4 arguments may be `P4_STATIC`, `P4_DYNAMIC`, `P4_TRANSIENT` (ownership varies)

### 3.4 VDBE ↔ B-Tree

**Interface Location**: `btree.h`

```c
/* Open a B-Tree database file */
int sqlite3BtreeOpen(
  sqlite3_vfs *pVfs,       /* VFS to use */
  const char *zFilename,   /* Name of file */
  sqlite3 *db,             /* Associated database connection */
  Btree **ppBtree,         /* OUT: Btree handle */
  int flags,               /* Options */
  int vfsFlags             /* VFS options */
);

/* Create a cursor for navigating B-Tree */
int sqlite3BtreeCursor(
  Btree*,                  /* B-Tree containing table to open */
  Pgno iTable,             /* Root page of table */
  int wrFlag,              /* 1 for writing, 0 for read-only */
  struct KeyInfo*,         /* Key comparison info */
  BtCursor *pCursor        /* Space for cursor */
);

/* Seek to a specific key */
int sqlite3BtreeTableMoveto(BtCursor*, i64 intKey, int bias, int *pRes);

/* Insert a record */
int sqlite3BtreeInsert(BtCursor*, const BtreePayload*, int flags, int seekResult);
```

**Ownership Rules**:
- `Btree*` lifetime tied to database connection
- `BtCursor*` memory provided by caller (VDBE pre-allocates in `VdbeCursor`)
- Payload data copied into B-Tree pages

**Error Propagation**: Return codes (`SQLITE_OK`, `SQLITE_DONE`, `SQLITE_CORRUPT`, etc.)

### 3.5 B-Tree ↔ Pager

**Interface Location**: `pager.h`

```c
/* Open a pager */
int sqlite3PagerOpen(
  sqlite3_vfs*,
  Pager **ppPager,
  const char *zFilename,
  int nExtra,              /* Extra bytes per page */
  int flags,
  int vfsFlags,
  void(*xReinit)(DbPage*)
);

/* Get a page from the database */
int sqlite3PagerGet(Pager*, Pgno pgno, DbPage **ppPage, int clrFlag);

/* Mark a page as dirty (written) */
int sqlite3PagerWrite(DbPage*);

/* Commit a transaction */
int sqlite3PagerCommitPhaseOne(Pager*, const char *zSuper, int);
int sqlite3PagerCommitPhaseTwo(Pager*);
```

**Ownership Rules**:
- `Pager*` lifetime tied to `Btree*`
- `DbPage*` reference counted; `sqlite3PagerRef()` / `sqlite3PagerUnref()`
- Page content directly accessible via `sqlite3PagerGetData()`

### 3.6 Pager ↔ OS Interface

**Interface Location**: `os.h`

```c
/* File operations wrapper */
int sqlite3OsRead(sqlite3_file*, void*, int amt, i64 offset);
int sqlite3OsWrite(sqlite3_file*, const void*, int amt, i64 offset);
int sqlite3OsSync(sqlite3_file*, int flags);
int sqlite3OsLock(sqlite3_file*, int lockType);
int sqlite3OsTruncate(sqlite3_file*, i64 size);

/* VFS operations */
int sqlite3OsOpen(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
int sqlite3OsDelete(sqlite3_vfs*, const char*, int);
int sqlite3OsAccess(sqlite3_vfs*, const char*, int, int*);
```

**Ownership Rules**:
- `sqlite3_vfs*` typically global singletons
- `sqlite3_file*` allocated by VFS, memory provided by pager
- File handles managed by OS layer

---

## STEP 3B — Layer Boundary Interactions (Code Deep-Dive)

This section provides a detailed code-level analysis of how each layer communicates with its adjacent layers. We examine the actual C data structures, function calls, and ownership patterns that enforce the architectural boundaries.

### 3B.1 Architectural Boundary Overview

Each layer boundary in SQLite is defined by three key elements:
1. **Handle Structure** — An opaque pointer that the upper layer holds to the lower layer
2. **Interface Functions** — The set of `sqlite3<Layer>*` functions that cross the boundary
3. **Data Exchange** — How data flows between layers (by reference, by copy, or through buffers)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     DATA STRUCTURE RELATIONSHIPS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌───────────────┐                                                         │
│   │  VdbeCursor   │  (vdbeInt.h)                                            │
│   │ ┌───────────┐ │                                                         │
│   │ │ BtCursor* │─┼──────────────────────┐                                  │
│   │ └───────────┘ │                      │                                  │
│   └───────────────┘                      ▼                                  │
│                                  ┌───────────────┐                          │
│   VDBE Layer                     │   BtCursor    │  (btreeInt.h)            │
│   ─────────────────────────────  │ ┌───────────┐ │                          │
│   B-Tree Layer                   │ │  Btree*   │─┼───┐                      │
│                                  │ └───────────┘ │   │                      │
│                                  └───────────────┘   │                      │
│                                                      ▼                      │
│                                              ┌───────────────┐              │
│                                              │   BtShared    │              │
│                                              │ ┌───────────┐ │              │
│                                              │ │  Pager*   │─┼───┐          │
│                                              │ └───────────┘ │   │          │
│                                              └───────────────┘   │          │
│   ─────────────────────────────────────────────────────────────  │          │
│   Pager Layer                                                    ▼          │
│                                              ┌───────────────────────┐      │
│                                              │        Pager          │      │
│                                              │ ┌───────────────────┐ │      │
│                                              │ │ sqlite3_vfs *pVfs │ │      │
│                                              │ │ sqlite3_file *fd  │─┼──┐   │
│                                              │ └───────────────────┘ │  │   │
│                                              └───────────────────────┘  │   │
│   ───────────────────────────────────────────────────────────────────   │   │
│   OS Interface Layer                                                    ▼   │
│                                              ┌───────────────────────┐      │
│                                              │    sqlite3_file       │      │
│                                              │ ┌───────────────────┐ │      │
│                                              │ │ sqlite3_io_methods│ │      │
│                                              │ │   ->xRead()       │ │      │
│                                              │ │   ->xWrite()      │ │      │
│                                              │ └───────────────────┘ │      │
│                                              └───────────────────────┘      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文解释:**  
上图展示了各层之间的数据结构关系。每一层通过持有下一层的不透明指针来访问其服务。VDBE 的 `VdbeCursor` 持有 `BtCursor` 指针，B-Tree 的 `BtShared` 持有 `Pager` 指针，Pager 持有 `sqlite3_file` 指针来访问操作系统文件接口。

---

### 3B.2 VDBE ↔ B-Tree Boundary

#### Data Structures

**VdbeCursor** (VDBE layer's cursor abstraction):

```c
/* From src/vdbeInt.h - VDBE's cursor wrapper */
typedef struct VdbeCursor VdbeCursor;
struct VdbeCursor {
  u8 eCurType;            /* One of the CURTYPE_* values above */
  i8 iDb;                 /* Index of cursor database in db->aDb[] */
  u8 nullRow;             /* True if pointing to a row with no data */
  u8 deferredMoveto;      /* A call to sqlite3BtreeMoveto() is needed */
  u8 isTable;             /* True for rowid tables.  False for indexes */
  /* ... additional fields ... */
  
  union {
    BtCursor *pCursor;          /* CURTYPE_BTREE: Btree cursor */
    sqlite3_vtab_cursor *pVCur; /* CURTYPE_VTAB: Virtual table cursor */
    VdbeSorter *pSorter;        /* CURTYPE_SORTER: Sorter object */
  } uc;
  /* ... */
};
```

**Key Insight**: `VdbeCursor` acts as an **adapter** that wraps a `BtCursor`. The VDBE layer never directly accesses `BtCursor` fields — it only calls `sqlite3Btree*` functions.

#### Boundary Crossing: Opening a Cursor

When the VDBE executes `OP_OpenRead` or `OP_OpenWrite`, it crosses into the B-Tree layer:

```c
/* From src/vdbe.c - OP_OpenRead/OP_OpenWrite opcode handler */
case OP_OpenRead:
case OP_OpenWrite:

  /* ... parameter validation and setup ... */
  
  /* Get the Btree handle from the database connection */
  iDb = pOp->p3;
  pDb = &db->aDb[iDb];
  pX = pDb->pBt;                     /* <-- B-Tree handle */
  
  /* Determine read or write mode */
  if( pOp->opcode==OP_OpenWrite ){
    wrFlag = BTREE_WRCSR | (pOp->p5 & OPFLAG_FORDELETE);
  }else{
    wrFlag = 0;
  }
  
  /* Allocate VdbeCursor which contains space for BtCursor */
  pCur = allocateCursor(p, pOp->p1, nField, CURTYPE_BTREE);
  if( pCur==0 ) goto no_mem;
  pCur->iDb = iDb;
  pCur->nullRow = 1;
  pCur->pgnoRoot = p2;
  
  /*═══════════════════════════════════════════════════════════════════════╗
  ║  LAYER BOUNDARY CROSSING: VDBE → B-Tree                                ║
  ║  The VDBE calls into the B-Tree layer to initialize the cursor.        ║
  ║  pCur->uc.pCursor points to pre-allocated BtCursor memory.             ║
  ╚═══════════════════════════════════════════════════════════════════════*/
  rc = sqlite3BtreeCursor(pX, p2, wrFlag, pKeyInfo, pCur->uc.pCursor);
  
  pCur->pKeyInfo = pKeyInfo;
  pCur->isTable = pOp->p4type!=P4_KEYINFO;
  
  /* Set cursor hints (still goes through B-Tree interface) */
  sqlite3BtreeCursorHintFlags(pCur->uc.pCursor,
                               (pOp->p5 & (OPFLAG_BULKCSR|OPFLAG_SEEKEQ)));
  if( rc ) goto abort_due_to_error;
  break;
```

#### Ownership and Memory Rules

| Aspect | Rule |
|--------|------|
| **BtCursor memory** | Provided by VDBE (pre-allocated in `allocateCursor`) |
| **BtCursor initialization** | Done by B-Tree layer via `sqlite3BtreeCursor()` |
| **BtCursor lifetime** | Tied to `VdbeCursor`; closed when VDBE closes cursor |
| **Error codes** | Returned by B-Tree, propagated by VDBE |

#### B-Tree Functions Called by VDBE

```c
/* Navigation */
int sqlite3BtreeTableMoveto(BtCursor*, i64 intKey, int bias, int *pRes);
int sqlite3BtreeNext(BtCursor*, int flags);
int sqlite3BtreePrevious(BtCursor*, int flags);
int sqlite3BtreeFirst(BtCursor*, int *pRes);
int sqlite3BtreeLast(BtCursor*, int *pRes);

/* Data access */
i64 sqlite3BtreeIntegerKey(BtCursor*);
u32 sqlite3BtreePayloadSize(BtCursor*);
const void *sqlite3BtreePayloadFetch(BtCursor*, u32 *pAmt);

/* Modification */
int sqlite3BtreeInsert(BtCursor*, const BtreePayload*, int flags, int seekResult);
int sqlite3BtreeDelete(BtCursor*, u8 flags);
```

---

### 3B.3 B-Tree ↔ Pager Boundary

#### Data Structures

**BtShared** (B-Tree's shared database structure):

```c
/* From src/btreeInt.h - Internal B-Tree structure */
struct BtShared {
  Pager *pPager;        /* The page cache - HANDLE TO LOWER LAYER */
  sqlite3 *db;          /* Database connection currently using this Btree */
  BtCursor *pCursor;    /* A list of all open cursors */
  MemPage *pPage1;      /* First page of the database */
  u8 openFlags;         /* Flags to sqlite3BtreeOpen() */
  /* ... */
  u32 pageSize;         /* Total number of bytes on a page */
  u32 usableSize;       /* Number of usable bytes on each page */
  /* ... */
};
```

**MemPage** (B-Tree's in-memory page representation):

```c
/* MemPage is the B-Tree layer's view of a database page */
struct MemPage {
  u8 isInit;           /* True if previously initialized */
  u8 intKey;           /* True if table b-trees (rowid tables) */
  u8 leaf;             /* True if a leaf page */
  u8 *aData;           /* Pointer to disk image of the page data */
  DbPage *pDbPage;     /* Pager page handle - HANDLE TO LOWER LAYER */
  Pgno pgno;           /* Page number for this page */
  BtShared *pBt;       /* Pointer to BtShared that this page is part of */
  /* ... cell and free space management fields ... */
};
```

#### Boundary Crossing: Fetching a Page

When B-Tree needs to access a page, it calls into the Pager layer:

```c
/* From src/btree.c - Get a page from the pager */
static int btreeGetPage(
  BtShared *pBt,       /* The btree */
  Pgno pgno,           /* Number of the page to fetch */
  MemPage **ppPage,    /* Return the page in this parameter */
  int flags            /* PAGER_GET_NOCONTENT or PAGER_GET_READONLY */
){
  int rc;
  DbPage *pDbPage;

  assert( flags==0 || flags==PAGER_GET_NOCONTENT || flags==PAGER_GET_READONLY );
  assert( sqlite3_mutex_held(pBt->mutex) );
  
  /*═══════════════════════════════════════════════════════════════════════╗
  ║  LAYER BOUNDARY CROSSING: B-Tree → Pager                               ║
  ║  Request a page from the page cache.                                   ║
  ║  pBt->pPager is the B-Tree's handle to the Pager.                      ║
  ╚═══════════════════════════════════════════════════════════════════════*/
  rc = sqlite3PagerGet(pBt->pPager, pgno, (DbPage**)&pDbPage, flags);
  if( rc ) return rc;
  
  /* Convert pager's DbPage to B-Tree's MemPage */
  *ppPage = btreePageFromDbPage(pDbPage, pgno, pBt);
  return SQLITE_OK;
}
```

```c
/* From src/btree.c - Convert DbPage to MemPage */
static MemPage *btreePageFromDbPage(DbPage *pDbPage, Pgno pgno, BtShared *pBt){
  /*═══════════════════════════════════════════════════════════════════════╗
  ║  DATA EXTRACTION AT BOUNDARY                                            ║
  ║  B-Tree accesses page data through Pager's accessor functions.          ║
  ╚═══════════════════════════════════════════════════════════════════════*/
  MemPage *pPage = (MemPage*)sqlite3PagerGetExtra(pDbPage);
  if( pgno!=pPage->pgno ){
    pPage->aData = sqlite3PagerGetData(pDbPage);  /* Get raw page content */
    pPage->pDbPage = pDbPage;                      /* Keep reference to DbPage */
    pPage->pBt = pBt;
    pPage->pgno = pgno;
    pPage->hdrOffset = pgno==1 ? 100 : 0;
  }
  assert( pPage->aData==sqlite3PagerGetData(pDbPage) );
  return pPage;
}
```

#### Ownership and Reference Counting

```c
/* From src/btree.c - Release a page */
static void releasePageNotNull(MemPage *pPage){
  assert( pPage->aData );
  assert( pPage->pBt );
  assert( pPage->pDbPage!=0 );
  
  /* Verify ownership relationship is maintained */
  assert( sqlite3PagerGetExtra(pPage->pDbPage) == (void*)pPage );
  assert( sqlite3PagerGetData(pPage->pDbPage)==pPage->aData );
  
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  
  /*═══════════════════════════════════════════════════════════════════════╗
  ║  LAYER BOUNDARY CROSSING: B-Tree → Pager (release)                     ║
  ║  Tell the Pager we're done with this page.                             ║
  ║  Pager handles reference counting and potential page eviction.         ║
  ╚═══════════════════════════════════════════════════════════════════════*/
  sqlite3PagerUnrefNotNull(pPage->pDbPage);
}
```

#### Pager Functions Called by B-Tree

```c
/* Page acquisition and release */
int sqlite3PagerGet(Pager*, Pgno pgno, DbPage **ppPage, int flags);
void sqlite3PagerUnref(DbPage*);
void sqlite3PagerUnrefNotNull(DbPage*);
DbPage *sqlite3PagerLookup(Pager*, Pgno pgno);

/* Page modification */
int sqlite3PagerWrite(DbPage*);
void *sqlite3PagerGetData(DbPage*);
void *sqlite3PagerGetExtra(DbPage*);

/* Transaction control */
int sqlite3PagerBegin(Pager*, int exFlag, int subjInMemory);
int sqlite3PagerCommitPhaseOne(Pager*, const char *zSuper, int);
int sqlite3PagerCommitPhaseTwo(Pager*);
int sqlite3PagerRollback(Pager*);
```

---

### 3B.4 Pager ↔ OS Interface Boundary

#### Data Structures

**Pager** (holds OS handles):

```c
/* From src/pager.c - Pager structure (partial) */
struct Pager {
  sqlite3_vfs *pVfs;          /* OS functions to use for IO */
  /* ... state management fields ... */
  
  sqlite3_file *fd;           /* File descriptor for database */
  sqlite3_file *jfd;          /* File descriptor for main journal */
  sqlite3_file *sjfd;         /* File descriptor for sub-journal */
  
  /* ... cache and transaction fields ... */
  i64 journalOff;             /* Current write offset in the journal file */
  i64 journalHdr;             /* Byte offset to previous journal header */
  /* ... */
};
```

**sqlite3_file** (OS file abstraction):

```c
/* From src/sqlite.h.in - Abstract file handle */
struct sqlite3_file {
  const struct sqlite3_io_methods *pMethods;  /* Virtual method table */
};

/* Virtual method table for file operations */
struct sqlite3_io_methods {
  int iVersion;
  int (*xClose)(sqlite3_file*);
  int (*xRead)(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
  int (*xWrite)(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
  int (*xTruncate)(sqlite3_file*, sqlite3_int64 size);
  int (*xSync)(sqlite3_file*, int flags);
  int (*xFileSize)(sqlite3_file*, sqlite3_int64 *pSize);
  int (*xLock)(sqlite3_file*, int);
  int (*xUnlock)(sqlite3_file*, int);
  /* ... more methods ... */
};
```

#### Boundary Crossing: Reading a Page

```c
/* From src/pager.c - Read database page content */
static int readDbPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  int rc = SQLITE_OK;
  
  /* Calculate file offset for this page */
  i64 iOffset = (pPg->pgno-1)*(i64)pPager->pageSize;
  
  /*═══════════════════════════════════════════════════════════════════════╗
  ║  LAYER BOUNDARY CROSSING: Pager → OS Interface                         ║
  ║  Read page data from the database file.                                ║
  ║  pPager->fd is the file handle, sqlite3OsRead is the wrapper.          ║
  ╚═══════════════════════════════════════════════════════════════════════*/
  rc = sqlite3OsRead(pPager->fd, pPg->pData, pPager->pageSize, iOffset);
  
  if( rc==SQLITE_IOERR_SHORT_READ ){
    rc = SQLITE_OK;  /* Short read at EOF is acceptable */
  }
  return rc;
}
```

```c
/* From src/pager.c - Write page to database file */
static int pager_write_pagelist(Pager *pPager, PgHdr *pList){
  int rc = SQLITE_OK;
  
  while( rc==SQLITE_OK && pList ){
    Pgno pgno = pList->pgno;
    i64 offset = (pgno-1)*(i64)pPager->pageSize;
    void *pData = pList->pData;

    /*═══════════════════════════════════════════════════════════════════╗
    ║  LAYER BOUNDARY CROSSING: Pager → OS Interface                     ║
    ║  Write page data to the database file.                             ║
    ╚═══════════════════════════════════════════════════════════════════*/
    rc = sqlite3OsWrite(pPager->fd, pData, pPager->pageSize, offset);

    /* Update file size tracking */
    if( pgno>pPager->dbFileSize ){
      pPager->dbFileSize = pgno;
    }
    pList = pList->pDirty;
  }
  return rc;
}
```

#### The OS Wrapper Functions

```c
/* From src/os.c - OS wrapper functions (thin shim layer) */

/*═══════════════════════════════════════════════════════════════════════════╗
║  These functions dispatch to the virtual method table.                     ║
║  This indirection enables platform abstraction and testing.                ║
╚═══════════════════════════════════════════════════════════════════════════*/

int sqlite3OsRead(sqlite3_file *id, void *pBuf, int amt, i64 offset){
  DO_OS_MALLOC_TEST(id);
  return id->pMethods->xRead(id, pBuf, amt, offset);  /* Virtual dispatch */
}

int sqlite3OsWrite(sqlite3_file *id, const void *pBuf, int amt, i64 offset){
  DO_OS_MALLOC_TEST(id);
  return id->pMethods->xWrite(id, pBuf, amt, offset); /* Virtual dispatch */
}

int sqlite3OsSync(sqlite3_file *id, int flags){
  DO_OS_MALLOC_TEST(id);
  return flags ? id->pMethods->xSync(id, flags) : SQLITE_OK;
}

int sqlite3OsLock(sqlite3_file *id, int lockType){
  DO_OS_MALLOC_TEST(id);
  assert( lockType>=SQLITE_LOCK_SHARED && lockType<=SQLITE_LOCK_EXCLUSIVE );
  return id->pMethods->xLock(id, lockType);
}

void sqlite3OsClose(sqlite3_file *pId){
  if( pId->pMethods ){
    pId->pMethods->xClose(pId);
    pId->pMethods = 0;
  }
}
```

#### VFS Interface (Platform Abstraction)

```c
/* From src/sqlite.h.in - Virtual File System structure */
struct sqlite3_vfs {
  int iVersion;            /* Structure version number */
  int szOsFile;            /* Size of subclassed sqlite3_file */
  int mxPathname;          /* Maximum file pathname length */
  sqlite3_vfs *pNext;      /* Next registered VFS */
  const char *zName;       /* Name of this virtual file system */
  void *pAppData;          /* Pointer to application-specific data */
  
  /* Method pointers (virtual function table): */
  int (*xOpen)(sqlite3_vfs*, sqlite3_filename zName, sqlite3_file*,
               int flags, int *pOutFlags);
  int (*xDelete)(sqlite3_vfs*, const char *zName, int syncDir);
  int (*xAccess)(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
  int (*xFullPathname)(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
  /* ... more methods ... */
};
```

---

### 3B.5 Error Propagation Across Boundaries

All layer boundaries use the same error propagation pattern:

```c
/* PATTERN: Check-and-propagate */
int higher_layer_function(...) {
  int rc;
  
  /* Call into lower layer */
  rc = sqlite3LowerLayerFunction(...);
  
  /* Propagate error immediately */
  if( rc!=SQLITE_OK ){
    return rc;  /* Or: goto error_handler; */
  }
  
  /* Continue with success case */
  /* ... */
  return SQLITE_OK;
}
```

| Error Code | Meaning | Typical Origin |
|------------|---------|----------------|
| `SQLITE_OK` | Success | All layers |
| `SQLITE_ERROR` | Generic error | SQL Frontend |
| `SQLITE_CORRUPT` | Database corruption | B-Tree |
| `SQLITE_NOTFOUND` | Item not found | B-Tree, Pager |
| `SQLITE_FULL` | Database full | Pager, OS |
| `SQLITE_IOERR` | I/O error | OS Interface |
| `SQLITE_NOMEM` | Out of memory | All layers |
| `SQLITE_BUSY` | Database locked | Pager |
| `SQLITE_LOCKED` | Table locked | B-Tree |

---

### 3B.6 Complete Call Chain Example

Tracing a page read from VDBE to OS:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     COMPLETE CALL CHAIN: Read a Row                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  VDBE Layer (vdbe.c)                                                        │
│  │                                                                          │
│  │  case OP_SeekRowid:                                                      │
│  │    rc = sqlite3BtreeTableMoveto(pCur->uc.pCursor, iKey, 0, &res);       │
│  │                           │                                              │
│  └───────────────────────────┼──────────────────────────────────────────────│
│                              │                                              │
│  B-Tree Layer (btree.c)      ▼                                              │
│  │                                                                          │
│  │  sqlite3BtreeTableMoveto(BtCursor *pCur, ...):                          │
│  │    // Need to read a page to find the row                               │
│  │    rc = getAndInitPage(pCur->pBt, pgno, &pPage, 0);                     │
│  │                           │                                              │
│  │  getAndInitPage(...):     │                                              │
│  │    rc = sqlite3PagerGet(pBt->pPager, pgno, &pDbPage, flags);            │
│  │                           │                                              │
│  └───────────────────────────┼──────────────────────────────────────────────│
│                              │                                              │
│  Pager Layer (pager.c)       ▼                                              │
│  │                                                                          │
│  │  sqlite3PagerGet(Pager *pPager, Pgno pgno, ...):                        │
│  │    // Check page cache first                                            │
│  │    pPg = sqlite3PcacheFetch(pPager->pPCache, pgno, ...);                │
│  │    if( pPg==0 ){                                                        │
│  │      // Cache miss - must read from disk                                │
│  │      rc = readDbPage(pPg);                                              │
│  │    }                      │                                              │
│  │                           │                                              │
│  │  readDbPage(PgHdr *pPg):  │                                              │
│  │    i64 iOffset = (pgno-1) * pageSize;                                   │
│  │    rc = sqlite3OsRead(pPager->fd, pPg->pData, pageSize, iOffset);       │
│  │                           │                                              │
│  └───────────────────────────┼──────────────────────────────────────────────│
│                              │                                              │
│  OS Interface (os.c)         ▼                                              │
│  │                                                                          │
│  │  sqlite3OsRead(sqlite3_file *id, void *pBuf, int amt, i64 offset):      │
│  │    return id->pMethods->xRead(id, pBuf, amt, offset);                   │
│  │                           │                                              │
│  └───────────────────────────┼──────────────────────────────────────────────│
│                              │                                              │
│  Platform VFS (os_unix.c)    ▼                                              │
│  │                                                                          │
│  │  unixRead(sqlite3_file *id, void *pBuf, int amt, i64 offset):           │
│  │    got = pread(id->h, pBuf, amt, offset);  // POSIX system call         │
│  │    return (got==amt) ? SQLITE_OK : SQLITE_IOERR_SHORT_READ;             │
│  │                                                                          │
│  └──────────────────────────────────────────────────────────────────────────│
│                                                                             │
│  RETURN PATH: Errors propagate upward through each layer                    │
│  OS → Pager → B-Tree → VDBE → sqlite3_step() → Application                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文解释:**  
此图展示了从 VDBE 层读取一行数据时的完整调用链。VDBE 调用 B-Tree 定位行，B-Tree 调用 Pager 获取页面，Pager 检查缓存后调用 OS 接口读取文件，OS 接口通过虚函数表调度到平台特定实现（如 Unix 的 `pread`）。错误码沿原路返回，每一层都检查并传播错误。

---

## STEP 4 — Data & Control Flow (End-to-End)

### Tracing: `SELECT * FROM t WHERE id = 1;`

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 1: COMPILE TIME (sqlite3_prepare_v2)                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                  │
│  │   TOKENIZER  │───▶│    PARSER    │───▶│  CODE GEN    │                  │
│  │ tokenize.c   │    │   parse.y    │    │  select.c    │                  │
│  └──────────────┘    └──────────────┘    └──────────────┘                  │
│        │                    │                    │                          │
│        ▼                    ▼                    ▼                          │
│   Token stream         Parse tree         ┌──────────────┐                  │
│   (SELECT, *, FROM,    (Select struct)    │   PLANNER    │                  │
│    t, WHERE, id,              │           │   where.c    │                  │
│    =, 1)                      │           └──────────────┘                  │
│                               │                  │                          │
│                               └──────────────────┤                          │
│                                                  ▼                          │
│                                          ┌──────────────┐                  │
│                                          │     VDBE     │                  │
│                                          │   Bytecode   │                  │
│                                          └──────────────┘                  │
│                                                  │                          │
│  OUTPUT: Vdbe* with bytecode program:           │                          │
│    0: Init       0   12   0                      │                          │
│    1: OpenRead   0   2    0   (root=2)           │                          │
│    2: Integer    1   1    0   (id=1)             │                          │
│    3: SeekRowid  0   11   1                      │                          │
│    4: Column     0   0    2   (col 0)            │                          │
│    5: Column     0   1    3   (col 1)            │                          │
│    6: ...                                        │                          │
│   11: Close      0   0    0                      │                          │
│   12: Halt       0   0    0                      │                          │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 2: RUN TIME (sqlite3_step)                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐                                                           │
│  │     VDBE     │  Executes bytecode instruction-by-instruction             │
│  │   vdbe.c     │                                                           │
│  └──────┬───────┘                                                           │
│         │ OP_OpenRead                                                       │
│         ▼                                                                   │
│  ┌──────────────┐                                                           │
│  │   B-TREE     │  sqlite3BtreeCursor() - opens cursor on table             │
│  │   btree.c    │                                                           │
│  └──────┬───────┘                                                           │
│         │ Needs root page (page 2)                                          │
│         ▼                                                                   │
│  ┌──────────────┐                                                           │
│  │    PAGER     │  sqlite3PagerGet(pgno=2) - fetch page                     │
│  │   pager.c    │  Check page cache → if miss, read from disk               │
│  └──────┬───────┘                                                           │
│         │ Read 4096 bytes at offset (2-1)*4096                              │
│         ▼                                                                   │
│  ┌──────────────┐                                                           │
│  │  OS / VFS    │  sqlite3OsRead() → read() system call                     │
│  │  os_unix.c   │                                                           │
│  └──────────────┘                                                           │
│                                                                             │
│  Control returns upward with page data:                                     │
│  OS → Pager (caches page) → B-Tree (decodes cells) → VDBE (cursor ready)   │
│                                                                             │
│  VDBE continues:                                                            │
│    OP_SeekRowid: B-Tree binary search → Pager page fetches                  │
│    OP_Column: B-Tree payload extraction → returns to VDBE register          │
│    OP_ResultRow: Copy registers to output → SQLITE_ROW returned             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文解释:**  
SQL 语句执行分两个阶段。编译阶段：词法分析器将 SQL 文本转换为标记流，解析器构建语法树，代码生成器与查询规划器协作生成 VDBE 字节码。运行阶段：VDBE 逐条执行字节码，通过 B-Tree 层定位数据，B-Tree 通过 Pager 获取磁盘页面，Pager 通过 OS 接口执行实际 I/O。

### Layer Activity Summary

| Layer | Compile Time | Run Time | Memory Allocation | Performance Critical |
|-------|-------------|----------|-------------------|---------------------|
| Tokenizer | ✓ (tokenize SQL) | — | Token buffer | Low |
| Parser | ✓ (build AST) | — | Parse tree nodes | Medium |
| Code Generator | ✓ (emit bytecode) | — | VDBE ops array | Medium |
| Query Planner | ✓ (select access paths) | — | WhereLoop, WherePath | High (complex queries) |
| VDBE | ✓ (setup) | ✓ (execute) | Registers, cursors | **Very High** |
| B-Tree | — | ✓ (navigate/modify) | Cursor state, cell cache | **Very High** |
| Pager | — | ✓ (page I/O) | Page cache | **Critical** |
| OS Interface | — | ✓ (syscalls) | Minimal | **Critical** |

---

## STEP 5 — Stable vs Volatile Layers

### Layer Stability Analysis

```
    EXTREMELY STABLE                                  INTENTIONALLY FLEXIBLE
    (Rarely Changed)                                  (Designed for Extension)
          │                                                    │
          ▼                                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                                                                          │
│   OS Interface ◄─────────────────────────────────────────────────────►   │
│      (VFS)           Stable API, flexible implementations                │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│      Pager     ◄─────────────────────────────────────────────────────►   │
│                      Core invariants unchanging since 2004               │
│                      WAL added 2010 without changing interface           │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│     B-Tree     ◄─────────────────────────────────────────────────────►   │
│                      File format frozen since version 3.0                │
│                      Internal optimizations ongoing                      │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│      VDBE      ◄─────────────────────────────────────────────────────►   │
│                      Opcode set evolves (new ops added)                  │
│                      Core execution loop stable                          │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Query Planner ◄─────────────────────────────────────────────────────►   │
│                      VOLATILE - Major rewrites (3.8.0, 2013)             │
│                      Ongoing optimization work                           │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SQL Frontend  ◄─────────────────────────────────────────────────────►   │
│                      New SQL features added regularly                    │
│                      Window functions, CTEs, JSON                        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### Stability Classification

| Layer | Stability | Last Major Change | Change Frequency | Reason |
|-------|-----------|-------------------|------------------|--------|
| **OS Interface** | Extremely Stable | 2004 (VFS design) | API frozen, implementations added | Platform abstraction must not break |
| **Pager** | Extremely Stable | 2010 (WAL addition) | Rare, additive only | ACID guarantees depend on invariants |
| **B-Tree** | Very Stable | 2004 (file format frozen) | Optimization only | File format compatibility paramount |
| **VDBE** | Stable | Ongoing (new opcodes) | New ops added, core stable | Bytecode is internal, not exposed |
| **Query Planner** | Moderately Volatile | 2013 (NGQP rewrite) | Active development | Performance improvements valuable |
| **SQL Frontend** | Volatile | Ongoing | New features regularly | SQL standard evolves, users want features |

### How SQLite Protects Stable Layers

1. **File Format Guarantee**: The database file format has been stable since SQLite 3.0.0 (2004). This is enforced by:
   - Extensive backward compatibility tests
   - Version numbers in file header
   - Documented format specification

2. **API Stability Commitment**: Public API functions are never removed, only deprecated.

3. **Invariant Documentation**: Critical invariants are documented in code comments and separate documents (`pager-invariants.txt`).

4. **Regression Test Suite**: Over 100% branch coverage ensures changes don't break existing behavior.

5. **Change Isolation**: The query planner was completely rewritten (NGQP) without touching pager, B-tree, or VDBE code.

---

## STEP 6 — Layer Isolation Techniques in C

### 6.1 File-Level Scoping

SQLite uses `static` functions aggressively to enforce file-level encapsulation:

```c
/* In btree.c - INTERNAL function, not visible outside this file */
static int allocateBtreePage(BtShared *pBt, MemPage **ppPage, Pgno *pPgno, ...);

/* In btree.c - EXTERNAL function, declared in btree.h */
int sqlite3BtreeInsert(BtCursor *pCur, const BtreePayload *pPayload, ...);
```

**Rule**: If a function is not in the public header, it must be `static`.

### 6.2 Public vs Private Headers

```
┌─────────────────────────────────────────────────────────┐
│  btree.h      (PUBLIC)                                  │
│  - Forward declarations: typedef struct Btree Btree;   │
│  - Function prototypes: int sqlite3BtreeOpen(...);      │
│  - No internal struct definitions                       │
└─────────────────────────────────────────────────────────┘
                           │
                           │  #include "btreeInt.h" (only in btree.c)
                           ▼
┌─────────────────────────────────────────────────────────┐
│  btreeInt.h   (PRIVATE)                                 │
│  - Full struct definitions: struct Btree { ... };       │
│  - Internal macros and constants                        │
│  - Implementation details                               │
└─────────────────────────────────────────────────────────┘
```

**中文解释:**  
SQLite 使用公开头文件（如 `btree.h`）声明接口，使用私有头文件（如 `btreeInt.h`）定义内部数据结构。私有头文件仅被实现文件包含，上层永远看不到下层的内部结构。

### 6.3 Opaque Struct Pattern

```c
/* In btree.h - opaque forward declaration */
typedef struct Btree Btree;
typedef struct BtCursor BtCursor;

/* Users can only have POINTERS to these types */
/* The actual struct definition is in btreeInt.h, invisible to callers */

/* Access is only through functions: */
int sqlite3BtreeGetPageSize(Btree*);  /* Getter */
int sqlite3BtreeSetPageSize(Btree*, int, int, int);  /* Setter */
```

### 6.4 Naming Conventions as Layer Markers

```c
/* Layer identification via function prefix: */

sqlite3_*       /* Public API (sqlite3_prepare, sqlite3_step) */
sqlite3Vdbe*    /* VDBE layer */
sqlite3Btree*   /* B-Tree layer */
sqlite3Pager*   /* Pager layer */
sqlite3Pcache*  /* Page cache */
sqlite3Os*      /* OS interface wrappers */
sqlite3Wal*     /* Write-ahead log */
```

### 6.5 Indirection Tables (VFS)

The OS Interface uses a **virtual method table** pattern:

```c
/* In sqlite.h.in */
struct sqlite3_vfs {
  int iVersion;            /* Structure version number */
  int szOsFile;            /* Size of subclassed sqlite3_file */
  int mxPathname;          /* Maximum pathname length */
  sqlite3_vfs *pNext;      /* Next registered VFS */
  const char *zName;       /* Name of this VFS */
  void *pAppData;          /* Application-specific data */
  
  /* Method pointers (virtual function table): */
  int (*xOpen)(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
  int (*xDelete)(sqlite3_vfs*, const char*, int);
  int (*xAccess)(sqlite3_vfs*, const char*, int, int*);
  int (*xFullPathname)(sqlite3_vfs*, const char*, int, char*);
  /* ... more methods ... */
};

/* sqlite3_file is similarly abstract: */
struct sqlite3_file {
  const sqlite3_io_methods *pMethods;  /* Pointer to method table */
};

struct sqlite3_io_methods {
  int iVersion;
  int (*xClose)(sqlite3_file*);
  int (*xRead)(sqlite3_file*, void*, int, sqlite3_int64);
  int (*xWrite)(sqlite3_file*, const void*, int, sqlite3_int64);
  /* ... more methods ... */
};
```

**Indirection Usage:**
```c
/* Pager calls OS through indirection: */
rc = pPager->pVfs->xOpen(pPager->pVfs, zPath, pFile, flags, &flagsOut);

/* Or via wrapper (preferred): */
rc = sqlite3OsOpen(pVfs, zPath, pFile, flags, &flagsOut);
```

### 6.6 Comment-Based Invariants

```c
/* From pager.c - comments establish invariants: */

/*
** The pager state machine:
**
**                     OPEN <------+
**                       |         |
**                       V         |
**               +-> READER <------+
**               |      |         
**               |      V         
**               +-- WRITER_LOCKED 
**                      |
**                      V
**               WRITER_CACHEMOD
**                      |
**                      V
**               WRITER_DBMOD
**                      |
**                      V
**               WRITER_FINISHED
**
** These states are used to enforce the constraint that pages must
** be written to the journal before they can be modified...
*/
```

### Why These Techniques Scale

1. **Compile-Time Enforcement**: `static` functions cause linker errors if accessed cross-file.

2. **Grep-able Prefixes**: `grep "sqlite3Btree"` finds all B-tree API calls instantly.

3. **Opaque Structs Prevent Coupling**: Upper layers cannot depend on lower layer struct layouts.

4. **VFS Indirection Enables Testing**: Mock VFS can be injected for fault testing.

5. **Documentation in Code**: Invariants documented where they're enforced, not in separate docs that drift.

---

## STEP 7 — Testability Enabled by Layering

### 7.1 Layer-Independent Testing

SQLite's architecture enables testing at each layer in isolation:

```
┌─────────────────────────────────────────────────────────────────┐
│                     TEST ISOLATION POINTS                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SQL Frontend  ◄── Tested via SQL input/output comparison       │
│                    test/select1.test, test/insert.test          │
│                                                                 │
│  Query Planner ◄── Tested via EXPLAIN QUERY PLAN                │
│                    test/where*.test                             │
│                                                                 │
│  VDBE          ◄── Tested via EXPLAIN output                    │
│                    Bytecode verification                        │
│                                                                 │
│  B-Tree        ◄── Direct API testing (test3.c, test_btree.c)   │
│                    Can be tested WITHOUT SQL layer              │
│                                                                 │
│  Pager         ◄── Direct API testing (test2.c)                 │
│                    Journal verification                         │
│                                                                 │
│  OS Interface  ◄── Mock VFS injection (test_vfs.c)              │
│                    Can simulate any OS behavior                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Lower Layer Stress Testing

The **bottom layers receive the most aggressive testing** because upper layer correctness depends on them:

```c
/* From test_vfs.c - Fault injection VFS */

static int tvfsWrite(
  sqlite3_file *pFile,
  const void *zBuf,
  int iAmt,
  sqlite_int64 iOfst
){
  TestvfsFile *p = (TestvfsFile *)pFile;
  
  /* Check if we should simulate a failure */
  if( tvfsInjectFullerr(p->pVfs) ){
    return SQLITE_FULL;  /* Simulate disk full */
  }
  if( tvfsInjectIoerr(p->pVfs) ){
    return SQLITE_IOERR;  /* Simulate I/O error */
  }
  
  /* Otherwise, proceed with real write */
  return sqlite3OsWrite(REALFILE(pFile), zBuf, iAmt, iOfst);
}
```

### 7.3 Fault Injection Across Layers

SQLite systematically injects faults at layer boundaries:

| Injection Point | Mechanism | Tests |
|-----------------|-----------|-------|
| Memory allocation | `sqlite3_config(SQLITE_CONFIG_MALLOC, ...)` | OOM at every allocation |
| Disk I/O | Custom VFS | Read errors, write errors, disk full |
| File locking | Custom VFS | Lock contention, timeouts |
| System calls | `test_syscall.c` | `open()` fail, `mmap()` fail |
| Page cache | `test_pcache.c` | Cache pressure simulation |

### 7.4 Coverage Testing Strategy

```c
/* Bytecode coverage - from vdbe.c */
#ifdef SQLITE_VDBE_COVERAGE
  void sqlite3VdbeSetLineNumber(Vdbe *v, int iLineno){
    /* Track which source lines generated which bytecode */
  }
  #define VdbeCoverage(v) sqlite3VdbeSetLineNumber(v, __LINE__)
#endif
```

**Coverage metrics**:
- 100% branch coverage target
- Every VDBE opcode path exercised
- Every error return path verified

### 7.5 Boundary Testing Example

```c
/* From test2.c - Testing Pager directly */

static int pager_open(
  void *NotUsed,
  Tcl_Interp *interp,
  int argc,
  const char **argv
){
  Pager *pPager;
  int rc;
  
  /* Create pager WITHOUT any SQL or VDBE */
  rc = sqlite3PagerOpen(pVfs, &pPager, zFilename, 0, 
                        PAGER_OMIT_JOURNAL, vfsFlags, 0);
  
  /* Test pager operations directly */
  rc = sqlite3PagerGet(pPager, 1, &pPage, 0);
  /* ... */
}
```

---

## STEP 8 — Evolution & Change Scenarios

### Scenario 1: Add a New Storage Backend

**Layers Affected**: OS Interface only  
**Layers Untouched**: SQL Frontend, Planner, VDBE, B-Tree, Pager

```
┌─────────────────────────────────────────────────────────────────┐
│  Existing Architecture                New Backend Added          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SQL Frontend    ── unchanged ──     SQL Frontend               │
│       │                                   │                     │
│  Query Planner   ── unchanged ──     Query Planner              │
│       │                                   │                     │
│      VDBE        ── unchanged ──        VDBE                    │
│       │                                   │                     │
│     B-Tree       ── unchanged ──       B-Tree                   │
│       │                                   │                     │
│     Pager        ── unchanged ──        Pager                   │
│       │                                   │                     │
│  ┌─────────┐                         ┌─────────┐               │
│  │ os_unix │                         │ os_unix │               │
│  │ os_win  │                         │ os_win  │               │
│  └─────────┘                         │ os_kv   │  ◄── NEW      │
│                                      └─────────┘               │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation**: Create new VFS implementation (`os_kv.c` for key-value store backend):

```c
static sqlite3_vfs kvVfs = {
  3,                    /* iVersion */
  sizeof(KVFile),       /* szOsFile */
  256,                  /* mxPathname */
  0,                    /* pNext */
  "kvstore",            /* zName */
  0,                    /* pAppData */
  kvOpen,               /* xOpen */
  kvDelete,             /* xDelete */
  /* ... implement all methods ... */
};

/* Register at startup */
sqlite3_vfs_register(&kvVfs, 0);
```

### Scenario 2: Optimize Query Execution

**Layers Affected**: Query Planner, possibly VDBE (new opcodes)  
**Layers Untouched**: B-Tree, Pager, OS Interface

**Example**: The 2013 "Next Generation Query Planner" (NGQP) rewrite:
- Complete rewrite of `where.c`
- No changes to `btree.c`, `pager.c`, or `os_*.c`
- Same test suite passed before and after

### Scenario 3: Replace the OS Interface Layer

**Layers Affected**: OS Interface only  
**Layers Untouched**: Everything above

**Example**: Running SQLite on a real-time OS without POSIX:

```c
/* All that's needed: implement sqlite3_vfs interface */
static sqlite3_vfs rtosVfs = {
  .xOpen = rtos_file_open,
  .xRead = rtos_file_read,
  .xWrite = rtos_file_write,
  .xLock = rtos_file_lock,
  /* ... */
};
```

### Scenario 4: Improve Performance Without Violating Layering

**Layers Affected**: Varies by optimization  
**Constraint**: Layer boundaries must not be bypassed

**Examples of valid optimizations**:

| Optimization | Layer | Approach |
|--------------|-------|----------|
| Better join ordering | Planner | Improve cost model in `where.c` |
| Faster B-tree search | B-Tree | Optimize `sqlite3BtreeTableMoveto()` |
| Reduced syscalls | Pager | Memory-map I/O (stays within VFS) |
| VDBE op fusion | VDBE | Combine opcodes in `vdbe.c` |

**Anti-pattern** (rejected): Having VDBE directly call `read()` to "optimize" I/O would violate layering.

### Why the Architecture Supports Evolution

1. **Interface Stability**: Each layer's public API is narrower than its implementation. Changes below the interface don't ripple up.

2. **Substitutability**: VFS makes OS layer pluggable. Could add hypothetical `btree2.c` behind same `btree.h` interface.

3. **Isolation of Complexity**: Query planner is complex and evolves rapidly; B-tree is stable. They don't interfere.

4. **Test-Driven Confidence**: Comprehensive tests ensure changes don't break layer contracts.

---

## STEP 9 — Trade-offs & Architectural Discipline

### 9.1 Performance Costs of Layering

| Cost | Description | Mitigation |
|------|-------------|------------|
| Function call overhead | Each layer boundary is a function call | Amalgamation enables inlining |
| Data copying | Some data copied at boundaries | Careful buffer management |
| Abstraction overhead | VFS indirection table lookup | Compiler optimizes well-known patterns |
| Cache inefficiency | Opaque structs prevent layout optimization | Profile-guided placement |

**Measured overhead**: The amalgamation build (single `sqlite3.c`) is ~5-10% faster than separate compilation due to cross-layer inlining.

### 9.2 Places Where Layering is Bent

SQLite is pragmatic. Some intentional layer violations exist:

#### 9.2.1 B-Tree Statistics for Planner

```c
/* Query planner needs B-tree statistics for cost estimation */
/* This crosses layer boundary (Planner → B-Tree) */
LogEst sqlite3BtreeEst(BtCursor *pCur);
```

**Justification**: Cost-based optimization requires storage statistics. Pure layering would require duplicating statistics in a separate module.

#### 9.2.2 Direct Overflow Read Optimization

```c
#ifdef SQLITE_DIRECT_OVERFLOW_READ
/* B-Tree can bypass pager for reading overflow pages in WAL mode */
int sqlite3PagerDirectReadOk(Pager *pPager, Pgno pgno);
#endif
```

**Justification**: Significant performance gain for large BLOBs. Carefully constrained by compile-time flag.

#### 9.2.3 Schema Information Sharing

The B-tree layer stores schema pointers that the SQL layer accesses:

```c
void *sqlite3BtreeSchema(Btree*, int, void(*)(void*));
```

**Justification**: Schema is logically owned by SQL layer but must survive B-tree cache operations.

### 9.3 Explicit Trade-offs

| Trade-off | Decision | Rationale |
|-----------|----------|-----------|
| Simplicity vs Performance | Favor simplicity | Reliability > raw speed for most users |
| Flexibility vs Optimization | Moderate flexibility | VFS is pluggable; B-tree is not |
| Code size vs Features | Compile-time omit flags | `-DSQLITE_OMIT_*` removes features |
| Portability vs Platform features | Favor portability | Single codebase for all platforms |

### 9.4 What's NOT in SQLite (by design)

- **No pluggable storage engine**: Unlike MySQL, you cannot replace B-tree without forking.
- **No async I/O layer**: VFS is synchronous; async handled at application level.
- **No network protocol**: By design, SQLite is embedded-only.

These omissions preserve architectural simplicity.

---

## STEP 10 — Transferable Architecture Lessons

### Lesson 1: Establish Strict Layer Boundaries Early

**SQLite Approach**: Seven layers defined early, maintained for 20+ years.

**Your Application**:
```c
/* Define your layers explicitly in a header */
/* myapp_architecture.h */

/*
 * LAYER 1: Application Logic
 * LAYER 2: Business Rules  
 * LAYER 3: Data Access
 * LAYER 4: Platform Abstraction
 *
 * RULE: Layer N may only call Layer N+1
 */
```

### Lesson 2: Use Opaque Pointers Religiously

**SQLite Approach**:
```c
typedef struct Pager Pager;  /* Opaque - definition hidden */
```

**Your Application**:
```c
/* In public header: mymodule.h */
typedef struct MyHandle MyHandle;

MyHandle *my_create(void);
void my_destroy(MyHandle*);
int my_operation(MyHandle*, const void* data);

/* In implementation: mymodule.c */
struct MyHandle {
  /* All private fields here */
  int internal_state;
  void *internal_buffer;
};
```

### Lesson 3: Define Error Propagation Strategy Per Layer

**SQLite Approach**: Each layer returns `int` error codes. Extended errors available through callbacks.

**Your Application**:
```c
/* Define error codes per layer */
#define MYAPP_OK           0
#define MYAPP_ERROR        1
#define MYAPP_NOMEM        7
#define MYAPP_IO_ERROR    10

/* Each function returns error code */
int myapp_do_operation(MyHandle *h, int param);

/* Extended error info via getter */
const char *myapp_errmsg(MyHandle *h);
```

### Lesson 4: Use Naming Conventions as Architecture Enforcement

**SQLite Approach**: `sqlite3Btree*`, `sqlite3Pager*`, etc.

**Your Application**:
```c
/* Prefix indicates layer ownership */
int platform_file_open(...);    /* Platform layer */
int storage_write_record(...);  /* Storage layer */
int business_validate_input(...);/* Business layer */
int app_handle_request(...);    /* Application layer */

/* grep 'platform_' finds all platform layer functions */
/* grep 'storage_' finds all storage layer functions */
```

### Lesson 5: Keep Lower Layers Smaller and More Stable

**SQLite Approach**: OS interface is tiny and frozen. SQL frontend is large and evolving.

**Your Application**:
```
Layer Size and Stability

     Small ◄──────────────────────► Large
     Stable ◄──────────────────────► Volatile

     ┌─────────┐
     │Platform │  ◄── Smallest, most stable
     └────┬────┘
          │
     ┌────┴────┐
     │ Storage │  ◄── Medium size, stable
     └────┬────┘
          │
     ┌────┴────┐
     │Business │  ◄── Larger, moderate changes
     └────┬────┘
          │
     ┌────┴────┐
     │   App   │  ◄── Largest, most volatile
     └─────────┘
```

### Lesson 6: Provide Injection Points for Testing

**SQLite Approach**: VFS allows mock file systems; malloc wrappers allow OOM testing.

**Your Application**:
```c
/* Define injection points */
struct my_platform_ops {
  int (*open)(const char *path, int flags);
  int (*read)(int fd, void *buf, size_t n);
  int (*write)(int fd, const void *buf, size_t n);
  void *(*malloc)(size_t n);
  void (*free)(void *p);
};

/* Default uses real system calls */
extern struct my_platform_ops my_default_ops;

/* Tests can inject mock operations */
extern struct my_platform_ops my_mock_ops;

/* Module uses ops pointer, not direct syscalls */
static struct my_platform_ops *ops = &my_default_ops;

int storage_write_record(void *data, size_t len) {
  void *buf = ops->malloc(len);  /* Can be mocked */
  /* ... */
}
```

### Lesson 7: Document Invariants Where They're Enforced

**SQLite Approach**: Invariants documented in comments alongside enforcement code.

**Your Application**:
```c
/*
 * INVARIANT: buffer must be freed before handle is destroyed
 * ENFORCEMENT: destroy() asserts buffer == NULL
 * VIOLATION CONSEQUENCE: memory leak
 */
void my_destroy(MyHandle *h) {
  assert(h->buffer == NULL && "INVARIANT VIOLATION: buffer not freed");
  free(h);
}
```

### Lesson 8: The Amalgamation Pattern for C

**SQLite Approach**: All `.c` files concatenated into single `sqlite3.c` for production.

**Your Application**:
```bash
#!/bin/bash
# build_amalgamation.sh

# Concatenate all source files in dependency order
cat platform/*.c > myapp_amalgamation.c
cat storage/*.c >> myapp_amalgamation.c
cat business/*.c >> myapp_amalgamation.c
cat app/*.c >> myapp_amalgamation.c

# Compile as single translation unit - enables cross-layer inlining
gcc -O3 -o myapp myapp_amalgamation.c
```

---

## Summary: SQLite as Architectural Reference

SQLite demonstrates that **rigorous layered architecture scales in C** through:

1. **Convention over enforcement**: Naming, file scoping, and opaque pointers replace language features.

2. **Stability gradient**: Lower layers frozen, upper layers evolve.

3. **Interface narrowing**: Public APIs expose less than implementations provide.

4. **Testability by design**: Every layer boundary is a test injection point.

5. **Pragmatic exceptions**: Performance-critical paths may bend rules, but explicitly.

The architecture has remained fundamentally unchanged for 20 years while supporting continuous feature addition and performance improvement—the hallmark of successful systems design.

---

## References

- SQLite Source Files: `src/btree.h`, `src/pager.h`, `src/vdbe.h`, `src/os.h`
- SQLite Documentation: `doc/pager-invariants.txt`
- SQLite Architecture: https://www.sqlite.org/arch.html


