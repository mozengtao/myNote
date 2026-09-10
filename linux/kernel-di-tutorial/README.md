# Kernel Dependency Injection Mental Model 教程

一套系统教程，目标是为熟悉 C 语言和 Linux Kernel、但从未系统梳理过"DI（依赖注入）在内核里长什么样"的
读者，建立一套完整的 mental model。全部使用 ASCII 架构图/时序图/状态图/关系图/生命周期图讲解，
每种手法都配有 3 个**真实可编译、已实测运行**的用户态 C 示例（非伪代码）。

**全部 20 种 DI 手法已完成**（分两批交付：第一批 8 种核心手法 + 第二批 12 种手法），
覆盖从最原子的函数指针回调，到驱动/总线注册、通知链、安全钩子、异步执行、编译期/链接期注册、
调度器、内存分配器、可插拔策略、探测点、受验证的可加载执行体、中断处理各个层级的延后执行机制、
以及 VFS 文件系统注册。

---

## 阅读顺序

1. **[00-mental-model.md](00-mental-model.md)** —— 先读这篇。建立"什么是 DI"、"C 和 Java Spring 的
   DI 有什么本质区别"、"为什么 Kernel 到处都是 DI 却不需要 IoC 容器"、"零成本抽象是怎么做到的"这几个
   最基础的认知框架，后面 20 篇都是这个框架的具体实例化。

2. 20 个手法（建议按编号顺序阅读，后面的篇章会引用前面篇章的概念；序号即文档编号）：

   | # | 文档 | 一句话概括 |
   |---|---|---|
   | 01 | [01-function-pointer-callback.md](01-function-pointer-callback.md) | 最原子的 DI 单元：一个函数指针字段 |
   | 02 | [02-ops-table.md](02-ops-table.md) | 把一组相关回调打包成结构体，等价手写 vtable |
   | 03 | [03-context-injection.md](03-context-injection.md) | 回调如何携带"专属于这次注册"的私有数据（`container_of`） |
   | 04 | [04-driver-registration.md](04-driver-registration.md) | Callback + Ops Table + Context Injection 的完整对象化，加上"匹配"与"热插拔" |
   | 05 | [05-notifier-chain.md](05-notifier-chain.md) | 一对多广播，支持优先级与"短路"语义 |
   | 06 | [06-initcall.md](06-initcall.md) | 把"注册动作本身"也做成零运行时开销（编译期/链接期收集） |
   | 07 | [07-workqueue.md](07-workqueue.md) | Context Injection 在"异步执行、并发调度"场景下的应用 |
   | 08 | [08-security-hooks-lsm.md](08-security-hooks-lsm.md) | Notifier Chain 的"强制化"变体：一票否决式的安全检查点 |
   | 09 | [09-bus-device-driver-injection.md](09-bus-device-driver-injection.md) | 04 篇背后总线无关的通用驱动核心基础设施 |
   | 10 | [10-scheduler-class.md](10-scheduler-class.md) | Ops Table + Notifier Chain 结合体：链式多态调度策略族 |
   | 11 | [11-allocator-injection.md](11-allocator-injection.md) | 内存分配器的 `ctor` 回调注入与编译期可插拔后端（SLAB/SLUB） |
   | 12 | [12-policy-injection.md](12-policy-injection.md) | 08 的"互斥单选"变体：机制固定、策略可运行期整体切换 |
   | 13 | [13-linker-section-registration.md](13-linker-section-registration.md) | 06 之外的链接器 section 用法：按条件匹配执行而非全部执行 |
   | 14 | [14-tracepoint.md](14-tracepoint.md) | 以诊断为目的、对近零开销有极致要求的一对多探测点 |
   | 15 | [15-bpf-program-registration.md](15-bpf-program-registration.md) | 受验证字节码的运行期可加载执行体（本仓库版本不含 eBPF，概念性说明） |
   | 16 | [16-irq-handler.md](16-irq-handler.md) | 硬件中断上下文里的共享中断线链式分发 |
   | 17 | [17-softirq.md](17-softirq.md) | 编译期固定编号、每 CPU 独立 pending 位图的延后执行 |
   | 18 | [18-tasklet.md](18-tasklet.md) | 动态注册但仍在软中断上下文、保证同一 tasklet 不并发的延后执行 |
   | 19 | [19-rcu-callback.md](19-rcu-callback.md) | 延迟到"所有读者经过一次 grace period"才安全执行的回调 |
   | 20 | [20-filesystem-registration.md](20-filesystem-registration.md) | Ops Table + 注册表管理 + Initcall 的综合应用：VFS 文件系统类型 |

3. **[99-comparison-and-panorama.md](99-comparison-and-panorama.md)** —— 最后读这篇。包含：
   - 全部 **20 种** DI 手法的横向比较大表
   - 一张贯穿全部手法的 ASCII 全景关系图
   - 设计思想总结（解耦/可扩展/可测试/多态/插件化/生命周期管理/模块化/ABI 稳定性/性能/
     Cache Friendly/Zero-cost Abstraction）

---

## 每篇 `.md` 的统一结构

01~20 号文档都严格遵循下面的 10 节结构：

1. **Why** —— 问题背景、不用这个手法会怎样、设计目标
2. **Architecture** —— ASCII 架构图
3. **Lifecycle** —— ASCII 生命周期图（alloc -> init -> inject -> dispatch -> callback -> destroy）
4. **Relationship** —— 协作对象关系图
5. **Data Flow** —— 完整调用时序图，并与真实系统调用路径对照
6. **State** —— 运行期状态、哪些字段可变/不可变
7. **Failure** —— 常见错误 + 调试方法（`/proc`、`ftrace`、`lockdep`、`WARN_ON` 等）
8. **Kernel Core Pseudocode**（≥5 个）—— 每个都标注取材自本仓库的真实文件/行号
   （`15-bpf-program-registration.md` 例外：本仓库为 Linux 3.2.0，不含 eBPF 子系统，
   该篇第 8 节明确标注为"概念性伪代码，参考现代内核架构"，不冒充仓库行号引用）
9. **User Space Examples**（3 个，每个 ≥150 行真实可编译 C 代码）—— 附带**实测**编译命令与运行输出
10. **与其它 DI 手法比较** —— 对比表，说明何时用/何时不用

全部图表统一使用纯 ASCII（`+`、`-`、`|`、`v`、`^`、`>`、`<`），不使用 Unicode 制表符/箭头，
保证在任何终端、编辑器、`git diff` 里都能正确对齐显示。

---

## 如何编译和运行示例代码

每个手法对应一个 `c/<NN-topic>/` 目录，包含 3 个 `.c` 示例和一个 `Makefile`：

```bash
cd Documentation/kernel-di-tutorial/c/01-function-pointer-callback
make          # 编译全部 3 个示例
make run      # 编译并依次运行全部 3 个示例，打印真实输出
make clean    # 清理编译产物
```

对每一个手法目录都执行同样的操作即可：

```bash
for d in Documentation/kernel-di-tutorial/c/*/; do
    echo "=== $d ==="
    (cd "$d" && make clean && make run)
done
```

所有示例仅依赖标准 C 库和 `pthread`（部分异步/并发相关示例需要 `-pthread`），
在任意 Linux/gcc 环境下都可以直接编译，不需要内核头文件或特殊工具链。
`.md` 文档里贴出的运行输出都是**真实编译运行后复制**的结果，不是编造的；
全部 60 个示例已在本仓库环境下用 `gcc -Wall -Wextra -O2 -std=gnu11` 逐一验证过零警告编译
+ 成功运行（退出码 0）。

---

## 目录结构

```
Documentation/kernel-di-tutorial/
+-- README.md                          # 本文件
+-- 00-mental-model.md                 # 第一部分：DI 整体 Mental Model
+-- 01-function-pointer-callback.md
+-- 02-ops-table.md
+-- 03-context-injection.md
+-- 04-driver-registration.md
+-- 05-notifier-chain.md
+-- 06-initcall.md
+-- 07-workqueue.md
+-- 08-security-hooks-lsm.md
+-- 09-bus-device-driver-injection.md
+-- 10-scheduler-class.md
+-- 11-allocator-injection.md
+-- 12-policy-injection.md
+-- 13-linker-section-registration.md
+-- 14-tracepoint.md
+-- 15-bpf-program-registration.md
+-- 16-irq-handler.md
+-- 17-softirq.md
+-- 18-tasklet.md
+-- 19-rcu-callback.md
+-- 20-filesystem-registration.md
+-- 99-comparison-and-panorama.md       # 横向比较表 + 全景图 + 设计思想总结
+-- c/
    +-- 01-function-pointer-callback/   # plugin_system / event_loop_reactor / command_dispatcher
    +-- 02-ops-table/                   # vtable_shape_polymorphism / mini_vfs_filesystem / strategy_pattern_ops
    +-- 03-context-injection/           # container_of_demo / worker_thread_pool / timer_context_demo
    +-- 04-driver-registration/         # bus_driver_match_demo / plugin_registry_hotplug / multi_driver_probe
    +-- 05-notifier-chain/              # notifier_chain_impl / pubsub_event_bus / gui_event_broadcast
    +-- 06-initcall/                    # linker_section_ctor_sim / module_registry_levels / self_registering_tests
    +-- 07-workqueue/                   # workqueue_from_scratch / delayed_work_timer / thread_pool_task_queue
    +-- 08-security-hooks-lsm/          # lsm_hook_chain / policy_injection_acl / audit_hook_pipeline
    +-- 09-bus-device-driver-injection/ # generic_bus_core / multi_bus_types_demo / uevent_hotplug_notify
    +-- 10-scheduler-class/             # sched_class_chain / pluggable_scheduler_policies / scheduler_enqueue_dequeue
    +-- 11-allocator-injection/         # kmem_cache_ctor_demo / pluggable_allocator_backend / custom_arena_allocator
    +-- 12-policy-injection/            # io_scheduler_policy / congestion_control_policy / sysctl_tunable_policy_switch
    +-- 13-linker-section-registration/ # boot_param_setup_sim / pci_fixup_table_sim / section_based_plugin_table
    +-- 14-tracepoint/                  # static_tracepoint_sim / tracepoint_multi_probe / tracepoint_enable_disable_runtime
    +-- 15-bpf-program-registration/    # toy_bytecode_verifier / pluggable_prog_type_ops / hook_point_bpf_attach
    +-- 16-irq-handler/                 # irq_action_chain_sim / threaded_irq_demo / irq_free_and_race_demo
    +-- 17-softirq/                     # softirq_fixed_slots / softirq_vs_hardirq_demo / percpu_softirq_pending
    +-- 18-tasklet/                     # tasklet_schedule_sim / tasklet_disable_enable_demo / tasklet_vs_workqueue_compare
    +-- 19-rcu-callback/                # call_rcu_grace_period_sim / rcu_read_lock_replace_demo / rcu_vs_refcount_compare
    +-- 20-filesystem-registration/     # file_system_type_registry / mount_dispatch_demo / multi_fs_module_sim
```

篇幅统计（全部完成后的实际产出）：Markdown 22 个文件约 10600 行，C 示例代码 60 个文件约 12400 行，
20 个 Makefile；全仓库共 103 个文件。

---

## 内核版本基准

本仓库是 Linux 3.2.0（"Saber-toothed Squirrel"）。全部伪代码引用（`15` 号 BPF 篇除外）均取材自
**本仓库实际代码**的真实文件路径和行号，不凭记忆编造。个别子系统在这个版本里尚不存在（如 eBPF，
`15` 号文档已在开篇明确标注并采用概念性说明）或字段/接口与更现代内核有差异（如 `19` 号文档核实过
本仓库 `file_system_type` 已经是 `mount` 接口而非更旧版本的 `get_sb`），均已在对应文档中如实说明版本情况。
