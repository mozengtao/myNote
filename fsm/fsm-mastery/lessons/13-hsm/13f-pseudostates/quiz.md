# 第 13f 课测验

先自己写答案。本页不给解答。

1. 为什么 Choice / Fork / Join / Junction 不应出现在 `Fsm.outer` 或叶子枚举里？「当前状态」在有伪状态的图上指什么？
2. Choice 和第 04 课画在单支箭上的监护差在哪？为什么不要把 `PLAY` 拆成 `PLAY_CACHED` 与 `PLAY_UNCACHED` 两个事件？
3. Junction 和 Join 差在「等不等」？若把本课的 `STOP` 做成 Join，用户按停止时会卡在哪？
4. 未缓存路径上，为什么 `STARTED` 必须出现在 `WIFI_RECOVER` 之后、而不是 `CONNECTED` 当时？这证明了 Fork 和 Join 哪一点？
5. 为什么不能用线程实现 Fork？第 14 课的 Actor 解决的是哪件本课明确不做的事？
