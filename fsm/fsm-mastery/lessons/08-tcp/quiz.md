# 第 08 课测验

先自己写答案。本文件不含解答。

1. 用一句话写出本课 `fsm_dispatch` 做的三步（查找、动作、改状态）。若某格 `defined = 0`，这三步里哪一步不应发生？
2. 为什么 `TIMEOUT` 不该写成「一个全局重传函数」，而要写成表上的两行（`SYN_SENT` 与 `TIME_WAIT`）？这和嵌套 `switch` 的哪一种错误是同一类？
3. 教学路径 `LISTEN + RCV_SYN → ESTABLISHED` 省略了哪个 RFC 状态？省略之后，真实三次握手里哪一次 ACK 不再被单独等待？
4. 比较五种实现形态：嵌套 `switch`、表驱动 `{action, next}`、每格一个 handler 的函数指针表、State Pattern、生成式 FSM。本课采用哪一种？State Pattern 对「状态集合稳定、事件是段标志组合」的协议控制面为什么不合适？
5. 教学代码没有 socket。若要测试「`CLOSED + RCV_FIN` 必须忽略」，你的测试策略喂什么、断言什么？不要假设存在真实网卡。
