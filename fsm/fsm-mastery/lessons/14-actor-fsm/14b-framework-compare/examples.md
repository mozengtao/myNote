# 第 14b 课：五个代表例（框架如何宿主连接 FSM）

主例管道的可运行素描在 `code/python/sketch.py`（可选 `code/cpp/sketch.cpp`）。下面五段都是 **同一台教学连接 FSM** 住进不同宿主。伪代码不能编译，也不要去链真实库。

连接机复习：`CLOSED → SYN_SENT → ESTABLISHED`，`FAIL → ERROR`，监护发 `RESTART → CLOSED`。

---

## 例 1：第 14a 课教学运行时（对照基线）

**问题**：把连接 FSM 放进五十行 Actor 运行时，两条连接、轮询邮箱、失败只重启一个。

**状态图**：见第 14a 课。管道：`actor_send` → Mailbox → `scheduler_run` → `fsm_dispatch`。

**实现**：`lessons/14-actor-fsm/14a-mailbox/code/`。本课素描只保留单角色管道。

```
actor_send(A, CONNECT)
actor_send(A, SYNACK)
scheduler_run()          -- dequeue, dispatch, CLOSED-->SYN_SENT-->ESTABLISHED
actor_send(A, FAIL)
scheduler_run()          -- ERROR, supervisor enqueues RESTART, then CLOSED
```

**复杂度**：运行时很短；FSM 仍是 4 态。

**工业用途**：看清字段归属。嵌入式或教学环境可以停在这里。

**常见缺陷**：在基线上长出假监督树，却仍直接写 `other->state`。

**重构**：把 `if` 换成第 08 课的表，宿主不变。

---

## 例 2：Akka（JVM）宿主

**问题**：每条连接一个 Actor；父 Actor 监督；经典 FSM 混入或 Typed `Behaviors` 编码四态。

**Mailbox**：每 Actor 自带。`tell` 只入队。

**Isolation**：没有连接表指针传来传去；只持有 `ActorRef`。

**FSM**：下面用 Typed 风格的伪代码（不是可编译 Scala）：

```
object Conn {
  def apply(): Behavior[Event] = closed()

  def closed(): Behavior[Event] =
    Behaviors.receive { (ctx, ev) =>
      ev match {
        case CONNECT =>
          ctx.log.info("send SYN")
          synSent()
        case _ => Behaviors.unhandled
      }
    }

  def synSent(): Behavior[Event] =
    Behaviors.receive { (_, ev) =>
      ev match {
        case SYNACK => established()
        case FAIL   => error()
        case _      => Behaviors.unhandled
      }
    }

  def established(): Behavior[Event] = ...
  def error(): Behavior[Event] =
    Behaviors.receive { (_, ev) =>
      ev match {
        case RESTART => closed()
        case _       => Behaviors.unhandled
      }
    }
}

// parent
ctx.spawn(Conn(), "A")
ctx.spawn(Conn(), "B")
// supervision: OneForOne restart child A; B untouched
```

经典混入则是 `onEvent { case Event(SYNACK, _) => goto(ESTABLISHED) }`。同一管道：`ref ! SYNACK` → mailbox → dispatcher → 转移。

**Supervision**：子 Actor 失败，父按策略 `restart`。14a 的 `ERROR` 态可以省略，改由运行时销毁并新建实例——那时「回到 CLOSED」是新实例的 `fsm_init`，不是格子 `ERROR → CLOSED`。两种都能表达「只重启 A」。

**语言 / 用途 / 取舍**：Scala/Java；后台会话；付 JVM 成本。不要在本课建 sbt 工程。

**常见缺陷**：在 `receive` 里拿到别的连接的可变对象并赋值。

---

## 例 3：CAF（C++ Actor Framework）宿主

**问题**：类型化 Actor 把连接协议写成消息类型；邮箱与调度由 CAF 提供；状态仍可用 `enum` 或 `become`。

```
struct connect_atom {};
struct synack_atom {};
struct fail_atom {};

using conn_actor = typed_actor<
    result<void>(connect_atom),
    result<void>(synack_atom),
    result<void>(fail_atom)
>;

conn_actor::behavior_type closed(conn_actor::pointer self) {
    return {
        [=](connect_atom) {
            aout(self) << "send SYN\n";
            self->become(syn_sent(self));
        }
    };
}
// syn_sent / established / error similarly
self->send(actor_A, synack_atom{});
```

**Mailbox**：每 Actor 一个。`send` 入队，不立刻跑转移。

**Isolation**：类型化接口阻止随便发 `int`；仍不能阻止你捕获全局 `g_conns[]`——纪律要自己守。

**FSM**：CAF 无一等 Statechart。`become` 切换行为 ≈ 改当前状态。也可以行为函数内部持有第 14a 课的 `Fsm` 结构体，收到消息就 `fsm_dispatch`。

**Supervision**：链接 / 错误处理与 Akka 树不完全一样。教学映射：A 退出时只重启 A 的 `spawn`。

**语言 / 用途 / 取舍**：C++；低延迟原生并发；API 陡。不要在本课写 CMake 去拉 CAF。

**常见缺陷**：在消息处理器里锁住全局连接表，把类型化 Actor 当普通函数调用。

---

## 例 4：XState（JS/TS）宿主

**问题**：连接机先画成状态图（数据），再用解释器跑。两条连接 = `spawn` 两次，各有各的事件队列。

```
const connMachine = createMachine({
  id: 'conn',
  initial: 'CLOSED',
  states: {
    CLOSED:      { on: { CONNECT: { target: 'SYN_SENT', actions: 'sendSyn' } } },
    SYN_SENT:    { on: { SYNACK: 'ESTABLISHED', FAIL: 'ERROR' } },
    ESTABLISHED: { on: { DATA: 'ESTABLISHED', FAIL: 'ERROR' } },
    ERROR:       { on: { RESTART: 'CLOSED' } }
  }
});

const a = interpret(connMachine).start();
const b = interpret(connMachine).start();
a.send({ type: 'CONNECT' });
b.send({ type: 'DATA' });   // ignore; B still CLOSED — own mailbox
```

或在父机器里 `spawn(connMachine, 'A')`。

**Mailbox**：解释器队列。`send` 入队，解释器串行转移。

**Isolation**：JS 单线程；隔离靠「不改另一个 actor 的 `getSnapshot()`」。正交区域 **不是** 第二条连接：区域共享一次 `send` 的广播。两条 TCP 必须 `spawn`（或两个 `interpret`）。

**FSM / Statechart**：这是四框架里图最强的。层次、历史、正交可以直接用第 13 课的词。连接主图仍建议扁平，省电再套进 `ESTABLISHED`。

**Supervision**：`onError`、调用方 `stop`/`start`。不是 Akka 父 Actor 策略。不要把 `interpret` 说成集群。

**语言 / 用途 / 取舍**：TS/JS；UI 与工作流；不是分布式 Actor 系统。

**常见缺陷**：一张图里两个正交区域名叫 `connA` / `connB`，以为那就是隔离的两条连接。

---

## 例 5：SObjectizer（C++）宿主

**问题**：每条连接一个 agent；消息进 mbox；若干连接 agent 放进一个 cooperation，失败时按组或按成员重启。

```
class conn_agent : public so_5::agent_t {
    State state_{State::Closed};
    void on_connect(mhood_t<connect_msg>) {
        /* CLOSED + CONNECT -> SYN_SENT */
        so_5::send<syn_out>(*this);
        state_ = State::SynSent;
    }
    void so_define_agent() override {
        so_subscribe_self()
            .event(&conn_agent::on_connect)
            .event(&conn_agent::on_synack)
            .event(&conn_agent::on_fail);
    }
};

so_5::send<synack_msg>(mbox_of_A);
```

**Mailbox**：mbox 是一等公民，可与 agent 分离（命名 mbox、多个 agent 订同一 mbox）。教学连接仍建议 **一对一**：A 的消息只进 A 的 mbox，否则隔离被总线破坏。

**Isolation**：不共享；只 `send`。多订阅 mbox 是刻意的广播，不要拿来当默认连接总线。

**FSM**：无一等状态图。`state_` 就是第 14a 课的字段，或用 SO-5 的 agent 状态 API 手写切换。

**Supervision**：cooperation 管理一组 agent 的寿命。可比 14a 更粗（整组重启）或配成只重启一个成员。对照时写清策略，不要默认「组内全杀」。

**语言 / 用途 / 取舍**：C++；信令 / 软实时；资料少于 Akka。不要在本课拉取 SObjectizer。

**常见缺陷**：所有连接订同一个命名 mbox，一条 `FAIL` 被所有 agent 处理。

---

五例对照（同一列含义见 `LESSON.md` 6.1）：

| 宿主 | 一条连接住在哪 | 失败时谁还活着 |
|------|----------------|----------------|
| 14a 运行时 | `Actor` + `Fsm.state` | 另一 `Actor` 的邮箱继续出队 |
| Akka | `Behavior` / FSM 混入 | 父重启子 A，B 的 `ActorRef` 仍可用 |
| CAF | typed actor 行为 / `become` | 重新 `spawn` A |
| XState | `interpret` 或 `spawn` 的解释器 | 另一个 actor 的队列 |
| SObjectizer | `agent` + 私有 mbox | 按 cooperation 策略；教学应只重启 A |
