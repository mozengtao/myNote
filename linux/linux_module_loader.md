# Linux Kernel Module Loader Architecture (v3.2)

## Overview

This document explains **Linux kernel module architecture** in v3.2, focusing on dynamic loading, symbol resolution, and plugin system ideas.

---

## Module Lifecycle

```
+------------------------------------------------------------------+
|  MODULE LIFECYCLE                                                |
+------------------------------------------------------------------+

    1. COMPILE (.ko file)
    ┌─────────────────────────────────────────────────────────────┐
    │ make modules                                                 │
    │   - Compiles source to .o                                   │
    │   - Links to .ko (kernel object)                            │
    │   - Contains ELF relocatable code                           │
    └───────────────────────────────────────────────────────────────┘
           │
           ▼
    2. LOAD (insmod/modprobe)
    ┌─────────────────────────────────────────────────────────────┐
    │ sys_init_module():                                           │
    │   - Read .ko file into memory                                │
    │   - Parse ELF headers                                        │
    │   - Allocate kernel memory for module                        │
    │   - Resolve symbol references                                │
    │   - Apply relocations                                        │
    └───────────────────────────────────────────────────────────────┘
           │
           ▼
    3. INITIALIZE
    ┌─────────────────────────────────────────────────────────────┐
    │ module_init(my_init):                                        │
    │   - Called after module is loaded                            │
    │   - Register with subsystems                                 │
    │   - Allocate resources                                       │
    │   - Return 0 on success, negative on error                   │
    └───────────────────────────────────────────────────────────────┘
           │
           ▼
    4. RUNNING
    ┌─────────────────────────────────────────────────────────────┐
    │ Module active:                                               │
    │   - Callbacks from kernel                                    │
    │   - Reference count tracked                                  │
    │   - try_module_get() / module_put()                          │
    └───────────────────────────────────────────────────────────────┘
           │
           ▼
    5. UNLOAD (rmmod)
    ┌─────────────────────────────────────────────────────────────┐
    │ module_exit(my_exit):                                        │
    │   - Called before module is removed                          │
    │   - Unregister from subsystems                               │
    │   - Free resources                                           │
    │   - Wait for reference count to reach 0                      │
    └───────────────────────────────────────────────────────────────┘
           │
           ▼
    6. FREE
    ┌─────────────────────────────────────────────────────────────┐
    │ sys_delete_module():                                         │
    │   - Free module memory                                       │
    │   - Remove from module list                                  │
    └───────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 模块生命周期：编译 → 加载 → 初始化 → 运行 → 卸载 → 释放
- 加载时：解析 ELF、分配内存、解析符号、应用重定位
- 初始化：注册到子系统、分配资源
- 卸载：注销、释放资源、等待引用计数为0

---

## Symbol Export/Import

```
+------------------------------------------------------------------+
|  SYMBOL RESOLUTION                                               |
+------------------------------------------------------------------+

    EXPORTING SYMBOLS:
    
    /* In module A (exporter) */
    int my_function(int x) { return x * 2; }
    EXPORT_SYMBOL(my_function);      /* Available to all modules */
    
    /* Or with GPL restriction */
    EXPORT_SYMBOL_GPL(my_function);  /* Only GPL modules can use */

    IMPORTING SYMBOLS:
    
    /* In module B (importer) */
    extern int my_function(int x);   /* Declaration */
    
    int user_function(void) {
        return my_function(42);      /* Uses exported symbol */
    }

    SYMBOL TABLE:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Kernel Symbol Table                                         │
    │  ┌──────────────────┬────────────────┬──────────────────┐   │
    │  │ Symbol Name      │ Address        │ Owner            │   │
    │  ├──────────────────┼────────────────┼──────────────────┤   │
    │  │ printk           │ 0xffffffff81.. │ vmlinux          │   │
    │  │ kmalloc          │ 0xffffffff81.. │ vmlinux          │   │
    │  │ my_function      │ 0xffffffffa0.. │ module_a         │   │
    │  │ usb_register     │ 0xffffffffa0.. │ usbcore          │   │
    │  └──────────────────┴────────────────┴──────────────────┘   │
    └─────────────────────────────────────────────────────────────┘

    RESOLUTION PROCESS:
    
    ┌─────────────────────────────────────────────────────────────┐
    │ Module B loading:                                            │
    │   1. Find unresolved symbols                                 │
    │   2. Look up each in kernel symbol table                     │
    │   3. Patch call sites with resolved addresses                │
    │   4. If symbol not found → load fails                        │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 导出符号：EXPORT_SYMBOL（所有模块）或 EXPORT_SYMBOL_GPL（仅GPL模块）
- 导入符号：extern 声明，加载时解析
- 符号表：符号名 → 地址 → 所有者
- 解析过程：查找未解析符号 → 查表 → 修补调用点

---

## Dependency Handling

```
+------------------------------------------------------------------+
|  MODULE DEPENDENCIES                                             |
+------------------------------------------------------------------+

    DEPENDENCY CHAIN:
    
    my_driver.ko
         │
         │ uses symbols from
         ▼
    usb_storage.ko
         │
         │ uses symbols from
         ▼
    usbcore.ko
         │
         │ uses symbols from
         ▼
    vmlinux (built-in)

    MODPROBE vs INSMOD:
    
    insmod my_driver.ko
    │
    └── Loads ONLY my_driver.ko
        If dependencies missing → FAILS

    modprobe my_driver
    │
    ├── Reads /lib/modules/$(uname -r)/modules.dep
    ├── Loads usbcore.ko (if not loaded)
    ├── Loads usb_storage.ko (if not loaded)
    └── Loads my_driver.ko

    modules.dep FORMAT:
    
    my_driver.ko: usb_storage.ko usbcore.ko
    usb_storage.ko: usbcore.ko
    usbcore.ko:

    UNLOAD ORDER:
    
    rmmod my_driver     ← Must unload users first
    rmmod usb_storage   ← Then intermediate
    rmmod usbcore       ← Finally base (if no other users)
```

**中文解释：**
- 依赖链：模块使用其他模块的符号
- insmod：只加载指定模块，依赖缺失则失败
- modprobe：读取 modules.dep，自动加载依赖
- 卸载顺序：先卸载依赖者，后卸载被依赖者

---

## Failure Cases

```
+------------------------------------------------------------------+
|  MODULE FAILURE HANDLING                                         |
+------------------------------------------------------------------+

    INIT FAILURE:
    
    static int __init my_init(void)
    {
        int ret;
        
        ret = alloc_resource_a();
        if (ret)
            goto fail_a;
        
        ret = alloc_resource_b();
        if (ret)
            goto fail_b;
        
        ret = register_device();
        if (ret)
            goto fail_register;
        
        return 0;  /* Success */
    
    fail_register:
        free_resource_b();
    fail_b:
        free_resource_a();
    fail_a:
        return ret;  /* Return error code */
    }

    INIT FAILURE HANDLING:
    +----------------------------------------------------------+
    | If init returns non-zero:                                 |
    | 1. Module is NOT added to module list                     |
    | 2. Module memory is freed                                 |
    | 3. insmod/modprobe returns error                          |
    | 4. exit function is NOT called                            |
    +----------------------------------------------------------+

    UNLOAD REFUSAL:
    
    $ rmmod my_driver
    rmmod: ERROR: Module my_driver is in use
    
    +----------------------------------------------------------+
    | Cannot unload while:                                      |
    | - module->refcount > 0                                    |
    | - Another module depends on it                            |
    | - Module marked as "unsafe to unload"                     |
    +----------------------------------------------------------+

    REFERENCE COUNTING:
    
    /* Before using module's function */
    if (!try_module_get(module))
        return -ENOENT;  /* Module is unloading */
    
    use_module_function();
    
    module_put(module);  /* Done using */
```

**中文解释：**
- 初始化失败：goto 清理模式，返回错误码
- 失败处理：不加入模块列表、释放内存、不调用 exit
- 卸载拒绝：引用计数 > 0 或有依赖模块
- 引用计数：try_module_get / module_put

---

## User-Space Plugin System

```c
/* User-space plugin system inspired by kernel modules */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>

/* Plugin interface */
struct plugin_ops {
    const char *name;
    int (*init)(void);
    void (*exit)(void);
    int (*process)(void *data);
};

/* Loaded plugin */
struct plugin {
    char *path;
    void *handle;           /* dlopen handle */
    struct plugin_ops *ops;
    int refcount;
    struct plugin *next;
};

/* Plugin registry */
struct plugin_registry {
    struct plugin *plugins;
    pthread_mutex_t lock;
};

static struct plugin_registry registry = {
    .plugins = NULL,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

/* Load plugin (like insmod) */
struct plugin *plugin_load(const char *path)
{
    pthread_mutex_lock(&registry.lock);
    
    /* Check if already loaded */
    struct plugin *p;
    for (p = registry.plugins; p; p = p->next) {
        if (strcmp(p->path, path) == 0) {
            pthread_mutex_unlock(&registry.lock);
            fprintf(stderr, "Plugin already loaded: %s\n", path);
            return NULL;
        }
    }
    
    /* Open shared library */
    void *handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        pthread_mutex_unlock(&registry.lock);
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return NULL;
    }
    
    /* Find plugin_ops symbol (like module symbol table) */
    struct plugin_ops *ops = dlsym(handle, "plugin_ops");
    if (!ops) {
        dlclose(handle);
        pthread_mutex_unlock(&registry.lock);
        fprintf(stderr, "No plugin_ops found\n");
        return NULL;
    }
    
    /* Call init function (like module_init) */
    if (ops->init) {
        int ret = ops->init();
        if (ret != 0) {
            dlclose(handle);
            pthread_mutex_unlock(&registry.lock);
            fprintf(stderr, "Plugin init failed: %d\n", ret);
            return NULL;
        }
    }
    
    /* Create plugin structure */
    p = malloc(sizeof(*p));
    p->path = strdup(path);
    p->handle = handle;
    p->ops = ops;
    p->refcount = 1;
    
    /* Add to registry */
    p->next = registry.plugins;
    registry.plugins = p;
    
    pthread_mutex_unlock(&registry.lock);
    printf("Plugin loaded: %s (%s)\n", path, ops->name);
    return p;
}

/* Get reference (like try_module_get) */
int plugin_get(struct plugin *p)
{
    pthread_mutex_lock(&registry.lock);
    if (p->refcount <= 0) {
        pthread_mutex_unlock(&registry.lock);
        return 0;  /* Plugin is unloading */
    }
    p->refcount++;
    pthread_mutex_unlock(&registry.lock);
    return 1;
}

/* Put reference (like module_put) */
void plugin_put(struct plugin *p)
{
    pthread_mutex_lock(&registry.lock);
    p->refcount--;
    pthread_mutex_unlock(&registry.lock);
}

/* Unload plugin (like rmmod) */
int plugin_unload(struct plugin *p)
{
    pthread_mutex_lock(&registry.lock);
    
    /* Check refcount */
    if (p->refcount > 1) {
        pthread_mutex_unlock(&registry.lock);
        fprintf(stderr, "Plugin in use (refcount=%d)\n", p->refcount);
        return -1;
    }
    
    /* Remove from registry */
    struct plugin **pp;
    for (pp = &registry.plugins; *pp; pp = &(*pp)->next) {
        if (*pp == p) {
            *pp = p->next;
            break;
        }
    }
    
    pthread_mutex_unlock(&registry.lock);
    
    /* Call exit function (like module_exit) */
    if (p->ops->exit) {
        p->ops->exit();
    }
    
    /* Close library and free */
    dlclose(p->handle);
    free(p->path);
    free(p);
    
    printf("Plugin unloaded\n");
    return 0;
}

/* Example plugin (in separate .so file) */
/*
static int my_init(void) {
    printf("My plugin initializing\n");
    return 0;
}

static void my_exit(void) {
    printf("My plugin exiting\n");
}

static int my_process(void *data) {
    printf("Processing: %s\n", (char *)data);
    return 0;
}

struct plugin_ops plugin_ops = {
    .name = "my_plugin",
    .init = my_init,
    .exit = my_exit,
    .process = my_process,
};
*/

int main(void)
{
    /* Load plugin */
    struct plugin *p = plugin_load("./my_plugin.so");
    if (!p) return 1;
    
    /* Use plugin */
    if (plugin_get(p)) {
        p->ops->process("Hello, plugin!");
        plugin_put(p);
    }
    
    /* Unload plugin */
    plugin_unload(p);
    
    return 0;
}
```

**中文解释：**
- 用户态插件系统：模拟内核模块
- plugin_load：dlopen 加载、查找 plugin_ops、调用 init
- plugin_get/put：引用计数（类似 try_module_get/module_put）
- plugin_unload：检查引用计数、调用 exit、dlclose

