# 第 11 课测验

先自己写答案，再看 `answers/11.md`。

1. 为什么一次 HTTP 请求要单独一台 FSM，而不是复用第 08 课的连接状态机？连接还开着、请求已经 `DONE`，这两件事各属于谁？
2. `cancel_requested` 放在上下文里，而不是增加状态 `CANCELLING`。取消之后迟到的 `BODY_DONE` 靠什么被挡住？若只改状态不置标志，调用者会看到什么错误行为？
3. 本课取消路径是 `SENDING`/`RECEIVING` → `ERROR` → `IDLE`，而 `FAIL` 停在 `ERROR`。为什么取消要多走回 `IDLE` 这一步？
4. `REDIRECT` 回到 `SENDING` 而不是 `IDLE`。用「一次用户 `START` 是否已经结束」说明理由。次数为什么不能做成 `REDIRECT1` `REDIRECT2` 状态？
5. 流水线、HTTP/2 多路复用、连接池内部，本课为什么省略？若要用第 10 课的多机思想补上「连接复用」，第二台机该管什么，请求机枚举里不该出现哪些名字？
