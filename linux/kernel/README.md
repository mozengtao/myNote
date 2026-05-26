# Linux 内核学习路径 - 完整指南

## 🎯 学习理念

本系列文档基于"内核执行路径"方法，从用户态 → 内核态 → 硬件的完整链路出发，建立原理 → 源码 → 实验 → debug 的闭环能力。

## 📚 学习顺序

按照内核复杂度和依赖关系，建议按以下顺序学习：

### 基础篇
1. **[read/write 系统调用](01-read-write.md)** - VFS 基础，最简单的内核路径
2. **[VFS open 机制](02-vfs-open.md)** - 文件系统抽象层，理解内核分层架构
3. **[mmap 内存映射](03-mmap.md)** - 内存管理子系统，零拷贝基础

### 进阶篇  
4. **[epoll 多路复用](04-epoll.md)** - IO 多路复用，高性能服务器基础
5. **[TCP 收包流程](05-tcp-recv.md)** - 网络协议栈接收路径
6. **[TCP 发包流程](06-tcp-send.md)** - 网络协议栈发送路径

### 优化篇
7. **[sendfile 零拷贝](07-sendfile.md)** - 零拷贝优化机制

### 高级篇
8. **[eBPF & XDP 深度解析](08-ebpf-xdp.md)** - 可编程数据平面与高性能包处理

## 🧭 学习方法

每个主题都包含 8 个核心部分：

1. **🧩 宏观架构图** - 建立全局认知
2. **🔬 内核执行路径** - 核心调用链分析  
3. **🧱 核心数据结构** - 理解内核存储机制
4. **⚙️ 最小可运行实验** - 代码验证理解
5. **🔍 可观测性 & Debug** - 工具验证内核行为
6. **⚡ 性能与设计权衡** - 深度设计思考
7. **🔗 横向对比** - 建立知识网络
8. **🧠 一句话本质总结** - 抽象核心思想

## 🚀 实践建议

- 每个主题都要运行 demo 代码
- 使用 strace、perf、ftrace 验证理论
- 对比不同机制的设计权衡
- 建立从用户态到硬件的完整认知链条

## 🎯 最终目标

通过这套方法，达到能够独立分析任何 Linux 行为：
- 画出完整执行路径
- 写出验证 demo
- 用工具观测内核行为  
- 定位性能瓶颈

## 📊 文档规模参考

| 文档 | 主题 | 深度 |
|------|------|------|
| 01-read-write | VFS 基础 I/O | 基础 |
| 02-vfs-open | 文件打开与路径解析 | 基础 |
| 03-mmap | 虚拟内存映射 | 进阶 |
| 04-epoll | I/O 多路复用 | 进阶 |
| 05-tcp-recv | TCP 收包全路径 | 深入 |
| 06-tcp-send | TCP 发包全路径 | 深入 |
| 07-sendfile | 零拷贝优化 | 优化 |
| 08-ebpf-xdp | eBPF 可编程数据平面 | 高级 |

## 🔗 知识网络

```
read/write ──→ VFS open ──→ mmap (Zero-Copy Foundation)
                                ↓
epoll (Event-driven) ──→ TCP Receive ←──→ TCP Send ──→ sendfile (Zero-Copy)
     ↑                    ↓              ↓
  EPOLLIN            sk_data_ready   sk_write_space
  EPOLLOUT           (Wake recv)     (Wake send)
                           ↓              ↓
                    eBPF/XDP (Programmable Data Plane)
                           ↓
                   ┌─────────────────┐
                   │ NIC → XDP → Stack│
                   │ High-perf Packet │
                   │ Processing       │
                   └─────────────────┘
```

*图表说明：Linux 内核学习路径的知识网络——从 read/write 和 VFS 基础到 mmap 零拷贝，经 epoll 事件驱动连接 TCP 收发包和 sendfile，最终延伸到 eBPF/XDP 可编程数据平面实现网卡级高性能包处理。*