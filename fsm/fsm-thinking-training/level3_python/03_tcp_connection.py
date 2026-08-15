"""Lesson 03: 应用层视角的TCP连接状态机 (Level 3 - Python工程场景).

配合 03_tcp_connection.py 阅读。这里建模的是"应用开发者用socket API
时能观察到的粗粒度阶段",不是TCP协议内部的11状态机(那是Level 4的
内容)。

四版实现:
    v1: Enum + match-case
    v2: Transition Table(只放"真正的转移",发送数据的合法性检查独立)
    v3: State Pattern(每个状态给出针对性的拒绝理由)
    v4: 通用FSM框架
"""

from __future__ import annotations

from abc import ABC
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum, auto


class ConnState(Enum):
    CLOSED = auto()
    CONNECTING = auto()
    CONNECTED = auto()
    CLOSING = auto()


class ConnEvent(Enum):
    CONNECT = auto()
    CONNECT_OK = auto()
    CONNECT_FAILED = auto()
    CLOSE = auto()
    PEER_CLOSED = auto()
    CLOSE_DONE = auto()


class InvalidTransitionError(Exception):
    """收到当前状态不允许的事件——通常意味着底层实现有bug(迟到的回调)。"""

    def __init__(self, state: ConnState, event: ConnEvent) -> None:
        self.state = state
        self.event = event
        super().__init__(
            f"非法转移: 当前状态={state.name}, 事件={event.name} 不允许发生"
        )


class SendNotAllowedError(Exception):
    """尝试在非CONNECTED状态下发送数据——这不是转移,是非转移性合法性检查。"""

    def __init__(self, state: ConnState, reason: str) -> None:
        self.state = state
        super().__init__(reason)


def _send_rejection_reason(state: ConnState) -> str:
    match state:
        case ConnState.CLOSED:
            return "连接尚未建立,请先调用 connect()"
        case ConnState.CONNECTING:
            return "连接正在建立中,请等待完成后再发送"
        case ConnState.CLOSING:
            return "连接正在关闭中,不能再发送新数据"
        case ConnState.CONNECTED:
            raise AssertionError("CONNECTED 状态下不应调用本函数")


# ============================================================
# v1: 最简单版本 —— Enum + match-case
#
# 设计理由: send() 在不同状态下要给出三种不同的拒绝理由,这一版
# 把这三个分支直接写在同一个方法里,读起来一目了然;转移逻辑同样
# 用 match 写在各自的方法中,能立刻跑起来,但转移规则分散。
# ============================================================


@dataclass
class ConnectionV1:
    state: ConnState = ConnState.CLOSED
    last_error: str | None = None

    def connect(self) -> None:
        match self.state:
            case ConnState.CLOSED:
                self.state = ConnState.CONNECTING
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.CONNECT)

    def connect_ok(self) -> None:
        match self.state:
            case ConnState.CONNECTING:
                self.state = ConnState.CONNECTED
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.CONNECT_OK)

    def connect_failed(self, reason: str) -> None:
        match self.state:
            case ConnState.CONNECTING:
                self.state = ConnState.CLOSED
                self.last_error = reason
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.CONNECT_FAILED)

    def close(self) -> None:
        match self.state:
            case ConnState.CONNECTED:
                self.state = ConnState.CLOSING
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.CLOSE)

    def peer_closed(self) -> None:
        match self.state:
            case ConnState.CONNECTED:
                self.state = ConnState.CLOSING
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.PEER_CLOSED)

    def close_done(self) -> None:
        match self.state:
            case ConnState.CLOSING:
                self.state = ConnState.CLOSED
            case _:
                raise InvalidTransitionError(self.state, ConnEvent.CLOSE_DONE)

    def send(self, data: bytes) -> None:
        if self.state is not ConnState.CONNECTED:
            raise SendNotAllowedError(self.state, _send_rejection_reason(self.state))


# ============================================================
# v2: Transition Table
#
# 相比v1的改进: 表里只放"真正会引起状态跳变"的 (状态,事件),
# send() 是一次非转移性操作,单独用 can_send() 做合法性检查,
# 不混进转移表——转移表的职责应该保持单一。
# ============================================================


@dataclass
class ConnectionV2:
    state: ConnState = ConnState.CLOSED
    last_error: str | None = None
    log: list[str] = field(default_factory=list)


def _v2_connect(c: ConnectionV2) -> None:
    c.state = ConnState.CONNECTING
    c.log.append("开始连接")


def _v2_connect_ok(c: ConnectionV2) -> None:
    c.state = ConnState.CONNECTED
    c.log.append("连接建立成功")


def _v2_connect_failed(c: ConnectionV2, reason: str = "unknown") -> None:
    c.state = ConnState.CLOSED
    c.last_error = reason
    c.log.append(f"连接失败: {reason}")


def _v2_close(c: ConnectionV2) -> None:
    c.state = ConnState.CLOSING
    c.log.append("本地主动关闭")


def _v2_peer_closed(c: ConnectionV2) -> None:
    c.state = ConnState.CLOSING
    c.log.append("检测到对端关闭")


def _v2_close_done(c: ConnectionV2) -> None:
    c.state = ConnState.CLOSED
    c.log.append("关闭收尾完成")


V2_TRANSITIONS: dict[tuple[ConnState, ConnEvent], Callable[..., None]] = {
    (ConnState.CLOSED, ConnEvent.CONNECT): _v2_connect,
    (ConnState.CONNECTING, ConnEvent.CONNECT_OK): _v2_connect_ok,
    (ConnState.CONNECTING, ConnEvent.CONNECT_FAILED): _v2_connect_failed,
    (ConnState.CONNECTED, ConnEvent.CLOSE): _v2_close,
    (ConnState.CONNECTED, ConnEvent.PEER_CLOSED): _v2_peer_closed,
    (ConnState.CLOSING, ConnEvent.CLOSE_DONE): _v2_close_done,
}


def apply_event_v2(c: ConnectionV2, event: ConnEvent, **kwargs: object) -> None:
    handler = V2_TRANSITIONS.get((c.state, event))
    if handler is None:
        raise InvalidTransitionError(c.state, event)
    handler(c, **kwargs)  # type: ignore[arg-type]


def can_send_v2(c: ConnectionV2) -> None:
    """非转移性的合法性检查,独立于转移表之外。"""
    if c.state is not ConnState.CONNECTED:
        raise SendNotAllowedError(c.state, _send_rejection_reason(c.state))


# ============================================================
# v3: State Pattern
#
# 适用场景: 每个状态类里既处理转移,也提供自己版本的"发送数据"
# 合法性检查(返回值或抛异常的理由由状态类自己决定),把状态相关
# 的所有行为(转移 + 拒绝理由)聚合到一处。
# ============================================================


@dataclass
class ConnectionV3:
    state: ConnState = ConnState.CLOSED
    last_error: str | None = None


class ConnStateHandler(ABC):
    state: ConnState

    def connect(self, c: ConnectionV3) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.CONNECT)

    def connect_ok(self, c: ConnectionV3) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.CONNECT_OK)

    def connect_failed(self, c: ConnectionV3, reason: str) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.CONNECT_FAILED)

    def close(self, c: ConnectionV3) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.CLOSE)

    def peer_closed(self, c: ConnectionV3) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.PEER_CLOSED)

    def close_done(self, c: ConnectionV3) -> None:
        raise InvalidTransitionError(self.state, ConnEvent.CLOSE_DONE)

    def check_send_allowed(self) -> None:
        raise SendNotAllowedError(self.state, _send_rejection_reason(self.state))


class _ClosedHandler(ConnStateHandler):
    state = ConnState.CLOSED

    def connect(self, c: ConnectionV3) -> None:
        c.state = ConnState.CONNECTING


class _ConnectingHandler(ConnStateHandler):
    state = ConnState.CONNECTING

    def connect_ok(self, c: ConnectionV3) -> None:
        c.state = ConnState.CONNECTED

    def connect_failed(self, c: ConnectionV3, reason: str) -> None:
        c.state = ConnState.CLOSED
        c.last_error = reason


class _ConnectedHandler(ConnStateHandler):
    state = ConnState.CONNECTED

    def close(self, c: ConnectionV3) -> None:
        c.state = ConnState.CLOSING

    def peer_closed(self, c: ConnectionV3) -> None:
        c.state = ConnState.CLOSING

    def check_send_allowed(self) -> None:
        return None  # CONNECTED 状态下允许发送


class _ClosingHandler(ConnStateHandler):
    state = ConnState.CLOSING

    def close_done(self, c: ConnectionV3) -> None:
        c.state = ConnState.CLOSED


_V3_HANDLERS: dict[ConnState, ConnStateHandler] = {
    ConnState.CLOSED: _ClosedHandler(),
    ConnState.CONNECTING: _ConnectingHandler(),
    ConnState.CONNECTED: _ConnectedHandler(),
    ConnState.CLOSING: _ClosingHandler(),
}


def _v3_handler_for(c: ConnectionV3) -> ConnStateHandler:
    return _V3_HANDLERS[c.state]


def apply_event_v3(c: ConnectionV3, event: ConnEvent, **kwargs: object) -> None:
    handler = _v3_handler_for(c)
    method: Callable[..., None] = getattr(handler, event.name.lower())
    method(c, **kwargs)


def can_send_v3(c: ConnectionV3) -> None:
    _v3_handler_for(c).check_send_allowed()


# ============================================================
# v4: 通用FSM框架
# ============================================================


@dataclass(frozen=True)
class Transition[S: Enum, E: Enum]:
    source: S
    event: E
    target: S
    action: Callable[[dict[str, object]], None] | None = None


class GenericFSM[S: Enum, E: Enum]:
    def __init__(self, initial: S, transitions: list[Transition[S, E]]) -> None:
        self._state = initial
        self._table: dict[tuple[S, E], Transition[S, E]] = {
            (t.source, t.event): t for t in transitions
        }
        self.history: list[str] = []

    @property
    def state(self) -> S:
        return self._state

    def fire(self, event: E, context: dict[str, object] | None = None) -> S:
        key = (self._state, event)
        transition = self._table.get(key)
        if transition is None:
            raise InvalidTransitionError(self._state, event)  # type: ignore[arg-type]
        if transition.action is not None:
            transition.action(context or {})
        self.history.append(
            f"{self._state.name} --{event.name}--> {transition.target.name}"
        )
        self._state = transition.target
        return self._state


def build_connection_fsm() -> GenericFSM[ConnState, ConnEvent]:
    transitions = [
        Transition(ConnState.CLOSED, ConnEvent.CONNECT, ConnState.CONNECTING),
        Transition(ConnState.CONNECTING, ConnEvent.CONNECT_OK, ConnState.CONNECTED),
        Transition(ConnState.CONNECTING, ConnEvent.CONNECT_FAILED, ConnState.CLOSED),
        Transition(ConnState.CONNECTED, ConnEvent.CLOSE, ConnState.CLOSING),
        Transition(ConnState.CONNECTED, ConnEvent.PEER_CLOSED, ConnState.CLOSING),
        Transition(ConnState.CLOSING, ConnEvent.CLOSE_DONE, ConnState.CLOSED),
    ]
    return GenericFSM(ConnState.CLOSED, transitions)


def can_send_v4(fsm: GenericFSM[ConnState, ConnEvent]) -> None:
    if fsm.state is not ConnState.CONNECTED:
        raise SendNotAllowedError(fsm.state, _send_rejection_reason(fsm.state))


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("--- v1: Enum + match-case ---")
    conn = ConnectionV1()
    try:
        conn.send(b"hello")
    except SendNotAllowedError as exc:
        print(f"预期的拒绝(CLOSED状态下发送): {exc}")

    conn.connect()
    print(f"状态: {conn.state.name}")
    try:
        conn.send(b"hello")
    except SendNotAllowedError as exc:
        print(f"预期的拒绝(CONNECTING状态下发送): {exc}")

    conn.connect_ok()
    conn.send(b"hello")  # 合法
    print(f"CONNECTED状态下发送成功,状态仍为: {conn.state.name}")

    conn.close()
    try:
        conn.connect()
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v2() -> None:
    print("\n--- v2: Transition Table ---")
    conn = ConnectionV2()
    apply_event_v2(conn, ConnEvent.CONNECT)
    apply_event_v2(conn, ConnEvent.CONNECT_OK)
    can_send_v2(conn)
    print(f"状态: {conn.state.name}, 日志: {conn.log}")

    apply_event_v2(conn, ConnEvent.PEER_CLOSED)
    print(f"对端关闭后状态: {conn.state.name}")
    try:
        can_send_v2(conn)
    except SendNotAllowedError as exc:
        print(f"预期的拒绝(CLOSING状态下发送): {exc}")

    apply_event_v2(conn, ConnEvent.CLOSE_DONE)
    print(f"最终状态: {conn.state.name}")


def _demo_v3() -> None:
    print("\n--- v3: State Pattern ---")
    conn = ConnectionV3()
    apply_event_v3(conn, ConnEvent.CONNECT)
    apply_event_v3(conn, ConnEvent.CONNECT_FAILED, reason="connection refused")
    print(f"连接失败后状态: {conn.state.name}, 错误: {conn.last_error}")

    try:
        apply_event_v3(conn, ConnEvent.CONNECT_OK)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v4() -> None:
    print("\n--- v4: 通用FSM框架 ---")
    fsm = build_connection_fsm()
    fsm.fire(ConnEvent.CONNECT)
    fsm.fire(ConnEvent.CONNECT_OK)
    can_send_v4(fsm)
    print(f"状态: {fsm.state.name}")

    fsm.fire(ConnEvent.CLOSE)
    fsm.fire(ConnEvent.CLOSE_DONE)
    print(f"最终状态: {fsm.state.name}")

    try:
        can_send_v4(fsm)
    except SendNotAllowedError as exc:
        print(f"预期的拒绝(CLOSED状态下发送): {exc}")

    print("转移历史:")
    for line in fsm.history:
        print(f"  {line}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
