# 第 13c 课测验

先自己写答案。本页不给解答。

1. `PLAYING` 上 `VOLUME` 若走 `fsm_transition(self)`，轨迹会多出哪两行？用户会看见什么？
2. 「下一状态仍是 PLAYING」为什么不能用来判断内部转移？自环外部和内部差在哪？
3. 为什么 `DATA_STARVE` 必须是外部，而 `VOLUME` 必须是内部？用 13b 的渲染进入 / 退出回答。
4. `STOP` 为什么不能做成 `PLAYING` 上的内部转移？`exit ACTIVE` 在哪一种 Kind 里才会发生？
5. 第 12 课 `CONNECTED` 上的 `DATA_OK` 自环，在已经有进入 / 退出之后，应标成内部还是外部？若标错，可能毁掉什么？
