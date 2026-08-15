"""
Lesson 05: DPDK Port状态 (Level 5 - Linux系统)

教学用简化模型,模拟DPDK网卡端口(rte_eth_dev)的生命周期状态
(UNINITIALIZED/CONFIGURED/STARTED/STOPPED/CLOSED),对照
rte_eth_dev_configure/start/stop/close的调用顺序约定,
重点演示"转移代价(资源分配/释放)"作为FSM设计的额外维度,
以及link up/down作为独立于生命周期状态的并行维度。

不是DPDK的真实实现,只提炼这一课的核心教学点。
Python 3.12+, 无第三方依赖。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Literal


# ---------------------------------------------------------------------------
# 公共类型
# ---------------------------------------------------------------------------


class PortState(Enum):
    UNINITIALIZED = auto()
    CONFIGURED = auto()
    STARTED = auto()
    STOPPED = auto()
    CLOSED = auto()


class LinkStatus(Enum):
    """并行维度:不属于端口生命周期状态机,独立追踪。"""

    UP = auto()
    DOWN = auto()


class PortEvent(Enum):
    CONFIGURE_SUCCEEDED = auto()
    CONFIGURE_FAILED = auto()
    START_SUCCEEDED = auto()
    START_FAILED = auto()
    STOP = auto()
    CLOSE = auto()


# ---------------------------------------------------------------------------
# v1: 简单版 —— Enum + match-case 硬编码
# ---------------------------------------------------------------------------
# 设计取舍:
#   link_status从一开始就作为独立字段管理,不参与match语句的状态判断,
#   示范"一旦确认是并行维度,实现时就该从第一版开始分开",不要等后期重构。
# ---------------------------------------------------------------------------


class EthDevV1:
    def __init__(self, port_id: int) -> None:
        self.port_id = port_id
        self.state = PortState.UNINITIALIZED
        self.link_status = LinkStatus.DOWN  # 并行维度,与主状态机的match无关

    def handle(self, event: PortEvent) -> None:
        match self.state, event:
            case PortState.UNINITIALIZED, PortEvent.CONFIGURE_SUCCEEDED:
                self._go(PortState.CONFIGURED, "分配DMA描述符环,绑定mempool(高代价)")
            case PortState.CONFIGURED, PortEvent.START_SUCCEEDED:
                self._go(PortState.STARTED, "编程硬件寄存器,启用收发路径(中高代价)")
            case PortState.CONFIGURED, PortEvent.START_FAILED:
                print(f"[port{self.port_id}] START失败,保持CONFIGURED,资源不释放")
            case PortState.STARTED, PortEvent.STOP:
                self._go(PortState.STOPPED, "停用收发路径,配置/mempool绑定保留(低代价)")
            case PortState.STOPPED, PortEvent.START_SUCCEEDED:
                self._go(PortState.STARTED, "复用已有配置直接启用(低代价)")
            case PortState.STOPPED, PortEvent.CONFIGURE_SUCCEEDED:
                self._go(PortState.CONFIGURED, "释放旧配置并重新分配(高代价,应尽量避免频繁走这条路径)")
            case PortState.STOPPED, PortEvent.CLOSE:
                self._go(PortState.CLOSED, "释放DMA资源,注销中断(高代价,一次性)")
            case PortState.CONFIGURED, PortEvent.CLOSE:
                self._go(PortState.CLOSED, "释放已分配但未启用的资源(非规范推荐路径)")
            case _:
                print(f"[port{self.port_id}] {self.state.name}状态下不允许 {event.name}")

    def set_link(self, status: LinkStatus) -> None:
        print(f"[port{self.port_id}] link_status: {self.link_status.name} -> {status.name} (与生命周期状态无关)")
        self.link_status = status

    def _go(self, next_state: PortState, action_desc: str) -> None:
        print(f"[port{self.port_id}] 动作: {action_desc}")
        print(f"[port{self.port_id}] {self.state.name} -> {next_state.name}")
        self.state = next_state


# ---------------------------------------------------------------------------
# v2: Transition Table版(数据驱动 + 显式代价标注)
# ---------------------------------------------------------------------------
# 设计取舍:
#   除了next_state,每条规则额外携带cost字段,并在执行时打印出来。
#   这是对前四课转移表的扩展:转移表可以承载超出"逻辑合法性"的元数据,
#   只要这对使用者有实际参考价值。
# ---------------------------------------------------------------------------


Cost = Literal["low", "medium", "high"]

_COST_SCORE: dict[Cost, int] = {"low": 1, "medium": 3, "high": 10}


@dataclass(frozen=True)
class TransitionV2:
    next_state: PortState
    action_desc: str
    cost: Cost


TABLE_V2: dict[tuple[PortState, PortEvent], TransitionV2] = {
    (PortState.UNINITIALIZED, PortEvent.CONFIGURE_SUCCEEDED): TransitionV2(
        PortState.CONFIGURED, "分配DMA描述符环,绑定mempool", "high"
    ),
    (PortState.CONFIGURED, PortEvent.START_SUCCEEDED): TransitionV2(
        PortState.STARTED, "编程硬件寄存器,启用收发路径", "medium"
    ),
    (PortState.STARTED, PortEvent.STOP): TransitionV2(
        PortState.STOPPED, "停用收发路径,保留配置/mempool绑定", "low"
    ),
    (PortState.STOPPED, PortEvent.START_SUCCEEDED): TransitionV2(
        PortState.STARTED, "复用已有配置直接启用", "low"
    ),
    (PortState.STOPPED, PortEvent.CONFIGURE_SUCCEEDED): TransitionV2(
        PortState.CONFIGURED, "释放旧配置并重新分配", "high"
    ),
    (PortState.STOPPED, PortEvent.CLOSE): TransitionV2(
        PortState.CLOSED, "释放DMA资源,注销中断", "high"
    ),
    (PortState.CONFIGURED, PortEvent.CLOSE): TransitionV2(
        PortState.CLOSED, "释放已分配但未启用的资源(非规范推荐路径)", "medium"
    ),
}


class EthDevV2:
    def __init__(self, port_id: int) -> None:
        self.port_id = port_id
        self.state = PortState.UNINITIALIZED
        self.link_status = LinkStatus.DOWN
        self.total_cost = 0

    def handle(self, event: PortEvent) -> None:
        if event is PortEvent.START_FAILED and self.state is PortState.CONFIGURED:
            print(f"[port{self.port_id}] START失败,保持CONFIGURED")
            return

        transition = TABLE_V2.get((self.state, event))
        if transition is None:
            print(f"[port{self.port_id}] {self.state.name}状态下不允许 {event.name}")
            return

        cost_score = _COST_SCORE[transition.cost]
        self.total_cost += cost_score
        print(
            f"[port{self.port_id}] 动作: {transition.action_desc} "
            f"(代价={transition.cost}, +{cost_score}, 累计={self.total_cost})"
        )
        print(f"[port{self.port_id}] {self.state.name} -> {transition.next_state.name}")
        self.state = transition.next_state


# ---------------------------------------------------------------------------
# v3: State Pattern版 —— STOPPED状态显式区分fast_restart()与reconfigure()
# ---------------------------------------------------------------------------
# 设计取舍:
#   不用一个通用的handle(event)让内部再判断走哪条路径,而是给STOPPED状态
#   类提供两个语义清晰的方法,在类型层面就提醒调用者"这是两个代价不同的
#   操作,请明确选择",比"传一个事件枚举再内部分支"更直接地暴露这个取舍。
# ---------------------------------------------------------------------------


class PortStateHandler:
    state: PortState

    @property
    def name(self) -> str:
        return self.state.name


class UninitializedHandler(PortStateHandler):
    state = PortState.UNINITIALIZED

    def configure(self, ctx: "EthDevV3") -> "PortStateHandler":
        print(f"[port{ctx.port_id}] 分配DMA描述符环,绑定mempool(高代价)")
        return ConfiguredHandler()


class ConfiguredHandler(PortStateHandler):
    state = PortState.CONFIGURED

    def start(self, ctx: "EthDevV3") -> "PortStateHandler":
        print(f"[port{ctx.port_id}] 编程硬件寄存器,启用收发路径(中高代价)")
        return StartedHandler()

    def close(self, ctx: "EthDevV3") -> "PortStateHandler":
        print(f"[port{ctx.port_id}] 释放已分配但未启用的资源(非规范推荐路径)")
        return ClosedHandler()


class StartedHandler(PortStateHandler):
    state = PortState.STARTED

    def stop(self, ctx: "EthDevV3") -> "PortStateHandler":
        print(f"[port{ctx.port_id}] 停用收发路径,配置/mempool绑定保留(低代价)")
        return StoppedHandler()


class StoppedHandler(PortStateHandler):
    state = PortState.STOPPED

    def fast_restart(self, ctx: "EthDevV3") -> "PortStateHandler":
        """低代价路径:复用已有队列配置和mempool绑定,直接回到STARTED。"""
        print(f"[port{ctx.port_id}] 复用已有配置直接启用(低代价)")
        return StartedHandler()

    def reconfigure(self, ctx: "EthDevV3") -> "PortStateHandler":
        """高代价路径:释放旧配置,回到CONFIGURED,后续需要重新start。"""
        print(f"[port{ctx.port_id}] 释放旧配置并重新分配(高代价,应尽量避免频繁走这条路径)")
        return ConfiguredHandler()

    def close(self, ctx: "EthDevV3") -> "PortStateHandler":
        print(f"[port{ctx.port_id}] 释放DMA资源,注销中断(高代价,一次性)")
        return ClosedHandler()


class ClosedHandler(PortStateHandler):
    state = PortState.CLOSED


class EthDevV3:
    def __init__(self, port_id: int) -> None:
        self.port_id = port_id
        self.link_status = LinkStatus.DOWN
        self._handler: PortStateHandler = UninitializedHandler()

    @property
    def state_name(self) -> str:
        return self._handler.name

    def _apply(self, method_name: str) -> None:
        method = getattr(self._handler, method_name, None)
        if method is None:
            print(f"[port{self.port_id}] {self._handler.name}状态下不支持操作 {method_name}")
            return
        before = self._handler.name
        self._handler = method(self)
        print(f"[port{self.port_id}] {before} -> {self._handler.name}")

    def configure(self) -> None:
        self._apply("configure")

    def start(self) -> None:
        self._apply("start")

    def stop(self) -> None:
        self._apply("stop")

    def fast_restart(self) -> None:
        self._apply("fast_restart")

    def reconfigure(self) -> None:
        self._apply("reconfigure")

    def close(self) -> None:
        self._apply("close")

    def set_link(self, status: LinkStatus) -> None:
        print(f"[port{self.port_id}] link_status: {self.link_status.name} -> {status.name} (与生命周期状态无关)")
        self.link_status = status


# ---------------------------------------------------------------------------
# v4: 可复用FSM框架 + 累计资源代价统计
# ---------------------------------------------------------------------------
# 设计取舍:
#   在GenericFSM里加一个"累计代价"计数器,把Step7的文字讨论
#   ("总是走近路 vs 频繁走远路,代价差多少")变成一段可运行、可对比的代码。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Rule:
    state: PortState
    event: PortEvent
    next_state: PortState
    cost: Cost
    action: Callable[["GenericFSM"], None] | None = None


@dataclass
class GenericFSM:
    name: str
    state: PortState
    rules: list[Rule] = field(default_factory=list)
    total_cost: int = 0
    history: list[str] = field(default_factory=list)

    def add_rule(self, rule: Rule) -> None:
        self.rules.append(rule)

    def dispatch(self, event: PortEvent) -> None:
        for rule in self.rules:
            if rule.state == self.state and rule.event == event:
                if rule.action is not None:
                    rule.action(self)
                score = _COST_SCORE[rule.cost]
                self.total_cost += score
                self.history.append(f"{self.state.name}--{event.name}(cost={rule.cost})-->{rule.next_state.name}")
                print(
                    f"[{self.name}] {self.state.name} -> {rule.next_state.name} "
                    f"(代价={rule.cost}, +{score}, 累计={self.total_cost})"
                )
                self.state = rule.next_state
                return
        print(f"[{self.name}] {self.state.name}状态下不允许 {event.name}")


def build_port_fsm(name: str) -> GenericFSM:
    fsm = GenericFSM(name=name, state=PortState.UNINITIALIZED)
    fsm.add_rule(Rule(PortState.UNINITIALIZED, PortEvent.CONFIGURE_SUCCEEDED, PortState.CONFIGURED, "high"))
    fsm.add_rule(Rule(PortState.CONFIGURED, PortEvent.START_SUCCEEDED, PortState.STARTED, "medium"))
    fsm.add_rule(Rule(PortState.STARTED, PortEvent.STOP, PortState.STOPPED, "low"))
    fsm.add_rule(Rule(PortState.STOPPED, PortEvent.START_SUCCEEDED, PortState.STARTED, "low"))
    fsm.add_rule(Rule(PortState.STOPPED, PortEvent.CONFIGURE_SUCCEEDED, PortState.CONFIGURED, "high"))
    fsm.add_rule(Rule(PortState.STOPPED, PortEvent.CLOSE, PortState.CLOSED, "high"))
    return fsm


def simulate_usage_pattern(name: str, events: list[PortEvent]) -> GenericFSM:
    fsm = build_port_fsm(name)
    for evt in events:
        fsm.dispatch(evt)
    return fsm


if __name__ == "__main__":
    print("=== v1: 简单版(Enum + match-case,link_status从一开始独立管理) ===")
    dev1 = EthDevV1(port_id=0)
    dev1.handle(PortEvent.CONFIGURE_SUCCEEDED)
    dev1.handle(PortEvent.START_SUCCEEDED)
    dev1.set_link(LinkStatus.UP)
    dev1.handle(PortEvent.STOP)
    dev1.set_link(LinkStatus.DOWN)  # STOPPED状态下link仍可能是UP/DOWN,与主状态无关
    dev1.handle(PortEvent.START_SUCCEEDED)
    print(f"最终生命周期状态: {dev1.state.name}, link: {dev1.link_status.name}")

    print("\n=== v2: Transition Table版(带累计代价) ===")
    dev2 = EthDevV2(port_id=1)
    dev2.handle(PortEvent.CONFIGURE_SUCCEEDED)
    dev2.handle(PortEvent.START_SUCCEEDED)
    dev2.handle(PortEvent.STOP)
    dev2.handle(PortEvent.START_SUCCEEDED)  # 低代价近路
    print(f"累计代价: {dev2.total_cost}")

    print("\n=== v3: State Pattern版(显式区分fast_restart与reconfigure) ===")
    dev3 = EthDevV3(port_id=2)
    dev3.configure()
    dev3.start()
    dev3.stop()
    dev3.fast_restart()  # 低代价
    dev3.stop()
    dev3.reconfigure()  # 高代价
    dev3.start()
    print(f"最终状态: {dev3.state_name}")

    print("\n=== v4: 可复用FSM框架(对比两种使用模式的累计代价) ===")
    print("-- 模式A: 总是走stop->start近路 --")
    fsm_a = simulate_usage_pattern(
        "portA",
        [
            PortEvent.CONFIGURE_SUCCEEDED,
            PortEvent.START_SUCCEEDED,
            PortEvent.STOP,
            PortEvent.START_SUCCEEDED,
            PortEvent.STOP,
            PortEvent.START_SUCCEEDED,
        ],
    )
    print("-- 模式B: 频繁走stop->configure->start远路 --")
    fsm_b = simulate_usage_pattern(
        "portB",
        [
            PortEvent.CONFIGURE_SUCCEEDED,
            PortEvent.START_SUCCEEDED,
            PortEvent.STOP,
            PortEvent.CONFIGURE_SUCCEEDED,
            PortEvent.START_SUCCEEDED,
            PortEvent.STOP,
            PortEvent.CONFIGURE_SUCCEEDED,
            PortEvent.START_SUCCEEDED,
        ],
    )
    print(f"\n对比结论: 模式A累计代价={fsm_a.total_cost}, 模式B累计代价={fsm_b.total_cost}")
    print("同样是两次'重新可用',模式B因为多走了CONFIGURE_SUCCEEDED这条高代价边,累计代价明显更高。")
