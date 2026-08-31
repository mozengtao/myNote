# 11. NUMA-Aware Shared Memory

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 12 节。

## 心智模型

- **问题**：多路服务器上，每个 CPU 插槽（socket）通常挂着一部分"本地"
  内存；CPU 访问本地内存比访问挂在别的 CPU 插槽下的"远端"内存慢得多
  （NUMA = Non-Uniform Memory Access，"非统一内存访问"）。原文档的图示
  是：Node0 的 CPU 配一份绑定在 Node0 上的共享内存，Node1 配一份绑定在
  Node1 上的——**原则是 CPU 尽量只访问本地 NUMA 节点的内存，减少
  Remote Memory Access**。
- **核心技巧**：两步绑定
  1. **数据绑定**：用 `numa_alloc_onnode(size, node)` 在指定节点分配
     内存（底层是 `mmap` + `mbind()` 系统调用，把这段虚拟地址的物理页
     锚定在某个节点上）。
  2. **计算绑定**：用 `numa_run_on_node(node)` 把处理这份数据的
     线程/进程也调度到同一个节点的 CPU 上执行，让"计算"紧挨着"数据"。
- 只做第 1 步而不做第 2 步（数据绑在 Node0，计算却跑在 Node1 的 CPU
  上）等于白费功夫——这是 NUMA 优化最容易被忽视的一点：**内存绑定和
  CPU 亲和性必须成对配置**。

## 本机环境与自动降级

本机只有 1 个 NUMA node，且未安装 `libnuma`（`ldconfig -p | grep numa`
为空）。因此本示例用 `#ifdef HAVE_LIBNUMA` 条件编译两条路径：

```c
#ifdef HAVE_LIBNUMA
    void *p = numa_alloc_onnode(size, node);
#else
    void *p = malloc(size);
#endif
```

`Makefile` 会在构建前用一段最小程序试探性编译 + 链接 `-lnuma`：

```makefile
HAVE_LIBNUMA := $(shell printf '#include <numa.h>\nint main(void){return numa_available();}\n' | \
	$(CC) -xc - -lnuma -o /tmp/.numa_probe_$$$$ 2>/dev/null && \
	rm -f /tmp/.numa_probe_$$$$ && echo 1 || echo 0)
```

成功则追加 `-DHAVE_LIBNUMA -lnuma`，走真正的 NUMA API；失败（本机情况）
则自动降级为 `malloc`，并在运行时打印明确提示"当前环境未安装
libnuma...自动降级为 malloc + 单节点模式"，把节点数视为 1——不会因为
环境不满足就编译失败或运行报错，这也是用户确认过的"优雅降级"策略，
与 06/07/08 里"无法用真实硬件特性时如何设计一个仍然完整可运行的
demo"的思路一致。

无论走哪条路径，代码结构完全一致：**按节点分配一份内存 → fork 出一个
绑定到该节点的 worker → worker 验证数据**，这正是本示例想传达的心智
模型——NUMA-aware 不改变共享内存本身的读写逻辑，只是在"分配"和
"调度"这两步各加一个"绑定到节点 N"的提示。

## 构建与运行

```bash
make
./run_demo.sh
```

## 预期输出（节选，本机 1 个 NUMA node、未装 libnuma 时）

```text
[Makefile] 未检测到 libnuma，使用 malloc fallback 编译
--- 当前环境 NUMA 拓扑 ---
node0
--------------------------
[numa] 当前环境未安装 libnuma（或编译时未启用），自动降级为 malloc + 单节点模式
[numa] node 0: allocated 16777216 bytes, filled pattern
[numa worker node 0] pattern=OK
[numa] all_nodes_ok=yes
```

如果在装有 `libnuma` 且有多个 NUMA node 的机器上编译运行，`Makefile`
会自动打印"libnuma 可用，已启用 -DHAVE_LIBNUMA"，程序会对检测到的
每一个节点各分配一份内存、各 fork 一个绑定到该节点的 worker 验证，
输出形如 `[numa worker node 0] ...` / `[numa worker node 1] ...`
多行结果。

## 与 10_hugepage 的关系

两者都用了"父进程预加载数据 → fork 出子进程只读验证"的结构（避免
COW 写分裂破坏"共享"语义），区别在于优化的维度不同：HugePage
优化的是"页表项数量 / TLB miss 率"，NUMA 优化的是"内存访问的物理
距离"；生产环境里两者常常一起用——在正确的 NUMA 节点上，用大页分配
一大块共享内存。
