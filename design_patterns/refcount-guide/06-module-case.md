# Case 4: Module Reference Counting

## Subsystem Background

```
+=============================================================================+
|                    MODULE REFERENCE COUNTING                                 |
+=============================================================================+

    THE PROBLEM:
    ============

    Kernel modules can be loaded and unloaded dynamically.
    But what if code is being executed when module is unloaded?

    CPU A                          CPU B
    -----                          -----
    Calls function in module X     
                                   rmmod X
                                   Module X unloaded!
    Returns to module X            
    --> CRASH! Code is gone!


    THE SOLUTION:
    =============

    Reference count on module.
    - Increment when starting to use module
    - Decrement when done
    - Module can only unload when refcount == 0


    STRUCT MODULE:
    ==============

    struct module {
        /* ... */
        struct module_ref ref;     /* Per-CPU reference counter */
        /* ... */
    };

    Note: Per-CPU for performance (avoid cache line bouncing)
```

**中文说明：**

模块引用计数解决的问题：内核模块可以动态加载和卸载，但如果模块被卸载时代码正在执行怎么办？一个CPU在执行模块中的函数，另一个CPU卸载模块，当第一个CPU返回时，代码已经不存在了，会崩溃。解决方案：模块上的引用计数，使用模块时增加，使用完减少，只有引用计数为0时才能卸载模块。

---

## Module Reference Counting API

```c
/* include/linux/module.h */

/**
 * try_module_get - try to increment module refcount
 * @module: module to reference
 *
 * Returns true if successful, false if module is being unloaded.
 * 
 * IMPORTANT: Returns false if module is going away.
 * Caller must check return value!
 */
bool try_module_get(struct module *module);

/**
 * module_put - decrement module refcount
 * @module: module to release
 */
void module_put(struct module *module);

/**
 * __module_get - unconditionally increment refcount
 * @module: module to reference
 *
 * Only use when you KNOW module won't be unloaded
 * (e.g., you're calling from within the module itself)
 */
void __module_get(struct module *module);
```

---

## try_module_get vs __module_get

```
+=============================================================================+
|              TRY_MODULE_GET vs __MODULE_GET                                  |
+=============================================================================+

    try_module_get():
    =================
    
    - CAN FAIL if module is being unloaded
    - Must check return value
    - Safe to use from any context
    
    if (try_module_get(mod)) {
        /* Success - module won't unload while we hold ref */
        use_module(mod);
        module_put(mod);
    } else {
        /* Module is going away - don't use it! */
    }


    __module_get():
    ===============
    
    - CANNOT FAIL (will crash if module going away)
    - Only use when you KNOW module is stable
    - Typically used within module's own code
    
    __module_get(THIS_MODULE);  /* I'm in the module, it exists */
    /* ... */
    module_put(THIS_MODULE);


    WHEN TO USE WHICH:
    ==================
    
    +------------------------+-------------------------+
    | Situation              | Use                     |
    +------------------------+-------------------------+
    | External code calling  | try_module_get()        |
    | into module            |                         |
    +------------------------+-------------------------+
    | Within module's own    | __module_get() or       |
    | code                   | no get needed           |
    +------------------------+-------------------------+
    | Callback from module's | try_module_get() in     |
    | registered function    | registration code       |
    +------------------------+-------------------------+
```

**中文说明：**

`try_module_get()`可能失败（如果模块正在卸载），必须检查返回值，从任何上下文使用都安全。`__module_get()`不会失败（如果模块正在卸载会崩溃），只有在确定模块稳定时使用，通常在模块自己的代码中使用。

---

## Minimal C Code Simulation

```c
/*
 * MODULE REFERENCE COUNTING SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>

/* ==========================================================
 * MODULE STRUCTURE (Simplified)
 * ========================================================== */

enum module_state {
    MODULE_STATE_LIVE,
    MODULE_STATE_GOING,
    MODULE_STATE_UNFORMED,
};

struct module {
    char name[64];
    enum module_state state;
    atomic_int refcount;
    
    /* Module's functions */
    int (*init)(void);
    void (*exit)(void);
};

/* ==========================================================
 * MODULE REFERENCE OPERATIONS
 * ========================================================== */

bool try_module_get(struct module *mod)
{
    if (!mod) return true;  /* NULL module (built-in) always succeeds */
    
    /* Check if module is going away */
    if (mod->state == MODULE_STATE_GOING) {
        printf("[MODULE] try_module_get('%s'): FAILED - module going away\n",
               mod->name);
        return false;
    }
    
    int old = atomic_fetch_add(&mod->refcount, 1);
    printf("[MODULE] try_module_get('%s'): refcount %d -> %d\n",
           mod->name, old, old + 1);
    return true;
}

void __module_get(struct module *mod)
{
    if (!mod) return;
    
    /* No check - assumes caller knows module is safe */
    int old = atomic_fetch_add(&mod->refcount, 1);
    printf("[MODULE] __module_get('%s'): refcount %d -> %d\n",
           mod->name, old, old + 1);
}

void module_put(struct module *mod)
{
    if (!mod) return;
    
    int old = atomic_fetch_sub(&mod->refcount, 1);
    printf("[MODULE] module_put('%s'): refcount %d -> %d\n",
           mod->name, old, old - 1);
    
    if (old == 1 && mod->state == MODULE_STATE_GOING) {
        printf("[MODULE] '%s': refcount 0, can now unload\n", mod->name);
    }
}

/* ==========================================================
 * MODULE LOADING/UNLOADING
 * ========================================================== */

struct module *modules[16];
int num_modules = 0;

struct module *load_module(const char *name, int (*init)(void), void (*exit)(void))
{
    struct module *mod = malloc(sizeof(*mod));
    strncpy(mod->name, name, sizeof(mod->name) - 1);
    mod->state = MODULE_STATE_UNFORMED;
    atomic_store(&mod->refcount, 0);
    mod->init = init;
    mod->exit = exit;
    
    printf("[LOAD] Loading module '%s'\n", name);
    
    /* Run init */
    if (mod->init) {
        mod->init();
    }
    
    mod->state = MODULE_STATE_LIVE;
    modules[num_modules++] = mod;
    
    printf("[LOAD] Module '%s' loaded and live\n", name);
    return mod;
}

bool unload_module(struct module *mod)
{
    printf("\n[UNLOAD] Attempting to unload '%s'\n", mod->name);
    
    /* Mark as going */
    mod->state = MODULE_STATE_GOING;
    printf("[UNLOAD] '%s' marked as GOING\n", mod->name);
    
    /* Check refcount */
    int count = atomic_load(&mod->refcount);
    if (count > 0) {
        printf("[UNLOAD] '%s': refcount = %d, cannot unload yet\n",
               mod->name, count);
        return false;
    }
    
    printf("[UNLOAD] '%s': refcount = 0, unloading...\n", mod->name);
    
    /* Run exit */
    if (mod->exit) {
        mod->exit();
    }
    
    /* Remove from list */
    for (int i = 0; i < num_modules; i++) {
        if (modules[i] == mod) {
            modules[i] = modules[--num_modules];
            break;
        }
    }
    
    free(mod);
    printf("[UNLOAD] Module unloaded\n");
    return true;
}

/* ==========================================================
 * EXAMPLE MODULE: Network Driver
 * ========================================================== */

int my_driver_init(void)
{
    printf("  [my_driver] init: Registering device\n");
    return 0;
}

void my_driver_exit(void)
{
    printf("  [my_driver] exit: Unregistering device\n");
}

/* Function exported by module */
void my_driver_do_something(struct module *mod)
{
    printf("  [my_driver] Doing something...\n");
}

/* ==========================================================
 * SIMULATION OF EXTERNAL CODE USING MODULE
 * ========================================================== */

void external_code_using_module(struct module *mod)
{
    printf("\n[EXTERNAL] Trying to use module '%s'\n", mod->name);
    
    if (!try_module_get(mod)) {
        printf("[EXTERNAL] Cannot use module - it's going away!\n");
        return;
    }
    
    /* Now safe to use module */
    printf("[EXTERNAL] Got reference, using module...\n");
    my_driver_do_something(mod);
    
    printf("[EXTERNAL] Done, releasing module\n");
    module_put(mod);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("MODULE REFERENCE COUNTING DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Load module */
    printf("--- Loading module ---\n");
    struct module *mod = load_module("my_driver", 
                                      my_driver_init, 
                                      my_driver_exit);
    
    /* Normal usage */
    printf("\n--- Normal usage (module live) ---\n");
    external_code_using_module(mod);
    
    /* Simulate multiple users */
    printf("\n--- Multiple users acquiring references ---\n");
    try_module_get(mod);  /* User A */
    try_module_get(mod);  /* User B */
    
    /* Try to unload while in use */
    printf("\n--- Try to unload while in use ---\n");
    bool unloaded = unload_module(mod);
    printf("Unload result: %s\n", unloaded ? "SUCCESS" : "BLOCKED");
    
    /* Module is GOING but still has refs */
    printf("\n--- Try to use module that's GOING ---\n");
    external_code_using_module(mod);  /* Should fail */
    
    /* Users release */
    printf("\n--- Users release references ---\n");
    module_put(mod);  /* User A done */
    module_put(mod);  /* User B done */
    
    /* Now unload succeeds */
    printf("\n--- Retry unload ---\n");
    unload_module(mod);  /* Should succeed */
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- Module has refcount to prevent unload while in use\n");
    printf("- try_module_get() fails if module is GOING\n");
    printf("- Module marked GOING before checking refcount\n");
    printf("- Unload waits for all users to release\n");
    printf("- Safe cleanup: no code execution after unload\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## Real Kernel Usage

```c
/* Example: File system module */

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .owner = THIS_MODULE,  /* Module that owns these ops */
};

/* When opening file with these ops */
int my_open(struct inode *inode, struct file *file)
{
    /* Kernel automatically does try_module_get(fops->owner) */
    /* If module is being unloaded, open fails */
}

int my_release(struct inode *inode, struct file *file)
{
    /* Kernel automatically does module_put(fops->owner) */
}
```

---

## Key Takeaways

1. **Protects against unload**: Module can't unload while code is executing
2. **try_module_get can fail**: Must check return value
3. **GOING state**: Window where new refs are refused
4. **Per-CPU counters**: Real kernel uses per-CPU for performance
5. **Owner field**: Many structures have `owner` pointing to module
