# Unified Registration Skeleton

A generic C skeleton capturing the Registration pattern.

---

## Complete Skeleton

```c
/*
 * Generic Registration Pattern Skeleton
 * Based on Linux kernel registration patterns
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * PART 1: SUBSYSTEM SIDE (CORE)
 * ================================================================ */

/* ID for matching */
struct component_id {
    int type;
    int subtype;
};

/* Component structure - drivers/filesystems implement this */
struct component {
    const char *name;
    const struct component_id *id_table;
    
    /* Lifecycle callbacks */
    int (*probe)(void *device, const struct component_id *id);
    void (*remove)(void *device);
    
    /* Internal linkage */
    struct component *next;
};

/* Global registered component list */
static struct component *component_list = NULL;

/* Match component to device */
const struct component_id *match_component(struct component *comp,
                                           int type, int subtype)
{
    const struct component_id *id;
    
    for (id = comp->id_table; id->type || id->subtype; id++) {
        if (id->type == type && id->subtype == subtype)
            return id;
    }
    return NULL;
}

/* Register component with subsystem */
int register_component(struct component *comp)
{
    printf("[CORE] Registering: %s\n", comp->name);
    
    /* Add to list */
    comp->next = component_list;
    component_list = comp;
    
    return 0;
}

/* Unregister component */
void unregister_component(struct component *comp)
{
    struct component **p;
    
    printf("[CORE] Unregistering: %s\n", comp->name);
    
    for (p = &component_list; *p; p = &(*p)->next) {
        if (*p == comp) {
            *p = comp->next;
            return;
        }
    }
}

/* Find and bind component to device */
int bind_device(void *device, int type, int subtype)
{
    struct component *comp;
    const struct component_id *id;
    
    printf("[CORE] Finding component for device type=%d subtype=%d\n",
           type, subtype);
    
    for (comp = component_list; comp; comp = comp->next) {
        id = match_component(comp, type, subtype);
        if (id) {
            printf("[CORE] Match found: %s\n", comp->name);
            return comp->probe(device, id);
        }
    }
    
    printf("[CORE] No matching component found\n");
    return -1;
}

/* ================================================================
 * PART 2: COMPONENT SIDE (DRIVER/MODULE)
 * ================================================================ */

/* Example device structure */
struct my_device {
    int type;
    int subtype;
    char name[32];
};

/* ID table - which devices this component supports */
static const struct component_id my_ids[] = {
    { 1, 100 },
    { 1, 200 },
    { 0, 0 }  /* Terminator */
};

/* Probe callback */
int my_probe(void *dev, const struct component_id *id)
{
    struct my_device *d = dev;
    printf("  [MY_COMP] Probe: %s (type=%d subtype=%d)\n",
           d->name, id->type, id->subtype);
    printf("  [MY_COMP] Initializing...\n");
    return 0;
}

/* Remove callback */
void my_remove(void *dev)
{
    struct my_device *d = dev;
    printf("  [MY_COMP] Remove: %s\n", d->name);
}

/* Component structure */
static struct component my_component = {
    .name = "my_component",
    .id_table = my_ids,
    .probe = my_probe,
    .remove = my_remove,
};

/* Module init */
int my_module_init(void)
{
    printf("\n=== Module Init ===\n");
    return register_component(&my_component);
}

/* Module exit */
void my_module_exit(void)
{
    printf("\n=== Module Exit ===\n");
    unregister_component(&my_component);
}

/* ================================================================
 * PART 3: USAGE
 * ================================================================ */

int main(void)
{
    struct my_device dev1 = { 1, 100, "device1" };
    struct my_device dev2 = { 1, 200, "device2" };
    struct my_device dev3 = { 2, 300, "device3" };
    
    printf("=== REGISTRATION SKELETON ===\n");
    
    /* Load module */
    my_module_init();
    
    /* Discover devices */
    printf("\n=== Device Discovery ===\n");
    bind_device(&dev1, dev1.type, dev1.subtype);
    bind_device(&dev2, dev2.type, dev2.subtype);
    bind_device(&dev3, dev3.type, dev3.subtype);  /* No match */
    
    /* Unload module */
    my_module_exit();
    
    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON                KERNEL
    ========                ======
    
    struct component        struct pci_driver
                            struct file_system_type
    
    register_component      pci_register_driver
                            register_filesystem
    
    unregister_component    pci_unregister_driver
                            unregister_filesystem
    
    component_id            pci_device_id
                            (filesystem has no ID table)
    
    probe()                 driver->probe()
                            fs_type->mount()
    
    remove()                driver->remove()
                            fs_type->kill_sb()
```

---

## Key Implementation Points

```
    1. SUBSYSTEM MAINTAINS LIST
       - Global list of registered components
       - Add on register, remove on unregister
    
    2. MATCHING MECHANISM
       - ID tables for device matching
       - Or name matching for filesystems
    
    3. CALLBACKS
       - probe/mount when match found
       - remove/kill_sb on cleanup
    
    4. MODULE LIFECYCLE
       - module_init calls register
       - module_exit calls unregister
```

---

## Version

Based on **Linux kernel v3.2** registration patterns.
