# 附录 TLS 参考答案

先自己做 `extras/tls/quiz.md`，再对答案。

## 1. 握手 FSM 不是加密 FSM

本附录只排握手控制面的局面（等 ServerHello、等 Finished、已建立）。密钥计算、证书校验算法不在状态枚举里。`CERT_OK` 假定「证书已经验完且通过」这一步计算已经在 FSM 外面做完，只把结果当成事件喂进来。

## 2. CERT_OK 做成自环

教学上证书校验是 `WAIT_FINISHED` 期间的一次副作用，不必单独占一个局面。值得加 `WAIT_CERT` 的情况：校验证书会卡住很久、需要独立超时，或失败路径与 Finished 超时必须分开记账。

## 3. 两种 TIMEOUT

`WAIT_SERVER_HELLO + TIMEOUT`：握手还没开始成功，对端沉默，进 `CLOSED`。
`ESTABLISHED` 上的超时若存在，通常是会话空闲或重协商，不是「握手失败」。

不要写成一个全局 `TIMEOUT` 处理函数：同一事件名在不同状态下语义不同，合成后握手失败会误伤已建立会话。本附录在 `ESTABLISHED` 上对 `TIMEOUT` 缺格，就是为了把这件事钉死。

## 4. 故意省略

任举三件：完整 TLS 1.3 flight 细节、0-RTT、会话票证（还可补：HelloRetryRequest、密钥更新、双向认证）。省略是为了保住「握手是一台小生命周期机，事件来自记录层，不是密码学本身」这个问题。

## 5. START + FINISHED 不能建会话

脚本：`fsm_init` 后只喂 `FINISHED`（或 `START` 之前就喂 `FINISHED`）。断言：打印里没有 `handshake complete`，状态不是 `ESTABLISHED`（仍是 `START` 或按你的表停在 `START`）。
