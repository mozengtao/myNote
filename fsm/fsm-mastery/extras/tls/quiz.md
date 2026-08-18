# 附录 TLS 测验

先自己写答案。本文件不含解答。

1. 为什么本附录是「握手 FSM」而不是「加密 FSM」？`CERT_OK` 事件假定已经发生了哪一步计算？
2. `CERT_OK` 为什么做成 `WAIT_FINISHED` 上的自环，而不是新状态 `WAIT_CERT`？什么情况下你才值得加那一态？
3. `WAIT_SERVER_HELLO + TIMEOUT` 进 `CLOSED`，但 `ESTABLISHED + TIMEOUT` 是缺格。这两种超时各代表什么？为什么不要合成一个全局 `TIMEOUT` 处理函数？
4. 列出本附录故意省略的三件 TLS 1.3 能力（flight / 0-RTT / 票证中任举，可再补别的）。省略它们是为了保住哪一个教学问题？
5. 如何用事件脚本证明 `START + FINISHED` 不会建立会话？断言哪两样（打印与状态）？
