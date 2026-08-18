# 第 02 课测验

先自己写答案，再看 `answers/02.md`。

1. 转移和状态有什么不同？只说出当前状态，为什么还不够描述一扇门？
2. 为什么用 `switch(state)` 列出出发箭头，比 `is_open` / `is_closed` 两个布尔更安全？
3. 门已经是 `OPEN` 时再调用 `door_open`，当前状态应该是什么？这还算不算一条转移？
4. 画出停车场道闸 `DOWN` / `UP` 的状态图（两个框，标出 `raise` / `lower` 两条主箭头即可）。
5. 状态表的 Event 列写了 `door_open`。这是不是已经引入了「事件类型」？本课为什么还不用 `typedef enum Event`？
