# Composite Anti-Patterns

Common mistakes to avoid when implementing hierarchical structures.

---

## Anti-Pattern 1: Orphan Nodes

```c
/* BAD: Node initialized but never added */
void bad_function(void)
{
    struct kobject kobj;
    
    kobject_init(&kobj, &my_ktype);
    /* Never called kobject_add! */
    /* kobject not in hierarchy */
    /* sysfs entry not created */
}

/* CORRECT: Always add after init */
void good_function(void)
{
    struct kobject kobj;
    
    kobject_init(&kobj, &my_ktype);
    kobject_add(&kobj, parent, "name");
    /* ... use kobject ... */
    kobject_del(&kobj);
    kobject_put(&kobj);
}
```

**中文说明：**

反模式1：孤立节点——初始化后未添加到层次结构。正确做法是初始化后总是调用add。

---

## Anti-Pattern 2: Reference Count Errors

```c
/* BAD: Using parent without holding reference */
void bad_function(struct kobject *kobj)
{
    struct kobject *parent = kobj->parent;
    /* ... do something else ... */
    use(parent);  /* Parent may be freed! */
}

/* CORRECT: Get reference before use */
void good_function(struct kobject *kobj)
{
    struct kobject *parent = kobject_get(kobj->parent);
    if (parent) {
        use(parent);
        kobject_put(parent);
    }
}
```

---

## Anti-Pattern 3: Deleting with Children

```c
/* BAD: Deleting parent before children */
void bad_cleanup(void)
{
    kobject_del(&parent);  /* Children still attached! */
    kobject_del(&child);   /* Too late */
}

/* CORRECT: Delete children first */
void good_cleanup(void)
{
    kobject_del(&child);   /* Children first */
    kobject_del(&parent);  /* Then parent */
}
```

**中文说明：**

反模式3：在子节点之前删除父节点——必须先删除子节点再删除父节点。

---

## Anti-Pattern 4: Missing Release Function

```c
/* BAD: No release function */
static struct kobj_type bad_ktype = {
    /* .release = NULL */
};

/* Memory leak when refcount reaches 0! */

/* CORRECT: Provide release function */
static void my_release(struct kobject *kobj)
{
    struct my_object *obj = container_of(kobj, struct my_object, kobj);
    kfree(obj);
}

static struct kobj_type good_ktype = {
    .release = my_release,
};
```

---

## Anti-Pattern 5: Circular References

```c
/* BAD: Child holds reference to parent */
void bad_init(void)
{
    kobject_get(parent);  /* Parent refcount++ */
    kobject_add(&child, parent, "child");
    child.my_parent = kobject_get(parent);  /* Extra ref! */
    /* Now: parent -> child -> parent (circular) */
    /* Neither can be freed */
}

/* CORRECT: Don't hold extra parent reference */
void good_init(void)
{
    kobject_add(&child, parent, "child");
    /* Child has implicit reference via parent pointer */
    /* No extra get needed */
}
```

---

## Summary Checklist

```
+=============================================================================+
|                    COMPOSITE SAFE USAGE                                      |
+=============================================================================+

    [X] Always add after init
    [X] Get reference before using parent
    [X] Delete children before parent
    [X] Provide release function
    [X] Avoid circular references
    [X] Put after done using
```

---

## Version

Based on **Linux kernel v3.2** kobject patterns.
