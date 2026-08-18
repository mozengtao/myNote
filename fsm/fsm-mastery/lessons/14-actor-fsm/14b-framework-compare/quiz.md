# 第 14b 课测验

先自己写答案，再看 `answers/14b.md`。

1. 用四个箭头写出本课所有框架共用的概念序列（从消息进系统到 `state` 改变）。在 Akka 的 `tell`、CAF 的 `send`、XState 的 `actor.send`、SObjectizer 的 `so_5::send` 里，哪一步对应 14a 的 `actor_send`（只入队、不 `dispatch`）？
2. XState 也使用 actor 这个词，为什么它 **不是** Akka 的 JVM Actor 系统？举出「邮箱 / 状态图 / 监督 / 分布式」里至少两点实质差别。两条教学 TCP 为什么必须 `spawn`（或两次 `interpret`），而不能做成同一张图里的两个正交区域？
3. 用一张四行对照（扁平 FSM、HSM、Statechart、Actor）各写「它解决哪一种膨胀」。N 条连接互相写 `state` 属于哪一行？`ESTABLISHED` 内部还要省电，属于哪一行？
4. 什么情况下选 CAF（或 SObjectizer）而不是继续用 14a 的五十行运行时？举出一个 14a **不够**、框架才提供的能力，再举出一个框架 **不必上**、五十行就够的能力。
5. Akka 的监督树重启子 Actor，与 14a 调度器看到 `ERROR` 后入队 `RESTART`，对「B 继续跑」这一点是否相同？对「A 的 `state` 字段还在不在」这一点可能有何不同？这和第 12 课「同一台机回到熟知状态」为什么可以共存、却不能当成同一个机制？
