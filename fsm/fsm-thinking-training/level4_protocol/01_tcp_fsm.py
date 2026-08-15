"""Lesson 01: TCP连接状态机 (Level 4 - 网络协议)

教学用简化模拟,依据 RFC793 简化建模,不是真实协议栈实现。
覆盖: 三次握手 -> 数据传输 -> 四次挥手(含同时关闭分支)。

四个版本:
    v1 TCPConnectionV1   - Enum + match-case 硬编码
    v2 TCPConnectionV2   - Transition Table(数据驱动)
    v3 TCPConnectionV3   - State Pattern(每个状态是一个类)
    v4 TCPConnectionV4   - 复用通用 FSM 框架
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Protocol


class TCPState(Enum):
    CLOSED = auto()
    LISTEN = auto()
    SYN_SENT = auto()
    SYN_RECEIVED = auto()
    ESTABLISHED = auto()
    FIN_WAIT_1 = auto()
    FIN_WAIT_2 = auto()
    CLOSE_WAIT = auto()
    CLOSING = auto()
    LAST_ACK = auto()
    TIME_WAIT = auto()


class TCPEvent(Enum):
    PASSIVE_OPEN = auto()
    ACTIVE_OPEN = auto()
    APP_CLOSE = auto()
    RCV_SYN = auto()
    RCV_SYN_ACK = auto()
    RCV_ACK = auto()
    RCV_FIN = auto()
    RCV_FIN_ACK = auto()
    RCV_RST = auto()
    TIMEOUT_2MSL = auto()


class IllegalTransitionError(RuntimeError):
    """当前状态不允许该事件时抛出。"""


# ---------------------------------------------------------------------------
# v1: Enum + match-case 硬编码版
# ---------------------------------------------------------------------------
class TCPConnectionV1:
    """逐字对照 RFC793 状态图的最直观写法。"""

    def __init__(self) -> None:
        self.state: TCPState = TCPState.CLOSED

    def handle(self, event: TCPEvent) -> TCPState:
        match (self.state, event):
            case (TCPState.CLOSED, TCPEvent.PASSIVE_OPEN):
                self.state = TCPState.LISTEN
            case (TCPState.CLOSED, TCPEvent.ACTIVE_OPEN):
                self.state = TCPState.SYN_SENT
            case (TCPState.LISTEN, TCPEvent.RCV_SYN):
                self.state = TCPState.SYN_RECEIVED
            case (TCPState.LISTEN, TCPEvent.APP_CLOSE):
                self.state = TCPState.CLOSED
            case (TCPState.SYN_SENT, TCPEvent.RCV_SYN_ACK):
                self.state = TCPState.ESTABLISHED
            case (TCPState.SYN_SENT, TCPEvent.RCV_SYN):
                self.state = TCPState.SYN_RECEIVED
            case (TCPState.SYN_SENT, TCPEvent.RCV_RST):
                self.state = TCPState.CLOSED
            case (TCPState.SYN_RECEIVED, TCPEvent.RCV_ACK):
                self.state = TCPState.ESTABLISHED
            case (TCPState.SYN_RECEIVED, TCPEvent.APP_CLOSE):
                self.state = TCPState.FIN_WAIT_1
            case (TCPState.SYN_RECEIVED, TCPEvent.RCV_RST):
                self.state = TCPState.LISTEN
            case (TCPState.ESTABLISHED, TCPEvent.APP_CLOSE):
                self.state = TCPState.FIN_WAIT_1
            case (TCPState.ESTABLISHED, TCPEvent.RCV_FIN):
                self.state = TCPState.CLOSE_WAIT
            case (TCPState.ESTABLISHED, TCPEvent.RCV_RST):
                self.state = TCPState.CLOSED
            case (TCPState.FIN_WAIT_1, TCPEvent.RCV_ACK):
                self.state = TCPState.FIN_WAIT_2
            case (TCPState.FIN_WAIT_1, TCPEvent.RCV_FIN):
                self.state = TCPState.CLOSING
            case (TCPState.FIN_WAIT_1, TCPEvent.RCV_FIN_ACK):
                self.state = TCPState.TIME_WAIT
            case (TCPState.FIN_WAIT_2, TCPEvent.RCV_FIN):
                self.state = TCPState.TIME_WAIT
            case (TCPState.CLOSE_WAIT, TCPEvent.APP_CLOSE):
                self.state = TCPState.LAST_ACK
            case (TCPState.CLOSING, TCPEvent.RCV_ACK):
                self.state = TCPState.TIME_WAIT
            case (TCPState.LAST_ACK, TCPEvent.RCV_ACK):
                self.state = TCPState.CLOSED
            case (TCPState.TIME_WAIT, TCPEvent.TIMEOUT_2MSL):
                self.state = TCPState.CLOSED
            case (_, TCPEvent.RCV_RST):
                self.state = TCPState.CLOSED
            case _:
                raise IllegalTransitionError(
                    f"{self.state.name} 状态不接受事件 {event.name}"
                )
        return self.state


# ---------------------------------------------------------------------------
# v2: Transition Table 版(数据驱动)
# ---------------------------------------------------------------------------
TCP_TRANSITIONS: dict[tuple[TCPState, TCPEvent], tuple[TCPState, str]] = {
    (TCPState.CLOSED, TCPEvent.PASSIVE_OPEN): (TCPState.LISTEN, "进入监听"),
    (TCPState.CLOSED, TCPEvent.ACTIVE_OPEN): (TCPState.SYN_SENT, "发送SYN"),
    (TCPState.LISTEN, TCPEvent.RCV_SYN): (TCPState.SYN_RECEIVED, "发送SYN+ACK"),
    (TCPState.LISTEN, TCPEvent.APP_CLOSE): (TCPState.CLOSED, "释放监听"),
    (TCPState.SYN_SENT, TCPEvent.RCV_SYN_ACK): (TCPState.ESTABLISHED, "发送ACK,握手完成"),
    (TCPState.SYN_SENT, TCPEvent.RCV_SYN): (TCPState.SYN_RECEIVED, "同时打开,发送SYN+ACK"),
    (TCPState.SYN_SENT, TCPEvent.RCV_RST): (TCPState.CLOSED, "连接尝试失败"),
    (TCPState.SYN_RECEIVED, TCPEvent.RCV_ACK): (TCPState.ESTABLISHED, "三次握手完成"),
    (TCPState.SYN_RECEIVED, TCPEvent.APP_CLOSE): (TCPState.FIN_WAIT_1, "握手未完成即关闭,发送FIN"),
    (TCPState.SYN_RECEIVED, TCPEvent.RCV_RST): (TCPState.LISTEN, "回到监听"),
    (TCPState.ESTABLISHED, TCPEvent.APP_CLOSE): (TCPState.FIN_WAIT_1, "主动关闭,发送FIN"),
    (TCPState.ESTABLISHED, TCPEvent.RCV_FIN): (TCPState.CLOSE_WAIT, "回复ACK,对方已停止发送"),
    (TCPState.ESTABLISHED, TCPEvent.RCV_RST): (TCPState.CLOSED, "异常终止"),
    (TCPState.FIN_WAIT_1, TCPEvent.RCV_ACK): (TCPState.FIN_WAIT_2, "我方FIN已被确认"),
    (TCPState.FIN_WAIT_1, TCPEvent.RCV_FIN): (TCPState.CLOSING, "同时关闭,等待对方确认我方FIN"),
    (TCPState.FIN_WAIT_1, TCPEvent.RCV_FIN_ACK): (TCPState.TIME_WAIT, "双方关闭同时收敛"),
    (TCPState.FIN_WAIT_2, TCPEvent.RCV_FIN): (TCPState.TIME_WAIT, "回复ACK,进入收尾等待"),
    (TCPState.CLOSE_WAIT, TCPEvent.APP_CLOSE): (TCPState.LAST_ACK, "本地数据发完,发送我方FIN"),
    (TCPState.CLOSING, TCPEvent.RCV_ACK): (TCPState.TIME_WAIT, "对方确认了我方FIN"),
    (TCPState.LAST_ACK, TCPEvent.RCV_ACK): (TCPState.CLOSED, "连接结束"),
    (TCPState.TIME_WAIT, TCPEvent.TIMEOUT_2MSL): (TCPState.CLOSED, "2MSL到期,安全销毁"),
}


class TCPConnectionV2:
    """转移规则完全数据化,可被打印/审计/单独测试。"""

    def __init__(self) -> None:
        self.state: TCPState = TCPState.CLOSED
        self.history: list[tuple[TCPState, TCPEvent, TCPState]] = []

    def handle(self, event: TCPEvent) -> TCPState:
        # RST 在几乎所有已建立连接的状态下都直接回 CLOSED,
        # 用一条兜底规则代替在表里为每个状态都重复一行。
        if event is TCPEvent.RCV_RST and (self.state, event) not in TCP_TRANSITIONS:
            next_state, action = TCPState.CLOSED, "收到RST,立即异常终止"
        else:
            try:
                next_state, action = TCP_TRANSITIONS[(self.state, event)]
            except KeyError as exc:
                raise IllegalTransitionError(
                    f"{self.state.name} 状态不接受事件 {event.name}"
                ) from exc
        print(f"  [v2] {self.state.name:13s} --{event.name:12s}--> {next_state.name:13s} ({action})")
        self.history.append((self.state, event, next_state))
        self.state = next_state
        return self.state


# ---------------------------------------------------------------------------
# v3: State Pattern 版
# ---------------------------------------------------------------------------
class TCPStateHandler(Protocol):
    def on_event(self, event: TCPEvent) -> TCPState | None: ...
    def can_send(self) -> bool: ...
    def can_receive(self) -> bool: ...


@dataclass
class _BaseStateHandler:
    state: TCPState
    transitions: dict[TCPEvent, TCPState] = field(default_factory=dict)
    sendable: bool = True
    receivable: bool = True

    def on_event(self, event: TCPEvent) -> TCPState | None:
        return self.transitions.get(event)

    def can_send(self) -> bool:
        return self.sendable

    def can_receive(self) -> bool:
        return self.receivable


def _build_state_handlers() -> dict[TCPState, _BaseStateHandler]:
    return {
        TCPState.CLOSED: _BaseStateHandler(
            TCPState.CLOSED,
            {TCPEvent.PASSIVE_OPEN: TCPState.LISTEN, TCPEvent.ACTIVE_OPEN: TCPState.SYN_SENT},
            sendable=False, receivable=False,
        ),
        TCPState.LISTEN: _BaseStateHandler(
            TCPState.LISTEN,
            {TCPEvent.RCV_SYN: TCPState.SYN_RECEIVED, TCPEvent.APP_CLOSE: TCPState.CLOSED},
            sendable=False, receivable=False,
        ),
        TCPState.SYN_SENT: _BaseStateHandler(
            TCPState.SYN_SENT,
            {
                TCPEvent.RCV_SYN_ACK: TCPState.ESTABLISHED,
                TCPEvent.RCV_SYN: TCPState.SYN_RECEIVED,
                TCPEvent.RCV_RST: TCPState.CLOSED,
            },
            sendable=False, receivable=False,
        ),
        TCPState.SYN_RECEIVED: _BaseStateHandler(
            TCPState.SYN_RECEIVED,
            {
                TCPEvent.RCV_ACK: TCPState.ESTABLISHED,
                TCPEvent.APP_CLOSE: TCPState.FIN_WAIT_1,
                TCPEvent.RCV_RST: TCPState.LISTEN,
            },
            sendable=False, receivable=False,
        ),
        TCPState.ESTABLISHED: _BaseStateHandler(
            TCPState.ESTABLISHED,
            {
                TCPEvent.APP_CLOSE: TCPState.FIN_WAIT_1,
                TCPEvent.RCV_FIN: TCPState.CLOSE_WAIT,
                TCPEvent.RCV_RST: TCPState.CLOSED,
            },
        ),
        # FIN_WAIT_2: 半关闭 —— 我方已不能再发送,但仍能接收对方数据。
        TCPState.FIN_WAIT_1: _BaseStateHandler(
            TCPState.FIN_WAIT_1,
            {
                TCPEvent.RCV_ACK: TCPState.FIN_WAIT_2,
                TCPEvent.RCV_FIN: TCPState.CLOSING,
                TCPEvent.RCV_FIN_ACK: TCPState.TIME_WAIT,
            },
            sendable=False,
        ),
        TCPState.FIN_WAIT_2: _BaseStateHandler(
            TCPState.FIN_WAIT_2,
            {TCPEvent.RCV_FIN: TCPState.TIME_WAIT},
            sendable=False,
        ),
        # CLOSE_WAIT: 半关闭 —— 对方已不再发送,但我方仍可继续发送。
        TCPState.CLOSE_WAIT: _BaseStateHandler(
            TCPState.CLOSE_WAIT,
            {TCPEvent.APP_CLOSE: TCPState.LAST_ACK},
            receivable=False,
        ),
        TCPState.CLOSING: _BaseStateHandler(
            TCPState.CLOSING,
            {TCPEvent.RCV_ACK: TCPState.TIME_WAIT},
            sendable=False, receivable=False,
        ),
        TCPState.LAST_ACK: _BaseStateHandler(
            TCPState.LAST_ACK,
            {TCPEvent.RCV_ACK: TCPState.CLOSED},
            sendable=False, receivable=False,
        ),
        TCPState.TIME_WAIT: _BaseStateHandler(
            TCPState.TIME_WAIT,
            {TCPEvent.TIMEOUT_2MSL: TCPState.CLOSED},
            sendable=False, receivable=False,
        ),
    }


class TCPConnectionV3:
    """每个状态自带'能不能发/能不能收'的能力查询,凸显半关闭语义。"""

    def __init__(self) -> None:
        self._handlers = _build_state_handlers()
        self.state: TCPState = TCPState.CLOSED

    @property
    def _current(self) -> _BaseStateHandler:
        return self._handlers[self.state]

    def handle(self, event: TCPEvent) -> TCPState:
        if event is TCPEvent.RCV_RST and event not in self._current.transitions:
            self.state = TCPState.CLOSED
            return self.state
        next_state = self._current.on_event(event)
        if next_state is None:
            raise IllegalTransitionError(
                f"{self.state.name} 状态不接受事件 {event.name}"
            )
        self.state = next_state
        return self.state

    def can_send(self) -> bool:
        return self._current.can_send()

    def can_receive(self) -> bool:
        return self._current.can_receive()


# ---------------------------------------------------------------------------
# v4: 复用通用 FSM 框架
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Transition:
    from_state: TCPState
    event: TCPEvent
    to_state: TCPState
    action: Callable[[], None] | None = None


class GenericFSM:
    """与 Level 1~3 中同名框架同构:只关心 (state, event) -> state 的查表与执行。"""

    def __init__(self, initial: TCPState, transitions: list[Transition]) -> None:
        self.state = initial
        self._table: dict[tuple[TCPState, TCPEvent], Transition] = {
            (t.from_state, t.event): t for t in transitions
        }

    def fire(self, event: TCPEvent) -> TCPState:
        key = (self.state, event)
        if key not in self._table:
            raise IllegalTransitionError(
                f"{self.state.name} 状态不接受事件 {event.name}"
            )
        transition = self._table[key]
        if transition.action:
            transition.action()
        self.state = transition.to_state
        return self.state


def build_tcp_fsm() -> GenericFSM:
    transitions = [
        Transition(from_state, event, to_state)
        for (from_state, event), (to_state, _action) in TCP_TRANSITIONS.items()
    ]
    return GenericFSM(TCPState.CLOSED, transitions)


# ---------------------------------------------------------------------------
# 演示: 三次握手 -> 数据传输 -> 四次挥手(演示两条关闭支路)
# ---------------------------------------------------------------------------
def _demo_v2_full_lifecycle() -> None:
    print("=== v2: 客户端主动打开 -> 数据传输 -> 主动关闭(标准四次挥手) ===")
    client = TCPConnectionV2()
    client.handle(TCPEvent.ACTIVE_OPEN)     # CLOSED -> SYN_SENT
    client.handle(TCPEvent.RCV_SYN_ACK)     # SYN_SENT -> ESTABLISHED
    client.handle(TCPEvent.APP_CLOSE)       # ESTABLISHED -> FIN_WAIT_1
    client.handle(TCPEvent.RCV_ACK)         # FIN_WAIT_1 -> FIN_WAIT_2
    client.handle(TCPEvent.RCV_FIN)         # FIN_WAIT_2 -> TIME_WAIT
    client.handle(TCPEvent.TIMEOUT_2MSL)    # TIME_WAIT -> CLOSED
    assert client.state is TCPState.CLOSED

    print("\n=== v2: 服务端被动打开 -> 数据传输 -> 被动关闭 ===")
    server = TCPConnectionV2()
    server.handle(TCPEvent.PASSIVE_OPEN)    # CLOSED -> LISTEN
    server.handle(TCPEvent.RCV_SYN)         # LISTEN -> SYN_RECEIVED
    server.handle(TCPEvent.RCV_ACK)         # SYN_RECEIVED -> ESTABLISHED
    server.handle(TCPEvent.RCV_FIN)         # ESTABLISHED -> CLOSE_WAIT
    server.handle(TCPEvent.APP_CLOSE)       # CLOSE_WAIT -> LAST_ACK
    server.handle(TCPEvent.RCV_ACK)         # LAST_ACK -> CLOSED
    assert server.state is TCPState.CLOSED

    print("\n=== v2: 同时关闭(CLOSING分支)演示 ===")
    peer_a = TCPConnectionV2()
    peer_a.handle(TCPEvent.ACTIVE_OPEN)
    peer_a.handle(TCPEvent.RCV_SYN_ACK)
    peer_a.handle(TCPEvent.APP_CLOSE)       # ESTABLISHED -> FIN_WAIT_1
    peer_a.handle(TCPEvent.RCV_FIN)         # FIN_WAIT_1 -> CLOSING (双方几乎同时关闭)
    peer_a.handle(TCPEvent.RCV_ACK)         # CLOSING -> TIME_WAIT
    peer_a.handle(TCPEvent.TIMEOUT_2MSL)    # TIME_WAIT -> CLOSED
    assert peer_a.state is TCPState.CLOSED


def _demo_v3_half_close_semantics() -> None:
    print("\n=== v3: 用 can_send()/can_receive() 观察半关闭语义 ===")
    conn = TCPConnectionV3()
    for event in (TCPEvent.ACTIVE_OPEN, TCPEvent.RCV_SYN_ACK):
        conn.handle(event)
    print(f"  ESTABLISHED: can_send={conn.can_send()}, can_receive={conn.can_receive()}")

    conn.handle(TCPEvent.APP_CLOSE)  # -> FIN_WAIT_1
    conn.handle(TCPEvent.RCV_ACK)    # -> FIN_WAIT_2
    print(f"  FIN_WAIT_2 (我方已关闭写方向): can_send={conn.can_send()}, "
          f"can_receive={conn.can_receive()}")

    passive = TCPConnectionV3()
    for event in (TCPEvent.PASSIVE_OPEN, TCPEvent.RCV_SYN, TCPEvent.RCV_ACK, TCPEvent.RCV_FIN):
        passive.handle(event)
    print(f"  CLOSE_WAIT (对方已关闭写方向): can_send={passive.can_send()}, "
          f"can_receive={passive.can_receive()}")


def _demo_v4_generic_framework() -> None:
    print("\n=== v4: 通用FSM框架跑一遍完整握手 ===")
    fsm = build_tcp_fsm()
    for event in (TCPEvent.ACTIVE_OPEN, TCPEvent.RCV_SYN_ACK):
        fsm.fire(event)
    print(f"  当前状态: {fsm.state.name}")
    assert fsm.state is TCPState.ESTABLISHED


if __name__ == "__main__":
    print("=== v1: Enum+match-case 手工验证一次三次握手 ===")
    conn_v1 = TCPConnectionV1()
    conn_v1.handle(TCPEvent.PASSIVE_OPEN)
    conn_v1.handle(TCPEvent.RCV_SYN)
    conn_v1.handle(TCPEvent.RCV_ACK)
    print(f"  最终状态: {conn_v1.state.name}\n")

    _demo_v2_full_lifecycle()
    _demo_v3_half_close_semantics()
    _demo_v4_generic_framework()
