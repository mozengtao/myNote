# Lesson 06: FRR/Quagga源码中的FSM工程实现模式 (Level 4 - 网络协议)

## 这一课和前五课的区别

前五课训练的是"看到一个协议问题,自己发现状态、事件、转移"。这一课换一个角度:**当你打开一个真实的C语言路由协议实现(比如FRR——Free Range Routing,前身是Quagga,业界广泛使用的开源路由协议栈)的源码,你应该带着什么样的眼光去识别"这里其实是一台FSM",以及这台FSM在C语言里通常长什么样子。**

这一课不需要独立可运行的 `.py` 文件——原因很直接:我们要研究的对象是"C语言项目里FSM的工程实现套路",而不是重新发明一个协议状态机(前面五课已经做过这件事)。本课会用伪代码/简化Python移植版辅助理解,帮助你把"C语言里的表驱动写法"和你已经熟悉的Python Transition Table版对照起来看。

## Step A: 为什么C语言项目几乎从不用"面向对象的State Pattern"

在Level 4前几课的Python实现里,我们每一课都写了一版"State Pattern"(每个状态是一个类,自己知道该怎么处理事件)。这在Python、Java、C++这类支持类和多态的语言里是很自然的选择。但C语言**没有对象、没有类、没有虚函数表意义上的"多态"这个语言特性**(C++的虚函数表是编译器帮你生成和维护的,C语言没有这个机制,一切都要自己动手管理)。

那C语言要表达"不同状态下,同一个事件该调用不同的处理逻辑"这件事,最自然、最贴近语言特性的写法是什么?答案是:**函数指针 + 二维数组(表驱动)**。

## Step B: 函数指针状态转移表模式

这是FRR、Quagga、以及绝大多数用C写的路由协议栈(包括很多商业厂商的私有实现)里最常见的FSM写法,基本套路是:

```c
/* 简化示意,不是FRR源码原文,用于说明模式结构 */

typedef int (*fsm_action_func)(struct peer *peer, void *arg);

/* 二维表: 第一维是当前状态,第二维是收到的事件,
 * 每个格子存放一个"该怎么处理"的函数指针 */
static const fsm_action_func FSM_TABLE[BGP_STATUS_MAX][BGP_EVENTS_MAX] = {
    [Idle][BGP_Start]              = bgp_start,
    [Connect][TCP_connection_open] = bgp_connect_success,
    [Connect][TCP_connection_closed] = bgp_connect_fail,
    [OpenSent][Receive_OPEN_message] = bgp_fsm_open,
    [OpenConfirm][Receive_KEEPALIVE_message] = bgp_establish,
    [Established][Receive_UPDATE_message] = bgp_update_receive,
    /* ... 其余组合 ... */
};

/* 收到事件时,直接按 (当前状态, 事件) 下标取出函数指针并调用 */
int bgp_event_update(struct peer *peer, int event)
{
    int current_state = peer->status;
    fsm_action_func action = FSM_TABLE[current_state][event];

    if (action == NULL) {
        /* 这个(状态,事件)组合没有定义处理函数,按非法/忽略处理 */
        return bgp_fsm_illegal(peer, event);
    }
    return action(peer, NULL);
}
```

这段简化示意其实和FRR真实源码里`bgpd/bgp_fsm.c`的核心结构高度相似(FRR的BGP实现里,`bgp_event`函数正是通过一张名为`FSM`的二维函数指针数组,按`[peer->status][event]`取出对应的处理函数并调用,这与本课程Level 4第3课"BGP FSM"里Python版的`TRANSITIONS`字典在设计思想上完全一致,只是C语言用数组+函数指针,Python用字典+可调用对象)。

**这个模式的关键特征:**
- **状态和事件都是小的整数(通常是枚举,底层是`int`)**,因此可以直接当数组下标使用,不需要哈希查找,是O(1)的直接寻址,比字典查找更快、更可预测(没有哈希冲突,没有动态内存分配)。
- **转移逻辑高度集中**:整张表在源码里通常就是一段连续的数组初始化代码,审计"这个协议实现到底支持哪些转移"时,读这一张表就够了,不需要在几十个函数里翻找`if (state == X && event == Y)`这样的判断散落在各处。
- **每个格子对应的处理函数,内部除了执行动作(发送报文、设置定时器)之外,通常自己负责把`peer->status`更新为下一个状态**——这一点和Python版的Transition Table略有不同:Python版通常是"引擎"统一负责状态赋值,处理函数只负责执行动作;而C语言这种写法里,状态转移函数(action function)经常同时兼顾"做事"和"改状态"两件事,这是因为C语言里把两者拆开反而需要多传递返回值、多写胶水代码,不一定划算。

## Step C: 事件驱动回调注册模式

除了"表驱动+函数指针"这种编译期就把整张表定死的写法,FRR/Quagga这类项目里还大量使用另一种相关但略有不同的模式:**事件驱动回调注册**,尤其用在"和外部世界打交道"的地方(读取socket数据、定时器到期、收到系统信号)。

这类代码通常长这样(同样是简化示意):

```c
/* 简化示意: Quagga/FRR 的事件调度框架 thread.c 大致思路 */

struct thread *thread_add_timer(struct thread_master *master,
                                 int (*func)(struct thread *),
                                 void *arg, long timer_sec);

struct thread *thread_add_read(struct thread_master *master,
                                int (*func)(struct thread *),
                                void *arg, int fd);

/* 使用方式: 注册一个"定时器到期后要调用的函数",
 * 而不是自己写一个死循环去轮询时间 */
peer->t_connect = thread_add_timer(master, bgp_connect_timer_expire,
                                    peer, peer->v_connect);
```

这种模式的本质是:**把"某个事件将来发生时该做什么"这个函数指针,提前注册进一个中央的事件循环(event loop)里,由事件循环负责在合适的时机(定时器到期、socket可读)去调用它**。它和上一节的"二维状态转移表"其实是在解决两个不同层次的问题:
- **状态转移表**回答的是:"在当前协议状态下,收到某类事件,该跳到哪个状态、执行哪个动作";
- **事件驱动回调注册**回答的是:"这个事件本身(定时器到期、数据可读)该怎么被系统发现并送进程序里"。

两者组合起来,才是一个完整的协议实现:回调注册机制负责"发现事件、把事件送进来",状态转移表负责"收到事件后决定做什么"。这和本系列Python代码里`GenericFSM.fire(event)`只负责"收到事件后查表处理"、而"计时器超时该在什么时候生成事件"交给`tick()`模拟循环负责,是完全对应的分工方式——只是Python教学代码里用一个简单的`tick()`函数模拟了事件循环,真实的FRR用的是一整套基于`select`/`poll`/`epoll`的事件循环框架(`thread.c`/`event.c`)。

## Step D: 为什么C项目偏爱"表驱动+函数指针",而不是面向对象的State Pattern

这是本课要讲清楚的核心问题,答案可以从三个角度回答:

1. **语言特性决定的自然选择**:C语言没有类、没有继承、没有虚函数表这类语言原生支持"多态"的机制。要在C里手工模拟State Pattern(比如每个状态定义一个`struct`,里面塞满函数指针字段,模拟"虚函数表"),本质上就是自己手搓一份简化版的C++虚函数表——这样做不是不可以(某些C项目确实会这么干,比如Linux内核里大量使用`struct file_operations`这种"函数指针结构体"来模拟面向对象的接口),但对于"状态转移"这种问题,直接用一张二维数组比每个状态单独定义一个结构体更直接、更省代码,因为**表驱动的核心优势(转移规则集中、可审计)在数组形式下体现得最彻底,不需要额外的抽象层**。

2. **性能开销的考量**:路由协议栈往往运行在需要处理大量并发会话(比如一台边界路由器可能同时维护成百上千个BGP对等体)的场景下,每一次事件处理都要尽可能快。二维数组按下标直接寻址是O(1)且没有额外的间接跳转层级(不像面向对象里方法调用可能要经过虚函数表查找),这种性能特性在C语言的系统级网络编程里是被认真考虑的因素,尽管现代CPU分支预测和缓存已经很大程度上抹平了这类差异,但"表驱动"作为一种历史悠久、经过验证的高性能写法,依然是C项目里的默认选择。

3. **审计和正确性验证的便利性**:前几课反复强调的一点——协议标准文档几乎都直接给出状态转移表。**用一张二维数组去实现一张表,能做到"代码结构和规范文档结构一一对应"**,做代码审查(review)或者对照RFC核实实现是否遗漏了某个转移组合时,维护者可以直接把这张C数组和RFC里的表格并排对照,逐格核对——这对于路由协议这种"实现错误可能导致大范围网络故障"的关键基础设施代码而言,是一个非常实际的工程收益。如果用一堆分散在不同状态类里的方法实现同样的逻辑,想要做这种"逐格核对"的审计会麻烦得多。

## Step E: 用简化Python代码模拟"二维表+函数引用"的写法

下面这段Python代码,不是要替代前面课程里已经写过的`dict[tuple[State, Event], ...]`版本,而是专门模拟C语言"数组下标直接寻址+函数指针"这种写法的神韵,方便你把两种语言的思路做对比:

```python
"""模拟C语言 FSM_TABLE[state][event] = handler_func 这种写法的Python版本。

和本系列其它课程的 Transition Table(dict[tuple[State, Event], ...])相比,
这里刻意使用"嵌套列表 + 下标"而不是字典,力求在结构上更贴近C语言的数组写法。
"""

from __future__ import annotations

from enum import IntEnum
from typing import Callable, Optional


class PeerState(IntEnum):
    IDLE = 0
    CONNECT = 1
    OPEN_SENT = 2
    ESTABLISHED = 3


class PeerEvent(IntEnum):
    START = 0
    TCP_OPEN = 1
    RECEIVE_OPEN = 2
    RECEIVE_KEEPALIVE = 3


HandlerFunc = Callable[["Peer"], None]


def bgp_start(peer: "Peer") -> None:
    print(f"  peer={peer.name}: 发起TCP连接")
    peer.state = PeerState.CONNECT


def bgp_connect_success(peer: "Peer") -> None:
    print(f"  peer={peer.name}: TCP已连接,发送OPEN报文")
    peer.state = PeerState.OPEN_SENT


def bgp_fsm_open(peer: "Peer") -> None:
    print(f"  peer={peer.name}: 收到对方OPEN报文,发送KEEPALIVE")
    # 简化模型:直接跳到 ESTABLISHED,省略 OpenConfirm 中间态以突出表结构本身
    peer.state = PeerState.ESTABLISHED


class Peer:
    def __init__(self, name: str) -> None:
        self.name = name
        self.state: PeerState = PeerState.IDLE


# 和C语言 FSM_TABLE[state][event] 结构完全对应的嵌套列表:
# 外层下标是状态,内层下标是事件,取值是函数引用(而不是C里的函数指针,
# 但在"直接按下标寻址取出可调用对象"这一点上,神韵完全一致)。
STATE_COUNT = len(PeerState)
EVENT_COUNT = len(PeerEvent)

FSM_TABLE: list[list[Optional[HandlerFunc]]] = [
    [None for _ in range(EVENT_COUNT)] for _ in range(STATE_COUNT)
]
FSM_TABLE[PeerState.IDLE][PeerEvent.START] = bgp_start
FSM_TABLE[PeerState.CONNECT][PeerEvent.TCP_OPEN] = bgp_connect_success
FSM_TABLE[PeerState.OPEN_SENT][PeerEvent.RECEIVE_OPEN] = bgp_fsm_open


def dispatch_event(peer: Peer, event: PeerEvent) -> None:
    """对应C语言里的 bgp_event_update(): 按 [state][event] 直接取出处理函数并调用。"""
    handler = FSM_TABLE[peer.state][event]
    if handler is None:
        print(f"  peer={peer.name}: 状态{peer.state.name}不接受事件{event.name},忽略")
        return
    handler(peer)


if __name__ == "__main__":
    peer = Peer("203.0.113.1")
    dispatch_event(peer, PeerEvent.START)
    dispatch_event(peer, PeerEvent.TCP_OPEN)
    dispatch_event(peer, PeerEvent.RECEIVE_OPEN)
    print(f"  最终状态: {peer.state.name}")
```

## Step F: 共性与差异——对比Level 4其它课程的Python Transition Table版

**共性:**
- 本质上都是"用一份数据结构表达`(状态, 事件) -> 处理逻辑`这个映射关系",都属于"表驱动"这一大类思想,只是具体的数据结构选型不同(Python字典 vs C数组/Python嵌套列表)。
- 都追求"转移规则集中、可审计",而不是把判断逻辑散落进一堆`if-else`或者散落在多个方法体里。
- 都需要一个统一的"查表并派发"入口(Python版的`GenericFSM.fire()` 对应 C版的`bgp_event_update()`)。

**差异:**
- **查找方式不同**:Python字典是哈希查找(`dict[(state, event)]`),嵌套列表/C数组是下标直接寻址(`table[state][event]`)。前者代码更灵活(可以用任意可哈希的key,不需要事先知道状态和事件的取值范围),后者性能更可预测,但要求状态和事件必须能映射成连续小整数(用`IntEnum`而不是普通`Enum`正是为了满足这个前提)。
- **谁负责改状态**:本系列Python课程里,通常是`GenericFSM`这个引擎类统一负责在查表之后把`self.state`赋值为`next_state`,处理函数(Action)只管做事;而C语言/本课示范代码里,处理函数(如`bgp_start`)经常直接负责修改`peer->state`/`peer.state`,引擎本身不关心目标状态是什么,只管"找到函数就调用它"。这是两种同样合理的职责划分方式:前者更"纯粹"(状态转移和动作执行分离,便于单独测试转移是否正确,不需要真的执行动作),后者更贴近C语言"能省一次数据传递就省一次"的实用主义风格。
- **非法转移的处理**:Python字典版本"key不存在"和C数组/列表版本"格子里是NULL/None"在效果上等价,都表示"这个组合未定义",但实现细节不同——字典要处理`KeyError`,数组/列表要检查空指针/`None`,这是两种语言各自惯用的"缺失值"表达方式的自然延伸。

## 今天真正学会的一句话

面向对象的State Pattern和C语言的"函数指针二维表"并不是两种互相竞争、有优劣之分的FSM实现哲学,而是**同一个"表驱动"思想,在两种不同语言特性(有没有原生的类和多态)下各自最自然的落地形式**——识别一个真实C项目里的FSM工程实现,关键是先找到那张"转移表"(不管它是显式的二维数组,还是散落调用`thread_add_*`的一堆回调注册代码),而不是被"这段代码里没有class关键字"迷惑,误以为C语言项目里没有使用FSM思维。
