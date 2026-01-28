# Final Mental Model: Registration Pattern

## One-Paragraph Summary

The Registration pattern enables dynamic discovery and binding between kernel subsystems and their components (drivers, filesystems, protocols). Components self-register with the subsystem by calling a register function, providing a structure containing identification info (ID tables or names) and callbacks (probe, remove). The subsystem maintains a list of registered components and matches them to devices or requests at runtime. This pattern enables modular kernels where components are loaded on demand, hot-plug support where devices can appear and disappear, and clean separation between subsystem infrastructure and component-specific logic.

**中文总结：**

注册模式使内核子系统与组件（驱动、文件系统、协议）之间能够动态发现和绑定。组件调用register函数向子系统自我注册，提供包含标识信息（ID表或名称）和回调（probe、remove）的结构体。子系统维护注册组件列表，在运行时将它们与设备或请求匹配。此模式支持按需加载的模块化内核、设备热插拔、子系统基础设施与组件逻辑的清晰分离。

---

## Decision Flowchart

```
    Do you have dynamic components?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
REGISTRATION    Direct calls
    
    Do components provide services?
            |
    +-------+-------+
    |               |
   YES              NO (receive events)
    |               |
    v               v
REGISTRATION    OBSERVER
```

---

## Quick Reference

```
    REGISTRATION STRUCTURE:
    =======================
    
    struct xxx_driver {
        const char *name;
        const struct xxx_id *id_table;
        int (*probe)(device, id);
        void (*remove)(device);
    };

    LIFECYCLE:
    ==========
    
    module_init() --> register_xxx() --> added to list
                                              |
                         device discovered ---+
                                              |
                         match id_table       |
                                              v
                                        probe() called
                                              
    module_exit() --> unregister_xxx() --> remove() called
                                              |
                                        removed from list
```

---

## Version

Based on **Linux kernel v3.2** registration patterns.
