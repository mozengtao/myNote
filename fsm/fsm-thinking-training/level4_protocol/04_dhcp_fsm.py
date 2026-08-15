"""Lesson 04: DHCP客户端FSM (Level 4 - 网络协议)

教学用简化模拟,依据 RFC2131 第4.4节简化建模,不是真实协议栈实现。
重点: "租约(lease)"这种带时间属性的状态,以及T1/T2计时器触发续租。

四个版本:
    v1 DHCPClientV1  - Enum + match-case, Lease作为伴随数据字段
    v2 DHCPClientV2  - Transition Table(计时器事件与报文事件同等对待)
    v3 DHCPClientV3  - State Pattern(RENEWING/REBINDING各自声明请求范围)
    v4 DHCPClientV4  - 通用FSM框架 + tick驱动的租约生命周期模拟
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Literal


class DHCPState(Enum):
    INIT = auto()
    SELECTING = auto()
    REQUESTING = auto()
    BOUND = auto()
    RENEWING = auto()
    REBINDING = auto()
    INIT_REBOOT = auto()
    REBOOTING = auto()


class DHCPEvent(Enum):
    DISCOVER_NEEDED = auto()
    HAS_CACHED_LEASE = auto()
    OFFER_SELECTED = auto()
    RCV_ACK = auto()
    RCV_NAK = auto()
    TIMEOUT = auto()
    T1_EXPIRES = auto()
    T2_EXPIRES = auto()
    LEASE_EXPIRES = auto()
    APP_RELEASE = auto()


class IllegalTransitionError(RuntimeError):
    pass


@dataclass
class Lease:
    """租约本身就是一份带时间属性的数据,而不是单纯的状态名字能表达的。"""

    ip_address: str
    lease_time: float
    t1_time: float = field(init=False)
    t2_time: float = field(init=False)
    elapsed: float = 0.0

    def __post_init__(self) -> None:
        self.t1_time = self.lease_time * 0.5
        self.t2_time = self.lease_time * 0.875

    def renew(self, new_lease_time: float | None = None) -> None:
        """续租成功后刷新计时,状态可能仍是BOUND,但伴随数据必须重置。"""
        self.lease_time = new_lease_time if new_lease_time is not None else self.lease_time
        self.t1_time = self.lease_time * 0.5
        self.t2_time = self.lease_time * 0.875
        self.elapsed = 0.0

    def tick(self, seconds: float) -> DHCPEvent | None:
        """时间推进,按 T1 < T2 < lease_time 的顺序只报告第一个越过的门槛。"""
        before = self.elapsed
        self.elapsed += seconds
        if before < self.t1_time <= self.elapsed:
            return DHCPEvent.T1_EXPIRES
        if before < self.t2_time <= self.elapsed:
            return DHCPEvent.T2_EXPIRES
        if before < self.lease_time <= self.elapsed:
            return DHCPEvent.LEASE_EXPIRES
        return None


# ---------------------------------------------------------------------------
# v1: Enum + match-case, Lease 作为伴随数据字段
# ---------------------------------------------------------------------------
class DHCPClientV1:
    def __init__(self) -> None:
        self.state: DHCPState = DHCPState.INIT
        self.lease: Lease | None = None

    def handle(self, event: DHCPEvent, offered_ip: str = "", lease_time: float = 3600.0) -> DHCPState:
        match (self.state, event):
            case (DHCPState.INIT, DHCPEvent.DISCOVER_NEEDED):
                self.state = DHCPState.SELECTING
            case (DHCPState.INIT, DHCPEvent.HAS_CACHED_LEASE):
                self.state = DHCPState.INIT_REBOOT
            case (DHCPState.INIT_REBOOT, DHCPEvent.OFFER_SELECTED):
                self.state = DHCPState.REBOOTING
            case (DHCPState.REBOOTING, DHCPEvent.RCV_ACK):
                self.lease = Lease(offered_ip, lease_time)
                self.state = DHCPState.BOUND
            case (DHCPState.REBOOTING, DHCPEvent.RCV_NAK | DHCPEvent.TIMEOUT):
                self.state = DHCPState.INIT
            case (DHCPState.SELECTING, DHCPEvent.OFFER_SELECTED):
                self.state = DHCPState.REQUESTING
            case (DHCPState.REQUESTING, DHCPEvent.RCV_ACK):
                self.lease = Lease(offered_ip, lease_time)
                self.state = DHCPState.BOUND
            case (DHCPState.REQUESTING, DHCPEvent.RCV_NAK | DHCPEvent.TIMEOUT):
                self.state = DHCPState.INIT
            case (DHCPState.BOUND, DHCPEvent.T1_EXPIRES):
                self.state = DHCPState.RENEWING
            case (DHCPState.BOUND, DHCPEvent.APP_RELEASE):
                self.lease = None
                self.state = DHCPState.INIT
            case (DHCPState.RENEWING, DHCPEvent.RCV_ACK):
                assert self.lease is not None
                self.lease.renew(lease_time)
                self.state = DHCPState.BOUND
            case (DHCPState.RENEWING, DHCPEvent.RCV_NAK):
                self.lease = None
                self.state = DHCPState.INIT
            case (DHCPState.RENEWING, DHCPEvent.T2_EXPIRES):
                self.state = DHCPState.REBINDING
            case (DHCPState.REBINDING, DHCPEvent.RCV_ACK):
                assert self.lease is not None
                self.lease.renew(lease_time)
                self.state = DHCPState.BOUND
            case (DHCPState.REBINDING, DHCPEvent.RCV_NAK | DHCPEvent.LEASE_EXPIRES):
                self.lease = None
                self.state = DHCPState.INIT
            case _:
                raise IllegalTransitionError(
                    f"{self.state.name} 状态不接受事件 {event.name}"
                )
        return self.state


# ---------------------------------------------------------------------------
# v2: Transition Table 版
# ---------------------------------------------------------------------------
DHCP_TRANSITIONS: dict[tuple[DHCPState, DHCPEvent], tuple[DHCPState, str]] = {
    (DHCPState.INIT, DHCPEvent.DISCOVER_NEEDED): (DHCPState.SELECTING, "广播DHCPDISCOVER"),
    (DHCPState.INIT, DHCPEvent.HAS_CACHED_LEASE): (DHCPState.INIT_REBOOT, "准备复用历史缓存地址"),
    (DHCPState.INIT_REBOOT, DHCPEvent.OFFER_SELECTED): (DHCPState.REBOOTING, "广播DHCPREQUEST确认旧地址"),
    (DHCPState.REBOOTING, DHCPEvent.RCV_ACK): (DHCPState.BOUND, "复用旧地址成功"),
    (DHCPState.REBOOTING, DHCPEvent.RCV_NAK): (DHCPState.INIT, "旧地址不可用,回到INIT"),
    (DHCPState.REBOOTING, DHCPEvent.TIMEOUT): (DHCPState.INIT, "无应答超时,回到INIT"),
    (DHCPState.SELECTING, DHCPEvent.OFFER_SELECTED): (DHCPState.REQUESTING, "广播DHCPREQUEST确认选择"),
    (DHCPState.REQUESTING, DHCPEvent.RCV_ACK): (DHCPState.BOUND, "租约生效"),
    (DHCPState.REQUESTING, DHCPEvent.RCV_NAK): (DHCPState.INIT, "申请被拒,重新开始"),
    (DHCPState.REQUESTING, DHCPEvent.TIMEOUT): (DHCPState.INIT, "无应答超时,重新开始"),
    (DHCPState.BOUND, DHCPEvent.T1_EXPIRES): (DHCPState.RENEWING, "单播续租(向原服务器)"),
    (DHCPState.BOUND, DHCPEvent.APP_RELEASE): (DHCPState.INIT, "主动释放地址"),
    (DHCPState.RENEWING, DHCPEvent.RCV_ACK): (DHCPState.BOUND, "续租成功,刷新T1/T2"),
    (DHCPState.RENEWING, DHCPEvent.RCV_NAK): (DHCPState.INIT, "续租被拒,放弃地址"),
    (DHCPState.RENEWING, DHCPEvent.T2_EXPIRES): (DHCPState.REBINDING, "单播无响应,改为广播续租"),
    (DHCPState.REBINDING, DHCPEvent.RCV_ACK): (DHCPState.BOUND, "续租成功,刷新T1/T2"),
    (DHCPState.REBINDING, DHCPEvent.RCV_NAK): (DHCPState.INIT, "续租被拒,放弃地址"),
    (DHCPState.REBINDING, DHCPEvent.LEASE_EXPIRES): (DHCPState.INIT, "广播续租仍无响应且租约到期"),
}


class DHCPClientV2:
    def __init__(self) -> None:
        self.state: DHCPState = DHCPState.INIT
        self.lease: Lease | None = None

    def handle(self, event: DHCPEvent, offered_ip: str = "", lease_time: float = 3600.0) -> DHCPState:
        key = (self.state, event)
        if key not in DHCP_TRANSITIONS:
            raise IllegalTransitionError(
                f"{self.state.name} 状态不接受事件 {event.name}"
            )
        next_state, action = DHCP_TRANSITIONS[key]
        print(f"  [v2] {self.state.name:12s} --{event.name:16s}--> {next_state.name:12s} ({action})")

        if event is DHCPEvent.RCV_ACK and next_state is DHCPState.BOUND:
            if self.lease is None:
                self.lease = Lease(offered_ip or "0.0.0.0", lease_time)
            else:
                self.lease.renew(lease_time)
        if next_state is DHCPState.INIT:
            self.lease = None

        self.state = next_state
        return self.state


# ---------------------------------------------------------------------------
# v3: State Pattern(体现RENEWING/REBINDING的请求范围差异)
# ---------------------------------------------------------------------------
RequestScope = Literal["none", "unicast", "broadcast"]


@dataclass
class _DHCPStateHandler:
    state: DHCPState
    transitions: dict[DHCPEvent, DHCPState] = field(default_factory=dict)
    request_scope: RequestScope = "none"

    def on_event(self, event: DHCPEvent) -> DHCPState | None:
        return self.transitions.get(event)


def _build_dhcp_handlers() -> dict[DHCPState, _DHCPStateHandler]:
    return {
        DHCPState.INIT: _DHCPStateHandler(
            DHCPState.INIT,
            {DHCPEvent.DISCOVER_NEEDED: DHCPState.SELECTING, DHCPEvent.HAS_CACHED_LEASE: DHCPState.INIT_REBOOT},
        ),
        DHCPState.INIT_REBOOT: _DHCPStateHandler(
            DHCPState.INIT_REBOOT, {DHCPEvent.OFFER_SELECTED: DHCPState.REBOOTING}, request_scope="broadcast"
        ),
        DHCPState.REBOOTING: _DHCPStateHandler(
            DHCPState.REBOOTING,
            {DHCPEvent.RCV_ACK: DHCPState.BOUND, DHCPEvent.RCV_NAK: DHCPState.INIT, DHCPEvent.TIMEOUT: DHCPState.INIT},
        ),
        DHCPState.SELECTING: _DHCPStateHandler(
            DHCPState.SELECTING, {DHCPEvent.OFFER_SELECTED: DHCPState.REQUESTING}, request_scope="broadcast"
        ),
        DHCPState.REQUESTING: _DHCPStateHandler(
            DHCPState.REQUESTING,
            {DHCPEvent.RCV_ACK: DHCPState.BOUND, DHCPEvent.RCV_NAK: DHCPState.INIT, DHCPEvent.TIMEOUT: DHCPState.INIT},
        ),
        DHCPState.BOUND: _DHCPStateHandler(
            DHCPState.BOUND, {DHCPEvent.T1_EXPIRES: DHCPState.RENEWING, DHCPEvent.APP_RELEASE: DHCPState.INIT}
        ),
        # 关键差异点: RENEWING 用单播,REBINDING 用广播 —— 续租紧迫程度的递进策略。
        DHCPState.RENEWING: _DHCPStateHandler(
            DHCPState.RENEWING,
            {
                DHCPEvent.RCV_ACK: DHCPState.BOUND,
                DHCPEvent.RCV_NAK: DHCPState.INIT,
                DHCPEvent.T2_EXPIRES: DHCPState.REBINDING,
            },
            request_scope="unicast",
        ),
        DHCPState.REBINDING: _DHCPStateHandler(
            DHCPState.REBINDING,
            {
                DHCPEvent.RCV_ACK: DHCPState.BOUND,
                DHCPEvent.RCV_NAK: DHCPState.INIT,
                DHCPEvent.LEASE_EXPIRES: DHCPState.INIT,
            },
            request_scope="broadcast",
        ),
    }


class DHCPClientV3:
    def __init__(self) -> None:
        self._handlers = _build_dhcp_handlers()
        self.state: DHCPState = DHCPState.INIT
        self.lease: Lease | None = None

    def handle(self, event: DHCPEvent, offered_ip: str = "", lease_time: float = 3600.0) -> DHCPState:
        next_state = self._handlers[self.state].on_event(event)
        if next_state is None:
            raise IllegalTransitionError(
                f"{self.state.name} 状态不接受事件 {event.name}"
            )
        if event is DHCPEvent.RCV_ACK and next_state is DHCPState.BOUND:
            if self.lease is None:
                self.lease = Lease(offered_ip or "0.0.0.0", lease_time)
            else:
                self.lease.renew(lease_time)
        if next_state is DHCPState.INIT:
            self.lease = None
        self.state = next_state
        return self.state

    def current_request_scope(self) -> RequestScope:
        return self._handlers[self.state].request_scope


# ---------------------------------------------------------------------------
# v4: 通用FSM框架 + tick驱动的租约生命周期模拟
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Transition:
    from_state: DHCPState
    event: DHCPEvent
    to_state: DHCPState
    action: Callable[[], None] | None = None


class GenericFSM:
    def __init__(self, initial: DHCPState, transitions: list[Transition]) -> None:
        self.state = initial
        self._table: dict[tuple[DHCPState, DHCPEvent], Transition] = {
            (t.from_state, t.event): t for t in transitions
        }

    def fire(self, event: DHCPEvent) -> DHCPState:
        key = (self.state, event)
        if key not in self._table:
            raise IllegalTransitionError(f"{self.state.name} 不接受事件 {event.name}")
        transition = self._table[key]
        if transition.action:
            transition.action()
        self.state = transition.to_state
        return self.state


def _build_dhcp_transitions() -> list[Transition]:
    return [
        Transition(from_state, event, to_state)
        for (from_state, event), (to_state, _action) in DHCP_TRANSITIONS.items()
    ]


class DHCPClientV4:
    """状态机 + 租约计时的组合驱动:tick() 模拟时间推进,自动生成T1/T2/到期事件。"""

    def __init__(self) -> None:
        self.fsm = GenericFSM(DHCPState.INIT, _build_dhcp_transitions())
        self.lease: Lease | None = None

    def handle_event(self, event: DHCPEvent, offered_ip: str = "", lease_time: float = 3600.0) -> DHCPState:
        state = self.fsm.fire(event)
        if event is DHCPEvent.RCV_ACK and state is DHCPState.BOUND:
            if self.lease is None:
                self.lease = Lease(offered_ip or "0.0.0.0", lease_time)
            else:
                self.lease.renew(lease_time)
        if state is DHCPState.INIT:
            self.lease = None
        return state

    def tick(self, seconds: float) -> None:
        if self.lease is None:
            return
        fired = self.lease.tick(seconds)
        if fired is not None:
            print(f"  租约计时器到期 -> 注入事件 {fired.name}")
            self.handle_event(fired)


# ---------------------------------------------------------------------------
# 演示
# ---------------------------------------------------------------------------
def _demo_v2_normal_lifecycle() -> None:
    print("=== v2: 从零申请地址 -> 正常使用 -> 续租成功 ===")
    client = DHCPClientV2()
    client.handle(DHCPEvent.DISCOVER_NEEDED)
    client.handle(DHCPEvent.OFFER_SELECTED)
    client.handle(DHCPEvent.RCV_ACK, offered_ip="192.168.1.100", lease_time=3600)
    assert client.state is DHCPState.BOUND
    assert client.lease is not None and client.lease.ip_address == "192.168.1.100"

    client.handle(DHCPEvent.T1_EXPIRES)
    client.handle(DHCPEvent.RCV_ACK, lease_time=3600)
    assert client.state is DHCPState.BOUND

    print("\n=== v2: 单播续租无响应 -> 广播续租仍无响应 -> 放弃地址 ===")
    unlucky = DHCPClientV2()
    unlucky.handle(DHCPEvent.DISCOVER_NEEDED)
    unlucky.handle(DHCPEvent.OFFER_SELECTED)
    unlucky.handle(DHCPEvent.RCV_ACK, offered_ip="192.168.1.200", lease_time=3600)
    unlucky.handle(DHCPEvent.T1_EXPIRES)
    unlucky.handle(DHCPEvent.T2_EXPIRES)
    unlucky.handle(DHCPEvent.LEASE_EXPIRES)
    assert unlucky.state is DHCPState.INIT
    assert unlucky.lease is None


def _demo_v3_init_reboot_shortcut() -> None:
    print("\n=== v3: INIT-REBOOT 捷径复用历史地址成功 ===")
    client = DHCPClientV3()
    client.handle(DHCPEvent.HAS_CACHED_LEASE)
    print(f"  {client.state.name}: request_scope={client.current_request_scope()}")
    client.handle(DHCPEvent.OFFER_SELECTED)
    client.handle(DHCPEvent.RCV_ACK, offered_ip="10.0.0.50", lease_time=7200)
    assert client.state is DHCPState.BOUND
    print(f"  复用成功,当前地址: {client.lease.ip_address if client.lease else None}")


def _demo_v4_tick_driven_lease() -> None:
    print("\n=== v4: tick() 驱动完整租约生命周期(BOUND -> RENEWING -> 续租成功) ===")
    client = DHCPClientV4()
    client.handle_event(DHCPEvent.DISCOVER_NEEDED)
    client.handle_event(DHCPEvent.OFFER_SELECTED)
    client.handle_event(DHCPEvent.RCV_ACK, offered_ip="172.16.0.10", lease_time=10)
    print(f"  初始状态: {client.fsm.state.name}, 租约: {client.lease}")

    for second in range(1, 6):
        client.tick(1)
        if client.fsm.state is DHCPState.RENEWING:
            print(f"  第{second}秒: 已进入 RENEWING,模拟服务器确认续租")
            client.handle_event(DHCPEvent.RCV_ACK, lease_time=10)
            break

    print(f"  续租后状态: {client.fsm.state.name}, 新租约: {client.lease}")
    assert client.fsm.state is DHCPState.BOUND


if __name__ == "__main__":
    print("=== v1: Enum+match-case 手工验证一次基本申请流程 ===")
    client_v1 = DHCPClientV1()
    client_v1.handle(DHCPEvent.DISCOVER_NEEDED)
    client_v1.handle(DHCPEvent.OFFER_SELECTED)
    client_v1.handle(DHCPEvent.RCV_ACK, offered_ip="192.168.1.10", lease_time=1800)
    print(f"  当前状态: {client_v1.state.name}, 租约: {client_v1.lease}\n")

    _demo_v2_normal_lifecycle()
    _demo_v3_init_reboot_shortcut()
    _demo_v4_tick_driven_lease()
