# 第 13a 课测验

先自己写答案。本页不给解答。

1. 第 07 课三个活动状态各画一支 `STOP`，和本课把 `STOP` 画在 `ACTIVE` 上，运行时用户看见的行为有何相同、实现上差在哪？为什么扁平爆炸会从 `PlayingSeeking`、`ConnectingRetry1` 这类名字开始？
2. `ACTIVE` 为什么不应成为 `Fsm.state` 的一个可停靠取值？「任意时刻恰好一个当前状态」在有盒子之后，指的是叶子还是父框？
3. 用本课的对照表：扁平 FSM、HSM、Statechart、Actor 各解决什么问题？为什么本课只实现复合，却要先把 Statechart 和 Actor 的边界说清？
4. `in_active(state)` 和「再设一个 `bool in_active` 字段」有何不同？哪一种更容易和叶子枚举打架（例如 `state==STOPPED` 且 `in_active==true`）？
5. 第 12 课为什么不准把省电做成与 `CONNECTED` 并列的顶层状态？用「盒子」而不是后面课程的术语回答：省电应该和谁共享哪一条退出？
