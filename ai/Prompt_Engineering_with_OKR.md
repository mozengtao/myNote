# Prompt Engineering with OKR 框架

## 目录

1. [OKR 框架本质](#1-okr-框架本质用工程视角理解)
2. [为什么 OKR 能显著提升 Prompt 质量](#2-为什么-okr-能显著提升-prompt-质量)
3. [OKR Prompt 结构](#3-okr-prompt-结构核心模板)
4. [经典示例对比](#4-经典示例从普通提问到-okr-提问)
5. [OKR Prompt 底层原理](#5-okr-prompt-的底层原理重点)
6. [终极模板](#6-终极模板可直接复用)
7. [核心建议](#7-核心建议)
8. [Prompt DSL 架构](#8-prompt-dsl-架构)
9. [DSL 核心语法](#9-dsl-核心语法统一模板)
10. [领域模板库](#10-领域模板库linux--dpdk--frr)
11. [DSL 进阶：可组合](#11-dsl-进阶可组合像写代码一样)
12. [终极技巧](#12-终极技巧把-prompt-当接口定义)

---

## 1. OKR 框架本质（用工程视角理解）

**OKR = Objective + Key Results**

可以这样映射：

```
OKR ≈ 接口定义 + 验收标准
```

### 1.1 Objective（目标）

- **定义**：定性描述你最终想要什么
- **类似**：系统设计中的 "High-Level Goal"

**特点**：
- 清晰（不能模糊）
- 有方向（不是任务列表）
- 有价值（不是低级操作）

### 1.2 Key Results（关键结果）

- **定义**：定量/可验证，怎么判断达成
- **类似**：测试用例 / SLA / 验收标准

**特点**：
- 可测量（Measurable）
- 可验证（Verifiable）
- 有边界（Scope clear）

### 1.3 工程类比示例

```markdown
Objective:
  构建一个高性能网络包处理系统

Key Results:
  - QPS ≥ 10M
  - 延迟 ≤ 50us
  - CPU 使用率 ≤ 70%
```

> 💡 这已经非常像你在做 DPDK 或内核调优时的目标定义了。

---

## 2. 为什么 OKR 能显著提升 Prompt 质量？

### 2.1 问题诊断

**普通提问的问题**：
```
模糊输入 → 模糊输出
```

**OKR Prompt 的本质**：
```
目标约束 + 验收标准 → 可控输出
```

### 2.2 核心转变

> 💡 你不是"问问题"，而是在定义一个任务 contract 给 LLM

---

## 3. OKR Prompt 结构（核心模板）

这是最关键的部分，你可以直接当"标准接口"用：

```markdown
# Objective（目标）
清晰描述你想让模型完成什么（最终产出是什么）

# Key Results（关键结果 / 验收标准）
- KR1: 输出必须包含……
- KR2: 结构必须是……
- KR3: 深度要求……
- KR4: 限制条件……
- KR5: 输出格式……

# Context（上下文，可选但强烈建议）
- 背景信息
- 你的水平
- 使用场景

# Constraints（约束）
- 不要做什么
- 避免什么错误
- 风格要求

# Output Format（输出格式）
（明确结构）
```

### 3.1 本质公式

```
Prompt = Objective + (Key Results as constraints) + Output schema
```

---

## 4. 经典示例（从普通提问到 OKR 提问）

### 4.1 示例1：学习一个技术（Node.js）

#### ❌ 普通问法
```
讲一下 Node.js
```

**问题**：
- 无目标
- 无深度控制
- 输出随机

#### ✅ OKR问法

```markdown
# Objective
系统掌握 Node.js 的核心运行机制，并能用于后端开发

# Key Results
- KR1: 必须解释 Node.js 的事件循环（Event Loop）机制
- KR2: 必须覆盖 module、global、async I/O
- KR3: 提供一个最小可运行示例（HTTP server）
- KR4: 用"架构图 + 分层解释"方式讲解
- KR5: 输出内容覆盖 20% 核心知识 → 支撑 80%理解

# Context
我是一名有5年经验的C/C++工程师

# Output Format
- 架构图
- 核心机制拆解
- 最小代码示例
- 常见误区
```

### 4.2 示例2：源码分析（Linux Kernel）

#### ❌ 普通问法
```
讲一下 VFS
```

#### ✅ OKR问法

```markdown
# Objective
理解 Linux VFS 的核心抽象及其在系统调用中的作用

# Key Results
- KR1: 必须从 sys_open → VFS → filesystem 的调用路径讲解
- KR2: 解释 inode / dentry / file 三者关系
- KR3: 提供 ASCII 架构图
- KR4: 解释设计动机（为什么这样设计）
- KR5: 给出一个"面向对象思想"的映射

# Output Format
- 调用链路
- 核心数据结构
- 架构图
- 设计思想
```

### 4.3 示例3：Prompt工程优化

#### ❌ 普通问法
```
帮我优化这个 prompt
```

#### ✅ OKR问法

```markdown
# Objective
将一个普通 Prompt 优化为"专家级 Prompt"

# Key Results
- KR1: 输出必须包含 Role / Context / Task / Constraints
- KR2: 必须增强任务约束（减少歧义）
- KR3: 提升输出结构化程度
- KR4: 给出优化前 vs 优化后对比
- KR5: 解释优化背后的设计原则

# Output Format
- 原始 Prompt
- 优化后 Prompt
- 关键改进点
```

### 4.4 示例4：系统设计（DPDK）

#### ❌ 普通问法
```
怎么用 DPDK 做高性能网络
```

#### ✅ OKR问法

```markdown
# Objective
设计一个基于 DPDK 的高性能数据包处理架构

# Key Results
- KR1: 必须包含 RX/TX pipeline
- KR2: 描述 poll-mode driver 工作方式
- KR3: 提供多核扩展模型（lcore绑定）
- KR4: 解释 cache / NUMA 优化策略
- KR5: 输出 ASCII 架构图

# Output Format
- 系统架构图
- 数据路径
- 性能优化点
- 实践建议
```

### 4.5 示例5：知识总结（OpenTelemetry）

#### ❌ 普通问法
```
讲一下 OpenTelemetry
```

#### ✅ OKR问法

```markdown
# Objective
掌握 OpenTelemetry 的完整架构及数据流

# Key Results
- KR1: 必须包含 trace / metric / log 三大信号
- KR2: 描述 SDK → Collector → Backend 数据流
- KR3: 提供完整架构图
- KR4: 解释每个组件职责
- KR5: 输出结构化 markdown

# Output Format
- 架构图
- 数据流
- 核心组件
- 关键概念
```

---

## 5. OKR Prompt 的"底层原理"（重点）

### 5.1 理论模型

```
LLM ≈ 非确定性函数
OKR Prompt ≈ 给函数加约束 + 测试用例
```

### 5.2 本质提升点

| 维度 | 普通 Prompt | OKR Prompt |
|------|------------|------------|
| 目标 | 模糊 | 明确 |
| 输出 | 随机 | 可控 |
| 深度 | 不稳定 | 稳定 |
| 结构 | 松散 | 强结构 |
| 可复用性 | 低 | 高 |

---

## 6. 终极模板（可直接复用）

```markdown
# Objective
（你真正想得到的最终结果）

# Key Results
- KR1: 内容范围
- KR2: 深度要求
- KR3: 必须包含的点
- KR4: 输出结构
- KR5: 示例 / 图 / 代码

# Context
（你的背景 + 使用场景）

# Constraints
- 不要泛泛而谈
- 不要跳步骤
- 不要省略关键机制

# Output Format
（严格结构）
```

---

## 7. 核心建议

> **如果你只记住一句话：不要问"问题"，要定义"任务 + 验收标准"**

**你会发现**：
- LLM 输出稳定性大幅提升
- 你可以"调试 Prompt"像调试程序一样
- Prompt 可以沉淀为"可复用资产"

---

## 8. Prompt DSL 架构

### 8.1 整体设计心智模型

```
                           最终 Prompt
                ┌──────────────────────────────┐
                │     Prompt DSL (Top Layer)   │
                └──────────────┬───────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
     目标模块              约束模块                输出结构       
┌──────────────┐     ┌────────────────┐     ┌────────────────┐
│ Objective    │     │ Key Results    │     │ Output Schema  │
└──────────────┘     └────────────────┘     └────────────────┘
        │                      │                      │
        └──────────────┬───────┴──────────────┬───────┘
                       │                      │
                     上下文                  限制         
              ┌────────────────┐     ┌────────────────┐
              │ Context Module │     │ Constraints    │
              └────────────────┘     └────────────────┘
```

### 8.2 本质公式

```
Prompt = O + KR + Schema + Context + Constraints
```

---

## 9. DSL 核心语法（统一模板）

这是你的"基础协议"，所有子模板都基于它：

```markdown
# === Objective ===
<WHAT: 你要解决什么问题>

# === Key Results ===
<KR1: 必须覆盖的核心点>
<KR2: 必须解释的机制>
<KR3: 必须提供的结构/图>
<KR4: 必须包含的示例>
<KR5: 深度/约束要求>

# === Context ===
<你的背景 + 使用场景>

# === Constraints ===
<不要做什么 + 风格约束>

# === Output Schema ===
<结构化输出定义>
```

---

## 10. 领域模板库（Linux / DPDK / FRR）

下面是你可以直接复用的**高频模板（重点）**

### 10.1 内核源码分析模板（Linux Kernel）

**适用于**：VFS / Net stack / Scheduler / Memory

```markdown
# === Objective ===
深入理解 Linux 内核中某个子系统的设计与实现

# === Key Results ===
- KR1: 从 syscall → 内核路径完整调用链分析
- KR2: 必须解释核心数据结构及其关系
- KR3: 提供 ASCII 架构图（模块关系）
- KR4: 分析设计动机（为什么这么设计）
- KR5: 映射到用户态认知（类比）

# === Context ===
我是一名有5年以上经验的C/C++工程师，熟悉系统调用但希望深入内核实现

# === Constraints ===
- 不要只讲概念，必须结合代码路径
- 不要省略关键结构体关系
- 避免泛泛描述

# === Output Schema ===
- 调用链路（Call Path）
- 核心数据结构（Struct关系图）
- 架构图（ASCII）
- 关键机制拆解
- 设计动机（Why）
- 用户态类比
```

### 10.2 网络协议栈分析模板（Linux Networking）

**适用于**：TCP/IP / Netfilter / XDP

```markdown
# === Objective ===
理解 Linux 网络协议栈中某个路径或协议的处理流程

# === Key Results ===
- KR1: 必须给出数据包从 NIC → 用户态完整路径
- KR2: 标出关键 hook 点（如 netfilter / XDP）
- KR3: 解释关键结构（sk_buff / socket）
- KR4: 提供性能瓶颈分析
- KR5: 给出优化思路

# === Output Schema ===
- 数据路径（Packet Flow）
- 关键 Hook 点
- 核心数据结构
- 性能瓶颈分析
- 优化建议
```

### 10.3 DPDK 架构设计模板（高性能）

**适用于**：DPDK / 用户态网络栈

```markdown
# === Objective ===
设计一个基于 DPDK 的高性能数据包处理系统

# === Key Results ===
- KR1: 必须包含 RX/TX pipeline
- KR2: 描述 poll-mode driver 工作机制
- KR3: 提供多核模型（lcore 分配）
- KR4: 分析 cache / NUMA 影响
- KR5: 给出吞吐与延迟优化策略

# === Constraints ===
- 不要停留在概念层，必须落到架构
- 必须考虑实际部署问题

# === Output Schema ===
- 系统架构图（ASCII）
- 数据路径（RX/TX）
- 多核模型
- 性能优化点
- 实战建议
```

### 10.4 FRR / 路由协议分析模板

**适用于**：BGP / OSPF / Zebra

```markdown
# === Objective ===
理解 FRR 中某个路由协议的实现与数据流

# === Key Results ===
- KR1: 描述控制平面数据流（Protocol flow）
- KR2: 分析 RIB → FIB 转换过程
- KR3: 解释关键模块（zebra / bgpd）
- KR4: 提供状态机或流程图
- KR5: 分析收敛与性能问题

# === Output Schema ===
- 协议流程
- 模块关系图
- RIB/FIB 数据流
- 状态机分析
- 性能与收敛
```

### 10.5 源码导读模板（快速上手）

**适用于**：任何大型项目（FRR / DPDK / Kernel）

```markdown
# === Objective ===
快速建立对某个项目源码的整体认知

# === Key Results ===
- KR1: 给出目录结构分层说明
- KR2: 标出核心入口点
- KR3: 提供阅读路径（推荐顺序）
- KR4: 标记关键模块
- KR5: 给出学习路线

# === Output Schema ===
- 项目结构
- 核心入口
- 阅读路径（Step-by-step）
- 模块说明
- 学习建议
```

---

## 11. DSL 进阶：可组合（像写代码一样）

你可以"组合模板"，比如：

### 11.1 示例：DPDK + Linux 网络栈对比分析

```markdown
# Objective
对比 Linux Kernel 网络栈 与 DPDK 的架构差异

# Key Results
- KR1: 对比数据路径（Kernel vs User space）
- KR2: 对比性能模型（interrupt vs polling）
- KR3: 分析延迟与吞吐差异
- KR4: 给出适用场景
- KR5: 提供架构图对比

# Output Schema
- 架构对比图
- 数据路径对比
- 性能模型对比
- 使用场景
```

### 11.2 模板组合原理

```
Template = Linux + DPDK + Compare Pattern
```

---

## 12. 终极技巧：把 Prompt 当"接口定义"

### 12.1 思维转换

你写的不是 Prompt，而是：

```
LLM Function Interface
```

### 12.2 函数映射

例如：
```javascript
function analyze_dpdk_architecture() -> Structured_Knowledge
```

其中：
- **Key Results** = Unit Test
- **Output Schema** = Return Type

---

## 总结

OKR 框架为 Prompt Engineering 提供了一个系统性的方法论：

1. **Objective** 定义明确目标
2. **Key Results** 设定验收标准
3. **Context** 提供背景信息
4. **Constraints** 设置边界条件
5. **Output Format** 规范输出结构

通过这种结构化的方法，你可以：
- 显著提升 LLM 输出的稳定性和质量
- 将 Prompt 视为可调试、可复用的代码资产
- 建立专业领域的模板库，提高工作效率

记住核心原则：**不要问问题，要定义任务 + 验收标准**。