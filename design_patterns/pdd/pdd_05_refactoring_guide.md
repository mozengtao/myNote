# Recognizing & Refactoring Module Boundaries

## Introduction

```
+------------------------------------------------------------------+
|  REFACTORING TOWARD PDD IS INCREMENTAL AND REVERSIBLE            |
+------------------------------------------------------------------+

    Key Principles:
    
    1. UNDERSTAND BEFORE CHANGING - Map existing boundaries first
    2. INCREMENTAL STEPS - One boundary at a time
    3. MAINTAIN BEHAVIOR - Tests before and after each step
    4. DEPENDENCY DIRECTION - Always enforce downward flow
    5. EXTRACT, DON'T REWRITE - Pull logic out, don't start over
```

---

## Step 1: Identifying Hidden Coupling

### What to Look For

```
+------------------------------------------------------------------+
|  COUPLING INDICATORS                                             |
+------------------------------------------------------------------+

    HEADER DEPENDENCIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  Run: grep -r "#include" src/ | sort | uniq -c | sort -rn  │
    │                                                              │
    │  Look for:                                                   │
    │  • Headers included everywhere                               │
    │  • Cross-layer includes (domain → presentation)             │
    │  • Utility headers that pull in everything                  │
    └─────────────────────────────────────────────────────────────┘

    FUNCTION CALL PATTERNS:
    ┌─────────────────────────────────────────────────────────────┐
    │  Run: cscope -R or grep for function calls                  │
    │                                                              │
    │  Look for:                                                   │
    │  • Presentation calling data layer directly                 │
    │  • Data layer calling domain functions                      │
    │  • Callbacks that bypass domain                             │
    └─────────────────────────────────────────────────────────────┘

    GLOBAL STATE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Run: grep "extern" include/*.h                             │
    │                                                              │
    │  Look for:                                                   │
    │  • Global variables shared between layers                   │
    │  • Singletons accessed across boundaries                    │
    │  • Static state in headers                                  │
    └─────────────────────────────────────────────────────────────┘
```

### Coupling Analysis Script

```bash
#!/bin/bash
# coupling_analysis.sh - Find coupling issues

echo "=== HEADER DEPENDENCY ANALYSIS ==="
echo "Most included headers:"
grep -rh "#include" src/ include/ 2>/dev/null | \
    grep -v "^//" | \
    sed 's/.*#include *[<"]\([^>"]*\)[">].*/\1/' | \
    sort | uniq -c | sort -rn | head -20

echo ""
echo "=== CROSS-LAYER INCLUDES ==="
echo "Domain including presentation:"
grep -rn "#include.*presentation" src/domain/ include/domain/ 2>/dev/null

echo "Data including domain (not interface):"
grep -rn "#include.*domain/" src/data/ include/data/ 2>/dev/null | \
    grep -v "_ops\|_interface\|_if"

echo ""
echo "=== GLOBAL STATE ==="
echo "Extern declarations:"
grep -rn "^extern" include/ 2>/dev/null | head -20

echo ""
echo "=== FUNCTION CALL DIRECTIONS ==="
# This requires ctags/cscope for accurate analysis
echo "Check manually with cscope or IDE tools"
```

### Coupling Matrix

```
+------------------------------------------------------------------+
|  COUPLING MATRIX TEMPLATE                                        |
+------------------------------------------------------------------+

    Build a matrix of dependencies:

                    │ Present │ Domain │ Data │
    ────────────────┼─────────┼────────┼──────┤
    Presentation    │    -    │   ✓    │  ✗   │  (Should only → Domain)
    Domain          │    ✗    │   -    │  ✓*  │  (* via interface only)
    Data            │    ✗    │   ✗    │   -  │  (Should be independent)

    Mark each cell:
    ✓  = Correct dependency
    ✗  = Should not exist
    !  = Exists but shouldn't
    ?  = Unknown, needs investigation
```

---

## Step 2: Finding Policy vs Mechanism

### The Key Question

```
+------------------------------------------------------------------+
|  ASK: "WHAT should happen?" vs "HOW does it happen?"             |
+------------------------------------------------------------------+

    WHAT (Policy - belongs in Domain):
    ┌─────────────────────────────────────────────────────────────┐
    │  • "Should we accept this connection?"                      │
    │  • "Is this user authorized?"                               │
    │  • "What's the priority of this request?"                   │
    │  • "Should we retry this operation?"                        │
    │  • "What's the timeout value?"                              │
    └─────────────────────────────────────────────────────────────┘

    HOW (Mechanism - belongs in Data):
    ┌─────────────────────────────────────────────────────────────┐
    │  • "How to read from the socket"                            │
    │  • "How to check file permissions"                          │
    │  • "How to send to the queue"                               │
    │  • "How to set a timer"                                     │
    │  • "How to encrypt this data"                               │
    └─────────────────────────────────────────────────────────────┘
```

### Code Examination Technique

```c
/* BEFORE: Mixed policy and mechanism */

int process_request(struct request *r)
{
    /* HOW: Read from socket - MECHANISM */
    int n = recv(r->socket, r->buf, sizeof(r->buf), 0);
    
    /* WHAT: Validate format - POLICY */
    if (!validate_request_format(r->buf, n))
        return -1;
    
    /* HOW: Query database - MECHANISM */
    struct user *u = db_find_user(r->user_id);
    
    /* WHAT: Check authorization - POLICY */
    if (!user_can_access(u, r->resource))
        return -EACCES;
    
    /* HOW: Execute operation - MECHANISM */
    return execute_sql(r->query);
}

/* Analysis: This function mixes all three layers */
```

```c
/* AFTER: Separated */

/* PRESENTATION: Protocol handling */
int handle_request(int socket)
{
    char buf[MAX_REQUEST];
    int n = recv(socket, buf, sizeof(buf), 0);  /* Mechanism OK here */
    
    struct request r;
    if (parse_request(buf, n, &r) < 0)  /* Format parsing OK here */
        return send_error(socket, "parse error");
    
    /* Delegate to domain */
    enum result res = process_request(&r);
    return send_response(socket, res);
}

/* DOMAIN: Policy decisions */
enum result process_request(struct request *r)
{
    /* POLICY: Validate business rules */
    if (!request_valid(r))
        return RESULT_INVALID;
    
    /* POLICY: Authorization */
    struct user u;
    if (user_repo_find(&u, r->user_id) != REPO_OK)
        return RESULT_NOT_FOUND;
    
    if (!policy_check_access(&u, r->resource))
        return RESULT_FORBIDDEN;
    
    /* POLICY: Execute business operation */
    return execute_operation(r);
}

/* DATA: Mechanism implementation */
int user_repo_find(struct user *u, int id)
{
    return db_query_user(db_ctx, id, u);  /* Pure mechanism */
}
```

### Policy Extraction Checklist

```
+------------------------------------------------------------------+
|  POLICY EXTRACTION CHECKLIST                                     |
+------------------------------------------------------------------+

    For each function, ask:

    □ Does it decide IF something should happen? → Policy
    □ Does it decide WHICH option to choose? → Policy  
    □ Does it validate business rules? → Policy
    □ Does it manage state transitions? → Policy
    □ Does it calculate business values? → Policy

    If YES to any above:
    □ Extract to domain layer
    □ Pass mechanism operations as callbacks/ops
    □ Return decisions, don't execute mechanisms
```

---

## Step 3: Recognizing Unstable Dependencies

### Volatility Analysis

```
+------------------------------------------------------------------+
|  VOLATILITY: How Often Does Each Part Change?                    |
+------------------------------------------------------------------+

    HIGH VOLATILITY (should depend on nothing):
    ┌─────────────────────────────────────────────────────────────┐
    │  • UI/CLI formatting                                        │
    │  • Protocol versions                                        │
    │  • Database schemas                                         │
    │  • External API clients                                     │
    │  • Configuration parsing                                    │
    └─────────────────────────────────────────────────────────────┘

    LOW VOLATILITY (others should depend on this):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Core domain entities                                     │
    │  • Business rule interfaces                                 │
    │  • Fundamental algorithms                                   │
    │  • Error types and result codes                            │
    └─────────────────────────────────────────────────────────────┘

    RULE: High volatility should depend on low volatility
          Never the reverse!
```

### Git-Based Volatility Analysis

```bash
#!/bin/bash
# volatility_analysis.sh - Which files change most?

echo "=== FILE CHANGE FREQUENCY (Last 6 months) ==="
git log --since="6 months ago" --name-only --pretty=format: | \
    grep -v "^$" | \
    sort | uniq -c | sort -rn | head -30

echo ""
echo "=== BY DIRECTORY ==="
git log --since="6 months ago" --name-only --pretty=format: | \
    grep -v "^$" | \
    sed 's|/[^/]*$||' | \
    sort | uniq -c | sort -rn | head -20

echo ""
echo "=== CO-CHANGE ANALYSIS ==="
echo "Files that change together (indicates coupling):"
# This is simplified - real analysis needs more sophisticated tools
git log --since="6 months ago" --name-only --pretty=format:"---" | \
    awk '/^---/{if(NR>1)print files; files=""} /^[^-]/{files=files" "$0} END{print files}' | \
    tr ' ' '\n' | sort | uniq -c | sort -rn | head -20
```

### Dependency Direction Rules

```
+------------------------------------------------------------------+
|  STABLE DEPENDENCIES PRINCIPLE                                   |
+------------------------------------------------------------------+

    ALWAYS CORRECT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Volatile (UI) ──────────────────→ Stable (Domain)         │
    │  Changing (Protocol) ────────────→ Stable (Domain)         │
    │  Replaceable (Storage) ──────────→ Stable (Interface)     │
    └─────────────────────────────────────────────────────────────┘

    ALWAYS WRONG:
    ┌─────────────────────────────────────────────────────────────┐
    │  Stable (Domain) ──────────────→ Volatile (UI)             │
    │  Core (Algorithm) ─────────────→ Changing (Protocol)       │
    │  Interface ────────────────────→ Implementation            │
    └─────────────────────────────────────────────────────────────┘
```

---

## Step 4: Extracting Domain Logic Safely

### The Strangler Pattern

```
+------------------------------------------------------------------+
|  STRANGLER PATTERN: Gradual Extraction                           |
+------------------------------------------------------------------+

    Phase 1: IDENTIFY
    ┌─────────────────────────────────────────────────────────────┐
    │  • Find business logic in wrong layer                       │
    │  • Mark it but don't move yet                              │
    │  • Write tests for current behavior                        │
    └─────────────────────────────────────────────────────────────┘

    Phase 2: DUPLICATE
    ┌─────────────────────────────────────────────────────────────┐
    │  • Create domain function with same logic                   │
    │  • Test domain function in isolation                        │
    │  • Don't change original yet                               │
    └─────────────────────────────────────────────────────────────┘

    Phase 3: DELEGATE
    ┌─────────────────────────────────────────────────────────────┐
    │  • Change original to call domain function                  │
    │  • Verify behavior unchanged                                │
    │  • Original becomes thin wrapper                           │
    └─────────────────────────────────────────────────────────────┘

    Phase 4: REMOVE
    ┌─────────────────────────────────────────────────────────────┐
    │  • Remove duplicated logic from original                    │
    │  • Original now only adapts input/output                   │
    │  • Run all tests                                            │
    └─────────────────────────────────────────────────────────────┘
```

### Step-by-Step Extraction Example

```c
/* STEP 1: Original code with mixed concerns */
/* api/handler.c */
int handle_create_order(struct http_request *req)
{
    /* Parse JSON (OK in presentation) */
    struct order_data data;
    if (parse_json(req->body, &data) < 0)
        return http_error(400, "invalid json");
    
    /* BUSINESS LOGIC - WRONG PLACE! */
    if (data.quantity <= 0)
        return http_error(400, "invalid quantity");
    
    if (data.price < 0)
        return http_error(400, "invalid price");
    
    double total = data.quantity * data.price;
    if (total < MIN_ORDER_TOTAL)
        return http_error(400, "order too small");
    
    double tax = total * get_tax_rate(data.region);
    double final = total + tax;
    
    /* Store in database */
    int id = db_insert_order(&data, final);
    if (id < 0)
        return http_error(500, "database error");
    
    return http_json(200, "{\"id\": %d}", id);
}
```

```c
/* STEP 2: Create domain function */
/* domain/order.h */
struct order_input {
    int quantity;
    double price;
    const char *region;
};

struct order {
    int id;
    double total;
    double tax;
    double final_price;
    enum order_status status;
};

enum order_result {
    ORDER_OK = 0,
    ORDER_INVALID_QUANTITY,
    ORDER_INVALID_PRICE,
    ORDER_TOO_SMALL,
};

enum order_result order_create(struct order *out, 
                               const struct order_input *in);

/* domain/order.c */
#define MIN_ORDER_TOTAL 10.0

enum order_result order_create(struct order *out,
                               const struct order_input *in)
{
    /* All business logic HERE */
    if (in->quantity <= 0)
        return ORDER_INVALID_QUANTITY;
    
    if (in->price < 0)
        return ORDER_INVALID_PRICE;
    
    out->total = in->quantity * in->price;
    if (out->total < MIN_ORDER_TOTAL)
        return ORDER_TOO_SMALL;
    
    out->tax = out->total * tax_rate_for_region(in->region);
    out->final_price = out->total + out->tax;
    out->status = ORDER_PENDING;
    
    return ORDER_OK;
}
```

```c
/* STEP 3: Delegate from original */
/* api/handler.c */
int handle_create_order(struct http_request *req)
{
    struct order_data data;
    if (parse_json(req->body, &data) < 0)
        return http_error(400, "invalid json");
    
    /* Convert to domain input */
    struct order_input input = {
        .quantity = data.quantity,
        .price = data.price,
        .region = data.region,
    };
    
    /* DELEGATE TO DOMAIN */
    struct order order;
    enum order_result res = order_create(&order, &input);
    
    /* Map domain result to HTTP */
    if (res != ORDER_OK)
        return http_error(400, order_result_message(res));
    
    /* Persist via repository */
    int id = order_repo_save(&order);
    if (id < 0)
        return http_error(500, "storage error");
    
    return http_json(200, "{\"id\": %d}", id);
}

/* STEP 4: Refine - handler is now thin */
```

---

## Step 5: Refactoring Toward PDD Incrementally

### Prioritization Matrix

```
+------------------------------------------------------------------+
|  REFACTORING PRIORITY MATRIX                                     |
+------------------------------------------------------------------+

                        │ Easy to Extract │ Hard to Extract │
    ────────────────────┼─────────────────┼─────────────────┤
    High Business Value │      DO FIRST   │   PLAN CAREFULLY│
    Low Business Value  │     DO SECOND   │    SKIP/DEFER   │

    START WITH:
    1. High value + Easy = Quick wins, visible improvement
    2. Low value + Easy = Practice, build confidence
    3. High value + Hard = Plan thoroughly, allocate time
    4. Low value + Hard = Don't bother (yet)
```

### Incremental Refactoring Checklist

```
+------------------------------------------------------------------+
|  BEFORE EACH REFACTORING STEP                                    |
+------------------------------------------------------------------+

    □ Tests exist for current behavior
    □ Tests pass
    □ Clear goal for this step
    □ Rollback plan if things break
    □ Team aware of changes
    
+------------------------------------------------------------------+
|  DURING REFACTORING                                              |
+------------------------------------------------------------------+

    □ One logical change at a time
    □ Compile after each change
    □ Run tests frequently
    □ Commit at stable points
    □ Document why, not just what
    
+------------------------------------------------------------------+
|  AFTER REFACTORING                                               |
+------------------------------------------------------------------+

    □ All tests pass
    □ New boundary is enforced (compile checks)
    □ No behavior change (unless intended)
    □ Code review completed
    □ Update documentation
```

---

## Step 6: Validating Correctness After Refactoring

### Compile-Time Validation

```c
/* Use include guards to enforce boundaries */

/* domain/core.h */
#ifndef DOMAIN_CORE_H
#define DOMAIN_CORE_H

/* DOMAIN BOUNDARY MARKER */
#ifdef PRESENTATION_LAYER
#error "Domain must not include presentation headers"
#endif

/* ... domain code ... */

#endif
```

```makefile
# Makefile rules to enforce boundaries
CFLAGS_DOMAIN = -DDOMAIN_LAYER
CFLAGS_PRESENT = -DPRESENTATION_LAYER
CFLAGS_DATA = -DDATA_LAYER

domain/%.o: domain/%.c
    $(CC) $(CFLAGS_DOMAIN) -c $< -o $@

presentation/%.o: presentation/%.c
    $(CC) $(CFLAGS_PRESENT) -c $< -o $@
```

### Behavioral Validation

```
+------------------------------------------------------------------+
|  BEHAVIORAL VALIDATION CHECKLIST                                 |
+------------------------------------------------------------------+

    FUNCTIONAL TESTS:
    □ All existing tests pass
    □ New domain tests added
    □ Edge cases still handled
    □ Error paths verified

    INTEGRATION TESTS:
    □ End-to-end scenarios work
    □ Performance not degraded
    □ No new memory leaks (valgrind)
    □ Thread safety maintained

    BOUNDARY TESTS:
    □ Domain works without presentation
    □ Data layer mocked successfully
    □ Interface contracts honored
```

### Dependency Verification Script

```bash
#!/bin/bash
# verify_boundaries.sh - Check layer dependencies

echo "=== CHECKING DOMAIN LAYER ==="
echo "Domain should NOT include presentation:"
if grep -r "#include.*presentation" src/domain/ include/domain/ 2>/dev/null; then
    echo "VIOLATION FOUND!"
    exit 1
fi

echo "Domain should NOT include data implementations:"
if grep -rn "#include.*data/.*impl" src/domain/ 2>/dev/null; then
    echo "VIOLATION FOUND!"
    exit 1
fi

echo ""
echo "=== CHECKING DATA LAYER ==="
echo "Data should NOT include domain (except interfaces):"
grep -rn "#include.*domain" src/data/ 2>/dev/null | \
    grep -v "_ops\|_interface\|_if" && {
    echo "POSSIBLE VIOLATION - check manually"
}

echo ""
echo "=== BOUNDARY CHECK PASSED ==="
```

---

## Summary: Refactoring Workflow

```
+------------------------------------------------------------------+
|  PDD REFACTORING WORKFLOW                                        |
+------------------------------------------------------------------+

    PHASE 1: ANALYZE (1-2 days)
    ┌─────────────────────────────────────────────────────────────┐
    │  □ Run coupling analysis scripts                            │
    │  □ Build dependency matrix                                  │
    │  □ Identify policy vs mechanism                             │
    │  □ Analyze volatility                                       │
    │  □ Document current state                                   │
    └─────────────────────────────────────────────────────────────┘

    PHASE 2: PLAN (1 day)
    ┌─────────────────────────────────────────────────────────────┐
    │  □ Prioritize extractions                                   │
    │  □ Define target architecture                               │
    │  □ Identify interfaces needed                               │
    │  □ Estimate effort                                          │
    │  □ Get team buy-in                                          │
    └─────────────────────────────────────────────────────────────┘

    PHASE 3: PREPARE (1-2 days)
    ┌─────────────────────────────────────────────────────────────┐
    │  □ Write tests for current behavior                         │
    │  □ Create domain directory structure                        │
    │  □ Define interface headers                                 │
    │  □ Set up build rules                                       │
    └─────────────────────────────────────────────────────────────┘

    PHASE 4: EXTRACT (iterative)
    ┌─────────────────────────────────────────────────────────────┐
    │  For each piece of domain logic:                            │
    │  □ Create domain function                                   │
    │  □ Write domain tests                                       │
    │  □ Change caller to delegate                                │
    │  □ Verify behavior                                          │
    │  □ Commit                                                   │
    └─────────────────────────────────────────────────────────────┘

    PHASE 5: VERIFY (ongoing)
    ┌─────────────────────────────────────────────────────────────┐
    │  □ Run boundary checks                                      │
    │  □ All tests pass                                           │
    │  □ No performance regression                                │
    │  □ Documentation updated                                    │
    └─────────────────────────────────────────────────────────────┘
```

### Before/After Example

```
BEFORE:
┌────────────────────────────────────────────────────────────────┐
│  main.c                                                        │
│  ├── Parses CLI args                                          │
│  ├── Opens database                                           │
│  ├── Validates business rules (!)                             │
│  ├── Calculates results (!)                                   │
│  ├── Writes to database                                       │
│  └── Formats output                                           │
│                                                                │
│  Everything in one place. 500 lines. Untestable.              │
└────────────────────────────────────────────────────────────────┘

AFTER:
┌────────────────────────────────────────────────────────────────┐
│  main.c (20 lines)                                             │
│  └── Wires up layers, calls cli_run()                          │
│                                                                │
│  presentation/cli.c (50 lines)                                 │
│  ├── Parses CLI args                                           │
│  ├── Calls domain functions                                    │
│  └── Formats output                                            │
│                                                                │
│  domain/calculator.c (100 lines)                               │
│  ├── Validates business rules                                  │
│  ├── Calculates results                                        │
│  └── Uses storage interface                                    │
│                                                                │
│  data/sqlite_storage.c (80 lines)                              │
│  ├── Implements storage_ops                                    │
│  └── SQL operations                                            │
│                                                                │
│  Each layer testable independently. Clear boundaries.          │
└────────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **识别耦合**：使用脚本分析头文件依赖、函数调用方向、全局状态
- **区分策略与机制**：问"应该发生什么？"（策略）vs "如何发生？"（机制）
- **识别不稳定依赖**：高变化的代码应该依赖低变化的代码，反之不可
- **安全提取**：使用 Strangler 模式——识别、复制、委托、移除
- **增量重构**：每次一个逻辑变更，频繁测试，提交稳定点
- **验证正确性**：编译时边界检查、行为测试、依赖验证脚本

