# 第 06 课测验

先自己写答案，再看 `answers/06.md`。

1. `TICK` 和 `TIMEOUT_DOOR` 差在哪里？为什么「减到 0 直接 `state = IDLE`」不算本课的超时？
2. `DOOR_CLOSE` 与 `TIMEOUT_DOOR` 可以共用同一个关门动作吗？若可以，表上必须留下什么差别，复盘才分得清？
3. 为什么 `door_ticks`、`current_floor` 放在 `Fsm` 里当数据，而不是做成 `STATE_DOOR_TICK_2` 这种状态？
4. `IDLE` 里收到 `TICK` 应该做什么？若你在 `IDLE` 也递减 `door_ticks`，会犯什么错？
5. 看门狗的 `KICK` 对应电梯的哪一次「重新装载」？指出两者共同的三步：装载、递减、到期事件。
