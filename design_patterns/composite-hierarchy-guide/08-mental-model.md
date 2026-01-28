# Final Mental Model: Composite/Hierarchy Pattern

## One-Paragraph Summary

The Composite pattern creates tree hierarchies where each node can have a parent and children. In the Linux kernel, kobject is the primary implementation - every kobject has a parent pointer and can be part of a kset (container). This enables: uniform interface for all hierarchical objects, automatic sysfs representation of the hierarchy, reference-counted lifecycle management, and tree operations (add, delete, traverse). The device model builds on kobject to represent hardware hierarchy. Key rule: always delete children before parents, always hold references when accessing nodes, always provide release functions.

**中文总结：**

组合模式创建树层次结构，每个节点可以有父节点和子节点。Linux内核中，kobject是主要实现——每个kobject有parent指针，可以是kset（容器）的一部分。这支持：所有层次对象的统一接口、层次结构自动sysfs表示、引用计数生命周期管理、树操作（添加、删除、遍历）。设备模型基于kobject表示硬件层次。关键规则：总是先删除子节点再删除父节点，访问节点时总是持有引用，总是提供release函数。

---

## Decision Flowchart

```
    Do objects form parent-child relationships?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
COMPOSITE       Simple pointers
    
    Need sysfs visibility?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
Use kobject     Custom parent pointer
```

---

## Quick Reference

```
    HIERARCHY OPERATIONS:
    =====================
    
    kobject_init(&kobj, ktype)     Initialize
    kobject_add(&kobj, parent, name)  Add to tree
    kobject_del(&kobj)             Remove from tree
    kobject_get(&kobj)             Get reference
    kobject_put(&kobj)             Put reference
    
    
    HIERARCHY RULES:
    ================
    
    1. Child -> Parent (always via kobj->parent)
    2. Parent -> Children (via kset->list or klist)
    3. Refcount prevents premature free
    4. Delete children before parent
```

---

## Version

Based on **Linux kernel v3.2** kobject patterns.
