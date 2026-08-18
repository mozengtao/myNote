# 状态机学习课程

这是一套从零到工业级直觉的有限状态机（FSM）自学教材。

拷贝本目录即可学习，不依赖任何外部工程。

## 你将获得什么

学完后，你应能：

- 从零设计一台有限状态机
- 把膨胀的扁平 FSM 重构为分层状态机（HSM）
- 阅读并简化协议状态机（TCP、BGP、OSPF、HTTP、WiFi）
- 用 C / C++ / Python 写出同一套 `Fsm` / `dispatch` 架构
- 比较 FSM、HSM、Statechart、Actor 模型的取舍

## 学习地图

```
01 State          On/Off
02 Transition     Door
03 Event          Vending Machine
04 Guard          101 Sequence Detector
05 Action         Traffic Light
06 Timeout        Elevator
07 Retry          Player
08 Table FSM      TCP（教学精简）
09 Event Loop     BGP（教学精简）
10 Multi-FSM      OSPF（教学精简）
11 App Lifecycle  HTTP Client（教学精简）
12 Recovery       WiFi（教学精简）
13 HSM            复合 / 进入退出 / 历史 / 正交 / 伪状态
14 Actor + FSM    Mailbox 与框架对比
```

细节见 [CURRICULUM.md](CURRICULUM.md)。代码约定见 [CONVENTIONS.md](CONVENTIONS.md)。

## 怎么学

每一课只引入 **一个** 新概念。请按编号顺序读，不要跳课。

每课目录里有：

| 文件 | 用途 |
|------|------|
| `LESSON.md` | 正文（动机、朴素解、FSM 解、图、表、实现、练习） |
| `examples.md` | 同一概念的五个代表例 |
| `quiz.md` | 课后测验（先自己答） |
| `code/c` `code/cpp` `code/python` | 主例可运行代码 |

测验答案在 [`answers/`](answers/)，建议先做题再看。

建议节奏：

1. 读 `LESSON.md` 到「实现」之前，自己先画状态图
2. 对照代码跑一遍
3. 做练习和测验
4. 再读下一课

## 语言

- 讲义、测验、设计讨论：中文
- 标识符、注释、源码：英文
- 实现语言：C、C++、Python（无 Rust）

## 协议课怎么读

第 08–12 课以及 `extras/` 里的 DHCP / TLS / QUIC **以学习为主**。

- 状态图可以接近 RFC，帮助建立完整图景
- 可运行代码只保留核心状态和主路径
- 讲义会标明「真实协议还有什么，这里故意省略」

这些示例不是协议栈，不能拿去对接真实网络。

## 目录

```
fsm-mastery/
  README.md
  CURRICULUM.md
  CONVENTIONS.md
  engine/          第 8 课起抽出的表驱动引擎，第 13 课扩展为 HSM
  lessons/         主线 14 阶段
  extras/          DHCP / TLS / QUIC 附录
  answers/         测验参考答案
```
