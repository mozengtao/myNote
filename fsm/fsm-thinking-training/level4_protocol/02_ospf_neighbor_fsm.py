"""Lesson 02: OSPF Neighbor FSM (Level 4 - 网络协议)

教学用简化模拟,依据 RFC2328 第10.1节简化建模,不是真实协议栈实现。
重点: 每一个邻居关系独立维护一台状态机("每个对象实例一台FSM")。

四个版本:
    v1 OSPFNeighborV1  - Enum + match-case, 每个邻居一个实例
    v2 OSPFNeighborV2  - Transition Table + 通用退化事件表
    v3 OSPFNeighborV3  - State Pattern
    v4 OSPFRouter      - 通用FSM框架 + 多邻居实例管理容器
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


class NeighborState(Enum):
    DOWN = auto()
    ATTEMPT = auto()
    INIT = auto()
    TWO_WAY = auto()
    EXSTART = auto()
    EXCHANGE = auto()
    LOADING = auto()
    FULL = auto()


class NeighborEvent(Enum):
    HELLO_RECEIVED = auto()
    START = auto()
    TWO_WAY_RECEIVED_NO_ADJ = auto()   # 双向确认,但无需建立完整邻接(非DR/BDR)
    TWO_WAY_RECEIVED_NEED_ADJ = auto()  # 双向确认,且需要建立完整邻接
    ADJ_OK = auto()
    NEGOTIATION_DONE = auto()
    EXCHANGE_DONE_NO_LSA_NEEDED = auto()
    EXCHANGE_DONE_LSA_NEEDED = auto()
    LOADING_DONE = auto()
    SEQ_NUMBER_MISMATCH = auto()
    BAD_LS_REQ = auto()
    ONE_WAY_RECEIVED = auto()
    INACTIVITY_TIMER = auto()
    KILL_NBR = auto()


# 跨越所有状态生效的"通用退化事件":无论当前处于哪个状态,都直接回到 DOWN。
GLOBAL_DOWNGRADE_EVENTS: frozenset[NeighborEvent] = frozenset(
    {NeighborEvent.INACTIVITY_TIMER, NeighborEvent.KILL_NBR}
)


class IllegalTransitionError(RuntimeError):
    pass


# ---------------------------------------------------------------------------
# v1: Enum + match-case, 每个邻居一个实例
# ---------------------------------------------------------------------------
class OSPFNeighborV1:
    """一个实例只代表'本路由器与某一个邻居'的关系,不代表本路由器本身。"""

    def __init__(self, neighbor_router_id: str) -> None:
        self.neighbor_router_id = neighbor_router_id
        self.state: NeighborState = NeighborState.DOWN

    def handle(self, event: NeighborEvent) -> NeighborState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = NeighborState.DOWN
            return self.state
        if event is NeighborEvent.ONE_WAY_RECEIVED and self.state.value >= NeighborState.TWO_WAY.value:
            self.state = NeighborState.INIT
            return self.state

        match (self.state, event):
            case (NeighborState.DOWN, NeighborEvent.HELLO_RECEIVED):
                self.state = NeighborState.INIT
            case (NeighborState.DOWN, NeighborEvent.START):
                self.state = NeighborState.ATTEMPT
            case (NeighborState.ATTEMPT, NeighborEvent.HELLO_RECEIVED):
                self.state = NeighborState.INIT
            case (NeighborState.INIT, NeighborEvent.TWO_WAY_RECEIVED_NO_ADJ):
                self.state = NeighborState.TWO_WAY
            case (NeighborState.INIT, NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ):
                self.state = NeighborState.EXSTART
            case (NeighborState.TWO_WAY, NeighborEvent.ADJ_OK):
                self.state = NeighborState.EXSTART
            case (NeighborState.EXSTART, NeighborEvent.NEGOTIATION_DONE):
                self.state = NeighborState.EXCHANGE
            case (NeighborState.EXCHANGE, NeighborEvent.EXCHANGE_DONE_NO_LSA_NEEDED):
                self.state = NeighborState.FULL
            case (NeighborState.EXCHANGE, NeighborEvent.EXCHANGE_DONE_LSA_NEEDED):
                self.state = NeighborState.LOADING
            case (NeighborState.EXCHANGE, NeighborEvent.SEQ_NUMBER_MISMATCH | NeighborEvent.BAD_LS_REQ):
                self.state = NeighborState.EXSTART
            case (NeighborState.LOADING, NeighborEvent.LOADING_DONE):
                self.state = NeighborState.FULL
            case (NeighborState.LOADING, NeighborEvent.SEQ_NUMBER_MISMATCH | NeighborEvent.BAD_LS_REQ):
                self.state = NeighborState.EXSTART
            case _:
                raise IllegalTransitionError(
                    f"邻居 {self.neighbor_router_id}: {self.state.name} 不接受事件 {event.name}"
                )
        return self.state


# ---------------------------------------------------------------------------
# v2: Transition Table + 通用退化事件表
# ---------------------------------------------------------------------------
OSPF_TRANSITIONS: dict[tuple[NeighborState, NeighborEvent], tuple[NeighborState, str]] = {
    (NeighborState.DOWN, NeighborEvent.HELLO_RECEIVED): (NeighborState.INIT, "记录邻居存在,启动不活跃计时器"),
    (NeighborState.DOWN, NeighborEvent.START): (NeighborState.ATTEMPT, "NBMA网络主动发送Hello"),
    (NeighborState.ATTEMPT, NeighborEvent.HELLO_RECEIVED): (NeighborState.INIT, "转入常规发现流程"),
    (NeighborState.INIT, NeighborEvent.TWO_WAY_RECEIVED_NO_ADJ): (NeighborState.TWO_WAY, "双向确认,无需完整邻接"),
    (NeighborState.INIT, NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ): (NeighborState.EXSTART, "双向确认,开始主从协商"),
    (NeighborState.TWO_WAY, NeighborEvent.ADJ_OK): (NeighborState.EXSTART, "决定升级为完整邻接"),
    (NeighborState.EXSTART, NeighborEvent.NEGOTIATION_DONE): (NeighborState.EXCHANGE, "主从关系确定,交换DBD摘要"),
    (NeighborState.EXCHANGE, NeighborEvent.EXCHANGE_DONE_NO_LSA_NEEDED): (NeighborState.FULL, "数据库已一致"),
    (NeighborState.EXCHANGE, NeighborEvent.EXCHANGE_DONE_LSA_NEEDED): (NeighborState.LOADING, "发送LSR请求缺失LSA"),
    (NeighborState.EXCHANGE, NeighborEvent.SEQ_NUMBER_MISMATCH): (NeighborState.EXSTART, "序列号不匹配,重新协商"),
    (NeighborState.EXCHANGE, NeighborEvent.BAD_LS_REQ): (NeighborState.EXSTART, "非法请求,重新协商"),
    (NeighborState.LOADING, NeighborEvent.LOADING_DONE): (NeighborState.FULL, "所有LSA已同步,邻接建立完成"),
    (NeighborState.LOADING, NeighborEvent.SEQ_NUMBER_MISMATCH): (NeighborState.EXSTART, "序列号不匹配,重新协商"),
    (NeighborState.LOADING, NeighborEvent.BAD_LS_REQ): (NeighborState.EXSTART, "非法请求,重新协商"),
}


class OSPFNeighborV2:
    def __init__(self, neighbor_router_id: str) -> None:
        self.neighbor_router_id = neighbor_router_id
        self.state: NeighborState = NeighborState.DOWN

    def handle(self, event: NeighborEvent) -> NeighborState:
        # 通用退化事件优先于具体状态转移表检查,体现"全局规则先于局部规则"的查表顺序。
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self._transition(NeighborState.DOWN, "判定失联/强制中断,清空同步进度")
            return self.state
        if event is NeighborEvent.ONE_WAY_RECEIVED and self.state.value >= NeighborState.TWO_WAY.value:
            self._transition(NeighborState.INIT, "双向确认失效,退回单向已知")
            return self.state

        key = (self.state, event)
        if key not in OSPF_TRANSITIONS:
            raise IllegalTransitionError(
                f"邻居 {self.neighbor_router_id}: {self.state.name} 不接受事件 {event.name}"
            )
        next_state, action = OSPF_TRANSITIONS[key]
        self._transition(next_state, action)
        return self.state

    def _transition(self, next_state: NeighborState, action: str) -> None:
        print(f"  [{self.neighbor_router_id}] {self.state.name:9s} -> {next_state.name:9s} ({action})")
        self.state = next_state


# ---------------------------------------------------------------------------
# v3: State Pattern
# ---------------------------------------------------------------------------
@dataclass
class _NeighborStateHandler:
    state: NeighborState
    transitions: dict[NeighborEvent, NeighborState] = field(default_factory=dict)
    is_fully_adjacent: bool = False

    def on_event(self, event: NeighborEvent) -> NeighborState | None:
        return self.transitions.get(event)


def _build_neighbor_handlers() -> dict[NeighborState, _NeighborStateHandler]:
    return {
        NeighborState.DOWN: _NeighborStateHandler(
            NeighborState.DOWN,
            {NeighborEvent.HELLO_RECEIVED: NeighborState.INIT, NeighborEvent.START: NeighborState.ATTEMPT},
        ),
        NeighborState.ATTEMPT: _NeighborStateHandler(
            NeighborState.ATTEMPT, {NeighborEvent.HELLO_RECEIVED: NeighborState.INIT}
        ),
        NeighborState.INIT: _NeighborStateHandler(
            NeighborState.INIT,
            {
                NeighborEvent.TWO_WAY_RECEIVED_NO_ADJ: NeighborState.TWO_WAY,
                NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ: NeighborState.EXSTART,
            },
        ),
        NeighborState.TWO_WAY: _NeighborStateHandler(
            NeighborState.TWO_WAY, {NeighborEvent.ADJ_OK: NeighborState.EXSTART}
        ),
        NeighborState.EXSTART: _NeighborStateHandler(
            NeighborState.EXSTART, {NeighborEvent.NEGOTIATION_DONE: NeighborState.EXCHANGE}
        ),
        NeighborState.EXCHANGE: _NeighborStateHandler(
            NeighborState.EXCHANGE,
            {
                NeighborEvent.EXCHANGE_DONE_NO_LSA_NEEDED: NeighborState.FULL,
                NeighborEvent.EXCHANGE_DONE_LSA_NEEDED: NeighborState.LOADING,
                NeighborEvent.SEQ_NUMBER_MISMATCH: NeighborState.EXSTART,
                NeighborEvent.BAD_LS_REQ: NeighborState.EXSTART,
            },
        ),
        NeighborState.LOADING: _NeighborStateHandler(
            NeighborState.LOADING,
            {
                NeighborEvent.LOADING_DONE: NeighborState.FULL,
                NeighborEvent.SEQ_NUMBER_MISMATCH: NeighborState.EXSTART,
                NeighborEvent.BAD_LS_REQ: NeighborState.EXSTART,
            },
        ),
        NeighborState.FULL: _NeighborStateHandler(NeighborState.FULL, {}, is_fully_adjacent=True),
    }


class OSPFNeighborV3:
    def __init__(self, neighbor_router_id: str) -> None:
        self.neighbor_router_id = neighbor_router_id
        self._handlers = _build_neighbor_handlers()
        self.state: NeighborState = NeighborState.DOWN

    def handle(self, event: NeighborEvent) -> NeighborState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = NeighborState.DOWN
            return self.state
        if event is NeighborEvent.ONE_WAY_RECEIVED and self.state.value >= NeighborState.TWO_WAY.value:
            self.state = NeighborState.INIT
            return self.state
        next_state = self._handlers[self.state].on_event(event)
        if next_state is None:
            raise IllegalTransitionError(
                f"邻居 {self.neighbor_router_id}: {self.state.name} 不接受事件 {event.name}"
            )
        self.state = next_state
        return self.state

    def is_fully_adjacent(self) -> bool:
        return self._handlers[self.state].is_fully_adjacent


# ---------------------------------------------------------------------------
# v4: 通用FSM框架 + 多邻居实例管理容器
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Transition:
    from_state: NeighborState
    event: NeighborEvent
    to_state: NeighborState
    action: Callable[[], None] | None = None


class GenericFSM:
    def __init__(self, initial: NeighborState, transitions: list[Transition]) -> None:
        self.state = initial
        self._table: dict[tuple[NeighborState, NeighborEvent], Transition] = {
            (t.from_state, t.event): t for t in transitions
        }

    def fire(self, event: NeighborEvent) -> NeighborState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = NeighborState.DOWN
            return self.state
        key = (self.state, event)
        if key not in self._table:
            raise IllegalTransitionError(f"{self.state.name} 不接受事件 {event.name}")
        transition = self._table[key]
        if transition.action:
            transition.action()
        self.state = transition.to_state
        return self.state


def _build_ospf_transitions() -> list[Transition]:
    return [
        Transition(from_state, event, to_state)
        for (from_state, event), (to_state, _action) in OSPF_TRANSITIONS.items()
    ]


class OSPFRouter:
    """路由器本身不是一台状态机,而是'一堆邻居状态机'的容器。

    这行代码结构本身就是 Step 2 结论的直接体现:状态机挂在
    (router_id -> GenericFSM) 这份映射的每一个值上,而不是挂在
    OSPFRouter 实例本身。
    """

    def __init__(self, router_id: str) -> None:
        self.router_id = router_id
        self._neighbors: dict[str, GenericFSM] = {}

    def neighbor_fsm(self, neighbor_router_id: str) -> GenericFSM:
        if neighbor_router_id not in self._neighbors:
            self._neighbors[neighbor_router_id] = GenericFSM(
                NeighborState.DOWN, _build_ospf_transitions()
            )
        return self._neighbors[neighbor_router_id]

    def handle_event_from(self, neighbor_router_id: str, event: NeighborEvent) -> NeighborState:
        return self.neighbor_fsm(neighbor_router_id).fire(event)

    def summary(self) -> dict[str, str]:
        return {rid: fsm.state.name for rid, fsm in self._neighbors.items()}


# ---------------------------------------------------------------------------
# 演示
# ---------------------------------------------------------------------------
def _demo_v2_full_adjacency() -> None:
    print("=== v2: 邻居 10.0.0.2 一路推进到 Full ===")
    nbr = OSPFNeighborV2("10.0.0.2")
    nbr.handle(NeighborEvent.HELLO_RECEIVED)
    nbr.handle(NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ)
    nbr.handle(NeighborEvent.NEGOTIATION_DONE)
    nbr.handle(NeighborEvent.EXCHANGE_DONE_LSA_NEEDED)
    nbr.handle(NeighborEvent.LOADING_DONE)
    assert nbr.state is NeighborState.FULL

    print("\n=== v2: 邻居 10.0.0.3 停留在 2-Way(非DR/BDR) ===")
    passer_by = OSPFNeighborV2("10.0.0.3")
    passer_by.handle(NeighborEvent.HELLO_RECEIVED)
    passer_by.handle(NeighborEvent.TWO_WAY_RECEIVED_NO_ADJ)
    assert passer_by.state is NeighborState.TWO_WAY

    print("\n=== v2: 序列号不匹配触发从 Exchange 退回 ExStart 重来 ===")
    flaky = OSPFNeighborV2("10.0.0.4")
    flaky.handle(NeighborEvent.HELLO_RECEIVED)
    flaky.handle(NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ)
    flaky.handle(NeighborEvent.NEGOTIATION_DONE)
    flaky.handle(NeighborEvent.SEQ_NUMBER_MISMATCH)
    assert flaky.state is NeighborState.EXSTART

    print("\n=== v2: 不活跃计时器超时,无论当前状态如何都直接回到 Down ===")
    flaky.handle(NeighborEvent.INACTIVITY_TIMER)
    assert flaky.state is NeighborState.DOWN


def _demo_v4_router_with_multiple_neighbors() -> None:
    print("\n=== v4: 一台路由器同时维护多个独立的邻居FSM实例 ===")
    router = OSPFRouter(router_id="1.1.1.1")

    for event in (
        NeighborEvent.HELLO_RECEIVED,
        NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ,
        NeighborEvent.NEGOTIATION_DONE,
        NeighborEvent.EXCHANGE_DONE_NO_LSA_NEEDED,
    ):
        router.handle_event_from("2.2.2.2", event)

    router.handle_event_from("3.3.3.3", NeighborEvent.HELLO_RECEIVED)
    router.handle_event_from("3.3.3.3", NeighborEvent.TWO_WAY_RECEIVED_NO_ADJ)

    print(f"  各邻居当前状态: {router.summary()}")
    assert router.summary() == {"2.2.2.2": "FULL", "3.3.3.3": "TWO_WAY"}


if __name__ == "__main__":
    print("=== v1: Enum+match-case 手工验证一次基本推进 ===")
    nbr_v1 = OSPFNeighborV1("192.168.1.2")
    nbr_v1.handle(NeighborEvent.HELLO_RECEIVED)
    nbr_v1.handle(NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ)
    print(f"  邻居 {nbr_v1.neighbor_router_id} 当前状态: {nbr_v1.state.name}\n")

    _demo_v2_full_adjacency()
    _demo_v4_router_with_multiple_neighbors()

    print("\n=== v3: 用 is_fully_adjacent() 判断是否可参与最短路径计算 ===")
    nbr_v3 = OSPFNeighborV3("4.4.4.4")
    for event in (
        NeighborEvent.HELLO_RECEIVED,
        NeighborEvent.TWO_WAY_RECEIVED_NEED_ADJ,
        NeighborEvent.NEGOTIATION_DONE,
        NeighborEvent.EXCHANGE_DONE_NO_LSA_NEEDED,
    ):
        nbr_v3.handle(event)
    print(f"  邻居 {nbr_v3.neighbor_router_id}: state={nbr_v3.state.name}, "
          f"is_fully_adjacent={nbr_v3.is_fully_adjacent()}")
