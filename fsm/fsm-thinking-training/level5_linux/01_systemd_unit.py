"""
Lesson 01: systemd Unit状态 (Level 5 - Linux系统)

教学用简化模型,模拟systemd管理单个Unit的ActiveState转移。
不是systemd的真实实现,只提炼"目标状态(want) vs 当前状态(current)"、
"Restart策略作为转移决策的外部参数"这两个核心教学点。

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
#   故意先不处理 Restart= 策略的分支复杂度,只做"单一路径"的转移,
#   目的是先让读者确认"这确实是一个离散状态机",再在 v2 补上策略参数。
# ---------------------------------------------------------------------------


class UnitStateV1(Enum):
    INACTIVE = auto()
    ACTIVATING = auto()
    ACTIVE = auto()
    DEACTIVATING = auto()
    FAILED = auto()
    RELOADING = auto()


class UnitEventV1(Enum):
    START_REQUESTED = auto()
    START_SUCCEEDED = auto()
    START_FAILED = auto()
    STOP_REQUESTED = auto()
    STOP_COMPLETED = auto()
    RELOAD_REQUESTED = auto()
    RELOAD_COMPLETED = auto()
    PROCESS_CRASHED = auto()


class SystemdUnitV1:
    """最直观的写法:状态和转移逻辑写在一起。"""

    def __init__(self, name: str) -> None:
        self.name = name
        self.state = UnitStateV1.INACTIVE

    def handle(self, event: UnitEventV1) -> None:
        match self.state:
            case UnitStateV1.INACTIVE:
                if event is UnitEventV1.START_REQUESTED:
                    print(f"[{self.name}] inactive -> activating (执行ExecStart)")
                    self.state = UnitStateV1.ACTIVATING
                else:
                    print(f"[{self.name}] inactive状态下忽略事件 {event.name}")

            case UnitStateV1.ACTIVATING:
                if event is UnitEventV1.START_SUCCEEDED:
                    print(f"[{self.name}] activating -> active")
                    self.state = UnitStateV1.ACTIVE
                elif event is UnitEventV1.START_FAILED:
                    print(f"[{self.name}] activating -> failed")
                    self.state = UnitStateV1.FAILED
                else:
                    print(f"[{self.name}] activating状态下忽略事件 {event.name}")

            case UnitStateV1.ACTIVE:
                if event is UnitEventV1.STOP_REQUESTED:
                    print(f"[{self.name}] active -> deactivating")
                    self.state = UnitStateV1.DEACTIVATING
                elif event is UnitEventV1.RELOAD_REQUESTED:
                    print(f"[{self.name}] active -> reloading (SIGHUP)")
                    self.state = UnitStateV1.RELOADING
                elif event is UnitEventV1.PROCESS_CRASHED:
                    print(f"[{self.name}] active -> failed (无重启策略, v1暂不处理Restart=)")
                    self.state = UnitStateV1.FAILED
                else:
                    print(f"[{self.name}] active状态下忽略事件 {event.name}")

            case UnitStateV1.RELOADING:
                if event is UnitEventV1.RELOAD_COMPLETED:
                    print(f"[{self.name}] reloading -> active")
                    self.state = UnitStateV1.ACTIVE
                else:
                    print(f"[{self.name}] reloading状态下忽略事件 {event.name}")

            case UnitStateV1.DEACTIVATING:
                if event is UnitEventV1.STOP_COMPLETED:
                    print(f"[{self.name}] deactivating -> inactive")
                    self.state = UnitStateV1.INACTIVE
                else:
                    print(f"[{self.name}] deactivating状态下忽略事件 {event.name}")

            case UnitStateV1.FAILED:
                if event is UnitEventV1.START_REQUESTED:
                    print(f"[{self.name}] failed -> activating (重新尝试启动)")
                    self.state = UnitStateV1.ACTIVATING
                else:
                    print(f"[{self.name}] failed状态下忽略事件 {event.name}")


# ---------------------------------------------------------------------------
# v2: Transition Table版(数据驱动) —— 补上 Restart= 策略这个"外部参数"
# ---------------------------------------------------------------------------
# 设计取舍:
#   (ACTIVE, PROCESS_CRASHED) 的下一状态不是唯一确定的,取决于 restart_policy。
#   转移表本身只回答"大方向",真正的分支决策放进 Action 函数,
#   避免为每一种策略组合都发明一个新的事件类型(事件数量会爆炸)。
# ---------------------------------------------------------------------------


class RestartPolicy(Enum):
    NO = auto()  # Restart=no (默认)
    ON_FAILURE = auto()  # Restart=on-failure
    ALWAYS = auto()  # Restart=always


TransitionActionV2 = Callable[["SystemdUnitV2"], None]


@dataclass
class TransitionV2:
    next_state: UnitStateV1
    action: TransitionActionV2 | None = None


def _action_start(unit: "SystemdUnitV2") -> None:
    print(f"[{unit.name}] 执行ExecStartPre/ExecStart,启动TimeoutStartSec计时器")


def _action_stop(unit: "SystemdUnitV2") -> None:
    print(f"[{unit.name}] 执行ExecStop,启动TimeoutStopSec计时器")


def _action_crash_no_restart(unit: "SystemdUnitV2") -> None:
    print(f"[{unit.name}] Restart=no,主进程崩溃直接判定failed")


class SystemdUnitV2:
    """转移表数据驱动,但把"依赖配置的决策"留在动作函数里。"""

    # (当前状态, 事件) -> Transition。PROCESS_CRASHED在active下的分支
    # 由 handle() 里的策略判断决定走哪一条,而不是塞进这张静态表。
    TABLE: dict[tuple[UnitStateV1, UnitEventV1], TransitionV2] = {
        (UnitStateV1.INACTIVE, UnitEventV1.START_REQUESTED): TransitionV2(
            UnitStateV1.ACTIVATING, _action_start
        ),
        (UnitStateV1.ACTIVATING, UnitEventV1.START_SUCCEEDED): TransitionV2(
            UnitStateV1.ACTIVE
        ),
        (UnitStateV1.ACTIVATING, UnitEventV1.START_FAILED): TransitionV2(
            UnitStateV1.FAILED
        ),
        (UnitStateV1.ACTIVE, UnitEventV1.STOP_REQUESTED): TransitionV2(
            UnitStateV1.DEACTIVATING, _action_stop
        ),
        (UnitStateV1.ACTIVE, UnitEventV1.RELOAD_REQUESTED): TransitionV2(
            UnitStateV1.RELOADING
        ),
        (UnitStateV1.RELOADING, UnitEventV1.RELOAD_COMPLETED): TransitionV2(
            UnitStateV1.ACTIVE
        ),
        (UnitStateV1.DEACTIVATING, UnitEventV1.STOP_COMPLETED): TransitionV2(
            UnitStateV1.INACTIVE
        ),
        (UnitStateV1.FAILED, UnitEventV1.START_REQUESTED): TransitionV2(
            UnitStateV1.ACTIVATING, _action_start
        ),
    }

    def __init__(self, name: str, restart_policy: RestartPolicy = RestartPolicy.NO) -> None:
        self.name = name
        self.state = UnitStateV1.INACTIVE
        self.restart_policy = restart_policy
        self.restart_count = 0
        self.start_limit_burst = 3

    def handle(self, event: UnitEventV1) -> None:
        # 特殊分支:PROCESS_CRASHED在active下的走向依赖restart_policy,
        # 这是"策略参数参与决策"而不是"事件类型爆炸"的示范。
        if self.state is UnitStateV1.ACTIVE and event is UnitEventV1.PROCESS_CRASHED:
            self._handle_crash()
            return

        transition = self.TABLE.get((self.state, event))
        if transition is None:
            print(f"[{self.name}] {self.state.name}状态下忽略/拒绝事件 {event.name}")
            return

        if transition.action is not None:
            transition.action(self)
        print(f"[{self.name}] {self.state.name} -> {transition.next_state.name}")
        self.state = transition.next_state

    def _handle_crash(self) -> None:
        if self.restart_policy is RestartPolicy.NO:
            _action_crash_no_restart(self)
            self.state = UnitStateV1.FAILED
            return

        self.restart_count += 1
        if self.restart_count > self.start_limit_burst:
            print(f"[{self.name}] 超过StartLimitBurst,停止自动重启,判定failed")
            self.state = UnitStateV1.FAILED
            return

        print(
            f"[{self.name}] Restart={self.restart_policy.name},"
            f"第{self.restart_count}次自动重启: active -> activating"
        )
        self.state = UnitStateV1.ACTIVATING


# ---------------------------------------------------------------------------
# v3: State Pattern版 —— 每个状态是一个类
# ---------------------------------------------------------------------------
# 设计取舍:
#   FailedState把"重启计数检查、StartLimitBurst判断"这些只跟failed相关的
#   复杂逻辑封装在自己的类里,不会污染其他状态类。这正是State Pattern
#   相比转移表字典的优势场景:某个状态的处理逻辑明显更复杂时,值得单独成类。
# ---------------------------------------------------------------------------


class UnitStateHandler:
    """状态处理器基类,子类实现各自的handle逻辑。"""

    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> "UnitStateHandler":
        raise NotImplementedError

    @property
    def name(self) -> str:
        return type(self).__name__.replace("State", "")


class InactiveState(UnitStateHandler):
    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.START_REQUESTED:
            _action_start(ctx)
            return ActivatingState()
        print(f"[{ctx.name}] inactive状态下忽略事件 {event.name}")
        return self


class ActivatingState(UnitStateHandler):
    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.START_SUCCEEDED:
            return ActiveState()
        if event is UnitEventV1.START_FAILED:
            return FailedState()
        print(f"[{ctx.name}] activating状态下忽略事件 {event.name}")
        return self


class ActiveState(UnitStateHandler):
    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.STOP_REQUESTED:
            _action_stop(ctx)
            return DeactivatingState()
        if event is UnitEventV1.RELOAD_REQUESTED:
            print(f"[{ctx.name}] active -> reloading (SIGHUP)")
            return ReloadingState()
        if event is UnitEventV1.PROCESS_CRASHED:
            return FailedState().on_crash(ctx)
        print(f"[{ctx.name}] active状态下忽略事件 {event.name}")
        return self


class ReloadingState(UnitStateHandler):
    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.RELOAD_COMPLETED:
            return ActiveState()
        print(f"[{ctx.name}] reloading状态下忽略事件 {event.name}")
        return self


class DeactivatingState(UnitStateHandler):
    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.STOP_COMPLETED:
            return InactiveState()
        print(f"[{ctx.name}] deactivating状态下忽略事件 {event.name}")
        return self


class FailedState(UnitStateHandler):
    """把重启计数/StartLimitBurst判断封装在这里,不污染ActiveState。"""

    def handle(self, ctx: "SystemdUnitV3", event: UnitEventV1) -> UnitStateHandler:
        if event is UnitEventV1.START_REQUESTED:
            ctx.restart_count = 0
            _action_start(ctx)
            return ActivatingState()
        print(f"[{ctx.name}] failed状态下忽略事件 {event.name}")
        return self

    def on_crash(self, ctx: "SystemdUnitV3") -> UnitStateHandler:
        if ctx.restart_policy is RestartPolicy.NO:
            _action_crash_no_restart(ctx)
            return self

        ctx.restart_count += 1
        if ctx.restart_count > ctx.start_limit_burst:
            print(f"[{ctx.name}] 超过StartLimitBurst,停止自动重启,判定failed")
            return self

        print(
            f"[{ctx.name}] Restart={ctx.restart_policy.name},"
            f"第{ctx.restart_count}次自动重启: active -> activating"
        )
        return ActivatingState()


class SystemdUnitV3:
    def __init__(self, name: str, restart_policy: RestartPolicy = RestartPolicy.NO) -> None:
        self.name = name
        self.restart_policy = restart_policy
        self.restart_count = 0
        self.start_limit_burst = 3
        self._handler: UnitStateHandler = InactiveState()

    @property
    def state_name(self) -> str:
        return self._handler.name

    def handle(self, event: UnitEventV1) -> None:
        next_handler = self._handler.handle(self, event)
        if type(next_handler) is not type(self._handler):
            print(f"[{self.name}] {self._handler.name} -> {next_handler.name}")
        self._handler = next_handler


# ---------------------------------------------------------------------------
# v4: 可复用FSM框架 —— 在Lesson 01(交通灯)的框架基础上,加入guard(前置条件)
# ---------------------------------------------------------------------------
# 设计取舍:
#   guard用来表达"输入合法但当前条件不满足,应跳过而非报错/转移"
#   (比如inactive状态下的reload请求、Condition检查失败),
#   这是这一课暴露出、旧框架(纯查表)没有覆盖的新需求。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class TransitionRule:
    """一条可复用的转移规则,guard返回False时视为"跳过",不算非法输入。"""

    state: UnitStateV1
    event: UnitEventV1
    next_state: UnitStateV1
    action: Callable[["GenericFSM"], None] | None = None
    guard: Callable[["GenericFSM"], bool] | None = None


@dataclass
class GenericFSM:
    """通用FSM引擎:接收事件、查表(含guard判断)、执行动作、切换状态。"""

    name: str
    state: UnitStateV1
    rules: list[TransitionRule] = field(default_factory=list)
    context: dict[str, object] = field(default_factory=dict)

    def add_rule(self, rule: TransitionRule) -> None:
        self.rules.append(rule)

    def dispatch(self, event: UnitEventV1) -> None:
        # 按规则登记顺序尝试:guard未通过不算"最终失败",只是"这条候选规则不适用",
        # 继续看下一条匹配的规则(典型用法:先放有guard的规则,再放无条件的兜底规则)。
        matched_any = False
        for rule in self.rules:
            if rule.state is not self.state or rule.event is not event:
                continue
            matched_any = True
            if rule.guard is not None and not rule.guard(self):
                continue
            if rule.action is not None:
                rule.action(self)
            print(f"[{self.name}] {self.state.name} -> {rule.next_state.name}")
            self.state = rule.next_state
            return
        if matched_any:
            print(f"[{self.name}] 所有候选规则的guard均未通过,状态保持{self.state.name}")
        else:
            print(f"[{self.name}] {self.state.name}状态下忽略事件 {event.name}(未定义转移)")


def _guard_restart_allowed(fsm: GenericFSM) -> bool:
    count = int(fsm.context.get("restart_count", 0)) + 1
    fsm.context["restart_count"] = count
    limit = int(fsm.context.get("start_limit_burst", 3))
    if count > limit:
        print(f"[{fsm.name}] 超过StartLimitBurst({limit}),不允许自动重启")
        return False
    print(f"[{fsm.name}] 第{count}次自动重启,未超过StartLimitBurst({limit})")
    return True


def build_systemd_fsm(name: str, restart_policy: RestartPolicy) -> GenericFSM:
    fsm = GenericFSM(name=name, state=UnitStateV1.INACTIVE)
    fsm.context["restart_count"] = 0
    fsm.context["start_limit_burst"] = 3

    fsm.add_rule(
        TransitionRule(UnitStateV1.INACTIVE, UnitEventV1.START_REQUESTED, UnitStateV1.ACTIVATING, _action_start)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.ACTIVATING, UnitEventV1.START_SUCCEEDED, UnitStateV1.ACTIVE)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.ACTIVATING, UnitEventV1.START_FAILED, UnitStateV1.FAILED)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.ACTIVE, UnitEventV1.STOP_REQUESTED, UnitStateV1.DEACTIVATING, _action_stop)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.ACTIVE, UnitEventV1.RELOAD_REQUESTED, UnitStateV1.RELOADING)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.RELOADING, UnitEventV1.RELOAD_COMPLETED, UnitStateV1.ACTIVE)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.DEACTIVATING, UnitEventV1.STOP_COMPLETED, UnitStateV1.INACTIVE)
    )
    fsm.add_rule(
        TransitionRule(UnitStateV1.FAILED, UnitEventV1.START_REQUESTED, UnitStateV1.ACTIVATING, _action_start)
    )

    if restart_policy is not RestartPolicy.NO:
        fsm.add_rule(
            TransitionRule(
                UnitStateV1.ACTIVE,
                UnitEventV1.PROCESS_CRASHED,
                UnitStateV1.ACTIVATING,
                guard=_guard_restart_allowed,
            )
        )
        fsm.add_rule(
            TransitionRule(
                UnitStateV1.ACTIVE,
                UnitEventV1.PROCESS_CRASHED,
                UnitStateV1.FAILED,
            )
        )
    else:
        fsm.add_rule(
            TransitionRule(UnitStateV1.ACTIVE, UnitEventV1.PROCESS_CRASHED, UnitStateV1.FAILED, _action_crash_no_restart)
        )

    return fsm


if __name__ == "__main__":
    print("=== v1: 简单版(Enum + match-case) ===")
    unit1 = SystemdUnitV1("nginx.service")
    for evt in (
        UnitEventV1.START_REQUESTED,
        UnitEventV1.START_SUCCEEDED,
        UnitEventV1.RELOAD_REQUESTED,
        UnitEventV1.RELOAD_COMPLETED,
        UnitEventV1.STOP_REQUESTED,
        UnitEventV1.STOP_COMPLETED,
    ):
        unit1.handle(evt)

    print("\n=== v2: Transition Table版(带Restart=on-failure) ===")
    unit2 = SystemdUnitV2("myapp.service", restart_policy=RestartPolicy.ON_FAILURE)
    unit2.handle(UnitEventV1.START_REQUESTED)
    unit2.handle(UnitEventV1.START_SUCCEEDED)
    for _ in range(4):  # 连续崩溃4次,第4次应超过StartLimitBurst=3
        unit2.handle(UnitEventV1.PROCESS_CRASHED)
        if unit2.state is UnitStateV1.ACTIVATING:
            unit2.handle(UnitEventV1.START_SUCCEEDED)

    print("\n=== v3: State Pattern版 ===")
    unit3 = SystemdUnitV3("redis.service", restart_policy=RestartPolicy.ALWAYS)
    unit3.handle(UnitEventV1.START_REQUESTED)
    unit3.handle(UnitEventV1.START_SUCCEEDED)
    unit3.handle(UnitEventV1.PROCESS_CRASHED)
    print(f"当前状态: {unit3.state_name}")

    print("\n=== v4: 可复用FSM框架(含guard) ===")
    fsm4 = build_systemd_fsm("postgresql.service", RestartPolicy.ON_FAILURE)
    fsm4.dispatch(UnitEventV1.START_REQUESTED)
    fsm4.dispatch(UnitEventV1.START_SUCCEEDED)
    fsm4.dispatch(UnitEventV1.RELOAD_REQUESTED)
    fsm4.dispatch(UnitEventV1.RELOAD_COMPLETED)
    for _ in range(5):
        fsm4.dispatch(UnitEventV1.PROCESS_CRASHED)
        if fsm4.state is UnitStateV1.ACTIVATING:
            fsm4.dispatch(UnitEventV1.START_SUCCEEDED)
    print(f"最终状态: {fsm4.state.name}")
