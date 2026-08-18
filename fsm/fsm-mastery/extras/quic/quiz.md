# 附录 QUIC 测验

先自己写答案。本文件不含解答。

1. 为什么 QUIC 连接 FSM 不能当成「UDP 上的 TCP 十一态」？举一个 TCP 状态名，说明它在 QUIC 里没有对应物、或对应的是另一套机制。
2. 流状态机和连接状态机为什么要分开？`CONNECTED` 对「能不能 new stream」意味着什么？draining 时还允许 new stream 吗？
3. 丢包恢复（PTO）为什么不应写成连接表上的 `CONNECTED + TIMEOUT → 重传握手`？PTO 到期真正该推动的是哪一层？
4. `SEND_INITIAL + HANDSHAKE_DONE` 为什么必须是缺格？若允许跳到 `CONNECTED`，漏掉了哪一次确认？
5. 如何测试「已连接 `PATH_FAIL` 进 `DRAINING`，握手中 `PATH_FAIL` 进 `CLOSED`」？写出两段最短事件序列和各自的结束状态。
