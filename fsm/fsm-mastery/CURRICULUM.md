# 课程大纲

主线 14 阶段不跳过。每一课只引入一个新概念；第 13 课拆成子课，避免一次塞入多种 HSM 语法。

## 概念对照

| 课 | 目录 | 主例 | 唯一新概念 | 实现形态 | 本课不要提前使用 |
|----|------|------|------------|----------|------------------|
| 01 | [lessons/01-on-off](lessons/01-on-off/LESSON.md) | 灯 | State | `enum` + `if` | Transition / Event 作为术语 |
| 02 | [lessons/02-door](lessons/02-door/LESSON.md) | 门 | Transition | `switch(state)` | 独立 Event 类型 |
| 03 | [lessons/03-vending](lessons/03-vending/LESSON.md) | 售货机 | Event | 事件枚举 + `dispatch` | Guard 函数 |
| 04 | [lessons/04-101-detector](lessons/04-101-detector/LESSON.md) | 101 检测 | Guard | 转移前条件 | 独立 Action 回调 |
| 05 | [lessons/05-traffic-light](lessons/05-traffic-light/LESSON.md) | 交通灯 | Action | 转移时副作用 | 命名 Timer 对象 |
| 06 | [lessons/06-elevator](lessons/06-elevator/LESSON.md) | 电梯 | Timeout | 到期变成一次输入 | Retry / backoff |
| 07 | [lessons/07-player](lessons/07-player/LESSON.md) | 播放器 | Retry | 计数 + 退避 | 复合状态 |
| 08 | [lessons/08-tcp](lessons/08-tcp/LESSON.md) | TCP | 表驱动协议 FSM | `table[state][event]` | HSM |
| 09 | [lessons/09-bgp](lessons/09-bgp/LESSON.md) | BGP | 事件循环与定时器 | 队列 + tick | HSM |
| 10 | [lessons/10-ospf](lessons/10-ospf/LESSON.md) | OSPF | 多机协同 | 两个扁平 FSM | 正交区域术语 |
| 11 | [lessons/11-http-client](lessons/11-http-client/LESSON.md) | HTTP 客户端 | 应用层生命周期 | 上下文 + 取消 | Actor |
| 12 | [lessons/12-wifi](lessons/12-wifi/LESSON.md) | WiFi | 会话失败恢复 | scan / auth / assoc | HSM 语法 |
| 13a | [lessons/13-hsm/13a-composite](lessons/13-hsm/13a-composite/LESSON.md) | 回看播放器 | Composite | 嵌套状态 | History |
| 13b | [lessons/13-hsm/13b-entry-exit](lessons/13-hsm/13b-entry-exit/LESSON.md) | 同例加深 | Entry / Exit | | |
| 13c | [lessons/13-hsm/13c-internal-external](lessons/13-hsm/13c-internal-external/LESSON.md) | 同例加深 | Internal vs External | | |
| 13d | [lessons/13-hsm/13d-history](lessons/13-hsm/13d-history/LESSON.md) | 同例加深 | Shallow / Deep History | | |
| 13e | [lessons/13-hsm/13e-orthogonal](lessons/13-hsm/13e-orthogonal/LESSON.md) | 同例加深 | Orthogonal Regions | | |
| 13f | [lessons/13-hsm/13f-pseudostates](lessons/13-hsm/13f-pseudostates/LESSON.md) | 同例加深 | Choice / Fork / Join / Junction | | |
| 14a | [lessons/14-actor-fsm/14a-mailbox](lessons/14-actor-fsm/14a-mailbox/LESSON.md) | 连接管理 | Actor + Mailbox | 队列 + 单线程 dispatch | |
| 14b | [lessons/14-actor-fsm/14b-framework-compare](lessons/14-actor-fsm/14b-framework-compare/LESSON.md) | 框架对比 | Akka / CAF / XState / SObjectizer | 概念对比 | |

## 每课固定结构

1. Motivation
2. Problem Description
3. Naive Solution
4. FSM Solution
5. State Diagram
6. State Table
7. Event Flow
8. Implementation
9. Design Discussion
10. Common Mistakes
11. Refactoring
12. Exercise
13. Summary

## 附录

主线学完后再读，避免打乱概念顺序：

- [extras/dhcp](extras/dhcp/LESSON.md)
- [extras/tls](extras/tls/LESSON.md)
- [extras/quic](extras/quic/LESSON.md)

## 可复用引擎

从第 08 课抽出表驱动核心，第 13 课扩展为 HSM：

- [engine/](engine/README.md)
