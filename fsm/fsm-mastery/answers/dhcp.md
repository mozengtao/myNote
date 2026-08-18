# 附录 DHCP 参考答案

先自己做 `extras/dhcp/quiz.md`，再对答案。

## 1. 为什么 OFFER 不直接进 BOUND

`OFFER` 只是服务器提议。客户端还要发 REQUEST，等 ACK 才占用地址。直接进 `BOUND` 会把「别人也可能选同一份 offer」和「租约已确认」混成一个局面。

`REQUEST_SENT` 自环教的是：发出请求和收到 ACK 是两件事。卡在 `REQUESTING` 时可以重发 REQUEST，状态名不变。

## 2. 三个超时事件

`T1`、`T2`、`LEASE_EXPIRED` 发生在不同局面，动作也不同：续租、放弃单播改广播（RFC 里进 REBINDING）、租约作废回 `INIT`。合成一个全局 `TIMEOUT` 后，`BOUND` 上无法区分「该续租」还是「租约已经死了」，会走错箭头。

## 3. T2 省略了什么

省略了 RFC 的 `REBINDING`。省略后客户端不再做「向所有服务器广播续租」，教学代码在 `T2` 上直接放弃，回到 `INIT`。

## 4. 真实协议更多的部分

至少两点：`INIT-REBOOT` / `REBOOTING`（有上次地址时的重启路径）；DISCOVER/REQUEST 自己的重传定时器。还可以提 `REBINDING`。

DHCPv6 报文名、状态名、身份确认方式都不同（RFC 8415），不能把 DHCPv4 的五个名字套过去。

## 5. 测试 INIT + ACK 忽略

没有套接字：构造 `Fsm`，`fsm_init` 后当前为 `INIT`，喂 `ACK`。断言：状态仍是 `INIT`，且没有打印 `lease bound` 一类动作。
