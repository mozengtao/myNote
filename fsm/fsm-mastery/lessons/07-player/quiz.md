# 第 07 课测验

先自己写答案，再看 `answers/07.md`。

1. 「失败就立刻再连、没有次数」和本课的重试差在哪两样数据上？为什么必须打印 `retries left`？
2. 退避序列 1、2、4 解决什么问题？若每次失败都 `sleep` 而不是用 `TICK` 减 `wait_ticks`，会回到第 06 课的哪一个错误？
3. `CONNECTING` 收到第三次（预算内最后一次）`TIMEOUT` 时，为什么要把事件改写成 `GIVE_UP`，而不是直接 `state = STOPPED`？
4. `DATA_STARVE` 进入 `BUFFERING` 为什么不应减少 `retries_left`？若共用一个计数器，用户会看到什么错误行为？
5. 有人想加第五个状态 `WAITING_RETRY` 表示退避。用本课的话说：为什么 `wait_ticks` 当监护就够了？不要用后面课程的术语回答。
