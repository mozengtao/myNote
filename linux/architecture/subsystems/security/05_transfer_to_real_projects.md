# TRANSFER｜应用到实际项目

## 1. 应用程序中基于钩子的安全

```
HOOK-BASED SECURITY IN APPLICATIONS
+=============================================================================+
|                                                                              |
|  THE LSM PATTERN IN USER-SPACE                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  LSM approach:                                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Core logic ──► Security hook ──► Policy decision           │ │    │ |
|  │  │  │       │                │                  │                 │ │    │ |
|  │  │  │       │                ▼                  ▼                 │ │    │ |
|  │  │  │       │         if (denied)          allow/deny             │ │    │ |
|  │  │  │       │             return error                            │ │    │ |
|  │  │  │       │                                                     │ │    │ |
|  │  │  │       └──────────► proceed with operation                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Key insight: SEPARATION of enforcement point from policy        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  EXAMPLE 1: WEB APPLICATION AUTHORIZATION                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Define hook interface (like LSM's union security_list_opts)  │    │ |
|  │  │  typedef struct {                                                │    │ |
|  │  │      int (*can_read)(User *user, Resource *resource);            │    │ |
|  │  │      int (*can_write)(User *user, Resource *resource);           │    │ |
|  │  │      int (*can_delete)(User *user, Resource *resource);          │    │ |
|  │  │      int (*can_admin)(User *user);                               │    │ |
|  │  │  } SecurityHooks;                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Multiple policy implementations                              │    │ |
|  │  │  static SecurityHooks rbac_hooks = {                             │    │ |
|  │  │      .can_read = rbac_can_read,                                  │    │ |
|  │  │      .can_write = rbac_can_write,                                │    │ |
|  │  │      .can_delete = rbac_can_delete,                              │    │ |
|  │  │      .can_admin = rbac_can_admin,                                │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static SecurityHooks abac_hooks = {                             │    │ |
|  │  │      .can_read = abac_can_read,                                  │    │ |
|  │  │      // ... attribute-based access control                       │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Core logic calls hooks (like kernel calls security_*())      │    │ |
|  │  │  int handle_read_request(User *user, Resource *resource) {       │    │ |
|  │  │      // Hook call - policy decides                               │    │ |
|  │  │      if (!current_hooks->can_read(user, resource))               │    │ |
|  │  │          return HTTP_403_FORBIDDEN;                              │    │ |
|  │  │                                                                  │    │ |
|  │  │      // If allowed, proceed                                      │    │ |
|  │  │      return do_read(resource);                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  EXAMPLE 2: DATABASE QUERY FILTER                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like SELinux's security context, attach policy to data       │    │ |
|  │  │  struct RowSecurityContext {                                     │    │ |
|  │  │      int owner_id;                                               │    │ |
|  │  │      int classification;  // PUBLIC, INTERNAL, CONFIDENTIAL      │    │ |
|  │  │      int department_id;                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Security hooks for database operations                       │    │ |
|  │  │  typedef struct {                                                │    │ |
|  │  │      // Like security_inode_permission()                         │    │ |
|  │  │      bool (*can_select_row)(User *u, RowSecurityContext *ctx);   │    │ |
|  │  │      bool (*can_update_row)(User *u, RowSecurityContext *ctx);   │    │ |
|  │  │      bool (*can_delete_row)(User *u, RowSecurityContext *ctx);   │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Row filter - automatically applied to queries            │    │ |
|  │  │      char* (*get_row_filter)(User *u, Table *t);                 │    │ |
|  │  │  } DBSecurityHooks;                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Implementation: row-level security                           │    │ |
|  │  │  char* rls_get_row_filter(User *u, Table *t) {                   │    │ |
|  │  │      // Automatically filter rows user can see                   │    │ |
|  │  │      return format("department_id = %d OR classification = PUBLIC",│   │ |
|  │  │                    u->department_id);                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Query execution with security                                │    │ |
|  │  │  Result* execute_query(User *user, Query *query) {               │    │ |
|  │  │      // Add security filter (like LSM mediation)                 │    │ |
|  │  │      char *filter = hooks->get_row_filter(user, query->table);   │    │ |
|  │  │      query = add_where_clause(query, filter);                    │    │ |
|  │  │                                                                  │    │ |
|  │  │      return db_execute(query);                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  EXAMPLE 3: MICROSERVICE AUTHORIZATION                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Middleware as "hook" in request pipeline                     │    │ |
|  │  │  //                                                              │    │ |
|  │  │  // Request ──► Auth middleware ──► Rate limit ──► Handler       │    │ |
|  │  │  //                    │                │                        │    │ |
|  │  │  //                    └── deny ────────┴── deny                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like LSM's hlist of hooks, multiple middlewares              │    │ |
|  │  │  type Middleware func(ctx *Context) error                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  var securityMiddlewares = []Middleware{                         │    │ |
|  │  │      AuthenticationMiddleware,  // Verify identity               │    │ |
|  │  │      AuthorizationMiddleware,   // Check permissions             │    │ |
|  │  │      RateLimitMiddleware,       // Prevent abuse                 │    │ |
|  │  │      AuditMiddleware,           // Log access                    │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like call_int_hook - run all, stop on error                  │    │ |
|  │  │  func runSecurityChain(ctx *Context) error {                     │    │ |
|  │  │      for _, mw := range securityMiddlewares {                    │    │ |
|  │  │          if err := mw(ctx); err != nil {                         │    │ |
|  │  │              return err  // Any denial stops the chain           │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │      return nil                                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Handler with security                                        │    │ |
|  │  │  func handleRequest(ctx *Context) {                              │    │ |
|  │  │      if err := runSecurityChain(ctx); err != nil {               │    │ |
|  │  │          ctx.Error(403, err.Error())                             │    │ |
|  │  │          return                                                  │    │ |
|  │  │      }                                                           │    │ |
|  │  │      // Proceed with business logic                              │    │ |
|  │  │      doBusinessLogic(ctx)                                        │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**应用程序中基于钩子的安全**：

**LSM 模式在用户空间**：
- 核心逻辑 → 安全钩子 → 策略决策
- 如果拒绝则返回错误，否则继续操作
- **关键洞见**：强制点与策略分离

**示例 1：Web 应用授权**
- 定义钩子接口（can_read、can_write、can_delete）
- 多个策略实现（RBAC、ABAC）
- 核心逻辑调用钩子，策略决定

**示例 2：数据库查询过滤**
- 像 SELinux 安全上下文，将策略附加到数据
- 行级安全钩子（can_select_row、get_row_filter）
- 自动将安全过滤器添加到查询

**示例 3：微服务授权**
- 中间件作为请求管道中的"钩子"
- 多个中间件：认证、授权、速率限制、审计
- 像 call_int_hook：运行所有，出错停止

---

## 2. 避免过度工程

```
AVOIDING OVER-ENGINEERING
+=============================================================================+
|                                                                              |
|  WHEN TO USE LSM-STYLE ARCHITECTURE                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  USE HOOK ARCHITECTURE WHEN:                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ Security is a cross-cutting concern                      │ │    │ |
|  │  │  │    (needed in many places, not just one)                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ Multiple policy implementations needed                   │ │    │ |
|  │  │  │    (RBAC for some customers, ABAC for others)               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ Policy changes independently of code                     │ │    │ |
|  │  │  │    (update rules without redeploying)                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ Need to audit all access                                 │ │    │ |
|  │  │  │    (centralized logging point)                              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ Third-party plugins need sandboxing                      │ │    │ |
|  │  │  │    (don't trust plugin code)                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHEN NOT TO USE                                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  AVOID HOOK ARCHITECTURE WHEN:                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Single, simple access control                            │ │    │ |
|  │  │  │    Just use if statements!                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    // BAD: over-engineered                                  │ │    │ |
|  │  │  │    hooks->can_access(user, resource);                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    // GOOD: simple and clear                                │ │    │ |
|  │  │  │    if (user->is_admin || user->id == resource->owner_id)    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Performance-critical inner loops                         │ │    │ |
|  │  │  │    Indirect calls have overhead                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Policy never changes                                     │ │    │ |
|  │  │  │    Why add indirection?                                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Only one module needs security                           │ │    │ |
|  │  │  │    Not cross-cutting                                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SIMPLER ALTERNATIVES                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  INSTEAD OF:                      USE:                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Full hook framework              Single authorize() function    │    │ |
|  │  │  ┌────────────────────┐          ┌────────────────────┐         │    │ |
|  │  │  │ hooks->can_read()  │          │ bool authorize(    │         │    │ |
|  │  │  │ hooks->can_write() │    →     │   User *u,         │         │    │ |
|  │  │  │ hooks->can_delete()│          │   Action a,        │         │    │ |
|  │  │  │ hooks->can_admin() │          │   Resource *r      │         │    │ |
|  │  │  └────────────────────┘          │ );                 │         │    │ |
|  │  │                                  └────────────────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Multiple hook types              Decorator pattern              │    │ |
|  │  │  ┌────────────────────┐          ┌────────────────────┐         │    │ |
|  │  │  │ pre_hook()         │          │ @require_auth      │         │    │ |
|  │  │  │ post_hook()        │    →     │ @require_role("a") │         │    │ |
|  │  │  │ audit_hook()       │          │ def handler():     │         │    │ |
|  │  │  └────────────────────┘          └────────────────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Per-operation hooks              Guard clause                   │    │ |
|  │  │  ┌────────────────────┐          ┌────────────────────┐         │    │ |
|  │  │  │ security_file_open │          │ if (!authorized()) │         │    │ |
|  │  │  │ security_file_read │    →     │     return ERROR;  │         │    │ |
|  │  │  │ security_file_write│          │ // proceed...      │         │    │ |
|  │  │  └────────────────────┘          └────────────────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  COMPLEXITY GUIDELINES                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Security Check Points    Recommended Approach                   │    │ |
|  │  │  ───────────────────────────────────────────────────────────────│    │ |
|  │  │  1-3                      Inline if statements                   │    │ |
|  │  │  4-10                     Single authorize() function            │    │ |
|  │  │  10-30                    Decorator/middleware pattern           │    │ |
|  │  │  30+                      Hook framework (like LSM)              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Rule: Start simple, add abstraction when you feel the pain      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**避免过度工程**：

**何时使用 LSM 风格架构**：
- ✓ 安全是横切关注点（多处需要）
- ✓ 需要多个策略实现
- ✓ 策略独立于代码变化
- ✓ 需要审计所有访问
- ✓ 第三方插件需要沙箱

**何时不使用**：
- ✗ 单一、简单的访问控制 - 直接用 if 语句！
- ✗ 性能关键内循环 - 间接调用有开销
- ✗ 策略从不变化 - 为什么添加间接性？
- ✗ 只有一个模块需要安全 - 不是横切的

**更简单的替代方案**：
- 完整钩子框架 → 单个 authorize() 函数
- 多种钩子类型 → 装饰器模式
- 每操作钩子 → Guard 子句

**复杂度指南**：
- 1-3 检查点：内联 if 语句
- 4-10 检查点：单个 authorize() 函数
- 10-30 检查点：装饰器/中间件模式
- 30+ 检查点：钩子框架（如 LSM）

**规则**：从简单开始，感到痛苦时再添加抽象

---

## 3. 常见陷阱

```
COMMON PITFALLS
+=============================================================================+
|                                                                              |
|  PITFALL 1: INCOMPLETE MEDIATION                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Not all paths go through security checks               │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Some paths bypass security                         │ │    │ |
|  │  │  │  class DocumentService {                                    │ │    │ |
|  │  │  │      Document get(id) {                                     │ │    │ |
|  │  │  │          authorize(currentUser, "read", id);  // ✓ checked  │ │    │ |
|  │  │  │          return db.get(id);                                 │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      List<Document> search(query) {                         │ │    │ |
|  │  │  │          return db.search(query);  // ✗ NO CHECK!           │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      void export(ids) {                                     │ │    │ |
|  │  │  │          // Bulk operation, forgot security                 │ │    │ |
|  │  │  │          for (id : ids)                                     │ │    │ |
|  │  │  │              output(db.get(id));  // ✗ NO CHECK!            │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Enforce at data access layer                          │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Like LSM, check at actual access point            │ │    │ |
|  │  │  │  class SecureDocumentRepository {                           │ │    │ |
|  │  │  │      Document get(id) {                                     │ │    │ |
|  │  │  │          doc = db.get(id);                                  │ │    │ |
|  │  │  │          if (!authorize(currentUser, "read", doc))          │ │    │ |
|  │  │  │              throw AccessDenied();                          │ │    │ |
|  │  │  │          return doc;                                        │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      // ALL queries go through this, ALL are filtered       │ │    │ |
|  │  │  │      List<Document> query(q) {                              │ │    │ |
|  │  │  │          q = addSecurityFilter(q, currentUser);             │ │    │ |
|  │  │  │          return db.query(q);                                │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PITFALL 2: TOCTOU (TIME-OF-CHECK TO TIME-OF-USE)                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: State changes between check and use                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Check and use are separate                         │ │    │ |
|  │  │  │  if (canAccessFile(path)) {      // Check                   │ │    │ |
|  │  │  │      // Window where path could change!                     │ │    │ |
|  │  │  │      // Attacker: symlink(path, "/etc/shadow")              │ │    │ |
|  │  │  │      data = readFile(path);      // Use different file!     │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Check at use, use handles instead of names           │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Open first, check on the actual object            │ │    │ |
|  │  │  │  fd = open(path);  // Resolve path to object                │ │    │ |
|  │  │  │  if (!canAccess(fd)) {  // Check on OBJECT, not path        │ │    │ |
|  │  │  │      close(fd);                                             │ │    │ |
|  │  │  │      return ERROR;                                          │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │  data = read(fd);  // Same object we checked                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // This is how the kernel does it!                         │ │    │ |
|  │  │  │  // security_file_open() checks the opened file,            │ │    │ |
|  │  │  │  // not the path string.                                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PITFALL 3: CONFUSED DEPUTY                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Service acts on behalf of user with service's perms    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  User ────► Service (admin) ────► Database                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Service runs with elevated privileges              │ │    │ |
|  │  │  │  void handleRequest(userId, documentId) {                   │ │    │ |
|  │  │  │      // Service can access ALL documents                    │ │    │ |
|  │  │  │      // Only checks if user provided valid document ID      │ │    │ |
|  │  │  │      if (documentExists(documentId))                        │ │    │ |
|  │  │  │          return getDocument(documentId);  // Any doc!       │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Propagate user context, check against USER's perms   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Like SELinux tracks current->sid                  │ │    │ |
|  │  │  │  void handleRequest(userContext, documentId) {              │ │    │ |
|  │  │  │      // Check against USER's permissions, not service's     │ │    │ |
|  │  │  │      if (!authorize(userContext, "read", documentId))       │ │    │ |
|  │  │  │          return FORBIDDEN;                                  │ │    │ |
|  │  │  │      return getDocument(documentId);                        │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Always ask: "On whose BEHALF am I acting?"              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PITFALL 4: FAIL-OPEN                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Default to allow when error occurs                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Exception = allow access                           │ │    │ |
|  │  │  │  bool authorize(user, action, resource) {                   │ │    │ |
|  │  │  │      try {                                                  │ │    │ |
|  │  │  │          return policyService.check(user, action, resource);│ │    │ |
|  │  │  │      } catch (Exception e) {                                │ │    │ |
|  │  │  │          log.warn("Policy service down");                   │ │    │ |
|  │  │  │          return true;  // ✗ DANGEROUS: fail open            │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Fail closed (like SELinux)                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Default deny                                      │ │    │ |
|  │  │  │  bool authorize(user, action, resource) {                   │ │    │ |
|  │  │  │      try {                                                  │ │    │ |
|  │  │  │          return policyService.check(user, action, resource);│ │    │ |
|  │  │  │      } catch (Exception e) {                                │ │    │ |
|  │  │  │          log.error("Policy service down - DENYING");        │ │    │ |
|  │  │  │          return false;  // ✓ SAFE: fail closed              │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Like SELinux: if policy can't be consulted, DENY        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SUMMARY: KEY LESSONS FROM LSM                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. COMPLETE MEDIATION                                                   │ |
|  │     Every access path must go through security checks                    │ |
|  │                                                                          │ |
|  │  2. CHECK AT USE                                                         │ |
|  │     Verify on the actual object, not the name/path                       │ |
|  │                                                                          │ |
|  │  3. PROPAGATE CONTEXT                                                    │ |
|  │     Track whose permissions to use (subject identity)                    │ |
|  │                                                                          │ |
|  │  4. FAIL CLOSED                                                          │ |
|  │     When in doubt, deny access                                           │ |
|  │                                                                          │ |
|  │  5. SEPARATE POLICY FROM MECHANISM                                       │ |
|  │     Let policy change without code changes                               │ |
|  │                                                                          │ |
|  │  6. AUDIT EVERYTHING                                                     │ |
|  │     Log all access decisions for forensics                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见陷阱**：

**陷阱 1：不完整的调解**
- 问题：不是所有路径都经过安全检查
- 解决方案：在数据访问层强制检查，像 LSM 在实际访问点检查

**陷阱 2：TOCTOU（检查时间到使用时间）**
- 问题：检查和使用之间状态改变
- 解决方案：在使用时检查，使用句柄而非名称
- 这是内核的做法：security_file_open() 检查打开的文件，而非路径字符串

**陷阱 3：困惑的代理**
- 问题：服务代表用户行动，但使用服务的权限
- 解决方案：传播用户上下文，根据用户的权限检查
- 始终问："我代表谁行动？"

**陷阱 4：开放式失败**
- 问题：错误发生时默认允许
- 解决方案：关闭式失败（像 SELinux）- 如果无法咨询策略，拒绝

**从 LSM 学到的关键教训**：
1. **完整调解**：每条访问路径必须经过安全检查
2. **使用时检查**：在实际对象上验证，而非名称/路径
3. **传播上下文**：跟踪使用谁的权限
4. **关闭式失败**：有疑问时拒绝访问
5. **策略与机制分离**：让策略无需代码更改而改变
6. **审计一切**：记录所有访问决策
