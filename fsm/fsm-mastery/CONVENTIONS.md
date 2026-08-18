# 课程约定

## 语言

| 内容 | 语言 |
|------|------|
| 讲义、测验、设计讨论 | 中文 |
| 标识符、注释、源码 | 英文 |
| 实现 | C、C++、Python |

不提供 Rust 实现。

## 三语言同一心智模型

早期课只有状态。从第 03 课起出现事件分发：

```c
typedef enum State { ... } State;
typedef enum Event { ... } Event;   /* from lesson 03 */

typedef struct Fsm {
	State state;
} Fsm;

void fsm_init(Fsm *fsm);
void fsm_dispatch(Fsm *fsm, Event event);  /* from lesson 03 */
```

C++ 用 `enum class`，Python 用 `enum.Enum`。字段名与函数名保持一致：`state`、`fsm_init` / `init`、`fsm_dispatch` / `dispatch`。

## C 风格

```c
/*
 * dispatch one event into the lamp FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	...
}
```

- 函数上方块注释
- 返回类型单独一行
- 不引入第三方库
- 用 `printf` 观察行为

## C++ 风格

- C++17
- `enum class`
- 不强制异常；教学代码以返回值或静默忽略非法输入为主
- 不引入 Boost 等框架

## Python 风格

- Python 3.10+
- `enum.Enum` + `@dataclass`
- 标准库 only

## 如何运行

在对应课的 `code/` 目录：

```bash
# C
cc -std=c11 -Wall -Wextra -o demo lamp.c && ./demo

# C++
c++ -std=c++17 -Wall -Wextra -o demo lamp.cpp && ./demo

# Python
python3 lamp.py
```

具体文件名见各课 `LESSON.md`。

## 实现流派（按课出现，不提前堆）

| 课 | 形态 |
|----|------|
| 01–02 | `if` / `switch` |
| 03–07 | 事件枚举 + 函数分发 |
| 08 | 表驱动 `{action, next}` |
| 09–10 | 函数指针表 + 教学事件队列 |
| 11–12 | 上下文对象 + 取消 |
| 13 | HSM（entry / exit / history） |
| 14 | Actor mailbox + 每对象一台 FSM |

State Pattern、面向对象 FSM、生成式 FSM 的对比集中在第 08 课和第 13 课，前面各课不展开。

## 图例

- 状态图：Mermaid `stateDiagram-v2`
- 架构 / 时序：Mermaid 或对称 ASCII（`+` `-` `|`）
- 状态表列：Current State、Event、Guard、Action、Next State
- 第 01 课还没有 Event 术语时，表头会改成更朴素的列名

## 协议简化

协议课和附录都是教学 FSM：

- 不实现真实报文、socket、内核挂钩
- 用枚举输入 + 打印模拟收发包和超时
- 讲义标明省略了什么

这些代码不能对接真实网络。

## 独立性

本目录自包含。不要把外部工程的头文件或源码链进本课程。工业对照使用公开概念（RFC、Linux 内核 TCP、FRRouting 类守护进程、DPDK），不指向任何本地仓库。
