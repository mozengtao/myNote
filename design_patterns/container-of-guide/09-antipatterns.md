# Anti-Patterns: What NOT to Do with Container-of

## Anti-Pattern 1: Wrong Member Name

```c
/*
 * ANTI-PATTERN: Using wrong member name in container_of
 * =====================================================
 */

struct my_device {
    struct list_head list_a;   /* offset 0 */
    struct list_head list_b;   /* offset 16 */
    int id;
};

/* WRONG: Member name doesn't match pointer origin */
void bad_callback(struct list_head *ptr)
{
    /* ptr actually points to list_b, but we use list_a */
    struct my_device *dev = container_of(ptr, struct my_device, list_a);
    
    /* 
     * DISASTER: Calculated pointer is WRONG
     * ptr - offsetof(list_a) != ptr - offsetof(list_b)
     * Accessing dev-> will corrupt memory or crash
     */
}

/* CORRECT: Member name matches pointer origin */
void good_callback_a(struct list_head *ptr)
{
    struct my_device *dev = container_of(ptr, struct my_device, list_a);
    /* Correct if ptr came from &some_dev->list_a */
}

void good_callback_b(struct list_head *ptr)
{
    struct my_device *dev = container_of(ptr, struct my_device, list_b);
    /* Correct if ptr came from &some_dev->list_b */
}
```

**中文说明：**

反模式1：在container_of中使用错误的成员名。如果ptr指向list_b但在container_of中使用list_a，计算出的偏移量是错误的，访问结果指针会导致内存损坏或崩溃。必须确保成员名与指针来源匹配。

---

## Anti-Pattern 2: Using Pointer Instead of Embedding

```c
/*
 * ANTI-PATTERN: Storing pointer instead of embedding
 * ===================================================
 */

/* WRONG: list_head as pointer - container_of won't work */
struct bad_design {
    int id;
    struct list_head *list;   /* POINTER - wrong! */
};

void bad_usage(void)
{
    struct bad_design bd;
    struct list_head node;
    
    bd.list = &node;
    
    /* Later, given &node, can we get back to bd? */
    /* NO! node is not embedded in bd */
    /* container_of will give garbage */
}


/* CORRECT: list_head embedded */
struct good_design {
    int id;
    struct list_head list;    /* EMBEDDED - correct! */
};

void good_usage(void)
{
    struct good_design gd;
    INIT_LIST_HEAD(&gd.list);
    
    /* Later, given &gd.list, we can get back to gd */
    struct list_head *ptr = &gd.list;
    struct good_design *recovered = container_of(ptr, struct good_design, list);
    /* recovered == &gd */
}
```

**中文说明：**

反模式2：使用指针而不是嵌入。如果结构体包含指向list_head的指针而不是嵌入list_head，container_of就无法工作，因为被指向的节点不在结构体内部。必须嵌入而不是指向。

---

## Anti-Pattern 3: container_of on Stack/Heap Boundary

```c
/*
 * ANTI-PATTERN: Mixing allocation lifetimes
 * =========================================
 */

struct device {
    struct list_head list;
    int id;
};

/* WRONG: Embedded node outlives container */
struct list_head global_list;

void dangerous_function(void)
{
    struct device dev;  /* STACK allocated */
    dev.id = 42;
    
    list_add(&dev.list, &global_list);  /* Added to global list */
    
}   /* dev goes out of scope - DANGLING POINTER in global_list! */

void later_crash(void)
{
    struct list_head *pos;
    list_for_each(pos, &global_list) {
        /* pos points to dead stack memory! */
        struct device *dev = container_of(pos, struct device, list);
        /* CRASH: dev->id accesses invalid memory */
    }
}


/* CORRECT: Match lifetimes */
void safe_function(void)
{
    struct device *dev = malloc(sizeof(*dev));  /* HEAP allocated */
    dev->id = 42;
    
    list_add(&dev->list, &global_list);
    
    /* dev persists - can be safely accessed later */
}

void safe_cleanup(void)
{
    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &global_list) {
        struct device *dev = container_of(pos, struct device, list);
        list_del(pos);
        free(dev);
    }
}
```

**中文说明：**

反模式3：混合分配生命周期。如果在栈上分配结构体然后将其list_head添加到全局列表，当函数返回后栈内存被回收，全局列表中就有了悬空指针。使用container_of访问会导致崩溃。必须确保容器的生命周期足够长。

---

## Anti-Pattern 4: Type Mismatch

```c
/*
 * ANTI-PATTERN: Using wrong container type
 * ========================================
 */

struct device_a {
    int id_a;
    struct list_head list;
    char data_a[32];
};

struct device_b {
    char data_b[32];
    struct list_head list;  /* Same member name, different offset! */
    int id_b;
};

/* WRONG: Type confusion */
void type_confused(struct list_head *ptr)
{
    /* ptr actually came from device_b */
    struct device_a *dev = container_of(ptr, struct device_a, list);
    
    /*
     * WRONG OFFSET:
     * device_a: list at offset sizeof(int) = 4
     * device_b: list at offset 32
     * 
     * If ptr came from device_b at address 0x1000:
     * ptr = 0x1000 + 32 = 0x1032
     * 
     * container_of thinks: 0x1032 - 4 = 0x102E  <-- WRONG!
     * Correct would be:    0x1032 - 32 = 0x1000
     */
}


/* CORRECT: Use correct type */
void type_correct_a(struct list_head *ptr)
{
    /* Only call this when ptr came from device_a */
    struct device_a *dev = container_of(ptr, struct device_a, list);
}

void type_correct_b(struct list_head *ptr)
{
    /* Only call this when ptr came from device_b */
    struct device_b *dev = container_of(ptr, struct device_b, list);
}
```

---

## Anti-Pattern 5: NULL Pointer to container_of

```c
/*
 * ANTI-PATTERN: Passing NULL to container_of
 * ==========================================
 */

/* WRONG: NULL check after container_of */
void bad_null_handling(struct list_head *ptr)
{
    struct device *dev = container_of(ptr, struct device, list);
    
    /* This check is USELESS */
    if (dev == NULL) {  /* Never true unless ptr was some magic value */
        return;
    }
    
    /* If ptr was NULL, dev is now a small negative number (cast) */
    /* Accessing dev-> will crash */
}


/* CORRECT: NULL check before container_of */
void good_null_handling(struct list_head *ptr)
{
    if (ptr == NULL) {
        return;
    }
    
    struct device *dev = container_of(ptr, struct device, list);
    /* Now dev is valid */
}
```

**中文说明：**

反模式5：将NULL传递给container_of。container_of只是做减法，如果ptr是NULL，结果会是一个小的负数（被转换为指针），访问它会崩溃。必须在调用container_of之前检查NULL。

---

## Anti-Pattern 6: Using container_of on Copied Node

```c
/*
 * ANTI-PATTERN: container_of on copied embedded structure
 * =======================================================
 */

struct device {
    int id;
    struct list_head list;
    char name[32];
};

/* WRONG: Copy the embedded node */
void bad_copy(struct device *src)
{
    struct list_head copy = src->list;  /* Copied! Not embedded! */
    
    /* Later... */
    struct device *dev = container_of(&copy, struct device, list);
    
    /*
     * WRONG: copy is a separate local variable
     * It's not embedded in any device struct
     * container_of gives garbage pointer
     */
}


/* CORRECT: Keep pointer to original */
void good_reference(struct device *src)
{
    struct list_head *ptr = &src->list;  /* Pointer to embedded */
    
    /* Later... */
    struct device *dev = container_of(ptr, struct device, list);
    /* dev == src */
}
```

---

## Summary: container_of Rules

```
+=============================================================================+
|              CONTAINER_OF SAFETY RULES                                       |
+=============================================================================+

    1. MEMBER MUST BE EMBEDDED (not pointer)
       struct x { struct list_head list; }   <-- OK
       struct x { struct list_head *list; }  <-- WRONG

    2. MEMBER NAME MUST MATCH POINTER ORIGIN
       If ptr came from &obj->list_a, use container_of(..., list_a)
       NOT container_of(..., list_b)

    3. CONTAINER TYPE MUST MATCH ACTUAL TYPE
       Don't mix device_a and device_b

    4. CHECK NULL BEFORE container_of
       if (ptr) { dev = container_of(ptr, ...); }

    5. LIFETIME: Container must outlive embedded usage
       Don't add stack-allocated node to persistent list

    6. DON'T COPY EMBEDDED STRUCTURES
       Keep pointer to original, don't copy the node

    7. DON'T USE ON NON-EMBEDDED DATA
       container_of only works for embedded members
```
