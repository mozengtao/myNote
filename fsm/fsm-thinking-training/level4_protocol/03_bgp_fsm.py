"""Lesson 03: BGP FSM (Level 4 - 网络协议)

教学用简化模拟,依据 RFC4271 第8节简化建模,不是真实协议栈实现。
重点: 状态机与计时器(ConnectRetryTimer/HoldTimer/KeepaliveTimer)协同工作,
计时器超时本身就是一种"事件"。

四个版本:
    v1 BGPPeerV1     - Enum + match-case, 计时器作为事件喂入
    v2 BGPPeerV2     - Transition Table(含"状态不变"的自环转移)
    v3 BGPPeerV3     - State Pattern + TimerBank dataclass
    v4 BGPPeerV4     - 通用FSM框架 + tick驱动的模拟主循环
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


class BGPState(Enum):
    IDLE = auto()
    CONNECT = auto()
    ACTIVE = auto()
    OPEN_SENT = auto()
    OPEN_CONFIRM = auto()
    ESTABLISHED = auto()


class BGPEvent(Enum):
    MANUAL_START = auto()
    MANUAL_STOP = auto()
    TCP_CONNECTION_SUCCEEDS = auto()
    TCP_CONNECTION_FAILS = auto()
    CONNECT_RETRY_TIMER_EXPIRES = auto()
    RCV_OPEN_VALID = auto()
    RCV_OPEN_INVALID = auto()
    RCV_KEEPALIVE_CONFIRM = auto()
    RCV_NOTIFICATION = auto()
    HOLD_TIMER_EXPIRES = auto()
    KEEPALIVE_TIMER_EXPIRES = auto()


GLOBAL_DOWNGRADE_EVENTS: frozenset[BGPEvent] = frozenset(
    {BGPEvent.MANUAL_STOP, BGPEvent.RCV_NOTIFICATION}
)


class IllegalTransitionError(RuntimeError):
    pass


@dataclass
class TimerBank:
    """携带三种计时器剩余时间的数据。用 dataclass 是因为这些数值
    需要和状态一起被保存/打印/重置,而不是散落在多个裸变量里。
    """

    connect_retry_remaining: float = 0.0
    hold_remaining: float = 0.0
    keepalive_remaining: float = 0.0
    connect_retry_time: float = 30.0
    hold_time: float = 90.0
    keepalive_time: float = 30.0

    def reset_connect_retry(self) -> None:
        self.connect_retry_remaining = self.connect_retry_time

    def reset_hold_and_keepalive(self) -> None:
        self.hold_remaining = self.hold_time
        self.keepalive_remaining = self.keepalive_time

    def tick(self, seconds: float) -> list[BGPEvent]:
        """模拟时间推进,返回本次推进触发的计时器超时事件(可能为空)。"""
        fired: list[BGPEvent] = []
        if self.connect_retry_remaining > 0:
            self.connect_retry_remaining -= seconds
            if self.connect_retry_remaining <= 0:
                fired.append(BGPEvent.CONNECT_RETRY_TIMER_EXPIRES)
        if self.hold_remaining > 0:
            self.hold_remaining -= seconds
            if self.hold_remaining <= 0:
                fired.append(BGPEvent.HOLD_TIMER_EXPIRES)
        if self.keepalive_remaining > 0:
            self.keepalive_remaining -= seconds
            if self.keepalive_remaining <= 0:
                fired.append(BGPEvent.KEEPALIVE_TIMER_EXPIRES)
                self.keepalive_remaining = self.keepalive_time  # 周期性重新装填
        return fired


# ---------------------------------------------------------------------------
# v1: Enum + match-case, 计时器作为事件喂入(不做真实wall-clock计时)
# ---------------------------------------------------------------------------
class BGPPeerV1:
    def __init__(self, peer_as: int) -> None:
        self.peer_as = peer_as
        self.state: BGPState = BGPState.IDLE
        self.timers = TimerBank()

    def handle(self, event: BGPEvent) -> BGPState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = BGPState.IDLE
            return self.state

        match (self.state, event):
            case (BGPState.IDLE, BGPEvent.MANUAL_START):
                self.timers.reset_connect_retry()
                self.state = BGPState.CONNECT
            case (BGPState.CONNECT, BGPEvent.TCP_CONNECTION_SUCCEEDS):
                self.state = BGPState.OPEN_SENT
            case (BGPState.CONNECT, BGPEvent.TCP_CONNECTION_FAILS):
                self.state = BGPState.ACTIVE
            case (BGPState.CONNECT, BGPEvent.CONNECT_RETRY_TIMER_EXPIRES):
                self.timers.reset_connect_retry()  # 状态不变,重新尝试连接
            case (BGPState.ACTIVE, BGPEvent.TCP_CONNECTION_SUCCEEDS):
                self.state = BGPState.OPEN_SENT
            case (BGPState.ACTIVE, BGPEvent.CONNECT_RETRY_TIMER_EXPIRES):
                self.timers.reset_connect_retry()  # 状态不变,重新尝试连接
            case (BGPState.OPEN_SENT, BGPEvent.RCV_OPEN_VALID):
                self.state = BGPState.OPEN_CONFIRM
            case (BGPState.OPEN_SENT, BGPEvent.RCV_OPEN_INVALID):
                self.state = BGPState.IDLE
            case (BGPState.OPEN_SENT, BGPEvent.TCP_CONNECTION_FAILS):
                self.state = BGPState.ACTIVE
            case (BGPState.OPEN_CONFIRM, BGPEvent.RCV_KEEPALIVE_CONFIRM):
                self.timers.reset_hold_and_keepalive()
                self.state = BGPState.ESTABLISHED
            case (BGPState.OPEN_CONFIRM, BGPEvent.HOLD_TIMER_EXPIRES):
                self.state = BGPState.IDLE
            case (BGPState.OPEN_CONFIRM, BGPEvent.TCP_CONNECTION_FAILS):
                self.state = BGPState.ACTIVE
            case (BGPState.ESTABLISHED, BGPEvent.KEEPALIVE_TIMER_EXPIRES):
                pass  # 仅发送Keepalive,状态不变
            case (BGPState.ESTABLISHED, BGPEvent.RCV_KEEPALIVE_CONFIRM | BGPEvent.RCV_OPEN_VALID):
                self.timers.reset_hold_and_keepalive()  # 收到对端活跃迹象,重置HoldTimer
            case (BGPState.ESTABLISHED, BGPEvent.HOLD_TIMER_EXPIRES | BGPEvent.TCP_CONNECTION_FAILS):
                self.state = BGPState.IDLE
            case _:
                raise IllegalTransitionError(
                    f"AS{self.peer_as}: {self.state.name} 不接受事件 {event.name}"
                )
        return self.state


# ---------------------------------------------------------------------------
# v2: Transition Table 版("状态不变"的自环转移也老实写进表里)
# ---------------------------------------------------------------------------
BGP_TRANSITIONS: dict[tuple[BGPState, BGPEvent], tuple[BGPState, str]] = {
    (BGPState.IDLE, BGPEvent.MANUAL_START): (BGPState.CONNECT, "启动ConnectRetryTimer,发起TCP连接"),
    (BGPState.CONNECT, BGPEvent.TCP_CONNECTION_SUCCEEDS): (BGPState.OPEN_SENT, "发送本地OPEN报文"),
    (BGPState.CONNECT, BGPEvent.TCP_CONNECTION_FAILS): (BGPState.ACTIVE, "记录失败,准备重试"),
    (BGPState.CONNECT, BGPEvent.CONNECT_RETRY_TIMER_EXPIRES): (BGPState.CONNECT, "重新发起TCP连接(状态不变)"),
    (BGPState.ACTIVE, BGPEvent.TCP_CONNECTION_SUCCEEDS): (BGPState.OPEN_SENT, "发送本地OPEN报文"),
    (BGPState.ACTIVE, BGPEvent.CONNECT_RETRY_TIMER_EXPIRES): (BGPState.ACTIVE, "重新发起TCP连接(状态不变)"),
    (BGPState.OPEN_SENT, BGPEvent.RCV_OPEN_VALID): (BGPState.OPEN_CONFIRM, "发送Keepalive,等待确认"),
    (BGPState.OPEN_SENT, BGPEvent.RCV_OPEN_INVALID): (BGPState.IDLE, "发送Notification后退回Idle"),
    (BGPState.OPEN_SENT, BGPEvent.TCP_CONNECTION_FAILS): (BGPState.ACTIVE, "连接中断,回退重试"),
    (BGPState.OPEN_CONFIRM, BGPEvent.RCV_KEEPALIVE_CONFIRM): (BGPState.ESTABLISHED, "握手完成,启动Hold/KeepaliveTimer"),
    (BGPState.OPEN_CONFIRM, BGPEvent.HOLD_TIMER_EXPIRES): (BGPState.IDLE, "确认迟迟未到,判定超时失败"),
    (BGPState.OPEN_CONFIRM, BGPEvent.TCP_CONNECTION_FAILS): (BGPState.ACTIVE, "连接中断,回退重试"),
    (BGPState.ESTABLISHED, BGPEvent.KEEPALIVE_TIMER_EXPIRES): (BGPState.ESTABLISHED, "发送Keepalive(状态不变)"),
    (BGPState.ESTABLISHED, BGPEvent.RCV_KEEPALIVE_CONFIRM): (BGPState.ESTABLISHED, "重置HoldTimer(状态不变)"),
    (BGPState.ESTABLISHED, BGPEvent.RCV_OPEN_VALID): (BGPState.ESTABLISHED, "重置HoldTimer(状态不变)"),
    (BGPState.ESTABLISHED, BGPEvent.HOLD_TIMER_EXPIRES): (BGPState.IDLE, "会话失效,清空已学到的路由"),
    (BGPState.ESTABLISHED, BGPEvent.TCP_CONNECTION_FAILS): (BGPState.IDLE, "连接中断,会话彻底失效"),
}


class BGPPeerV2:
    def __init__(self, peer_as: int) -> None:
        self.peer_as = peer_as
        self.state: BGPState = BGPState.IDLE
        self.timers = TimerBank()

    def handle(self, event: BGPEvent) -> BGPState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self._transition(BGPState.IDLE, "管理员停用/收到错误通知")
            return self.state

        key = (self.state, event)
        if key not in BGP_TRANSITIONS:
            raise IllegalTransitionError(
                f"AS{self.peer_as}: {self.state.name} 不接受事件 {event.name}"
            )
        next_state, action = BGP_TRANSITIONS[key]
        self._transition(next_state, action)
        if event is BGPEvent.MANUAL_START:
            self.timers.reset_connect_retry()
        if event is BGPEvent.RCV_KEEPALIVE_CONFIRM and self.state is BGPState.ESTABLISHED:
            self.timers.reset_hold_and_keepalive()
        return self.state

    def _transition(self, next_state: BGPState, action: str) -> None:
        marker = "(自环)" if next_state is self.state else ""
        print(f"  [AS{self.peer_as}] {self.state.name:12s} -> {next_state.name:12s} {marker} ({action})")
        self.state = next_state


# ---------------------------------------------------------------------------
# v3: State Pattern + TimerBank
# ---------------------------------------------------------------------------
@dataclass
class _BGPStateHandler:
    state: BGPState
    transitions: dict[BGPEvent, BGPState] = field(default_factory=dict)

    def on_event(self, event: BGPEvent) -> BGPState | None:
        return self.transitions.get(event)


def _build_bgp_handlers() -> dict[BGPState, _BGPStateHandler]:
    return {
        BGPState.IDLE: _BGPStateHandler(BGPState.IDLE, {BGPEvent.MANUAL_START: BGPState.CONNECT}),
        BGPState.CONNECT: _BGPStateHandler(
            BGPState.CONNECT,
            {
                BGPEvent.TCP_CONNECTION_SUCCEEDS: BGPState.OPEN_SENT,
                BGPEvent.TCP_CONNECTION_FAILS: BGPState.ACTIVE,
                BGPEvent.CONNECT_RETRY_TIMER_EXPIRES: BGPState.CONNECT,
            },
        ),
        BGPState.ACTIVE: _BGPStateHandler(
            BGPState.ACTIVE,
            {
                BGPEvent.TCP_CONNECTION_SUCCEEDS: BGPState.OPEN_SENT,
                BGPEvent.CONNECT_RETRY_TIMER_EXPIRES: BGPState.ACTIVE,
            },
        ),
        BGPState.OPEN_SENT: _BGPStateHandler(
            BGPState.OPEN_SENT,
            {
                BGPEvent.RCV_OPEN_VALID: BGPState.OPEN_CONFIRM,
                BGPEvent.RCV_OPEN_INVALID: BGPState.IDLE,
                BGPEvent.TCP_CONNECTION_FAILS: BGPState.ACTIVE,
            },
        ),
        BGPState.OPEN_CONFIRM: _BGPStateHandler(
            BGPState.OPEN_CONFIRM,
            {
                BGPEvent.RCV_KEEPALIVE_CONFIRM: BGPState.ESTABLISHED,
                BGPEvent.HOLD_TIMER_EXPIRES: BGPState.IDLE,
                BGPEvent.TCP_CONNECTION_FAILS: BGPState.ACTIVE,
            },
        ),
        BGPState.ESTABLISHED: _BGPStateHandler(
            BGPState.ESTABLISHED,
            {
                BGPEvent.KEEPALIVE_TIMER_EXPIRES: BGPState.ESTABLISHED,
                BGPEvent.RCV_KEEPALIVE_CONFIRM: BGPState.ESTABLISHED,
                BGPEvent.RCV_OPEN_VALID: BGPState.ESTABLISHED,
                BGPEvent.HOLD_TIMER_EXPIRES: BGPState.IDLE,
                BGPEvent.TCP_CONNECTION_FAILS: BGPState.IDLE,
            },
        ),
    }


class BGPPeerV3:
    """每个状态是一个类;计时器数据独立用 TimerBank 携带,和状态解耦。"""

    def __init__(self, peer_as: int) -> None:
        self.peer_as = peer_as
        self._handlers = _build_bgp_handlers()
        self.state: BGPState = BGPState.IDLE
        self.timers = TimerBank()

    def handle(self, event: BGPEvent) -> BGPState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = BGPState.IDLE
            return self.state
        next_state = self._handlers[self.state].on_event(event)
        if next_state is None:
            raise IllegalTransitionError(
                f"AS{self.peer_as}: {self.state.name} 不接受事件 {event.name}"
            )
        if event is BGPEvent.MANUAL_START:
            self.timers.reset_connect_retry()
        elif event is BGPEvent.RCV_KEEPALIVE_CONFIRM and next_state is BGPState.ESTABLISHED:
            self.timers.reset_hold_and_keepalive()
        elif event is BGPEvent.RCV_KEEPALIVE_CONFIRM and self.state is BGPState.ESTABLISHED:
            self.timers.reset_hold_and_keepalive()
        self.state = next_state
        return self.state


# ---------------------------------------------------------------------------
# v4: 通用FSM框架 + tick驱动的模拟主循环
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Transition:
    from_state: BGPState
    event: BGPEvent
    to_state: BGPState
    action: Callable[[], None] | None = None


class GenericFSM:
    def __init__(self, initial: BGPState, transitions: list[Transition]) -> None:
        self.state = initial
        self._table: dict[tuple[BGPState, BGPEvent], Transition] = {
            (t.from_state, t.event): t for t in transitions
        }

    def fire(self, event: BGPEvent) -> BGPState:
        if event in GLOBAL_DOWNGRADE_EVENTS:
            self.state = BGPState.IDLE
            return self.state
        key = (self.state, event)
        if key not in self._table:
            raise IllegalTransitionError(f"{self.state.name} 不接受事件 {event.name}")
        transition = self._table[key]
        if transition.action:
            transition.action()
        self.state = transition.to_state
        return self.state


def _build_bgp_transitions() -> list[Transition]:
    return [
        Transition(from_state, event, to_state)
        for (from_state, event), (to_state, _action) in BGP_TRANSITIONS.items()
    ]


class BGPPeerV4:
    """把'状态机'和'计时器推进'封装成一个可以按时间片驱动的会话对象。"""

    def __init__(self, peer_as: int) -> None:
        self.peer_as = peer_as
        self.fsm = GenericFSM(BGPState.IDLE, _build_bgp_transitions())
        self.timers = TimerBank()

    def handle_event(self, event: BGPEvent) -> BGPState:
        state = self.fsm.fire(event)
        if event is BGPEvent.MANUAL_START:
            self.timers.reset_connect_retry()
        elif event is BGPEvent.CONNECT_RETRY_TIMER_EXPIRES:
            self.timers.reset_connect_retry()
        elif event is BGPEvent.TCP_CONNECTION_SUCCEEDS:
            # TCP已连接,ConnectRetryTimer的使命结束,停止计时避免继续误触发。
            self.timers.connect_retry_remaining = 0.0
        if event is BGPEvent.RCV_KEEPALIVE_CONFIRM and state is BGPState.ESTABLISHED:
            self.timers.reset_hold_and_keepalive()
        return state

    def tick(self, seconds: float) -> None:
        """模拟时间推进:计时器到期即注入对应事件,驱动FSM。"""
        for timer_event in self.timers.tick(seconds):
            print(f"  [AS{self.peer_as}] 计时器到期 -> 注入事件 {timer_event.name}")
            self.handle_event(timer_event)


# ---------------------------------------------------------------------------
# 演示
# ---------------------------------------------------------------------------
def _demo_v2_full_session() -> None:
    print("=== v2: AS65001 一次完整的会话建立(Idle -> Established) ===")
    peer = BGPPeerV2(peer_as=65001)
    peer.handle(BGPEvent.MANUAL_START)
    peer.handle(BGPEvent.TCP_CONNECTION_SUCCEEDS)
    peer.handle(BGPEvent.RCV_OPEN_VALID)
    peer.handle(BGPEvent.RCV_KEEPALIVE_CONFIRM)
    assert peer.state is BGPState.ESTABLISHED

    print("\n=== v2: 建连失败后经 Active 重试,最终成功 ===")
    flaky = BGPPeerV2(peer_as=65002)
    flaky.handle(BGPEvent.MANUAL_START)          # Idle -> Connect
    flaky.handle(BGPEvent.TCP_CONNECTION_FAILS)  # Connect -> Active(记录失败历史)
    flaky.handle(BGPEvent.CONNECT_RETRY_TIMER_EXPIRES)  # Active -> Active(自环,重试)
    flaky.handle(BGPEvent.TCP_CONNECTION_SUCCEEDS)      # Active -> OpenSent
    assert flaky.state is BGPState.OPEN_SENT

    print("\n=== v2: Established 状态下 HoldTimer 超时,会话跌回 Idle ===")
    peer.handle(BGPEvent.HOLD_TIMER_EXPIRES)
    assert peer.state is BGPState.IDLE


def _demo_v4_tick_driven_session() -> None:
    print("\n=== v4: 用 tick() 模拟时间推进,观察 KeepaliveTimer 周期性触发 ===")
    peer = BGPPeerV4(peer_as=65003)
    peer.timers.connect_retry_time = 5
    peer.timers.hold_time = 9
    peer.timers.keepalive_time = 3

    peer.handle_event(BGPEvent.MANUAL_START)
    peer.handle_event(BGPEvent.TCP_CONNECTION_SUCCEEDS)
    peer.handle_event(BGPEvent.RCV_OPEN_VALID)
    peer.handle_event(BGPEvent.RCV_KEEPALIVE_CONFIRM)
    print(f"  当前状态: {peer.fsm.state.name}")

    for second in range(1, 8):
        peer.tick(1)
        if second == 4:
            # 模拟收到对端 Keepalive,重置 HoldTimer,避免真的超时
            peer.handle_event(BGPEvent.RCV_KEEPALIVE_CONFIRM)
            print("  收到对端Keepalive,HoldTimer被重置")

    print(f"  7秒后状态仍为: {peer.fsm.state.name}")
    assert peer.fsm.state is BGPState.ESTABLISHED


if __name__ == "__main__":
    print("=== v1: Enum+match-case 手工验证一次基本推进 ===")
    peer_v1 = BGPPeerV1(peer_as=65000)
    peer_v1.handle(BGPEvent.MANUAL_START)
    peer_v1.handle(BGPEvent.TCP_CONNECTION_SUCCEEDS)
    peer_v1.handle(BGPEvent.RCV_OPEN_VALID)
    peer_v1.handle(BGPEvent.RCV_KEEPALIVE_CONFIRM)
    print(f"  AS{peer_v1.peer_as} 当前状态: {peer_v1.state.name}\n")

    _demo_v2_full_session()
    _demo_v4_tick_driven_session()

    print("\n=== v3: State Pattern 版验证同一条完整路径 ===")
    peer_v3 = BGPPeerV3(peer_as=65004)
    for event in (
        BGPEvent.MANUAL_START,
        BGPEvent.TCP_CONNECTION_SUCCEEDS,
        BGPEvent.RCV_OPEN_VALID,
        BGPEvent.RCV_KEEPALIVE_CONFIRM,
    ):
        peer_v3.handle(event)
    print(f"  AS{peer_v3.peer_as}: state={peer_v3.state.name}, "
          f"hold_remaining={peer_v3.timers.hold_remaining}")
