# Linux 内核学习笔记

[Linux v3.2 Kernel 调试指南（Ubuntu Server + QEMU + GDB）](./linux/evc_cli/qemu_gdb_linux32_v3.2_debug_guide.md)

## 🚀 快速导航

- [核心资源](#核心资源)
- [嵌入式C面向对象设计](#嵌入式c面向对象设计)  
- [开源操作系统项目](#开源操作系统项目)
- [Linux内核架构](#linux内核架构)
- [内核子系统](#内核子系统)
- [设备驱动模型](#设备驱动模型)
- [核心概念详解](#核心概念详解)
- [网络编程](#网络编程)
- [系统编程](#系统编程)
- [代码示例](#代码示例)
- [学习资源](#学习资源)
- [实用工具和命令](#实用工具和命令)

---

## 核心资源

### 内核学习资料
- [Understanding the Linux Kernel](https://github.com/hraverkar/books)  
- [**OS Development Wiki**](https://wiki.osdev.org/Main_Page)  
- [haiku OS](https://github.com/haiku/haiku)  
- [Linux-Kernel-Programming](https://github.com/PacktPublishing/Linux-Kernel-Programming)  
- [The Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)  
- [Learning operating system development using Linux kernel and Raspberry Pi](https://github.com/s-matyukevich/raspberry-pi-os)  
- [learning-linux-kernel](https://github.com/danbev/learning-linux-kernel)

### 实验环境
- [**Linux Playground**](./linux/playground.md)

---

## 嵌入式C面向对象设计

### 设计目标
嵌入式 C 的面向对象的目的：
- **驱动可替换** - 硬件抽象层设计
- **硬件可扩展** - 支持多种硬件平台  
- **平台可移植** - 代码跨平台复用
- **系统可长期维护** - 清晰的模块化架构

### 架构设计要点
关于一个项目时，尝试回答：
- ✅ 它的"核心对象"是谁？
- ✅ 对象之间如何组合/继承？
- ✅ 多态是怎么实现的？
- ✅ 是否有"协议（interface）"？
- ✅ 是否支持"可扩展插件"？
- ✅ 如何管理对象生命周期？

---

## 开源操作系统项目

### 实时操作系统

#### Zephyr RTOS
> A scalable real-time operating system (RTOS) supporting multiple hardware architectures, optimized for resource constrained devices, and built with security in mind.
- [zephyr](https://github.com/zephyrproject-rtos/zephyr)

#### RT-Thread
> An open source IoT Real-Time Operating System (RTOS)
- [rt-thread](https://github.com/RT-Thread/rt-thread)

#### QPC Framework
> QP/C real-time event framework (RTEF) is a lightweight implementation of the asynchronous, event-driven Active Object (a.k.a. Actor) model of computation specifically designed for real-time embedded systems, such as microcontrollers (MCUs).
- [qpc](https://github.com/QuantumLeaps/qpc)

### 系统组件库

#### 文件系统
- **LittleFS**: A little fail-safe filesystem designed for microcontrollers
  - [littlefs](https://github.com/littlefs-project/littlefs)

#### 网络协议栈
- **lwIP**: Small independent implementation of the TCP/IP protocol suite
  - [lwip](https://github.com/lwip-tcpip/lwip)
- **Mongoose**: Embedded web server, with TCP/IP network stack, MQTT and Websocket
  - [mongoose](https://github.com/cesanta/mongoose)

#### 开发平台
- **MicroPython**: Lean and efficient Python implementation for microcontrollers and constrained systems
  - [micropython](https://github.com/micropython/micropython)

#### 硬件抽象层
- **libopencm3**: Open source ARM Cortex-M microcontroller library
  - [libopencm3](https://github.com/libopencm3/libopencm3)

#### 状态机库
- **SML**: C++14 State Machine library
  - [sml](https://github.com/boost-ext/sml)

---

## Linux内核架构

### 内核面向对象模式研究
```
linux/kernel_oop_masterclass/
├── [00_overview_and_reference.md](./linux/kernel_oop_masterclass/00_overview_and_reference)  
├── [00b_mental_models_and_anchors.md](./linux/kernel_oop_masterclass/00b_mental_models_and_anchors.md)  
├── [01_encapsulation.md](./linux/kernel_oop_masterclass/01_encapsulation)  
├── [02_inheritance.md](./linux/kernel_oop_masterclass/02_inheritance)  
├── [03_polymorphism.md](./linux/kernel_oop_masterclass/03_polymorphism)  
├── [04_kobject_hierarchy.md](./linux/kernel_oop_masterclass/04_kobject_hierarchy)  
├── [05_design_patterns.md](./linux/kernel_oop_masterclass/05_design_patterns)  
└── [06_synthesis.md](./linux/kernel_oop_masterclass/06_synthesis)  
```

### 内核设计模式
```
linux/kernel_design_patterns/
├── [00_overview.md](./linux/kernel_design_patterns/00_overview.md)  
├── [01_strategy_and_adapter.md](./linux/kernel_design_patterns/01_strategy_and_adapter.md)  
├── [02_observer.md](./linux/kernel_design_patterns/02_observer.md)  
├── [03_template_method.md](./linux/kernel_design_patterns/03_template_method.md)  
├── [04_iterator.md](./linux/kernel_design_patterns/04_iterator.md)  
├── [05_state.md](./linux/kernel_design_patterns/05_state.md)  
├── [06_factory_and_singleton.md](./linux/kernel_design_patterns/06_factory_and_singleton.md)  
├── [07_facade.md](./linux/kernel_design_patterns/07_facade.md)  
└── [08_composition_decorator_builder.md](./linux/kernel_design_patterns/08_composition_decorator_builder.md)  
```

### 内核面向对象模式详解
```
kernel_oop_patterns/
├── [oop_patterns_in_linux_kernel_v3.2](./linux/kernel_oop_patterns/oop_patterns_in_linux_kernel_v3.2.md)  
├── [01_mental_model](./linux/kernel_oop_patterns/01_mental_model.md)  
├── [02_vfs_polymorphic_object_system](./linux/kernel_oop_patterns/02_vfs_polymorphic_object_system.md)
├── [03_device_model_object_hierarchies](./linux/kernel_oop_patterns/03_device_model_object_hierarchies.md)
├── [04_memory_and_ownership](./linux/kernel_oop_patterns/04_memory_and_ownership.md)
├── [05_guided_code_walkthrough](./linux/kernel_oop_patterns/05_guided_code_walkthrough.md)
└── [06_reflection_and_advanced](./linux/kernel_oop_patterns/06_reflection_and_advanced.md)
```

### 进程间通信(IPC)
```
ipc/
├── [01_file_based_ipc.md](./linux/ipc/01_file_based_ipc.md)  
├── [02_unix_domain_sockets.md](./linux/ipc/02_unix_domain_sockets.md)  
├── [03_posix_semaphores.md](./linux/ipc/03_posix_semaphores.md)  
├── [04_posix_message_queues.md](./linux/ipc/04_posix_message_queues.md)  
├── [05_shared_memory.md](./linux/ipc/05_shared_memory.md)  
├── [06_comparison.md](./linux/ipc/06_comparison.md)  
├── [README.md](./linux/ipc/README.md)  
└── examples/
    ├── 01_file/
    ├── 02_socket/
    ├── 03_semaphore/
    ├── 04_msgqueue/
    └── 05_sharedmem/
```

---

## 内核子系统

### 系统架构指南
#### 通用架构原则
- [什么是架构](./linux/architecture/guide/01_what_is_architecture.md)  
- [核心工程问题](./linux/architecture/guide/02_core_engineering_problems.md)  
- [架构vs实现](./linux/architecture/guide/03_architecture_vs_implementation.md)  
- [为什么C/C++需要架构](./linux/architecture/guide/04_why_c_cpp_needs_architecture.md)  
- [没有架构的失败](./linux/architecture/guide/05_failure_without_architecture.md)  
- [架构交付物](./linux/architecture/guide/06_architecture_deliverables.md)  
- [何时需要架构](./linux/architecture/guide/07_when_architecture_needed.md)  
- [思维锚点](./linux/architecture/guide/08_thinking_anchors.md)

#### 整体视图
- [架构动机](./linux/architecture/subsystems/whole/01_why_architectural_motivation.md)  
- [架构原则](./linux/architecture/subsystems/whole/02_how_architectural_principles.md)  
- [具体模式](./linux/architecture/subsystems/whole/03_what_concrete_patterns.md)  
- [源码映射](./linux/architecture/subsystems/whole/04_where_source_code_map.md)  
- [实际项目迁移](./linux/architecture/subsystems/whole/05_transfer_to_real_projects.md)

### 核心子系统

#### 进程管理
- [进程管理动机](./linux/architecture/subsystems/process_manage/01_why_process_management.md)  
- [架构策略](./linux/architecture/subsystems/process_manage/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/process_manage/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/process_manage/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/process_manage/05_transfer_to_real_projects.md)

#### 内存管理
- [内存管理动机](./linux/architecture/subsystems/memory_manage/01_why_memory_management.md)  
- [架构策略](./linux/architecture/subsystems/memory_manage/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/memory_manage/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/memory_manage/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/memory_manage/05_transfer_to_real_projects.md)

#### 虚拟文件系统(VFS)
- [VFS存在理由](./linux/architecture/subsystems/file_system/01_why_filesystem_subsystem.md)  
- [架构策略](./linux/architecture/subsystems/file_system/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/file_system/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/file_system/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/file_system/05_transfer_to_real_projects.md)

##### VFS详细分析
- [VFS inode设计模式 v3.2](./linux/vfs/vfs_inode_design_patterns_v3.2.md)  
- [VFS inode v3.2代码参考](./linux/vfs/vfs_inode_v32_code_reference.md)  
- [VFS内部指南](./linux/vfs/vfs_internals_guide.md)

#### 网络协议栈
- [网络栈存在理由](./linux/architecture/subsystems/network_stack/01_why_network_stack.md)  
- [架构策略](./linux/architecture/subsystems/network_stack/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/network_stack/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/network_stack/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/network_stack/05_transfer_to_real_projects.md)

#### 设备驱动
- [驱动子系统存在理由](./linux/architecture/subsystems/device_driver/01_why_driver_subsystem.md)  
- [架构策略](./linux/architecture/subsystems/device_driver/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/device_driver/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/device_driver/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/device_driver/05_transfer_to_real_projects.md)

#### 虚拟化
- [虚拟化存在理由](./linux/architecture/subsystems/virtulization/01_why_virtualization.md)  
- [架构策略](./linux/architecture/subsystems/virtulization/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/virtulization/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/virtulization/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/virtulization/05_transfer_to_real_projects.md)

#### 安全子系统
- [安全子系统存在理由](./linux/architecture/subsystems/security/01_why_security_subsystem.md)  
- [架构策略](./linux/architecture/subsystems/security/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/security/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/security/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/security/05_transfer_to_real_projects.md)

#### 跟踪子系统
- [跟踪存在理由](./linux/architecture/subsystems/tracing/01_why_tracing_exists.md)  
- [架构策略](./linux/architecture/subsystems/tracing/02_how_architectural_strategy.md)  
- [具体架构](./linux/architecture/subsystems/tracing/03_what_concrete_architecture.md)  
- [源码映射](./linux/architecture/subsystems/tracing/04_where_source_code_map.md)  
- [实际项目应用](./linux/architecture/subsystems/tracing/05_transfer_to_real_projects.md)

### 网络子系统详解
- [工程动机](./linux/net/3w1h/01_why_engineering_motivation.md)  
- [设计原则](./linux/net/3w1h/02_how_design_principles.md)  
- [架构模式](./linux/net/3w1h/03_what_architecture_patterns.md)  
- [源码位置](./linux/net/3w1h/04_where_source_code.md)  
- [反思与迁移](./linux/net/3w1h/05_reflection_and_migration.md)

### 有限状态机案例研究

#### TCP状态机
> Linux 3.2内核版本: `git clone --depth 1 --branch v3.2 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git`

- [状态机目的和问题陈述](./linux/tcp_fsm/01_fsm_purpose_problem_statement.md)  
- [状态表示](./linux/tcp_fsm/02_state_representation.md)  
- [事件输入](./linux/tcp_fsm/03_events_inputs.md)  
- [转换逻辑](./linux/tcp_fsm/04_transition_logic.md)  
- [状态机并发](./linux/tcp_fsm/05_fsm_concurrency.md)  
- [错误处理和恢复](./linux/tcp_fsm/06_error_handling_recovery.md)  
- [设计模式](./linux/tcp_fsm/07_design_patterns.md)  
- [工程权衡](./linux/tcp_fsm/08_engineering_tradeoffs.md)  
- [用户空间要点](./linux/tcp_fsm/09_userspace_takeaways.md)  
- [实战重构](./linux/tcp_fsm/10_hands_on_reconstruction.md)

#### Wireshark状态机
> Wireshark源码: `git clone --depth 1 https://gitlab.com/wireshark/wireshark.git`

- [README](./wireshark/fsm/README.md)  
- [为什么需要状态机](./wireshark/fsm/01-why-fsm.md)  
- [状态机边界](./wireshark/fsm/02-fsm-boundaries.md)  
- [状态表示](./wireshark/fsm/03-state-representation.md)  
- [事件转换](./wireshark/fsm/04-events-transitions.md)  
- [错误处理](./wireshark/fsm/05-error-handling.md)  
- [性能分析](./wireshark/fsm/06-performance.md)  
- [设计模式](./wireshark/fsm/07-patterns.md)  
- [比较分析](./wireshark/fsm/08-comparative.md)  
- [设计经验](./wireshark/fsm/09-design-lessons.md)  
- [实战练习](./wireshark/fsm/10-hands-on-exercise.md)

#### ngtcp2 QUIC状态机
> ngtcp2源码: `git clone --depth 1 https://github.com/ngtcp2/ngtcp2.git`

- [状态机在QUIC中的角色](./ngtcp2/fsm/01-fsm-role-in-quic.md)  
- [状态建模](./ngtcp2/fsm/02-state-modeling.md)  
- [事件和驱动器](./ngtcp2/fsm/03-events-and-drivers.md)  
- [转换实现](./ngtcp2/fsm/04-transition-implementation.md)  
- [状态机异步设计](./ngtcp2/fsm/05-fsm-async-design.md)  
- [错误状态拆除](./ngtcp2/fsm/06-error-states-teardown.md)  
- [设计模式](./ngtcp2/fsm/07-design-patterns.md)  
- [与TCP比较](./ngtcp2/fsm/08-comparison-with-tcp.md)  
- [用户空间要点](./ngtcp2/fsm/09-user-space-takeaways.md)  
- [实战设计任务](./ngtcp2/fsm/10-hands-on-design-task.md)  
- [README](./ngtcp2/fsm/README.md)

---

## 设备驱动模型

### RCU机制
> RCU 不是"内核专属 API"，而是一种 "读者无锁、写者等待、以时间换并发" 的设计思想
- [Userspace RCU](https://liburcu.org/)  

### 内核框架文档
- [Linux 3.2内核源码架构](./linux3_2内核源码架构.md)  
- [Linux 字符设备驱动框架](./Linux字符设备驱动框架.md)  
- [Linux 内核如何根据 `/dev/xxx` 找到对应的设备驱动](./linux_chardev_lookup.md)  
- [Linux 文件系统框架](./linux_filesystem_framework.md)  
- [Linux 内存管理框架](./linux_memory_management.md)  
- [Linux 进程调度框架](./linux_process_scheduler.md)  
- [Linux 网络子系统框架](./linux_network_subsystem.md)  
- [Linux 进程间通信框架](./linux_ipc_framework.md)  
- [Linux 内核链表](./linux_kernel_list.md)  
- [Linux 系统调用实现机制](./linux_syscall_mechanism.md)  
- [Linux 启动流程](./linux_boot_process.md)  

### 系统调用追踪
- [Linux sys_read 系统调用执行路径](./linux_sys_read_trace.md)  
- [Linux sys_getpid 系统调用执行路径](./linux_sys_getpid_trace.md)

### 详细架构分析
- [Linux 3.2 内存管理架构](./linux_memory_architecture.md)  
- [Linux 3.2 进程间通信(IPC)机制详解](./linux_ipc_mechanisms.md)  
- [Linux 启动流程与初始化系统](./Linux_Boot_Process_and_Init_Systems.md)  
- [Linux net_device 和 TCP/IP 架构](./linux/linux_netdev_tcp_architecture.md)  
- [Linux VFS 架构](./linux/linux_vfs_architecture.md)  
- [Linux RCU 架构](./linux/linux_rcu_architecture.md)  
- [Linux intrusive list 设计模式](./linux/linux_intrusive_list_pattern.md)  
- [Linux 设备驱动模型架构](./linux/linux_device_model_architecture.md)  
- [Linux 内核模块工作原理](./linux/linux_kernel_module_anatomy.md)

### 设计模式
- [Linux OPS 设计模式](./linux/linux_manual_polymorphism_ops_pattern.md)

#### 代码示例
[C OOP示例](https://gitee.com/event-os/elab/tree/master/lesson/ooc/lesson_07_ooc_pwm_driver)  
```
app layer:
    app_led.c
    app_pwm.c

device layer:
    eio_pin.h
    eio_pwm.h
    eio_pin.c
    eio_pwm.c

driver layer:
    driver_pin.c
    driver_pwm.c
```

[C OOP设备抽象示例](https://gitee.com/event-os/elab/tree/master/lesson/ooc/lesson_08_ooc_device/user)  
```
# 设备基类抽象
eio_object.h
eio_object.c
```

### VFS详细分析
- [VFS存在理由](./linux/vfs/vfs_01_why_vfs_exists.md)  
- [核心对象](./linux/vfs/vfs_02_core_objects.md)  
- [多态实现](./linux/vfs/vfs_03_polymorphism.md)  
- [PDD映射](./linux/vfs/vfs_04_pdd_mapping.md)  
- [注册机制](./linux/vfs/vfs_05_registration.md)  
- [生命周期](./linux/vfs/vfs_06_lifecycle.md)  
- [并发处理](./linux/vfs/vfs_07_concurrency.md)  
- [设计模式](./linux/vfs/vfs_08_patterns.md)  
- [练习](./linux/vfs/vfs_09_exercise.md)  
- [反模式](./linux/vfs/vfs_10_antipatterns.md)

### 网络子系统详解
- [网络存在理由](./linux/net/net_01_why_net_exists.md)  
- [核心对象](./linux/net/net_02_core_objects.md)  
- [skbuff机制](./linux/net/net_03_skbuff.md)  
- [多态实现](./linux/net/net_04_polymorphism.md)  
- [分层架构](./linux/net/net_05_layering.md)  
- [注册机制](./linux/net/net_06_registration.md)  
- [生命周期](./linux/net/net_07_lifecycle.md)  
- [性能优化](./linux/net/net_08_performance.md)  
- [设计模式](./linux/net/net_09_patterns.md)  
- [练习](./linux/net/net_10_exercise.md)

### 核心组件分析
- [Linux 块层](./linux/linux_block_layer.md)  
- [Linux 调试基础设施](./linux/linux_debug_infrastructure.md)  
- [Linux 延迟执行](./linux/linux_deferred_execution.md)  
- [Linux 错误处理](./linux/linux_error_handling.md)  
- [Linux IDR Radix](./linux/linux_idr_radix.md)  
- [Linux kobject sysfs](./linux/linux_kobject_sysfs.md)  
- [Linux 分层架构](./linux/linux_layered_architecture.md)  
- [Linux 锁排序](./linux/linux_lock_ordering.md)  
- [Linux 模块加载器](./linux/linux_module_loader.md)  
- [Linux 页缓存](./linux/linux_page_cache.md)  
- [Linux 红黑树](./linux/linux_rbtree.md)  
- [Linux 调度器核心](./linux/linux_scheduler_core.md)  
- [Linux slab分配器](./linux/linux_slab_allocator.md)  
- [Linux 等待队列](./linux/linux_wait_queues.md)  
- [Linux 零拷贝IO](./linux/linux_zerocopy_io.md)

### 原子操作
- [原子操作](./linux/linux_atomic_operations.md)

### 设计思想
- [Linux ops tables多态](./linux/linux_ops_tables_polymorphism.md)  
- [Linux 上下文感知编程](./linux/linux_context_aware_programming.md)  
- [Linux 嵌入式状态机](./linux/linux_embedded_state_machines.md)  
- [Linux 失败优先设计](./linux/linux_failure_first_design.md)  
- [Linux 快慢路径](./linux/linux_fast_slow_path.md)  
- [Linux 策略机制分离](./linux/linux_policy_mechanism_separation.md)  
- [Linux 零成本抽象](./linux/linux_zero_cost_abstractions.md)  
- [Linux 控制反转](./linux/linux_inversion_of_control.md)  
- [Linux 所有权生命周期](./linux/linux_ownership_lifetime.md)  
- [Linux 引用计数RCU模式](./linux/linux_refcount_rcu_pattern.md)

### 深度分析
- [PCI子系统](./linux/deepdive/linux_pci_subsystem_deep_dive_v3.2.md)  
- [inode->i_fop 决定了VFS的 file_operations](./linux/deepdive/linux_inode_fop_binding_deep_dive.md)  
- [块设备驱动](./linux/deepdive/linux_block_device_driver_deep_dive_v3.2.md)  
- [设备模型](./linux/deepdive/linux_device_model_kobject_sysfs_deep_dive_v3.2.md)  
- [网络子系统](./linux/deepdive/linux_network_subsystem_deep_dive_v3.2.md)  
- [字符设备驱动](./linux/deepdive/linux_character_device_driver_deep_dive_v3.2.md)  
- [字符设备](./linux/deepdive/linux_chardev_deep_dive_v3.2.md)  
- [字符设备驱动](./linux/deepdive/linux_char_device_driver_deep_dive_v3.2.md)  
- [VFS](./linux/deepdive/linux_vfs_deep_dive_v3.2.md)  
- [进程管理](./linux/deepdive/linux_process_management_deep_dive_v3.2.md)  
- [进程调度](./linux/deepdive/linux_scheduler_deep_dive_v3.2.md)  
- [RCU机制](./linux/linux_rcu_deep_dive_v3.2.md)

### 架构图解
- [Linux 分层架构交互](./linux/linux_layered_architecture_boundaries.md)  
- [C架构边界和契约](./linux/c_architecture_boundaries_contracts.md)  
- [Linux 内核边界和契约分析](./linux/linux_kernel_boundaries_contracts_analysis.md)  
- [Linux ops pattern详解](./linux/linux_ops_pattern_deep_dive.md)  
- [Linux 面向对象设计模式](./linux/linux_kernel_oo_patterns.md)  
- [Linux FSM pattern](./linux/linux_kernel_fsm_patterns.md)  
- [UART子系统](./linux/linux_uart_tty_subsystem_architecture.md)  
- [SPI子系统](./linux/linux_spi_subsystem_architecture.md)  
- [I2C子系统](./linux/linux_i2c_subsystem_architecture.md)  
- [GPIO子系统](./linux/linux_gpio_subsystem_architecture.md)

---

## 核心概念详解

### Memory Page
```
memory page:
A memory page is the basic fixed-size unit of memory management used by the OS and CPU.
操作系统把内存按固定大小切成一块一块的小单元，每一块就叫 page（页）

在 x86/Linux 上一般是 4KB 一页
Memory management uses pages for:
    Virtual → physical address mapping (page tables).
    Protection (read/write/execute permissions per page).
    Swapping/paging to disk (move whole pages in/out).
    Caching address translations in the TLB (Translation Lookaside Buffer), one entry per page.
    虚拟内存管理的基本单位：虚拟地址空间按页映射到物理内存。
    换页（paging）、缺页中断（page fault）、内存保护等都以"页"为粒度。
    CPU 的 TLB（Translation Lookaside Buffer）中，每一项通常缓存"一页的地址映射"。
内存不是按字节、也不是按数组，而是按"4KB 一块"来管理的，这一块就是 page

Huge page
定义：Huge page 就是 比普通页大很多的页，例如：
    普通页：4KB
    Huge page：2MB（常见）或 1GB 等。

为什么要有 huge page：
如果一个进程要用很多内存，用 4KB 页会有成千上万甚至更多的页：
    页表很大（page table entries 很多）。
    TLB 每一项只覆盖 4KB，容易 TLB miss。
换成 huge page：
    一个页表项覆盖的地址范围更大 → 页表更小。
    同样数量的 TLB 项能覆盖更多内存 → 减少 TLB miss，降低地址翻译开销。

适用场景：
    大内存、高吞吐、访问模式比较顺序或稳定的场景：
        数据库（如 Oracle、PostgreSQL）。
        大缓存（内存 KV 存储、内存池）。
        高性能网络包处理（DPDK、VPP 等）。

使用方式：
显式 huge page：应用自己通过 hugetlbfs 或 mmap(MAP_HUGETLB, ...) 申请，需要事先在系统里预留 huge pages。
透明大页（THP, Transparent Huge Pages）：内核自动尝试把合适的内存区域合并成 huge page，对应用透明。
```

### I/O设备概念
```
I/O设备 = 一个能与CPU交换数据的接口/控制器
就是 "几组约定好功能的线"（寄存器），通过握手信号从线上读/写数据
给寄存器"赋予"一个内存地址(Address Decoder)，CPU可以直接使用指令(in/out / MMIO)和设备交换数据

+---------------------------------------------------------------------+                                    
|                +-----------+    +---------------+     +-----------+ |                                    
|   Registers    |  Status   |    |    Command    |     |  Data     | |   Interface                        
|                +-----------+    +---------------+     +-----------+ |                                    
|                                                                     |                                    
| ------------------------------------------------------------------- |                                    
|                                                                     |                                    
|   Micro-controller(CPU)                                             |   Internals                        
|   Memory (DRAM or SRAM or Both)                                     |                                    
|   Other Hardware-specific Chips                                     |                                    
+---------------------------------------------------------------------+        
```

### Memory Barriers
> 内存屏障是一种同步原语，主要用于多处理器和多线程环境中对内存访问的顺序进行控制。内存屏障可以确保特定的内存操作按照预期的顺序执行，防止编译器和处理器对指令进行不符合预期的优化和重排序

- [Linux 内核内存屏障原理](http://www.shiyu.xn--6qq986b3xl/docs/ShiYu-AI/classify005/note069.html)  
- [LINUX KERNEL MEMORY BARRIERS](https://www.kernel.org/doc/Documentation/memory-barriers.txt)

### Page Cache
> Page Cache（页面缓存）​​ 是Linux内核用于缓存磁盘文件数据的内存区域，旨在通过减少磁盘I/O次数来提升系统性能。当应用程序访问文件时，数据首先被加载到Page Cache中，后续的读写操作可直接在内存中完成，从而避免频繁访问低速磁盘

- [The Page Cache and Page Writeback](https://github.com/firmianay/Life-long-Learner/blob/master/linux-kernel-development/chapter-16.md)  
- [Essential Page Cache theory](https://biriukov.dev/docs/page-cache/2-essential-page-cache-theory/)

### Udev设备管理
```
Hot-plugging
  Hot-plugging (which is the word used to describe the process of inserting devices into a running system) is achieved in a Linux distribution by a combination of three components: Udev, HAL, and Dbus.

Udev
  Udev is a userspace daemon, that supplies a dynamic device directory containing only the nodes for devices which are connected to the system. It creates or removes the device node files in the /dev directory as they are plugged in or taken out. Dbus is like a system bus which is used for inter-process communication. The HAL gets information from the Udev service, when a device is attached to the system and it creates an XML representation of that device. It then notifies the corresponding desktop application like Nautilus through the Dbus and Nautilus will open the mounted device files.

Dbus
  Dbus is an IPC mechanism, which allows applications to register for system device events.

how Udev mechanism works
  Udev depends on the sysfs file system which was introduced in the 2.5 kernel. It is sysfs which makes devices visible in user space. When a device is added or removed, kernel events are produced which will notify Udev in userspace. Udev directly listens to Netlink socket to know about device state change events (kernel uevents).

  +------------+                        +--------------+                        
  |            |   Kernel netlink msg   |              |                        
  |            |   indicates device has |              |                        
  |            |   been plugged or ...  |              |                        
  |            |<-----------------------|              |                        
  |            |          1             |              |                        
  |            |                        |              |                        
  |   Udev     |                        |    Kernel    |                        
  |            |                        |              |                        
  |            |                        |              |                        
  |            |  2   +----------+  3   |              |                        
  |            |----->| modprobe |----->|              |                        
  |            |      +----------+      |              |                        
  |            |                        |              |                        
  +------------+                        +--------------+                        
 Udev invokes modprobe                  Modprobe loads module                   
 with the module alias                  with alias mapping from                 
 containing vendor ID                   modules alias.                          
 and device ID.                                                                 
```

- [Linux设备模型(3)_Uevent](http://www.wowotech.net/device_model/uevent.html/)  
- [udev(7)](https://www.mankier.com/7/udev)  
- [Udev: Introduction to Device Management In Modern Linux System](https://www.linux.com/news/udev-introduction-device-management-modern-linux-system/)  
- [Writing udev rules](https://www.reactivated.net/writing_udev_rules.html)

### Loop Device
> ​Loop Device​​ 是一种虚拟块设备，允许将普通文件（如 ISO 镜像、磁盘镜像）作为块设备挂载使用, /dev/loop-control 是 Linux 内核提供的​​动态管理 Loop Device 的字符设备接口​​，主要用于按需分配和释放 Loop 设备号, 用户程序通过 ioctl 系统调用与其交互

- [Linux loop devices](https://blog.devops.dev/linux-loop-devices-451002bf69d9)  
- [loop(4)](https://www.mankier.com/4/loop)  
- [losetup(8)](https://www.mankier.com/8/losetup)  
- [Access Control Lists](https://wiki.archlinux.org/title/Access_Control_Lists)

### 文件系统接口

#### sysfs
> The sysfs filesystem is a pseudo-filesystem which provides an interface to kernel data structures. (More precisely, the files and directories in sysfs provide a view of the kobject structures defined internally within the kernel.) The files under sysfs provide information about devices, kernel modules, filesystems, and other kernel components.

- [sysfs(5)](https://www.mankier.com/5/sysfs)  
- [sysfs](https://www.kernel.org/doc/Documentation/filesystems/sysfs.txt)  
- [sysfs - _The_ filesystem for exporting kernel objects](https://docs.kernel.org/filesystems/sysfs.html)  
- [A complete guide to sysfs — Part 1: introduction to kobject](https://medium.com/@emanuele.santini.88/sysfs-in-linux-kernel-a-complete-guide-part-1-c3629470fc84)  
- [A complete guide to sysfs — Part 2: improving the attributes](https://medium.com/@emanuele.santini.88/a-complete-guide-to-sysfs-part-2-improving-the-attributes-1dbc1fca9b75)

#### procfs
> The directory /proc contains (among other things) one subdirectory for each process running on the system, which is named after the process ID (PID).

- [The /proc Filesystem](https://docs.kernel.org/filesystems/proc.html)  
- [proc(5)](https://www.mankier.com/5/proc)

#### debugfs
- [How To Use debugfs](https://linuxlink.timesys.com/docs/wiki/engineering/HOWTO_Use_debugfs)  
- [DebugFS](https://docs.kernel.org/filesystems/debugfs.html)  
- [Debugfs kernel debugging](https://developer.ridgerun.com/wiki/index.php/Debugfs_kernel_debugging)

---

## 网络编程

### Socket编程
- [GNU C Sockets](https://www.gnu.org/software/libc/manual/html_node/Sockets.html)  
- [How Linux creates sockets and counts them](https://ops.tips/blog/how-linux-creates-sockets/)  
- [Implementing a TCP server in C](https://ops.tips/blog/a-tcp-server-in-c/)  
- [Using C to inspect Linux syscalls](https://ops.tips/gists/using-c-to-inspect-linux-syscalls/)  
- [Write yourself an strace in 70 lines of code](https://blog.nelhage.com/2010/08/write-yourself-an-strace-in-70-lines-of-code/)  
- [Dmesg under the hood](https://ops.tips/blog/dmesg-under-the-hood/)  
- [booting a fresh linux kernel on qemu](https://ops.tips/notes/booting-linux-on-qemu/)

### Netlink编程
- [netlink(7)](https://www.mankier.com/7/netlink)  
- [**Netlink Library (libnl)**](https://www.infradead.org/~tgr/libnl/doc/core.html)  
- [Netlink Handbook](https://docs.kernel.org/userspace-api/netlink/index.html)  
- [rfc3549](https://datatracker.ietf.org/doc/html/rfc3549)  
- [Linux, Netlink, and Go - Part 1: netlink](https://mdlayher.com/blog/linux-netlink-and-go-part-1-netlink/)  
- [Implementing a New Custom Netlink Family Protocol](https://insujang.github.io/2019-02-07/implementing-a-new-custom-netlink-family-protocol/)  
- [Introduction to Generic Netlink](https://www.yaroslavps.com/weblog/genl-intro/)  
- [generic_netlink_howto](https://wiki.linuxfoundation.org/networking/generic_netlink_howto)  
- [Monitoring Linux networking state using netlink](https://olegkutkov.me/2018/02/14/monitoring-linux-networking-state-using-netlink/)

---

## 系统编程

### Namespaces
- [Digging into Linux namespaces - part 1](https://blog.quarkslab.com/digging-into-linux-namespaces-part-1.html)  
- [Digging into Linux namespaces - part 2](https://blog.quarkslab.com/digging-into-linux-namespaces-part-2.html)  
- [A deep dive into Linux namespaces](https://ifeanyi.co/posts/)  
- [Understanding Mounts and Mount Namespaces in Linux](https://influentcoder.com/posts/linux-mount/)

### Dynamic Linking
- [THE INSIDE STORY ON SHARED LIBRARIES AND DYNAMIC LOADING](https://cseweb.ucsd.edu/~gbournou/CSE131/the_inside_story_on_shared_libraries_and_dynamic_loading.pdf)  
- [A look at dynamic linking](https://lwn.net/Articles/961117/)  
- [Shared Libraries: Understanding Dynamic Loading](https://amir.rachum.com/shared-libraries/)  
- [Dynamic linking on Linux (x86_64)](https://blog.memzero.de/dynamic-linking-linux-x86-64/)  
- [Dynamic Linking](https://refspecs.linuxbase.org/elf/gabi4+/ch5.dynamic.html)  
- [LD_PRELOAD is super fun. And easy!](https://jvns.ca/blog/2014/11/27/ld-preload-is-super-fun-and-easy/)  
- [A ToC of the 20 part linker essay](https://lwn.net/Articles/276782/)  
- [Playing with LD_PRELOAD](https://axcheron.github.io/playing-with-ld_preload/)  
- [Abusing LD_PRELOAD for fun and profit](https://www.sweharris.org/post/2017-03-05-ld-preload/)  
- [A Simple LD_PRELOAD Tutorial](https://catonmat.net/simple-ld-preload-tutorial)

---

## 代码示例

### Linux设备驱动模型代码示例

核心概念说明：
```c
// 核心概念
kobject
    kobject 是 Linux 内核用于管理和表示对象的通用机制，具有以下特征
        统一的生命周期管理（引用计数
        层级结构（可以有父子
        支持事件通知（用于 udev
        可自动映射到 /sys 文件系统
kset
    一组 kobject 的集合，表示子系统或设备类
    /**
    * struct kset - a set of kobjects of a specific type, belonging to a specific subsystem.
    *
    * A kset defines a group of kobjects.  They can be individually
    * different "types" but overall these kobjects all want to be grouped
    * together and operated on in the same manner.  ksets are used to
    * define the attribute callbacks and other common events that happen to
    * a kobject.
    */
device
    表示系统中的一个物理或虚拟设备，内部嵌套了 kobject
    /**
    * At the lowest level, every device in a Linux system is represented by an
    * instance of struct device. The device structure contains the information
    * that the device model core needs to model the system. Most subsystems,
    * however, track additional information about the devices they host. As a
    * result, it is rare for devices to be represented by bare device structures;
    * instead, that structure, like kobject structures, is usually embedded within
    * a higher-level representation of the device.
    */
device_driver
    表示驱动程序，也含有 kobject
    /**
    * The device driver-model tracks all of the drivers known to the system.
    * The main reason for this tracking is to enable the driver core to match
    * up drivers with new devices. Once drivers are known objects within the
    * system, however, a number of other things become possible. Device drivers
    * can export information and configuration variables that are independent
    * of any specific device.
    */
bus_type
    表示一种总线类型（如 PCI、USB）
    /**
    * A bus is a channel between the processor and one or more devices. For the
    * purposes of the device model, all devices are connected via a bus, even if
    * it is an internal, virtual, "platform" bus. Buses can plug into each other.
    * A USB controller is usually a PCI device, for example. The device model
    * represents the actual connections between buses and the devices they control.
    * A bus is represented by the bus_type structure. It contains the name, the
    * default attributes, the bus' methods, PM operations, and the driver core's
    * private data.
    */
class
    逻辑设备类别（如 block、net、input）
    /**
    * A class is a higher-level view of a device that abstracts out low-level
    * implementation details. Drivers may see a SCSI disk or an ATA disk, but,
    * at the class level, they are all simply disks. Classes allow user space
    * to work with devices based on what they do, rather than how they are
    * connected or how they work.
    */
sysfs
    /sys 中呈现设备模型的接口，由 kobject 驱动
```

数据结构定义：
```c
struct kobject {
    const char      *name;
    struct list_head entry;
    struct kobject  *parent;
    struct kset     *kset;
    struct kobj_type *ktype;
    struct sysfs_dirent *sd;
    struct kref     kref;     // 引用计数
};

struct device {
    struct kobject kobj;
    struct device  *parent;
    struct device_driver *driver;
    struct bus_type *bus;
    void *platform_data;
    ...
};

struct device_driver {
    struct kobject kobj;
    struct bus_type *bus;
    const char *name;
    ...
};

struct bus_type {
    struct kobject kobj;
    struct kset subsys;
    struct kset drivers;
    struct kset devices;
    ...
};

struct kset {
	struct list_head list;
	spinlock_t list_lock;
	struct kobject kobj;
	const struct kset_uevent_ops *uevent_ops;
} __randomize_layout;

struct class {
	const char		*name;
	struct module		*owner;

	const struct attribute_group	**class_groups;
	const struct attribute_group	**dev_groups;
	struct kobject			*dev_kobj;

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
	char *(*devnode)(struct device *dev, umode_t *mode);

	void (*class_release)(struct class *class);
	void (*dev_release)(struct device *dev);

	int (*shutdown_pre)(struct device *dev);

	const struct kobj_ns_type_operations *ns_type;
	const void *(*namespace)(struct device *dev);

	void (*get_ownership)(struct device *dev, kuid_t *uid, kgid_t *gid);

	const struct dev_pm_ops *pm;

	struct subsys_private *p;
};
```

### Flash Cards学习卡片
- [块设备驱动](./linux/flashcards/blkdev_driver_flashcards.md)  
- [字符设备驱动](./linux/flashcards/chardev_kernel_flashcards.md)  
- [设备模型](./linux/flashcards/device_model_flashcards.md)  
- [file结构及相关操作](./linux/flashcards/file_structures_flashcards.md)  
- [文件系统](./linux/flashcards/filesystem_kernel_flashcards.md)  
- [中断和异常](./linux/flashcards/interrupt_exception_flashcards.md)  
- [IPC(进程间通信)](./linux/flashcards/ipc_kernel_flashcards.md)  
- [链表操作](./linux/flashcards/linked_list_flashcards.md)  
- [内存管理](./linux/flashcards/memory_mgmt_flashcards.md)  
- [网络设备驱动](./linux/flashcards/netdev_driver_flashcards.md)  
- [Netfilter](./linux/flashcards/netfilter_kernel_flashcards.md)  
- [Netlink](./linux/flashcards/netlink_kernel_flashcards.md)  
- [进程管理](./linux/flashcards/process_mgmt_flashcards.md)  
- [skb相关操作](./linux/flashcards/skb_kernel_flashcards.md)  
- [socket相关操作](./linux/flashcards/socket_kernel_flashcards.md)  
- [linux启动过程](./linux/flashcards/startup_kernel_flashcards.md)  
- [系统调用](./linux/flashcards/syscall_kernel_flashcards.md)  
- [TCP/IP网络处理](./linux/flashcards/tcpip_kernel_flashcards.md)  
- [虚拟内存和物理内存](./linux/patterns/vm_pm_flashcards.md)  
- [时间和timer](./linux/flashcards/timer_flashcards.md)  
- [DMA](./linux/flashcards/dma_flashcards.md)

### 更多Flash Cards
- [Linux 进程管理](./linux/flashcards/process-management-flashcards.md)  
- [Linux 内存管理](./linux/flashcards/memory-management-flashcards.md)  
- [linux device driver framework](./linux/flashcards/device_driver_framework_flashcards.md)  
- [linux interrupt handling](./linux/flashcards/interrupt_handling_flashcards.md)  
- [linux kernel module](./linux/flashcards/kernel_module_flashcards.md)  
- [linux kernel sync](./linux/flashcards/kernel_sync_flashcards.md)  
- [linux memory management](./linux/flashcards/memory_management_flashcards.md)  
- [linux network stack](./linux/flashcards/network_stack_flashcards.md)  
- [linux procfs sysfs](./linux/flashcards/procfs_sysfs_flashcards.md)  
- [linux timer subsystem](./linux/flashcards/timer_subsystem_flashcards.md)  
- [linux vfs](./linux/flashcards/vfs_flashcards.md)  
- [linux virtualization](./linux/flashcards/virtualization_flashcards.md)

### 驱动示例
- [Linux NIC driver virtio_net code walkthrough](./linux/virtio_net_driver_walkthrough.md)  
- [SKB及相关操作](./linux/skb_internals_deep_dive.md)

### 输入设备处理
```bash
morrism@localhost ~ $ ls /dev/input/
by-id  by-path  event0  event1  event2  event3  event4  event5  event6  js0  mice  mouse0  mouse1  mouse2  mouse3

morrism@localhost ~ $ cat /proc/bus/input/devices
I: Bus=0019 Vendor=0000 Product=0001 Version=0000
N: Name="Power Button"
P: Phys=LNXPWRBN/button/input0
S: Sysfs=/devices/LNXSYSTM:00/LNXPWRBN:00/input/input0
U: Uniq=
H: Handlers=kbd event0
B: PROP=0
B: EV=3
B: KEY=10000000000000 0
......

struct input_value {
    __u16 type;		// type of value (EV_KEY, EV_ABS, etc)
    __u16 code;		// the value code
    __s32 value;	// the value
};
```

---

## 学习资源

### 综合资源
- [**linux_kernel_wiki**](https://github.com/0voice/linux_kernel_wiki)  
- [Linux Device Driver Tutorials](https://github.com/Embetronicx/Tutorials/tree/master/Linux/Device_Driver)  
- [Linux Kernel Development Second Edition](https://litux.nl/mirror/kerneldevelopment/0672327201/toc.html)

### 进程和内存管理
- [Launching Linux threads and processes with clone](https://eli.thegreenplace.net/2018/launching-linux-threads-and-processes-with-clone/)  
- [Measuring context switching and memory overheads for Linux threads](https://eli.thegreenplace.net/2018/measuring-context-switching-and-memory-overheads-for-linux-threads/)  
- [Basics of Futexes](https://eli.thegreenplace.net/2018/basics-of-futexes/)  
- [Using /proc to get a process' current stack trace](https://ops.tips/blog/using-procfs-to-get-process-stack-trace/)  
- [Process resource limits under the hood](https://ops.tips/blog/proc-pid-limits-under-the-hood/)

### 文件系统和存储
- [Demystifying Overlay File Systems](https://influentcoder.com/posts/overlayfs/)

### 网络和系统
- [Unix 终端系统（TTY）是如何工作的](https://waynerv.com/posts/how-tty-system-works/)  
- [浅析 Linux 如何接收网络帧](https://waynerv.com/posts/how-linux-process-input-frames/)  
- [深入理解 netfilter 和 iptables](https://waynerv.com/posts/understanding-netfilter-and-iptables/)  
- [容器技术原理](https://waynerv.com/categories/linux/)

### 操作系统开发
- [Class Notes](https://www.cs.fsu.edu/~baker/realtime/restricted/notes/)  
- [How-to-Make-a-Computer-Operating-System](https://github.com/SamyPesse/How-to-Make-a-Computer-Operating-System/tree/master)  
- [Linux Kernel debugging with QEMU](https://blog.memzero.de/kernel-debugging-qemu/)  
- [**Computer Science from the Bottom Up**](https://www.bottomupcs.com/index.html)

### 系统管理
- [Linux Administration Guide](https://www.baeldung.com/linux/administration-series)  
- [The Linux System Administrator's Guide](https://tldp.org/LDP/sag/html/index.html)

### 标准和规范
- [System V Application Binary Interface](https://refspecs.linuxbase.org/elf/gabi4+/contents.html)  
- [zSeries ELF Application Binary Interface Supplement](https://refspecs.linuxfoundation.org/ELF/zSeries/lzsabi0_zSeries/book1.html)

### 学习指南
- [C Pointers](https://www.c-pointers.com/contents.html)  
- [Data Structures in the Linux Kernel](https://0xax.gitbooks.io/linux-insides/content/DataStructures/linux-datastructures-1.html)  
- [Kernel API](https://linux-kernel-labs.github.io/refs/pull/222/merge/labs/kernel_api.html)  
- [The Linux Kernel API](https://docs.kernel.org/core-api/kernel-api.html)  
- [Manpages](https://man.cx/list_add)  
- [Kernel Data Structures Linkedlist](https://medium.com/@414apache/kernel-data-structures-linkedlist-b13e4f8de4bf)  
- [How does the kernel implements Linked Lists?](https://kernelnewbies.org/FAQ/LinkedLists)  
- [The Linked-List Structure](https://litux.nl/mirror/kerneldevelopment/0672327201/app01lev1sec2.html)

### 编程实例
- [Concurrent Servers: Part 1 - Introduction](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)  
- [Finding out where a function was called from](https://eli.thegreenplace.net/2003/12/26/finding-out-where-a-function-was-called-from)  
- [Understanding fork(), vfork(), exec(), clone(), and more](https://felixcarmona.com/understanding-fork-vfork-exec-clone-processes-in-linux/)

### 设备驱动开发
- [Linux设备驱动开发详解](https://github.com/kevinwangkk/LDD4.0_note/blob/master/Linux%E8%AE%BE%E5%A4%87%E9%A9%B1%E5%8A%A8%E5%BC%80%E5%8F%91%E8%AF%A6%E8%A7%A3%EF%BC%9A%E5%9F%BA%E4%BA%8E%E6%9C%80%E6%96%B0%E7%9A%84Linux4.0%E5%86%85%E6%A0%B8.pdf)  
- [LinuxDrivers](https://sysplay.github.io/books/LinuxDrivers/book/index.html)  
- [Linux Device Drivers Series](https://www.opensourceforu.com/tag/linux-device-drivers-series/page/2/)  
- [Driver Model](https://www.kernel.org/doc/html/latest/driver-api/driver-model/)  
- [Linux Device Driver Tutorials](https://embetronicx.com/linux-device-driver-tutorials/)  
- [DriverPractice](https://github.com/starnight/DriverPractice)

### 驱动分层设计
- [lcd驱动应该怎么写](http://www.wujique.com/2021/05/16/lcd%e9%a9%b1%e5%8a%a8%e5%ba%94%e8%af%a5%e6%80%8e%e4%b9%88%e5%86%99%ef%bc%9f/)  
- [linux驱动子系统](https://www.cnblogs.com/lizhuming/tag/%2Flabel%2Flinux/)  
- [驱动程序分层分离概念](http://wiki.100ask.org/images/c/c1/%E9%B1%BC%E6%A0%91%E7%AC%94%E8%AE%B0%E4%B9%8B%E7%AC%AC14%E8%AF%BE%E9%A9%B1%E5%8A%A8%E7%A8%8B%E5%BA%8F%E5%88%86%E5%B1%82%E5%88%86%E7%A6%BB%E6%A6%82%E5%BF%B5.pdf)

### 具体驱动实现
- [Linux-IIC驱动详解](https://cloud.tencent.com/developer/article/1015834?areaSource=104001.229&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Linux-Nor Flash驱动详解](https://cloud.tencent.com/developer/article/1012379?areaSource=104001.239&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Linux-块设备驱动(详解)](https://cloud.tencent.com/developer/article/1012375?areaSource=104001.240&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Linux-块设备驱动之框架详细分析(详解)](https://cloud.tencent.com/developer/article/1012369?areaSource=104001.242&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Linux-Nand Flash驱动(分析MTD层并制作NAND驱动)](https://cloud.tencent.com/developer/article/1012366?areaSource=104001.243&traceId=x6TfYAuCHtxo8-owGalg1)  
- [linux-LCD层次分析(详解)](https://cloud.tencent.com/developer/article/1012349?areaSource=104001.248&traceId=x6TfYAuCHtxo8-owGalg1)  
- [linux-platform机制实现驱动层分离(详解)](https://cloud.tencent.com/developer/article/1012345?areaSource=104001.251&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Linux-LCD驱动(详解)](https://cloud.tencent.com/developer/article/1012342?areaSource=104001.252&traceId=x6TfYAuCHtxo8-owGalg1)  
- [Llinux-触摸屏驱动(详解)](https://cloud.tencent.com/developer/article/1012333?areaSource=104001.254&traceId=x6TfYAuCHtxo8-owGalg1)  
- [制作第一个驱动程序](https://cloud.tencent.com/developer/article/1012235?areaSource=104001.275&traceId=x6TfYAuCHtxo8-owGalg1)

### 软件架构思想
- [Linux设备驱动的软件架构思想](http://www.mrchen.love/Article/ID/28)  
- [Linux设备驱动中的软件架构思想](https://www.cnblogs.com/wanjianjun777/p/10951912.html)  
- [Linux Device Drivers data structures](https://www.linuxtv.org/downloads/v4l-dvb-internals/device-drivers/index.html)

### I2C子系统专题
- [bootlin: Basics of I2C on linux](https://bootlin.com/pub/conferences/2022/elce/ceresoli-basics-of-i2c-on-linux/ceresoli-basics-of-i2c-on-linux.pdf)  
- [I2C Device Interface](https://www.infradead.org/~mchehab/rst_conversion/i2c/dev-interface.html)  
- [Implementing I2C device drivers](https://docs.kernel.org/i2c/writing-clients.html)  
- [I2C Bus Driver Dummy Linux Device Driver](https://embetronicx.com/tutorials/linux/device-drivers/i2c-bus-driver-dummy-linux-device-driver-using-raspberry-pi/)  
- [I2C overview](https://wiki.st.com/stm32mpu/wiki/I2C_overview)  
- [Linux内核: I2C子系统分析](https://www.cnblogs.com/schips/p/linux-subsystem-i2c-0-about.html)

### 综合开发指南
- [Driver implementer's API guide](https://www.kernel.org/doc/html/latest/driver-api/)  
- [Buildroot examples](https://github.com/gvvsnrnaveen/buildroot/tree/main)  
- [Linux Kernel Programming by Examples](http://marcocorvi.altervista.org/games/lkpe/)  
- [Linux环境编程：从应用到内核](https://github.com/0voice/linux_kernel_wiki/blob/main/%E7%94%B5%E5%AD%90%E4%B9%A6%E7%B1%8D/Linux%E7%8E%AF%E5%A2%83%E7%BC%96%E7%A8%8B%EF%BC%9A%E4%BB%8E%E5%BA%94%E7%94%A8%E5%88%B0%E5%86%85%E6%A0%B8%20(Linux%20Unix%E6%8A%80%E6%9C%AF%E4%B8%9B%E4%B9%A6).pdf)

### 操作系统内核开发
- [linux-insides](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md)  
- [build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)  
- [Baking Pi – Operating Systems Development](https://www.cl.cam.ac.uk/projects/raspberrypi/tutorials/os/index.html)  
- [Writing a Tiny x86 Bootloader](https://www.joe-bergeron.com/posts/Writing%20a%20Tiny%20x86%20Bootloader/)  
- [linux-insides](https://0xax.gitbooks.io/linux-insides/content/)  
- [CS 372H, Spring 2011: Introduction to Operating Systems: Honors](https://cs.nyu.edu/~mwalfish/classes/ut/s11-cs372h/)  
- [CS372H Operating Systems Lab 1](https://www.cs.utexas.edu/~lorenzo/corsi/cs372h/07S/labs/lab1/lab1.html)  
- [CS372H Spring 2011 Lab 1: PC Bootstrap and GCC Calling Conventions](https://cs.nyu.edu/~mwalfish/classes/ut/s11-cs372h/labs/lab1.html)  
- [COS 318: Operating Systems](https://www.cs.princeton.edu/courses/archive/fall09/cos318/)

### 编程语言参考
- [GNU C Language Manual](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/index.html#SEC_Contents)  
- [Computer Systems A Programmer's Perspective](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/books/CSAPP_2016.pdf)

### 设备驱动API
- [Driver implementer's API guide](https://www.kernel.org/doc/html/next/driver-api/index.html)  
- [Core API Documentation](https://www.kernel.org/doc/html/next/core-api/index.html)  
- [Manpages of linux-manual-4.8](https://manpages.debian.org/testing/linux-manual-4.8/index.html)  
- [Linux Device Drivers](https://linuxtv.org/downloads/v4l-dvb-internals/device-drivers/)  
- [The Linux Kernel API](https://docs.huihoo.com/gnu_linux/kernel-api/index.htm)  
- [The Linux Kernel Module Programming Guide](https://tldp.org/LDP/lkmpg/2.6/html/index.html)  
- [The Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)  
- [Linux Loadable Kernel Module HOWTO](https://tldp.org/HOWTO/Module-HOWTO/index.html)

### 输入子系统
- [聊聊 Linux IO](https://www.0xffffff.org/2017/05/01/41-linux-io/)  
- [Input Documentation](https://www.kernel.org/doc/html/latest/input/index.html)  
- [Input Subsystem](https://www.kernel.org/doc/html/latest/driver-api/input.html)  
- [Creating an input device driver](https://docs.kernel.org/input/input-programming.html)  
- [Input Drivers](http://embeddedlinux.org.cn/essentiallinuxdevicedrivers/final/ch07.html)

### 构建系统
- [Kernel Build System](https://docs.kernel.org/kbuild/index.html)  
- [linux kernel development](https://github.com/firmianay/Life-long-Learner/tree/master/linux-kernel-development)

---

## 实用工具和命令

### 文件访问控制
- [Mastering Linux: 'setfacl' Command Installation Methods](https://ioflood.com/blog/install-setfacl-command-linux/)  
- [Using 'setfacl' | A Linux Command for File Access Control](https://ioflood.com/blog/setfacl-linux-command/)  
- [setfacl(1)](https://www.mankier.com/1/setfacl)  
- [getfacl(1)](https://www.mankier.com/1/getfacl)

> The setfacl command in Linux is used to set file access control lists, allowing you to manage permissions for different users and groups. A basic syntax template of the setfacl command might look like this: setfacl [arguments] [user_or_group_permissions] filename

### 系统文件位置
```bash
# /usr/include/linux/ (linux header files)
morrism@localhost ~ $ grep -rn "LOOP_CTL_" /usr/include/linux/
/usr/include/linux/loop.h:122:#define LOOP_CTL_ADD              0x4C80
/usr/include/linux/loop.h:123:#define LOOP_CTL_REMOVE           0x4C81
/usr/include/linux/loop.h:124:#define LOOP_CTL_GET_FREE 0x4C82

# /etc/protocols
morrism@localhost ~ $ cat /etc/protocols
ip      0       IP              # internet protocol, pseudo protocol number

# /proc/cmdline
morrism@localhost ~ $ cat /proc/cmdline
BOOT_IMAGE=(hd0,msdos1)/vmlinuz-4.18.0-305.25.1.el8_4.x86_64 root=/dev/mapper/cl-root ro resume=/dev/mapper/cl-swap rd.lvm.lv=cl/root rd.lvm.lv=cl/swap rhgb quiet
```

### 常用命令
```bash
# print all login users
print $(who | awk '{print $1}' | sort | uniq | sed 's/ /,/g')
```

---

## 📚 扩展阅读

本文档作为Linux内核学习的综合指南，涵盖了从基础概念到高级架构设计的各个方面。建议根据个人学习目标选择相应的章节进行深入学习。

对于初学者，建议从以下顺序开始：
1. 核心概念详解
2. 嵌入式C面向对象设计  
3. Linux内核架构
4. 设备驱动模型
5. 代码示例实践

对于有经验的开发者，可以直接参考：
- 内核子系统的具体架构文档
- 设计模式和最佳实践
- Flash Cards进行知识巩固
- 深度分析文档

---

*最后更新：2024年*