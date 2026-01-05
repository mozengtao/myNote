# FRR lib/ Infrastructure: REUSE | Applying lib/ Outside FRR

## Overview Diagram

```
+-----------------------------------------------------------------------------+
|                                                                             |
|              REUSING FRR lib/ IN EXTERNAL PROJECTS                          |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  DECISION TREE: WHEN TO REUSE                                               |
|  ============================                                               |
|                                                                             |
|                     Need event-driven                                       |
|                     network daemon?                                         |
|                           |                                                 |
|              +------------+------------+                                    |
|              |                         |                                    |
|              v                         v                                    |
|             YES                        NO                                   |
|              |                         |                                    |
|              v                         v                                    |
|    Need routing protocol        Consider other                              |
|    integration (Zebra)?         libraries                                   |
|              |                  (libevent, etc.)                            |
|    +---------+---------+                                                    |
|    |                   |                                                    |
|    v                   v                                                    |
|   YES                  NO                                                   |
|    |                   |                                                    |
|    v                   v                                                    |
|  +---------------+   +---------------+                                      |
|  | Use full FRR  |   | Use libfrr    |                                      |
|  | daemon infra  |   | selectively   |                                      |
|  +---------------+   +---------------+                                      |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  OPTION 1: REUSE libfrr AS-IS                                               |
|  ============================                                               |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  When to use:                                                       |    |
|  |  - Building a new FRR-style daemon                                  |    |
|  |  - Need routing table integration                                   |    |
|  |  - Need VRF support                                                 |    |
|  |  - Need CLI infrastructure                                          |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Project Structure:                                                 |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  my-daemon/                                               |      |    |
|  |  |    ├── configure.ac    (use FRR_* autoconf macros)        |      |    |
|  |  |    ├── Makefile.am     (link against libfrr)              |      |    |
|  |  |    ├── my_daemon.c     (use FRR_DAEMON_INFO)              |      |    |
|  |  |    ├── my_daemon.h                                        |      |    |
|  |  |    └── ...                                                |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Minimal main.c:                                                    |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  #include <zebra.h>                                       |      |    |
|  |  |  #include <libfrr.h>                                      |      |    |
|  |  |                                                           |      |    |
|  |  |  FRR_DAEMON_INFO(mydaemon, MYDAEMON,                      |      |    |
|  |  |      .vty_port = 2699,                                    |      |    |
|  |  |      .proghelp = "My custom daemon",                      |      |    |
|  |  |  );                                                       |      |    |
|  |  |                                                           |      |    |
|  |  |  int main(int argc, char **argv) {                        |      |    |
|  |  |      frr_preinit(&mydaemon_di, argc, argv);               |      |    |
|  |  |      struct event_loop *master = frr_init();              |      |    |
|  |  |                                                           |      |    |
|  |  |      // Initialize your module                            |      |    |
|  |  |      my_init(master);                                     |      |    |
|  |  |                                                           |      |    |
|  |  |      frr_run(master);                                     |      |    |
|  |  |      return 0;                                            |      |    |
|  |  |  }                                                        |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  OPTION 2: COPY CONCEPTS, REWRITE CODE                                      |
|  =====================================                                      |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  When to use:                                                       |    |
|  |  - Different licensing requirements                                 |    |
|  |  - No routing integration needed                                    |    |
|  |  - Want minimal dependencies                                        |    |
|  |  - Different platform requirements                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Concepts worth copying:                                            |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  1. EVENT LOOP DESIGN                                     |      |    |
|  |  |     - Single-threaded cooperative model                   |      |    |
|  |  |     - Timer heap (min-heap)                               |      |    |
|  |  |     - Unified fd + timer + immediate event handling       |      |    |
|  |  |                                                           |      |    |
|  |  |  2. MEMORY TYPE SYSTEM                                    |      |    |
|  |  |     - Typed allocators with compile-time checking         |      |    |
|  |  |     - Allocation counting per type                        |      |    |
|  |  |     - Automatic leak detection                            |      |    |
|  |  |                                                           |      |    |
|  |  |  3. TYPESAFE CONTAINERS                                   |      |    |
|  |  |     - Macro-based type-safe data structures               |      |    |
|  |  |     - Embedded list nodes (intrusive)                     |      |    |
|  |  |     - frr_each iteration pattern                          |      |    |
|  |  |                                                           |      |    |
|  |  |  4. STREAM BUFFER                                         |      |    |
|  |  |     - Network byte order abstraction                      |      |    |
|  |  |     - Separate read/write pointers                        |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  EXAMPLE: CUSTOM CONTROL-PLANE DAEMON                                       |
|  ====================================                                       |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Scenario: Building a custom SDN controller daemon                  |    |
|  |            that needs to interact with FRR routers                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Architecture:                                                      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |   +-------------------+                                   |      |    |
|  |  |   | SDN Controller    |                                   |      |    |
|  |  |   | (your daemon)     |                                   |      |    |
|  |  |   +--------+----------+                                   |      |    |
|  |  |            |                                              |      |    |
|  |  |            | REST/gRPC API                                |      |    |
|  |  |            v                                              |      |    |
|  |  |   +-------------------+     +-------------------+         |      |    |
|  |  |   |  FRR Router 1     |     |  FRR Router 2     |         |      |    |
|  |  |   |  (bgpd, zebra)    |     |  (bgpd, zebra)    |         |      |    |
|  |  |   +-------------------+     +-------------------+         |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Implementation:                                                    |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  // sdn_controller.c                                      |      |    |
|  |  |                                                           |      |    |
|  |  |  #include <zebra.h>                                       |      |    |
|  |  |  #include <libfrr.h>                                      |      |    |
|  |  |  #include <frrevent.h>                                    |      |    |
|  |  |  #include <memory.h>                                      |      |    |
|  |  |  #include <prefix.h>                                      |      |    |
|  |  |                                                           |      |    |
|  |  |  DEFINE_MGROUP(SDN, "SDN Controller");                    |      |    |
|  |  |  DEFINE_MTYPE(SDN, SDN_ROUTER, "Router connection");      |      |    |
|  |  |  DEFINE_MTYPE(SDN, SDN_ROUTE, "Route entry");             |      |    |
|  |  |                                                           |      |    |
|  |  |  PREDECL_HASH(router_table);                              |      |    |
|  |  |  struct router {                                          |      |    |
|  |  |      struct router_table_item item;                       |      |    |
|  |  |      char *name;                                          |      |    |
|  |  |      int fd;                                              |      |    |
|  |  |      struct event *read_event;                            |      |    |
|  |  |  };                                                       |      |    |
|  |  |  DECLARE_HASH(router_table, struct router, item,          |      |    |
|  |  |               router_cmp, router_hash);                   |      |    |
|  |  |                                                           |      |    |
|  |  |  void router_read(struct event *e) {                      |      |    |
|  |  |      struct router *r = EVENT_ARG(e);                     |      |    |
|  |  |      // Handle router communication                       |      |    |
|  |  |      event_add_read(master, router_read, r, r->fd,        |      |    |
|  |  |                     &r->read_event);                      |      |    |
|  |  |  }                                                        |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  CHECKLIST: BEFORE USING libfrr                                             |
|  ==============================                                             |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MUST UNDERSTAND:                                                   |    |
|  |  ================                                                   |    |
|  |                                                                     |    |
|  |  [ ] Event loop is single-threaded                                  |    |
|  |      - Don't block in callbacks                                     |    |
|  |      - Don't use multiple event loops carelessly                    |    |
|  |                                                                     |    |
|  |  [ ] Memory types are required                                      |    |
|  |      - Every allocation needs an MTYPE                              |    |
|  |      - Mismatched types cause statistics errors                     |    |
|  |                                                                     |    |
|  |  [ ] XFREE sets pointer to NULL                                     |    |
|  |      - Don't check pointer after XFREE                              |    |
|  |      - Don't double-free (already NULL)                             |    |
|  |                                                                     |    |
|  |  [ ] Events must be cancelled before freeing associated data        |    |
|  |      - event_cancel(&timer) before XFREE(data)                      |    |
|  |                                                                     |    |
|  |  [ ] Logging initialization happens in frr_init()                   |    |
|  |      - zlog_* works after frr_init() or zlog_aux_init()             |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MUST NEVER BYPASS:                                                 |    |
|  |  ==================                                                 |    |
|  |                                                                     |    |
|  |  [X] Direct malloc/free (use XMALLOC/XFREE)                         |    |
|  |                                                                     |    |
|  |  [X] Blocking calls in event callbacks                              |    |
|  |      - No sleep(), no synchronous I/O                               |    |
|  |      - Use async patterns or reschedule                             |    |
|  |                                                                     |    |
|  |  [X] Modifying daemon internals                                     |    |
|  |      - Don't patch FRR source for your needs                        |    |
|  |      - Use hooks and public APIs                                    |    |
|  |                                                                     |    |
|  |  [X] Ignoring VRF context                                           |    |
|  |      - Routes belong to VRFs                                        |    |
|  |      - Sockets need VRF awareness                                   |    |
|  |                                                                     |    |
|  |  [X] Assuming synchronous route installation                        |    |
|  |      - Zebra installation is asynchronous                           |    |
|  |      - Wait for confirmation before proceeding                      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  LINKING AGAINST libfrr                                                     |
|  ======================                                                     |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Option A: pkg-config (recommended)                                 |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  # In your Makefile                                       |      |    |
|  |  |  CFLAGS += $(shell pkg-config --cflags frr)               |      |    |
|  |  |  LDFLAGS += $(shell pkg-config --libs frr)                |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  Option B: CMake                                                    |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  find_package(PkgConfig REQUIRED)                         |      |    |
|  |  |  pkg_check_modules(FRR REQUIRED frr)                      |      |    |
|  |  |  target_include_directories(myapp ${FRR_INCLUDE_DIRS})    |      |    |
|  |  |  target_link_libraries(myapp ${FRR_LIBRARIES})            |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  Option C: Direct linking                                           |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  # Typical paths (may vary)                               |      |    |
|  |  |  CFLAGS += -I/usr/include/frr                             |      |    |
|  |  |  LDFLAGS += -lfrr -lyang                                  |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细解释

### 何时应该重用 libfrr

**适合重用的场景：**

1. **构建路由相关守护进程**
   - 需要与 Zebra 集成
   - 需要处理路由表
   - 需要 VRF 支持

2. **构建网络控制平面应用**
   - SDN 控制器
   - 网络编排器
   - 路由策略引擎

3. **需要 FRR 生态系统的功能**
   - CLI 基础设施
   - YANG 数据模型
   - 与现有 FRR 守护进程集成

**不适合重用的场景：**

1. **简单的网络工具**
   - 一次性脚本
   - 简单的网络探测工具

2. **不同的许可证要求**
   - FRR 使用 GPL/LGPL
   - 商业闭源产品可能有问题

3. **极简依赖需求**
   - libfrr 有一些依赖（libyang 等）
   - 嵌入式系统可能不适合

### 选项 1：完整使用 libfrr

这是构建 FRR 风格守护进程的标准方式。

**最小守护进程框架：**

```c
#include <zebra.h>
#include <libfrr.h>
#include <frrevent.h>
#include <memory.h>

// 声明内存类型
DEFINE_MGROUP(MYAPP, "My Application");
DEFINE_MTYPE(MYAPP, MYAPP_DATA, "Application data");

// 定义守护进程信息
FRR_DAEMON_INFO(myappd, MYAPP,
    .vty_port = 2700,
    .proghelp = "My custom network daemon",
);

static struct event_loop *master;

// 初始化你的模块
void myapp_init(struct event_loop *m)
{
    // 注册事件
    // 初始化数据结构
}

int main(int argc, char **argv)
{
    // 阶段 1：预初始化
    frr_preinit(&myappd_di, argc, argv);
    
    // 阶段 2：初始化
    master = frr_init();
    
    // 阶段 3：模块初始化
    myapp_init(master);
    
    // 阶段 4：运行
    frr_run(master);
    
    return 0;
}
```

**构建配置（Makefile）：**

```makefile
CFLAGS += $(shell pkg-config --cflags frr)
LDFLAGS += $(shell pkg-config --libs frr)

myappd: myappd.c
    $(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

### 选项 2：复制概念，重写代码

如果不能直接使用 libfrr，可以借鉴其设计理念：

**1. 事件循环设计**

```c
// 简化版事件循环
struct simple_event_loop {
    struct pollfd *fds;
    int nfds;
    // ... 定时器堆
};

int event_loop_run(struct simple_event_loop *loop)
{
    while (!loop->stop) {
        int timeout = calculate_next_timer();
        int n = poll(loop->fds, loop->nfds, timeout);
        
        // 处理就绪的文件描述符
        // 处理到期的定时器
    }
}
```

**2. 类型化内存分配**

```c
// 简化版内存类型系统
struct mem_type {
    const char *name;
    size_t count;
    size_t total_bytes;
};

#define DEFINE_MEMTYPE(name, desc) \
    struct mem_type memtype_##name = { .name = desc }

#define MY_MALLOC(mt, size) my_malloc(&memtype_##mt, size)
#define MY_FREE(mt, ptr) my_free(&memtype_##mt, ptr)

void *my_malloc(struct mem_type *mt, size_t size)
{
    void *p = malloc(size);
    if (p) {
        mt->count++;
        mt->total_bytes += size;
    }
    return p;
}
```

**3. 类型安全容器**

```c
// 简化版类型安全链表
#define DECLARE_LIST(name, type, field) \
    struct name##_head { type *first; }; \
    static inline void name##_add(struct name##_head *h, type *item) { \
        item->field.next = h->first; \
        h->first = item; \
    }
```

### 示例：自定义控制平面守护进程

**场景**：构建一个 SDN 控制器，需要管理多个 FRR 路由器。

```c
// sdn_controller.c

#include <zebra.h>
#include <libfrr.h>
#include <frrevent.h>
#include <memory.h>
#include <prefix.h>
#include <json.h>

// 内存类型定义
DEFINE_MGROUP(SDN, "SDN Controller");
DEFINE_MTYPE(SDN, SDN_ROUTER, "Router connection");
DEFINE_MTYPE(SDN, SDN_POLICY, "Routing policy");

// 路由器数据结构
PREDECL_HASH(router_db);
struct sdn_router {
    struct router_db_item hitem;
    char *name;
    struct in_addr addr;
    int control_fd;
    struct event *read_event;
    struct event *keepalive;
};
DECLARE_HASH(router_db, struct sdn_router, hitem, router_cmp, router_hash);

static struct router_db_head routers;
static struct event_loop *master;

// 路由器通信回调
void router_read_cb(struct event *e)
{
    struct sdn_router *r = EVENT_ARG(e);
    char buf[4096];
    
    ssize_t n = read(r->control_fd, buf, sizeof(buf));
    if (n <= 0) {
        zlog_warn("Router %s disconnected", r->name);
        router_cleanup(r);
        return;
    }
    
    // 处理消息
    process_router_message(r, buf, n);
    
    // 重新注册读事件
    event_add_read(master, router_read_cb, r, r->control_fd, &r->read_event);
}

// 清理路由器连接
void router_cleanup(struct sdn_router *r)
{
    event_cancel(&r->read_event);
    event_cancel(&r->keepalive);
    close(r->control_fd);
    router_db_del(&routers, r);
    XFREE(MTYPE_SDN_ROUTER, r->name);
    XFREE(MTYPE_SDN_ROUTER, r);
}

// 守护进程信息
FRR_DAEMON_INFO(sdnctl, SDN,
    .vty_port = 2700,
    .proghelp = "SDN Controller Daemon",
);

int main(int argc, char **argv)
{
    frr_preinit(&sdnctl_di, argc, argv);
    master = frr_init();
    
    router_db_init(&routers);
    
    // 初始化 REST API、路由器发现等
    sdn_init(master);
    
    frr_run(master);
    return 0;
}
```

### 使用前检查清单

**必须理解的内容：**

| 项目 | 说明 |
|------|------|
| 单线程事件循环 | 回调中不能阻塞 |
| 内存类型必需 | 每个分配需要 MTYPE |
| XFREE 置空指针 | 释放后指针为 NULL |
| 事件取消顺序 | 先取消事件再释放数据 |
| 日志初始化 | zlog_* 在 frr_init() 后可用 |

**绝不能做的事情：**

| 禁止项 | 后果 |
|--------|------|
| 使用 malloc/free | 泄漏检测失效 |
| 回调中阻塞 | 整个守护进程卡住 |
| 修改 FRR 内部 | 升级时破坏 |
| 忽略 VRF 上下文 | 路由安装错误 |
| 假设同步安装 | 状态不一致 |

### 链接 libfrr

**推荐方式：使用 pkg-config**

```makefile
# Makefile
CFLAGS += $(shell pkg-config --cflags frr)
LDFLAGS += $(shell pkg-config --libs frr)

myapp: myapp.c
    $(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

**CMake：**

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(FRR REQUIRED frr)

add_executable(myapp myapp.c)
target_include_directories(myapp PRIVATE ${FRR_INCLUDE_DIRS})
target_link_libraries(myapp ${FRR_LIBRARIES})
```

### 总结

重用 libfrr 的决策取决于：

1. **是否需要路由集成**：需要 → 用 libfrr
2. **许可证是否兼容**：不兼容 → 复制概念
3. **依赖是否可接受**：不可接受 → 精简重写
4. **团队是否熟悉 FRR**：不熟悉 → 先学习

无论选择哪种方式，FRR lib/ 的设计理念都值得学习：
- 事件驱动架构
- 类型化内存管理
- 失败优先的断言
- 可调试性优先
