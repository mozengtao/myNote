"""
Lesson 02: NetworkManager连接状态 (Level 5 - Linux系统)

教学用简化模型,模拟NetworkManager的Device状态机(NM_DEVICE_STATE)
以及与之协作的ActiveConnection状态机之间的分层关系。
不是NetworkManager的真实实现,只提炼"设备FSM vs 连接profile FSM分层"这一核心教学点。

Python 3.12+, 无第三方依赖。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


# ---------------------------------------------------------------------------
# v1: 简单版 —— Enum + match-case 硬编码
# ---------------------------------------------------------------------------
# 设计取舍:
#   十二个状态放进一个match语句,故意让读者感受"分支膨胀"的直观压力,
#   为v2切换到数据驱动做铺垫。这一版暂不处理ActiveConnection这台附属FSM。
# ---------------------------------------------------------------------------


class DeviceState(Enum):
    UNMANAGED = auto()
    UNAVAILABLE = auto()
    DISCONNECTED = auto()
    PREPARE = auto()
    CONFIG = auto()
    NEED_AUTH = auto()
    IP_CONFIG = auto()
    IP_CHECK = auto()
    SECONDARIES = auto()
    ACTIVATED = auto()
    DEACTIVATING = auto()
    FAILED = auto()


class DeviceEvent(Enum):
    CARRIER_UP = auto()
    CARRIER_DOWN = auto()
    MANAGE = auto()
    UNMANAGE = auto()
    ACTIVATE_REQUESTED = auto()
    L2_CONFIG_DONE = auto()
    SECRETS_NEEDED = auto()
    SECRETS_PROVIDED = auto()
    L3_CONFIG_DONE = auto()
    L3_CONFIG_FAILED = auto()
    IP_CHECK_PASSED = auto()
    IP_CHECK_CONFLICT = auto()
    SECONDARIES_READY = auto()
    DEACTIVATE_REQUESTED = auto()
    ACTIVATION_TIMEOUT = auto()


class NetworkDeviceV1:
    """最直观的写法,十二个状态全部硬编码在一个match里。"""

    def __init__(self, name: str) -> None:
        self.name = name
        self.state = DeviceState.UNAVAILABLE

    def handle(self, event: DeviceEvent) -> None:
        match self.state, event:
            case DeviceState.UNAVAILABLE, DeviceEvent.CARRIER_UP:
                self._go(DeviceState.DISCONNECTED)
            case DeviceState.DISCONNECTED, DeviceEvent.ACTIVATE_REQUESTED:
                self._go(DeviceState.PREPARE)
            case DeviceState.PREPARE, DeviceEvent.L2_CONFIG_DONE:
                self._go(DeviceState.CONFIG)
            case DeviceState.CONFIG, DeviceEvent.L2_CONFIG_DONE:
                self._go(DeviceState.IP_CONFIG)
            case DeviceState.CONFIG, DeviceEvent.SECRETS_NEEDED:
                self._go(DeviceState.NEED_AUTH)
            case DeviceState.NEED_AUTH, DeviceEvent.SECRETS_PROVIDED:
                self._go(DeviceState.CONFIG)
            case DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_DONE:
                self._go(DeviceState.IP_CHECK)
            case DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_FAILED:
                self._go(DeviceState.FAILED)
            case DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_PASSED:
                self._go(DeviceState.SECONDARIES)
            case DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_CONFLICT:
                self._go(DeviceState.FAILED)
            case DeviceState.SECONDARIES, DeviceEvent.SECONDARIES_READY:
                self._go(DeviceState.ACTIVATED)
            case DeviceState.ACTIVATED, DeviceEvent.DEACTIVATE_REQUESTED:
                self._go(DeviceState.DEACTIVATING)
            case DeviceState.ACTIVATED, DeviceEvent.CARRIER_DOWN:
                self._go(DeviceState.DEACTIVATING)
            case DeviceState.DEACTIVATING, DeviceEvent.CARRIER_DOWN:
                self._go(DeviceState.DISCONNECTED)
            case _, DeviceEvent.ACTIVATION_TIMEOUT:
                self._go(DeviceState.FAILED)
            case DeviceState.FAILED, DeviceEvent.ACTIVATE_REQUESTED:
                self._go(DeviceState.PREPARE)
            case _, DeviceEvent.UNMANAGE:
                self._go(DeviceState.UNMANAGED)
            case DeviceState.UNMANAGED, DeviceEvent.MANAGE:
                self._go(DeviceState.UNAVAILABLE)
            case _:
                print(f"[{self.name}] {self.state.name}状态下忽略事件 {event.name}")

    def _go(self, next_state: DeviceState) -> None:
        print(f"[{self.name}] {self.state.name} -> {next_state.name}")
        self.state = next_state


# ---------------------------------------------------------------------------
# v2: Transition Table版(数据驱动)
# ---------------------------------------------------------------------------
# 设计取舍:
#   转移表本身就是"文档",可以直接遍历导出成Markdown表格,
#   保证Step 4的转移表文档和代码不会因为人工维护两份而逐渐失去同步。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class TransitionV2:
    next_state: DeviceState
    action_desc: str = ""


TABLE_V2: dict[tuple[DeviceState, DeviceEvent], TransitionV2] = {
    (DeviceState.UNAVAILABLE, DeviceEvent.CARRIER_UP): TransitionV2(
        DeviceState.DISCONNECTED, "标记链路可用,评估autoconnect profile"
    ),
    (DeviceState.DISCONNECTED, DeviceEvent.ACTIVATE_REQUESTED): TransitionV2(
        DeviceState.PREPARE, "创建ActiveConnection对象,开始激活流程"
    ),
    (DeviceState.PREPARE, DeviceEvent.L2_CONFIG_DONE): TransitionV2(
        DeviceState.CONFIG, "进行L2层配置(如发起WiFi关联)"
    ),
    (DeviceState.CONFIG, DeviceEvent.L2_CONFIG_DONE): TransitionV2(
        DeviceState.IP_CONFIG, "无需认证,直接进入IP配置"
    ),
    (DeviceState.CONFIG, DeviceEvent.SECRETS_NEEDED): TransitionV2(
        DeviceState.NEED_AUTH, "请求上层agent提供密钥"
    ),
    (DeviceState.NEED_AUTH, DeviceEvent.SECRETS_PROVIDED): TransitionV2(
        DeviceState.CONFIG, "用新提供的密钥重试L2配置"
    ),
    (DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_DONE): TransitionV2(
        DeviceState.IP_CHECK, "进行IP冲突检测(ARP probe)"
    ),
    (DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_FAILED): TransitionV2(
        DeviceState.FAILED, "记录失败原因,清理已分配资源"
    ),
    (DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_PASSED): TransitionV2(
        DeviceState.SECONDARIES, "检查是否有次级连接需要等待"
    ),
    (DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_CONFLICT): TransitionV2(
        DeviceState.FAILED, "记录IP冲突,放弃本次激活"
    ),
    (DeviceState.SECONDARIES, DeviceEvent.SECONDARIES_READY): TransitionV2(
        DeviceState.ACTIVATED, "标记ActiveConnection状态为ACTIVATED"
    ),
    (DeviceState.ACTIVATED, DeviceEvent.DEACTIVATE_REQUESTED): TransitionV2(
        DeviceState.DEACTIVATING, "拆除路由/IP配置"
    ),
    (DeviceState.ACTIVATED, DeviceEvent.CARRIER_DOWN): TransitionV2(
        DeviceState.DEACTIVATING, "链路丢失,拆除连接"
    ),
    (DeviceState.DEACTIVATING, DeviceEvent.CARRIER_DOWN): TransitionV2(
        DeviceState.DISCONNECTED, "销毁ActiveConnection对象"
    ),
    (DeviceState.FAILED, DeviceEvent.ACTIVATE_REQUESTED): TransitionV2(
        DeviceState.PREPARE, "重新尝试激活"
    ),
    (DeviceState.UNMANAGED, DeviceEvent.MANAGE): TransitionV2(
        DeviceState.UNAVAILABLE, "重新纳入NM管理,先假设无carrier"
    ),
}

# ACTIVATION_TIMEOUT和UNMANAGE对"任意进行中的状态"都成立,
# 用单独的集合表达"通配"转移,避免为每个状态都重复登记一遍。
_TIMEOUT_APPLICABLE_STATES = {
    DeviceState.PREPARE,
    DeviceState.CONFIG,
    DeviceState.NEED_AUTH,
    DeviceState.IP_CONFIG,
    DeviceState.IP_CHECK,
    DeviceState.SECONDARIES,
}


def print_transition_table_as_markdown() -> None:
    """演示"转移表即文档":直接从数据生成Markdown,而不是人工维护第二份表格。"""
    print("| Current State | Event | Next State | Action |")
    print("|---|---|---|---|")
    for (state, event), t in TABLE_V2.items():
        print(f"| {state.name} | {event.name} | {t.next_state.name} | {t.action_desc} |")


class NetworkDeviceV2:
    def __init__(self, name: str) -> None:
        self.name = name
        self.state = DeviceState.UNAVAILABLE

    def handle(self, event: DeviceEvent) -> None:
        if event is DeviceEvent.UNMANAGE:
            print(f"[{self.name}] {self.state.name} -> UNMANAGED (交出管理权)")
            self.state = DeviceState.UNMANAGED
            return

        if event is DeviceEvent.ACTIVATION_TIMEOUT and self.state in _TIMEOUT_APPLICABLE_STATES:
            print(f"[{self.name}] {self.state.name} -> FAILED (激活超时)")
            self.state = DeviceState.FAILED
            return

        transition = TABLE_V2.get((self.state, event))
        if transition is None:
            print(f"[{self.name}] {self.state.name}状态下忽略事件 {event.name}")
            return

        if transition.action_desc:
            print(f"[{self.name}] 动作: {transition.action_desc}")
        print(f"[{self.name}] {self.state.name} -> {transition.next_state.name}")
        self.state = transition.next_state


# ---------------------------------------------------------------------------
# v3: State Pattern版 —— 两台协作的FSM(Device驱动ActiveConnection)
# ---------------------------------------------------------------------------
# 设计取舍:
#   ActiveConnectionFSM是一个生命周期比Device短得多的附属对象,
#   由DeviceFSM在激活开始时创建、在停用完成后销毁,
#   Device的部分转移动作会直接调用ActiveConnection实例的方法来同步状态。
# ---------------------------------------------------------------------------


class ActiveConnectionState(Enum):
    ACTIVATING = auto()
    ACTIVATED = auto()
    DEACTIVATING = auto()
    DEACTIVATED = auto()


class ActiveConnectionFSM:
    """连接profile这一次激活尝试的状态机,生命周期比Device短。"""

    def __init__(self, profile_name: str) -> None:
        self.profile_name = profile_name
        self.state = ActiveConnectionState.ACTIVATING
        print(f"  [ActiveConnection:{profile_name}] 创建,初始状态ACTIVATING")

    def mark_activated(self) -> None:
        print(f"  [ActiveConnection:{self.profile_name}] ACTIVATING -> ACTIVATED")
        self.state = ActiveConnectionState.ACTIVATED

    def mark_deactivating(self) -> None:
        print(f"  [ActiveConnection:{self.profile_name}] {self.state.name} -> DEACTIVATING")
        self.state = ActiveConnectionState.DEACTIVATING

    def mark_deactivated(self) -> None:
        print(f"  [ActiveConnection:{self.profile_name}] DEACTIVATING -> DEACTIVATED (即将被销毁)")
        self.state = ActiveConnectionState.DEACTIVATED


class DeviceStateHandler:
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> "DeviceStateHandler":
        raise NotImplementedError

    @property
    def name(self) -> str:
        return type(self).__name__.replace("State", "")


class UnavailableState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.CARRIER_UP:
            return DisconnectedState()
        return self


class DisconnectedState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.ACTIVATE_REQUESTED:
            ctx.active_connection = ActiveConnectionFSM(ctx.pending_profile or "unknown")
            return PrepareState()
        return self


class PrepareState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.L2_CONFIG_DONE:
            return ConfigState()
        if event is DeviceEvent.ACTIVATION_TIMEOUT:
            return FailedState()
        return self


class ConfigState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.L2_CONFIG_DONE:
            return IpConfigState()
        if event is DeviceEvent.SECRETS_NEEDED:
            return NeedAuthState()
        if event is DeviceEvent.ACTIVATION_TIMEOUT:
            return FailedState()
        return self


class NeedAuthState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.SECRETS_PROVIDED:
            return ConfigState()
        if event is DeviceEvent.ACTIVATION_TIMEOUT:
            return FailedState()
        return self


class IpConfigState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.L3_CONFIG_DONE:
            return IpCheckState()
        if event in (DeviceEvent.L3_CONFIG_FAILED, DeviceEvent.ACTIVATION_TIMEOUT):
            return FailedState()
        return self


class IpCheckState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.IP_CHECK_PASSED:
            return SecondariesState()
        if event in (DeviceEvent.IP_CHECK_CONFLICT, DeviceEvent.ACTIVATION_TIMEOUT):
            return FailedState()
        return self


class SecondariesState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.SECONDARIES_READY:
            if ctx.active_connection is not None:
                ctx.active_connection.mark_activated()
            return ActivatedState()
        if event is DeviceEvent.ACTIVATION_TIMEOUT:
            return FailedState()
        return self


class ActivatedState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event in (DeviceEvent.DEACTIVATE_REQUESTED, DeviceEvent.CARRIER_DOWN):
            if ctx.active_connection is not None:
                ctx.active_connection.mark_deactivating()
            return DeactivatingState()
        return self


class DeactivatingState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.CARRIER_DOWN:
            if ctx.active_connection is not None:
                ctx.active_connection.mark_deactivated()
                ctx.active_connection = None  # 附属FSM实例生命周期结束,随之销毁
            return DisconnectedState()
        return self


class FailedState(DeviceStateHandler):
    def handle(self, ctx: "NetworkDeviceV3", event: DeviceEvent) -> DeviceStateHandler:
        if event is DeviceEvent.ACTIVATE_REQUESTED:
            ctx.active_connection = ActiveConnectionFSM(ctx.pending_profile or "unknown")
            return PrepareState()
        return self


class NetworkDeviceV3:
    """DeviceFSM:长生命周期,协调一个可反复创建/销毁的ActiveConnectionFSM。"""

    def __init__(self, name: str) -> None:
        self.name = name
        self.pending_profile: str | None = None
        self.active_connection: ActiveConnectionFSM | None = None
        self._handler: DeviceStateHandler = UnavailableState()

    @property
    def state_name(self) -> str:
        return self._handler.name

    def handle(self, event: DeviceEvent, profile: str | None = None) -> None:
        if profile is not None:
            self.pending_profile = profile
        before = self._handler.name
        self._handler = self._handler.handle(self, event)
        after = self._handler.name
        if before != after:
            print(f"[{self.name}] {before} -> {after}")
        else:
            print(f"[{self.name}] {before}状态下忽略事件 {event.name}")


# ---------------------------------------------------------------------------
# v4: 可复用FSM框架 —— 用同一套引擎同时驱动两台联动的FSM
# ---------------------------------------------------------------------------
# 设计取舍:
#   框架只负责单台FSM内部的"查表->执行动作->切换状态",
#   两台FSM之间"谁通知谁"仍由业务代码(这里是Device的action)自行编排,
#   这是复用第1课框架时暴露出的边界,而不是框架的缺陷。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Rule:
    state: Enum
    event: Enum
    next_state: Enum
    action: Callable[["GenericFSM"], None] | None = None


@dataclass
class GenericFSM:
    name: str
    state: Enum
    rules: list[Rule] = field(default_factory=list)
    context: dict[str, object] = field(default_factory=dict)

    def add_rule(self, rule: Rule) -> None:
        self.rules.append(rule)

    def dispatch(self, event: Enum) -> None:
        for rule in self.rules:
            if rule.state == self.state and rule.event == event:
                if rule.action is not None:
                    rule.action(self)
                print(f"[{self.name}] {self.state.name} -> {rule.next_state.name}")
                self.state = rule.next_state
                return
        print(f"[{self.name}] {self.state.name}状态下忽略事件 {event.name}")


def _action_create_active_connection(fsm: GenericFSM) -> None:
    ac_fsm = fsm.context.get("active_connection_fsm")
    profile = fsm.context.get("pending_profile", "unknown")
    new_ac = GenericFSM(
        name=f"ActiveConnection:{profile}",
        state=ActiveConnectionState.ACTIVATING,
    )
    fsm.context["active_connection_fsm"] = new_ac
    print(f"  [{new_ac.name}] 创建,初始状态ACTIVATING")


def _action_notify_activated(fsm: GenericFSM) -> None:
    ac_fsm: GenericFSM | None = fsm.context.get("active_connection_fsm")  # type: ignore[assignment]
    if ac_fsm is not None:
        print(f"  [{ac_fsm.name}] ACTIVATING -> ACTIVATED (由Device激活完成驱动)")
        ac_fsm.state = ActiveConnectionState.ACTIVATED


def _action_notify_deactivating(fsm: GenericFSM) -> None:
    ac_fsm: GenericFSM | None = fsm.context.get("active_connection_fsm")  # type: ignore[assignment]
    if ac_fsm is not None:
        print(f"  [{ac_fsm.name}] {ac_fsm.state.name} -> DEACTIVATING (由Device停用驱动)")
        ac_fsm.state = ActiveConnectionState.DEACTIVATING


def _action_notify_deactivated_and_destroy(fsm: GenericFSM) -> None:
    ac_fsm: GenericFSM | None = fsm.context.get("active_connection_fsm")  # type: ignore[assignment]
    if ac_fsm is not None:
        print(f"  [{ac_fsm.name}] DEACTIVATING -> DEACTIVATED (即将销毁)")
        fsm.context["active_connection_fsm"] = None


def build_device_fsm(name: str) -> GenericFSM:
    fsm = GenericFSM(name=name, state=DeviceState.UNAVAILABLE)
    fsm.context["pending_profile"] = "office-wifi"
    fsm.context["active_connection_fsm"] = None

    fsm.add_rule(Rule(DeviceState.UNAVAILABLE, DeviceEvent.CARRIER_UP, DeviceState.DISCONNECTED))
    fsm.add_rule(
        Rule(
            DeviceState.DISCONNECTED,
            DeviceEvent.ACTIVATE_REQUESTED,
            DeviceState.PREPARE,
            _action_create_active_connection,
        )
    )
    fsm.add_rule(Rule(DeviceState.PREPARE, DeviceEvent.L2_CONFIG_DONE, DeviceState.CONFIG))
    fsm.add_rule(Rule(DeviceState.CONFIG, DeviceEvent.L2_CONFIG_DONE, DeviceState.IP_CONFIG))
    fsm.add_rule(Rule(DeviceState.CONFIG, DeviceEvent.SECRETS_NEEDED, DeviceState.NEED_AUTH))
    fsm.add_rule(Rule(DeviceState.NEED_AUTH, DeviceEvent.SECRETS_PROVIDED, DeviceState.CONFIG))
    fsm.add_rule(Rule(DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_DONE, DeviceState.IP_CHECK))
    fsm.add_rule(Rule(DeviceState.IP_CONFIG, DeviceEvent.L3_CONFIG_FAILED, DeviceState.FAILED))
    fsm.add_rule(Rule(DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_PASSED, DeviceState.SECONDARIES))
    fsm.add_rule(Rule(DeviceState.IP_CHECK, DeviceEvent.IP_CHECK_CONFLICT, DeviceState.FAILED))
    fsm.add_rule(
        Rule(
            DeviceState.SECONDARIES,
            DeviceEvent.SECONDARIES_READY,
            DeviceState.ACTIVATED,
            _action_notify_activated,
        )
    )
    fsm.add_rule(
        Rule(
            DeviceState.ACTIVATED,
            DeviceEvent.DEACTIVATE_REQUESTED,
            DeviceState.DEACTIVATING,
            _action_notify_deactivating,
        )
    )
    fsm.add_rule(
        Rule(
            DeviceState.DEACTIVATING,
            DeviceEvent.CARRIER_DOWN,
            DeviceState.DISCONNECTED,
            _action_notify_deactivated_and_destroy,
        )
    )
    fsm.add_rule(Rule(DeviceState.FAILED, DeviceEvent.ACTIVATE_REQUESTED, DeviceState.PREPARE, _action_create_active_connection))
    return fsm


if __name__ == "__main__":
    print("=== v1: 简单版(Enum + match-case) ===")
    dev1 = NetworkDeviceV1("eth0")
    for evt in (
        DeviceEvent.CARRIER_UP,
        DeviceEvent.ACTIVATE_REQUESTED,
        DeviceEvent.L2_CONFIG_DONE,
        DeviceEvent.L2_CONFIG_DONE,
        DeviceEvent.L3_CONFIG_DONE,
        DeviceEvent.IP_CHECK_PASSED,
        DeviceEvent.SECONDARIES_READY,
    ):
        dev1.handle(evt)
    print(f"最终状态: {dev1.state.name}")

    print("\n=== v2: Transition Table版(并演示表驱动即文档) ===")
    dev2 = NetworkDeviceV2("wlan0")
    dev2.handle(DeviceEvent.CARRIER_UP)
    dev2.handle(DeviceEvent.ACTIVATE_REQUESTED)
    dev2.handle(DeviceEvent.L2_CONFIG_DONE)
    dev2.handle(DeviceEvent.SECRETS_NEEDED)
    dev2.handle(DeviceEvent.SECRETS_PROVIDED)
    dev2.handle(DeviceEvent.L2_CONFIG_DONE)
    dev2.handle(DeviceEvent.L3_CONFIG_DONE)
    dev2.handle(DeviceEvent.IP_CHECK_PASSED)
    dev2.handle(DeviceEvent.SECONDARIES_READY)
    print("(转移表片段,自动从数据生成:)")
    print_transition_table_as_markdown()

    print("\n=== v3: State Pattern版(Device驱动ActiveConnection) ===")
    dev3 = NetworkDeviceV3("wlan0")
    dev3.handle(DeviceEvent.CARRIER_UP)
    dev3.handle(DeviceEvent.ACTIVATE_REQUESTED, profile="office-wifi")
    dev3.handle(DeviceEvent.L2_CONFIG_DONE)
    dev3.handle(DeviceEvent.L2_CONFIG_DONE)
    dev3.handle(DeviceEvent.L3_CONFIG_DONE)
    dev3.handle(DeviceEvent.IP_CHECK_PASSED)
    dev3.handle(DeviceEvent.SECONDARIES_READY)
    dev3.handle(DeviceEvent.DEACTIVATE_REQUESTED)
    dev3.handle(DeviceEvent.CARRIER_DOWN)
    print(f"最终状态: {dev3.state_name}")

    print("\n=== v4: 可复用FSM框架(两台FSM联动) ===")
    dev4 = build_device_fsm("wlan0")
    dev4.dispatch(DeviceEvent.CARRIER_UP)
    dev4.dispatch(DeviceEvent.ACTIVATE_REQUESTED)
    dev4.dispatch(DeviceEvent.L2_CONFIG_DONE)
    dev4.dispatch(DeviceEvent.L2_CONFIG_DONE)
    dev4.dispatch(DeviceEvent.L3_CONFIG_DONE)
    dev4.dispatch(DeviceEvent.IP_CHECK_PASSED)
    dev4.dispatch(DeviceEvent.SECONDARIES_READY)
    dev4.dispatch(DeviceEvent.DEACTIVATE_REQUESTED)
    dev4.dispatch(DeviceEvent.CARRIER_DOWN)
    print(f"最终状态: {dev4.state.name}")
