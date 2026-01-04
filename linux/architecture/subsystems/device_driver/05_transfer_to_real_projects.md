# TRANSFER｜应用到实际项目

## 1. 用户空间插件系统

```
PLUGIN SYSTEMS IN USER-SPACE
+=============================================================================+
|                                                                              |
|  KERNEL PATTERN: REGISTRATION-BASED DRIVER MODEL                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Linux driver model:                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Framework defines interface (struct device_driver)           │    │ |
|  │  │  2. Plugins implement interface (probe, remove, ...)             │    │ |
|  │  │  3. Plugins register with framework (driver_register)            │    │ |
|  │  │  4. Framework matches plugins to work items (bus matching)       │    │ |
|  │  │  5. Framework calls plugin callbacks (probe, remove)             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE TRANSFER: PLUGIN FRAMEWORK                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  // Plugin interface (like struct device_driver)                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct plugin {                                                 │    │ |
|  │  │      const char *name;                                           │    │ |
|  │  │      const char *version;                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Matching criteria (like id_table)                        │    │ |
|  │  │      const char **supported_types;                               │    │ |
|  │  │      int (*can_handle)(struct work_item *item);                  │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Lifecycle (like probe/remove)                            │    │ |
|  │  │      int (*init)(struct plugin_context *ctx);                    │    │ |
|  │  │      void (*cleanup)(struct plugin_context *ctx);                │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Work handling                                            │    │ |
|  │  │      int (*process)(struct work_item *item, void *priv);         │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Private data (like driver_data)                          │    │ |
|  │  │      void *priv;                                                 │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  // Plugin registry (like bus driver list)                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct plugin_registry {                                        │    │ |
|  │  │      struct plugin **plugins;                                    │    │ |
|  │  │      int count;                                                  │    │ |
|  │  │      int capacity;                                               │    │ |
|  │  │      pthread_mutex_t lock;                                       │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct plugin_registry g_registry;                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Registration (like driver_register)                          │    │ |
|  │  │  int plugin_register(struct plugin *p) {                         │    │ |
|  │  │      pthread_mutex_lock(&g_registry.lock);                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Check for duplicates                                     │    │ |
|  │  │      for (int i = 0; i < g_registry.count; i++) {                │    │ |
|  │  │          if (strcmp(g_registry.plugins[i]->name, p->name) == 0)  │    │ |
|  │  │              return -EEXIST;                                     │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Add to registry                                          │    │ |
|  │  │      g_registry.plugins[g_registry.count++] = p;                 │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Initialize plugin                                        │    │ |
|  │  │      if (p->init)                                                │    │ |
|  │  │          p->init(&ctx);                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │      pthread_mutex_unlock(&g_registry.lock);                     │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void plugin_unregister(struct plugin *p) {                      │    │ |
|  │  │      if (p->cleanup)                                             │    │ |
|  │  │          p->cleanup(&ctx);                                       │    │ |
|  │  │      // Remove from registry                                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  // Matching and dispatch (like bus matching)                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct plugin *find_plugin(struct work_item *item) {            │    │ |
|  │  │      for (int i = 0; i < g_registry.count; i++) {                │    │ |
|  │  │          struct plugin *p = g_registry.plugins[i];               │    │ |
|  │  │                                                                  │    │ |
|  │  │          // Try can_handle callback first                        │    │ |
|  │  │          if (p->can_handle && p->can_handle(item))               │    │ |
|  │  │              return p;                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │          // Fall back to type matching                           │    │ |
|  │  │          for (const char **t = p->supported_types; *t; t++) {    │    │ |
|  │  │              if (strcmp(*t, item->type) == 0)                    │    │ |
|  │  │                  return p;                                       │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │      return NULL;  // No matching plugin                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  int dispatch_work(struct work_item *item) {                     │    │ |
|  │  │      struct plugin *p = find_plugin(item);                       │    │ |
|  │  │      if (!p)                                                     │    │ |
|  │  │          return -ENOENT;  // No handler                          │    │ |
|  │  │      return p->process(item, p->priv);                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DYNAMIC LOADING (LIKE LOADABLE MODULES)                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Plugin shared object exports this                            │    │ |
|  │  │  struct plugin *plugin_get_interface(void);                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Framework loads plugin dynamically                           │    │ |
|  │  │  int load_plugin(const char *path) {                             │    │ |
|  │  │      void *handle = dlopen(path, RTLD_NOW);                      │    │ |
|  │  │      if (!handle)                                                │    │ |
|  │  │          return -ENOENT;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Like module_init                                         │    │ |
|  │  │      plugin_get_fn *get_interface =                              │    │ |
|  │  │          dlsym(handle, "plugin_get_interface");                  │    │ |
|  │  │      if (!get_interface) {                                       │    │ |
|  │  │          dlclose(handle);                                        │    │ |
|  │  │          return -EINVAL;                                         │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct plugin *p = get_interface();                         │    │ |
|  │  │      p->_dl_handle = handle;  // Store for unload                │    │ |
|  │  │                                                                  │    │ |
|  │  │      return plugin_register(p);                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void unload_plugin(struct plugin *p) {                          │    │ |
|  │  │      plugin_unregister(p);                                       │    │ |
|  │  │      dlclose(p->_dl_handle);                                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  EXAMPLE PLUGIN:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // json_plugin.c - handles JSON work items                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  static int json_init(struct plugin_context *ctx) {              │    │ |
|  │  │      // Initialize JSON parser                                   │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static int json_process(struct work_item *item, void *priv) {   │    │ |
|  │  │      // Parse and process JSON                                   │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static const char *supported[] = {"application/json", NULL};    │    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct plugin json_plugin = {                            │    │ |
|  │  │      .name = "json_handler",                                     │    │ |
|  │  │      .version = "1.0",                                           │    │ |
|  │  │      .supported_types = supported,                               │    │ |
|  │  │      .init = json_init,                                          │    │ |
|  │  │      .process = json_process,                                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct plugin *plugin_get_interface(void) {                     │    │ |
|  │  │      return &json_plugin;                                        │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**用户空间插件系统**：

**内核模式**：基于注册的驱动模型
1. 框架定义接口（struct device_driver）
2. 插件实现接口（probe, remove...）
3. 插件注册到框架（driver_register）
4. 框架匹配插件到工作项（总线匹配）
5. 框架调用插件回调

**迁移到用户空间**：

**插件接口**（类似 device_driver）：
- name/version：身份
- supported_types/can_handle：匹配条件
- init/cleanup：生命周期
- process：工作处理
- priv：私有数据

**插件注册表**（类似总线驱动列表）：
- 检查重复
- 添加到注册表
- 初始化插件

**匹配和分发**（类似总线匹配）：
- 遍历插件，调用 can_handle 或匹配类型
- 找到匹配后调用 process

**动态加载**（类似可加载模块）：
- `dlopen()` 加载 .so
- `dlsym()` 获取 plugin_get_interface
- 类似 module_init

---

## 2. 生命周期正确性

```
LIFECYCLE CORRECTNESS
+=============================================================================+
|                                                                              |
|  KERNEL PATTERN: SYMMETRIC LIFECYCLE                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  probe() and remove() are SYMMETRIC:                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  probe():                    remove():                           │    │ |
|  │  │  ───────                     ────────                            │    │ |
|  │  │  1. alloc private data       6. unregister subsystem             │    │ |
|  │  │  2. enable device            5. stop hardware                    │    │ |
|  │  │  3. map resources            4. free IRQ                         │    │ |
|  │  │  4. request IRQ              3. unmap resources                  │    │ |
|  │  │  5. init hardware            2. disable device                   │    │ |
|  │  │  6. register subsystem       1. free private data                │    │ |
|  │  │                                                                  │    │ |
|  │  │  RULE: remove() is EXACT REVERSE of probe()                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  APPLYING TO USER-SPACE: RAII PATTERN                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  C++ RAII:                                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  class Connection {                                              │    │ |
|  │  │  public:                                                         │    │ |
|  │  │      Connection(const char *addr) {                              │    │ |
|  │  │          // 1. Allocate buffer                                   │    │ |
|  │  │          m_buffer = new char[BUFFER_SIZE];                       │    │ |
|  │  │                                                                  │    │ |
|  │  │          // 2. Open socket                                       │    │ |
|  │  │          m_socket = socket(AF_INET, SOCK_STREAM, 0);             │    │ |
|  │  │          if (m_socket < 0) {                                     │    │ |
|  │  │              delete[] m_buffer;                                  │    │ |
|  │  │              throw std::runtime_error("socket failed");          │    │ |
|  │  │          }                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │          // 3. Connect                                           │    │ |
|  │  │          if (connect(m_socket, ...) < 0) {                       │    │ |
|  │  │              close(m_socket);                                    │    │ |
|  │  │              delete[] m_buffer;                                  │    │ |
|  │  │              throw std::runtime_error("connect failed");         │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      ~Connection() {                                             │    │ |
|  │  │          // REVERSE ORDER!                                       │    │ |
|  │  │          // 3. Disconnect (implicit in close)                    │    │ |
|  │  │          // 2. Close socket                                      │    │ |
|  │  │          close(m_socket);                                        │    │ |
|  │  │          // 1. Free buffer                                       │    │ |
|  │  │          delete[] m_buffer;                                      │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  private:                                                        │    │ |
|  │  │      int m_socket;                                               │    │ |
|  │  │      char *m_buffer;                                             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  C with GOTO cleanup (like kernel drivers):                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int init_connection(struct connection *conn, const char *addr) {│    │ |
|  │  │      int ret;                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 1. Allocate buffer                                       │    │ |
|  │  │      conn->buffer = malloc(BUFFER_SIZE);                         │    │ |
|  │  │      if (!conn->buffer)                                          │    │ |
|  │  │          return -ENOMEM;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 2. Open socket                                           │    │ |
|  │  │      conn->socket = socket(AF_INET, SOCK_STREAM, 0);             │    │ |
|  │  │      if (conn->socket < 0) {                                     │    │ |
|  │  │          ret = -errno;                                           │    │ |
|  │  │          goto err_free_buffer;                                   │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 3. Connect                                               │    │ |
|  │  │      ret = connect(conn->socket, ...);                           │    │ |
|  │  │      if (ret < 0) {                                              │    │ |
|  │  │          ret = -errno;                                           │    │ |
|  │  │          goto err_close_socket;                                  │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 4. Initialize state                                      │    │ |
|  │  │      conn->state = CONNECTED;                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  err_close_socket:                                               │    │ |
|  │  │      close(conn->socket);                                        │    │ |
|  │  │  err_free_buffer:                                                │    │ |
|  │  │      free(conn->buffer);                                         │    │ |
|  │  │      return ret;                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void cleanup_connection(struct connection *conn) {              │    │ |
|  │  │      // REVERSE ORDER of init!                                   │    │ |
|  │  │      // 4. (state automatically invalid)                         │    │ |
|  │  │      // 3. Disconnect (implicit)                                 │    │ |
|  │  │      close(conn->socket);  // 2                                  │    │ |
|  │  │      free(conn->buffer);   // 1                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MANAGED RESOURCES (LIKE DEVM_*)                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Context that tracks resources                                │    │ |
|  │  │  struct resource_ctx {                                           │    │ |
|  │  │      struct resource_entry *list;                                │    │ |
|  │  │      pthread_mutex_t lock;                                       │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct resource_entry {                                         │    │ |
|  │  │      void (*release)(void *);                                    │    │ |
|  │  │      void *data;                                                 │    │ |
|  │  │      struct resource_entry *next;                                │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like devm_add_action                                         │    │ |
|  │  │  void ctx_add_resource(struct resource_ctx *ctx,                 │    │ |
|  │  │                         void (*release)(void *), void *data) {   │    │ |
|  │  │      struct resource_entry *e = malloc(sizeof(*e));              │    │ |
|  │  │      e->release = release;                                       │    │ |
|  │  │      e->data = data;                                             │    │ |
|  │  │      e->next = ctx->list;                                        │    │ |
|  │  │      ctx->list = e;  // LIFO - last added, first released        │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like device release path                                     │    │ |
|  │  │  void ctx_release_all(struct resource_ctx *ctx) {                │    │ |
|  │  │      while (ctx->list) {                                         │    │ |
|  │  │          struct resource_entry *e = ctx->list;                   │    │ |
|  │  │          ctx->list = e->next;                                    │    │ |
|  │  │          e->release(e->data);  // Release in reverse order!      │    │ |
|  │  │          free(e);                                                │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Usage (like devm_kzalloc):                                   │    │ |
|  │  │  void *ctx_malloc(struct resource_ctx *ctx, size_t size) {       │    │ |
|  │  │      void *p = malloc(size);                                     │    │ |
|  │  │      if (p)                                                      │    │ |
|  │  │          ctx_add_resource(ctx, free, p);                         │    │ |
|  │  │      return p;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  int ctx_open_file(struct resource_ctx *ctx, const char *path) { │    │ |
|  │  │      int fd = open(path, O_RDONLY);                              │    │ |
|  │  │      if (fd >= 0)                                                │    │ |
|  │  │          ctx_add_resource(ctx, (void(*)(void*))close,            │    │ |
|  │  │                           (void*)(intptr_t)fd);                  │    │ |
|  │  │      return fd;                                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**生命周期正确性**：

**内核模式**：对称生命周期
- probe() 和 remove() 是对称的
- **规则**：remove() 是 probe() 的精确逆序

**C++ RAII**：
- 构造函数分配/初始化
- 析构函数以相反顺序清理
- 异常时也能正确清理

**C 的 goto 清理**（类似内核驱动）：
- 初始化失败时跳到对应标签
- 标签以逆序排列
- cleanup 函数也以逆序清理

**管理资源**（类似 devm_*）：
- 资源上下文跟踪所有资源
- `ctx_add_resource()` 添加到 LIFO 列表
- `ctx_release_all()` 以逆序释放
- 使用：`ctx_malloc()`、`ctx_open_file()` 自动跟踪

---

## 3. 常见驱动反模式

```
COMMON DRIVER ANTI-PATTERNS
+=============================================================================+
|                                                                              |
|  ANTI-PATTERN 1: ASYMMETRIC LIFECYCLE                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      a = alloc_a();                                              │    │ |
|  │  │      b = alloc_b();                                              │    │ |
|  │  │      c = alloc_c();                                              │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      free_a(a);   // WRONG ORDER!                                │    │ |
|  │  │      free_b(b);   // If c depends on b, crash!                   │    │ |
|  │  │      free_c(c);                                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      free_c(c);   // Last allocated, first freed                 │    │ |
|  │  │      free_b(b);                                                  │    │ |
|  │  │      free_a(a);                                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 2: MISSING ERROR HANDLING IN PROBE                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      priv = kzalloc(size, GFP_KERNEL);                           │    │ |
|  │  │      priv->regs = ioremap(addr, size);                           │    │ |
|  │  │      request_irq(irq, handler, 0, name, priv);                   │    │ |
|  │  │      // No error checks! If ioremap fails, request_irq crashes   │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      priv = kzalloc(size, GFP_KERNEL);                           │    │ |
|  │  │      if (!priv)                                                  │    │ |
|  │  │          return -ENOMEM;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      priv->regs = ioremap(addr, size);                           │    │ |
|  │  │      if (!priv->regs) {                                          │    │ |
|  │  │          ret = -ENOMEM;                                          │    │ |
|  │  │          goto err_free_priv;                                     │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      ret = request_irq(irq, handler, 0, name, priv);             │    │ |
|  │  │      if (ret)                                                    │    │ |
|  │  │          goto err_unmap;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  err_unmap:                                                      │    │ |
|  │  │      iounmap(priv->regs);                                        │    │ |
|  │  │  err_free_priv:                                                  │    │ |
|  │  │      kfree(priv);                                                │    │ |
|  │  │      return ret;                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 3: REMOVE CALLED DURING ACTIVE USE                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      // Thread might be using device right now!                  │    │ |
|  │  │      free_irq(irq, priv);    // IRQ handler might be running     │    │ |
|  │  │      iounmap(priv->regs);    // Other code might read regs!      │    │ |
|  │  │      kfree(priv);            // Use-after-free!                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      // 1. Stop accepting new work                               │    │ |
|  │  │      netif_stop_queue(netdev);                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 2. Unregister from subsystem (waits for users)           │    │ |
|  │  │      unregister_netdev(netdev);                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 3. Disable interrupts                                    │    │ |
|  │  │      disable_irq(irq);                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 4. Synchronize with IRQ handler                          │    │ |
|  │  │      synchronize_irq(irq);                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 5. Now safe to free                                      │    │ |
|  │  │      free_irq(irq, priv);                                        │    │ |
|  │  │      iounmap(priv->regs);                                        │    │ |
|  │  │      kfree(priv);                                                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 4: GLOBAL STATE FOR DEVICE DATA                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct my_device *g_device;  // Only one device!         │    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      g_device = kzalloc(...);                                    │    │ |
|  │  │      // What if user has TWO devices?                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  irqreturn_t irq_handler(int irq, void *data) {                  │    │ |
|  │  │      struct my_device *dev = g_device;  // Ignores data param!   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // No global! Each device has own instance                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe(struct pci_dev *pdev) {                               │    │ |
|  │  │      struct my_device *priv = kzalloc(...);                      │    │ |
|  │  │      pci_set_drvdata(pdev, priv);  // Store in device            │    │ |
|  │  │                                                                  │    │ |
|  │  │      request_irq(irq, irq_handler, 0, "name", priv);             │    │ |
|  │  │      //                                          ^^^^            │    │ |
|  │  │      //                              Pass as callback data       │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  irqreturn_t irq_handler(int irq, void *data) {                  │    │ |
|  │  │      struct my_device *dev = data;  // Use callback data!        │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 5: NOT USING DEVM_* WHEN APPROPRIATE                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      priv = kzalloc(size, GFP_KERNEL);                           │    │ |
|  │  │      priv->regs = ioremap(addr, size);                           │    │ |
|  │  │      ret = request_irq(irq, handler, 0, "name", priv);           │    │ |
|  │  │      if (ret)                                                    │    │ |
|  │  │          goto err_unmap;  // Easy to forget cleanup!             │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      free_irq(irq, priv);                                        │    │ |
|  │  │      iounmap(priv->regs);                                        │    │ |
|  │  │      kfree(priv);                                                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int probe() {                                                   │    │ |
|  │  │      // All devm_* are auto-cleaned on driver unbind/error       │    │ |
|  │  │      priv = devm_kzalloc(dev, size, GFP_KERNEL);                 │    │ |
|  │  │      priv->regs = devm_ioremap(dev, addr, size);                 │    │ |
|  │  │      ret = devm_request_irq(dev, irq, handler, 0, "name", priv); │    │ |
|  │  │      // No cleanup code needed! devm handles it                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void remove() {                                                 │    │ |
|  │  │      // Nothing to do! All resources auto-cleaned                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SUMMARY: KEY RULES                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. remove() must be EXACT REVERSE of probe()                            │ |
|  │  2. Every allocation must have corresponding free on error path          │ |
|  │  3. Stop activity before freeing resources                               │ |
|  │  4. No global state for per-device data                                  │ |
|  │  5. Use devm_* when possible                                             │ |
|  │  6. Test hotplug: insmod/rmmod repeatedly                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见驱动反模式**：

**反模式 1：不对称生命周期**
- 坏：释放顺序与分配顺序相同（应该相反）
- 好：最后分配的，最先释放

**反模式 2：probe 中缺少错误处理**
- 坏：不检查分配/映射是否成功
- 好：每次分配后检查，失败时跳到清理标签

**反模式 3：活动使用期间调用 remove**
- 坏：直接释放资源，可能 use-after-free
- 好：先停止接受新工作，注销，禁用中断，同步 IRQ，然后释放

**反模式 4：全局状态用于设备数据**
- 坏：使用全局指针，无法支持多设备
- 好：使用 `pci_set_drvdata()`，通过回调数据传递私有数据

**反模式 5：不使用 devm_***
- 坏：手动分配/释放，容易遗漏
- 好：使用 `devm_kzalloc`、`devm_ioremap`、`devm_request_irq`，自动清理

**关键规则**：
1. remove() 必须是 probe() 的精确逆序
2. 每次分配必须在错误路径上有对应的释放
3. 释放资源前先停止活动
4. 不要用全局状态存储每设备数据
5. 尽可能使用 devm_*
6. 测试热插拔：反复 insmod/rmmod
