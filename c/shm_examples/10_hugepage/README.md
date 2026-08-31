# 10. Shared Memory + HugePage

对应文档：[../../share_memory_design.md](../../share_memory_design.md) 第 11 节。

## 心智模型

- **问题**：普通页是 4KB，TLB（页表缓存）能覆盖的地址范围有限。当共享
  内存区域很大（几百 MB 的行情缓存、机器学习模型权重、DPDK 风格的
  收发包缓冲区）时，4KB 页会导致大量 TLB miss，每次 miss 都要走一次
  多级页表遍历。用 2MB（x86_64 常见）甚至 1GB 的 HugePage，可以把同样
  大小内存所需的页表项数量减少 512~262144 倍，显著降低 TLB miss 率，
  对大内存高频随机访问场景（哈希表、行情快照）收益明显。
- **两条实现路径**：
  1. **HugeTLB（预留页）**：管理员提前 `sysctl vm.nr_hugepages=N`
     预留一批物理大页，程序用 `mmap(MAP_HUGETLB)` 或挂载 `hugetlbfs`
     后 `shm_open` 从这个池子里拿——确定生效，但需要提前规划内存、
     预留后普通程序不能再用那部分内存。
  2. **Transparent HugePage（THP，本例采用）**：不需要任何预留，内核在
     缺页处理时"尽量"用大页支持某段内存，程序只需要
     `madvise(MADV_HUGEPAGE)` 给一个提示——更灵活，但"尽量"意味着不
     保证一定生效（取决于当时的内存碎片状况、内核策略）。

## 本机环境与踩坑记录

本机只有 `HugePages_Total=0`（未预留 HugeTLB 页），因此路径 1 无法
使用；THP 已开启且模式是 `madvise`，理论上适合路径 2。但第一次实现时
用 `MAP_SHARED | MAP_ANONYMOUS`（是为了让 fork 出的子进程也能共享同一
块内存），结果无论怎么 `madvise(MADV_HUGEPAGE)`，`/proc/self/smaps`
里的 `AnonHugePages` 始终是 0。

排查后发现：**`MAP_SHARED | MAP_ANONYMOUS` 在 Linux 上实际是由
tmpfs/shmem 承载的**，它的大页开关是独立的
`/sys/kernel/mm/transparent_hugepage/shmem_enabled`（本机是 `never`），
完全不受这段 VMA 上 `madvise(MADV_HUGEPAGE)` 提示的影响。真正遵从
per-VMA `madvise` 提示、由 `AnonHugePages` 字段体现效果的，是
`MAP_PRIVATE | MAP_ANONYMOUS` 这种"纯匿名私有页"。

这个限制换来了一个很有价值的心智模型：**预加载一份大的只读数据
（模型权重、词典、行情快照）用 HugePage 承载，然后 `fork()` 出多个
worker 进程**。`fork()` 之后子进程通过 COW（Copy-On-Write）继续读取
同一批物理页，只要子进程不写它，物理内存（以及大页承载）不会被复制——
相当于用"预加载 + fork 只读 workers"变相实现了"多进程共享同一份大页
支持的大内存"。这正是 Unicorn/Gunicorn 的 `preload_app`、Redis
`fork()` 做 BGSAVE、多进程模型推理服务预加载权重后 fork worker 的
标准套路，也是本示例最终选择的实现方式。

## 构建与运行

```bash
make
./run_demo.sh
```

流程：

1. `mmap` 一段 2MB 对齐、64MB（32 个 2MB 大页）的 `MAP_PRIVATE|MAP_ANONYMOUS`
   匿名内存；
2. `madvise(MADV_HUGEPAGE)` 提示内核这段值得用大页；
3. 一次性写满整段数据（预加载，触发缺页，内核借此决定是否用大页支持）；
4. 解析自己的 `/proc/self/smaps`，累加这段映射的 `AnonHugePages`
   字段，判断大页是否生效；
5. `fork()` 出 3 个只读 worker，每个 worker 各自校验数据完整性、
   并各自再检查一次自己继承到的映射的 `AnonHugePages`。

## 预期输出（节选）

```text
--- 当前环境 THP 配置 ---
always [madvise] never
HugePages_Total:       0
Hugepagesize:       2048 kB
-------------------------
[hugepage] mapping 64 MB (32 huge pages worth of alignment)
[hugepage] parent AnonHugePages covering this mapping = 44040192 / 67108864 bytes (65.6%)
[hugepage] SUCCESS: Transparent HugePage 生效，至少部分区域由 2MB 大页支持
[hugepage worker 0] pattern=OK AnonHugePages=44040192 bytes
[hugepage worker 1] pattern=OK AnonHugePages=44040192 bytes
[hugepage worker 2] pattern=OK AnonHugePages=44040192 bytes
[hugepage parent] all_workers_ok=yes
```

- `AnonHugePages` 达到映射总大小的 65.6%（44MB / 64MB，即 32 个大页里
  有 22 个被成功提升为 2MB 大页）：证明 THP 确实生效了，但没有覆盖
  100%——这正好印证了"THP 是尽力而为，不是保证"的心智模型，实际生效
  比例会随当时的内存碎片状况波动。
- 3 个 worker 都能正确校验数据、且各自看到的 `AnonHugePages`
  与父进程完全一致：证明 fork 之后子进程确实继续共享同一批父进程预加载
  好的物理大页，没有发生数据复制或损坏。

## 踩过的另一个坑：fork 与 stdio 缓冲区

调试过程中还发现一个经典陷阱：父进程在 `fork()` 之前如果有 `printf`
还停留在 libc 的缓冲区里没真正 `write()` 出去（标准输出定向到文件/管道
时默认是全缓冲，不像连到终端时是行缓冲），`fork()` 会把这份**未刷新的
缓冲区内容原样复制**给每一个子进程；子进程后续任何一次 flush
（包括 `_exit()` 前手动 `fflush(stdout)`）都会把"父进程本该只打印一次
的内容"重复打印出来——本例最初调试时就看到父进程的日志被 3 个 worker
各重复打印了一遍。修复方式是在 `main()` 开头
`setvbuf(stdout, NULL, _IOLBF, 0)` 显式切换成行缓冲，让每条以 `\n`
结尾的 `printf`立刻落地，不给 fork 留下"未刷新缓冲区"可复制。
