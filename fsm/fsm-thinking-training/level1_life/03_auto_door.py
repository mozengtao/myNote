"""Lesson 03: 自动门 (Level 1 - 生活场景).

对应 03_auto_door.md。核心训练点: "超时事件"——事件不一定来自外部输入
(传感器/按钮)，"时间到了、且没有被打断"本身就是一等事件来源。四版实现
都用一个显式的 tick(seconds) 方法模拟时间前进，倒计时归零时由状态机自己
产生 CLOSE_TIMER_EXPIRED 事件，让"时间驱动"和"外部输入驱动"共用同一套
事件处理入口。

    v1: 最简单版本 —— Enum + match-case，倒计时是裸露的实例属性
    v2: Transition Table 版 —— Action 显式包含"启动/重置/取消定时器"
    v3: State Pattern 版 —— on_enter/on_exit 钩子管理定时器生命周期
    v4: 通用 FSM 框架片段(升级版) —— 支持 on_enter/on_exit 与内部事件
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable

CLOSE_TIMER_DURATION = 5.0  # 秒，开门后无人经过多久自动关门


# ============ v1: 简单版 (Enum + match-case，倒计时是裸露属性) ============
#
# 设计思路: 状态用 Enum 表示，倒计时秒数是实例属性，tick() 手动递减；
# "该不该启动/重置/取消倒计时"这几件事分散写在各个方法里，容易遗漏。


class DoorStateV1(Enum):
    CLOSED = auto()
    OPENING = auto()
    OPEN = auto()
    CLOSING = auto()


class AutoDoorV1:
    def __init__(self) -> None:
        self.state: DoorStateV1 = DoorStateV1.CLOSED
        self.close_timer: float | None = None  # None 表示定时器未运行

    def person_detected(self) -> None:
        match self.state:
            case DoorStateV1.CLOSED:
                self.state = DoorStateV1.OPENING
                print("  action: 检测到有人，启动开门电机")
            case DoorStateV1.OPENING:
                print("  action: 门已经在开，忽略")
            case DoorStateV1.OPEN:
                self.close_timer = CLOSE_TIMER_DURATION
                print(f"  action: 重置关门倒计时为{CLOSE_TIMER_DURATION}秒")
            case DoorStateV1.CLOSING:
                self.state = DoorStateV1.OPENING
                self.close_timer = None
                print("  action: 关门中检测到有人，反转重新开门")

    def obstruction_detected(self) -> None:
        if self.state != DoorStateV1.CLOSING:
            print(f"  警告: [{self.state.name}] 异常的障碍物信号，忽略")
            return
        self.state = DoorStateV1.OPENING
        self.close_timer = None
        print("  action: 关门中检测到障碍物，反转重新开门")

    def fully_open(self) -> None:
        if self.state != DoorStateV1.OPENING:
            print(f"  警告: [{self.state.name}] 异常的FULLY_OPEN信号，忽略")
            return
        self.state = DoorStateV1.OPEN
        self.close_timer = CLOSE_TIMER_DURATION
        print(f"  action: 门已全开，启动关门倒计时{CLOSE_TIMER_DURATION}秒")

    def fully_closed(self) -> None:
        if self.state != DoorStateV1.CLOSING:
            print(f"  警告: [{self.state.name}] 异常的FULLY_CLOSED信号，忽略")
            return
        self.state = DoorStateV1.CLOSED
        self.close_timer = None
        print("  action: 门已全关，停止电机")

    def tick(self, seconds: float) -> None:
        """模拟时间前进 seconds 秒；倒计时归零时触发超时逻辑。"""
        if self.close_timer is None:
            return
        self.close_timer -= seconds
        if self.close_timer <= 0:
            self.close_timer = None
            self._close_timer_expired()

    def _close_timer_expired(self) -> None:
        if self.state != DoorStateV1.OPEN:
            print(f"  警告: [{self.state.name}] 异常的关门超时信号(定时器未被正确取消)，忽略")
            return
        self.state = DoorStateV1.CLOSING
        print("  action: 关门倒计时到时，启动关门电机  <- 纯由时间驱动，无外部输入")

    def __repr__(self) -> str:
        return f"AutoDoorV1(state={self.state.name}, close_timer={self.close_timer})"


def demo_v1() -> None:
    print("\n--- v1: 简单版 (match-case + 裸露倒计时属性) ---")
    door = AutoDoorV1()
    print(door)
    door.person_detected()
    door.fully_open()
    print(door)
    door.tick(3)
    print(f"3秒后: {door}")
    door.person_detected()  # 有人经过，续期
    print(f"续期后: {door}")
    door.tick(5)
    print(f"再过5秒: {door}")


# ============ v2: Transition Table 版 (Action 显式管理定时器) ============
#
# 设计思路: 转移表里的 Action 统一被设计成"可以启动/重置/取消定时器"，
# 表格本身就能回答"哪些转移会碰定时器"，不需要再去翻代码确认。


class DoorStateV2(Enum):
    CLOSED = auto()
    OPENING = auto()
    OPEN = auto()
    CLOSING = auto()


class DoorEventV2(Enum):
    PERSON_DETECTED = auto()
    FULLY_OPEN = auto()
    FULLY_CLOSED = auto()
    OBSTRUCTION_DETECTED = auto()
    CLOSE_TIMER_EXPIRED = auto()


@dataclass
class DoorContextV2:
    close_timer: float | None = None

    def start_timer(self) -> None:
        self.close_timer = CLOSE_TIMER_DURATION
        print(f"  action: 启动关门倒计时{CLOSE_TIMER_DURATION}秒")

    def reset_timer(self) -> None:
        self.close_timer = CLOSE_TIMER_DURATION
        print(f"  action: 重置关门倒计时为{CLOSE_TIMER_DURATION}秒")

    def cancel_timer(self) -> None:
        self.close_timer = None


@dataclass
class RuleV2:
    state: DoorStateV2
    event: DoorEventV2
    next_state: DoorStateV2
    action: Callable[[DoorContextV2], None] | None = None


class AutoDoorV2:
    def __init__(self) -> None:
        self.state: DoorStateV2 = DoorStateV2.CLOSED
        self.ctx = DoorContextV2()
        self._table: dict[tuple[DoorStateV2, DoorEventV2], RuleV2] = {
            (r.state, r.event): r for r in self._build_rules()
        }

    def _build_rules(self) -> list[RuleV2]:
        return [
            RuleV2(
                DoorStateV2.CLOSED,
                DoorEventV2.PERSON_DETECTED,
                DoorStateV2.OPENING,
                action=lambda ctx: print("  action: 检测到有人，启动开门电机"),
            ),
            RuleV2(
                DoorStateV2.OPENING,
                DoorEventV2.FULLY_OPEN,
                DoorStateV2.OPEN,
                action=lambda ctx: (ctx.start_timer(), print("  action: 门已全开，停止电机")),
            ),
            RuleV2(
                DoorStateV2.OPENING,
                DoorEventV2.PERSON_DETECTED,
                DoorStateV2.OPENING,
                action=lambda ctx: print("  action: 门已经在开，忽略"),
            ),
            RuleV2(
                DoorStateV2.OPEN,
                DoorEventV2.PERSON_DETECTED,
                DoorStateV2.OPEN,
                action=lambda ctx: ctx.reset_timer(),
            ),
            RuleV2(
                DoorStateV2.OPEN,
                DoorEventV2.CLOSE_TIMER_EXPIRED,
                DoorStateV2.CLOSING,
                action=lambda ctx: (
                    ctx.cancel_timer(),
                    print("  action: 关门倒计时到时，启动关门电机  <- 纯由时间驱动"),
                ),
            ),
            RuleV2(
                DoorStateV2.CLOSING,
                DoorEventV2.FULLY_CLOSED,
                DoorStateV2.CLOSED,
                action=lambda ctx: (ctx.cancel_timer(), print("  action: 门已全关，停止电机")),
            ),
            RuleV2(
                DoorStateV2.CLOSING,
                DoorEventV2.OBSTRUCTION_DETECTED,
                DoorStateV2.OPENING,
                action=lambda ctx: (ctx.cancel_timer(), print("  action: 检测到障碍物，反转重新开门")),
            ),
            RuleV2(
                DoorStateV2.CLOSING,
                DoorEventV2.PERSON_DETECTED,
                DoorStateV2.OPENING,
                action=lambda ctx: (ctx.cancel_timer(), print("  action: 关门中检测到有人，反转重新开门")),
            ),
        ]

    def fire(self, event: DoorEventV2) -> None:
        rule = self._table.get((self.state, event))
        if rule is None:
            print(f"  警告/忽略: state={self.state.name}, event={event.name} 无对应转移")
            return
        if rule.action is not None:
            rule.action(self.ctx)
        self.state = rule.next_state

    def tick(self, seconds: float) -> None:
        if self.ctx.close_timer is None:
            return
        self.ctx.close_timer -= seconds
        if self.ctx.close_timer <= 0:
            self.fire(DoorEventV2.CLOSE_TIMER_EXPIRED)

    def __repr__(self) -> str:
        return f"AutoDoorV2(state={self.state.name}, close_timer={self.ctx.close_timer})"


def demo_v2() -> None:
    print("\n--- v2: Transition Table 版 (Action 显式管理定时器) ---")
    door = AutoDoorV2()
    print(door)
    door.fire(DoorEventV2.PERSON_DETECTED)
    door.fire(DoorEventV2.FULLY_OPEN)
    print(door)
    door.tick(3)
    print(f"3秒后: {door}")
    door.fire(DoorEventV2.PERSON_DETECTED)
    print(f"续期后: {door}")
    door.tick(5)
    print(f"再过5秒: {door}")


# ============ v3: State Pattern 版 (on_enter/on_exit 管理定时器生命周期) ============
#
# 设计思路: 每个状态类实现 on_enter/on_exit 钩子，OPEN 状态的 on_enter
# 负责启动定时器，on_exit 统一负责取消定时器——不管是因为超时本身离开，
# 还是被外部事件打断离开，都会经过同一个 on_exit，避免"忘记取消定时器"。


@dataclass
class DoorContextV3:
    close_timer: float | None = None
    _state: "DoorStateBaseV3" = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._state = ClosedStateV3()
        self._state.on_enter(self)

    def _transition_to(self, new_state: "DoorStateBaseV3") -> None:
        self._state.on_exit(self)
        self._state = new_state
        self._state.on_enter(self)

    def person_detected(self) -> None:
        self._state.on_person_detected(self)

    def obstruction_detected(self) -> None:
        self._state.on_obstruction_detected(self)

    def fully_open(self) -> None:
        self._state.on_fully_open(self)

    def fully_closed(self) -> None:
        self._state.on_fully_closed(self)

    def tick(self, seconds: float) -> None:
        if self.close_timer is None:
            return
        self.close_timer -= seconds
        if self.close_timer <= 0:
            self._state.on_close_timer_expired(self)

    @property
    def state_name(self) -> str:
        return self._state.name


class DoorStateBaseV3:
    name: str = "AbstractState"

    def on_enter(self, ctx: DoorContextV3) -> None:
        pass

    def on_exit(self, ctx: DoorContextV3) -> None:
        pass

    def on_person_detected(self, ctx: DoorContextV3) -> None:
        print(f"  [{self.name}] 忽略PERSON_DETECTED")

    def on_obstruction_detected(self, ctx: DoorContextV3) -> None:
        print(f"  警告: [{self.name}] 异常的障碍物信号，忽略")

    def on_fully_open(self, ctx: DoorContextV3) -> None:
        print(f"  警告: [{self.name}] 异常的FULLY_OPEN信号，忽略")

    def on_fully_closed(self, ctx: DoorContextV3) -> None:
        print(f"  警告: [{self.name}] 异常的FULLY_CLOSED信号，忽略")

    def on_close_timer_expired(self, ctx: DoorContextV3) -> None:
        print(f"  警告: [{self.name}] 异常的关门超时信号，忽略")


class ClosedStateV3(DoorStateBaseV3):
    name = "CLOSED"

    def on_person_detected(self, ctx: DoorContextV3) -> None:
        print("  action: 检测到有人，启动开门电机")
        ctx._transition_to(OpeningStateV3())


class OpeningStateV3(DoorStateBaseV3):
    name = "OPENING"

    def on_person_detected(self, ctx: DoorContextV3) -> None:
        print("  action: 门已经在开，忽略")

    def on_fully_open(self, ctx: DoorContextV3) -> None:
        print("  action: 门已全开，停止电机")
        ctx._transition_to(OpenStateV3())


class OpenStateV3(DoorStateBaseV3):
    name = "OPEN"

    def on_enter(self, ctx: DoorContextV3) -> None:
        ctx.close_timer = CLOSE_TIMER_DURATION
        print(f"  on_enter[OPEN]: 启动关门倒计时{CLOSE_TIMER_DURATION}秒")

    def on_exit(self, ctx: DoorContextV3) -> None:
        ctx.close_timer = None  # 无论因何种原因离开OPEN，统一取消定时器

    def on_person_detected(self, ctx: DoorContextV3) -> None:
        ctx.close_timer = CLOSE_TIMER_DURATION
        print(f"  action: 重置关门倒计时为{CLOSE_TIMER_DURATION}秒")

    def on_close_timer_expired(self, ctx: DoorContextV3) -> None:
        print("  action: 关门倒计时到时，启动关门电机  <- 纯由时间驱动")
        ctx._transition_to(ClosingStateV3())


class ClosingStateV3(DoorStateBaseV3):
    name = "CLOSING"

    def on_person_detected(self, ctx: DoorContextV3) -> None:
        print("  action: 关门中检测到有人，反转重新开门")
        ctx._transition_to(OpeningStateV3())

    def on_obstruction_detected(self, ctx: DoorContextV3) -> None:
        print("  action: 检测到障碍物，反转重新开门")
        ctx._transition_to(OpeningStateV3())

    def on_fully_closed(self, ctx: DoorContextV3) -> None:
        print("  action: 门已全关，停止电机")
        ctx._transition_to(ClosedStateV3())


def demo_v3() -> None:
    print("\n--- v3: State Pattern 版 (on_enter/on_exit 管理定时器) ---")
    door = DoorContextV3()
    print(f"初始状态: {door.state_name}")
    door.person_detected()
    door.fully_open()
    print(f"当前: {door.state_name}, timer={door.close_timer}")
    door.tick(3)
    print(f"3秒后: {door.state_name}, timer={door.close_timer}")
    door.person_detected()
    print(f"续期后: {door.state_name}, timer={door.close_timer}")
    door.tick(5)
    print(f"再过5秒: {door.state_name}, timer={door.close_timer}")


# ============ v4: 通用 FSM 框架片段 (升级版: on_enter/on_exit + 内部事件) ============
#
# 设计思路: 在 Lesson 02 的 GuardedFSM 基础上，增加进入/离开状态时的
# 钩子调用，并允许 Action 内部再触发一次 fire()，让"内部事件"(定时器到时
# 后自己发给自己的事件)和"外部事件"共用同一套入口。什么时候值得升级到
# 这一版框架，而不是用 Lesson 02 的版本? 当案例里出现"进入/离开某状态时
# 必须做一件配套的收尾工作(启动/取消定时器、释放锁、关闭连接)"，且这件事
# 不管从哪条路径离开都必须发生时，就值得把它做成通用的 on_enter/on_exit
# 钩子，而不是在每一条相关的转移规则里都重复写一遍。


class DoorState(Enum):
    CLOSED = auto()
    OPENING = auto()
    OPEN = auto()
    CLOSING = auto()


class DoorEvent(Enum):
    PERSON_DETECTED = auto()
    FULLY_OPEN = auto()
    FULLY_CLOSED = auto()
    OBSTRUCTION_DETECTED = auto()
    CLOSE_TIMER_EXPIRED = auto()


@dataclass
class Transition:
    from_state: Enum
    event: Enum
    to_state: Enum
    action: Callable[["DoorCtx"], None] | None = None


@dataclass
class LifecycleFSM:
    """支持 on_enter/on_exit 钩子的通用 FSM 引擎。"""

    initial_state: Enum
    transitions: list[Transition]
    on_enter: dict[Enum, Callable[["DoorCtx"], None]] = field(default_factory=dict)
    on_exit: dict[Enum, Callable[["DoorCtx"], None]] = field(default_factory=dict)
    on_illegal: Callable[[Enum, Enum], None] | None = None
    _current: Enum = field(init=False)
    _table: dict[tuple[Enum, Enum], Transition] = field(init=False)

    def __post_init__(self) -> None:
        self._current = self.initial_state
        self._table = {(t.from_state, t.event): t for t in self.transitions}

    @property
    def current_state(self) -> Enum:
        return self._current

    def fire(self, event: Enum, ctx: "DoorCtx") -> None:
        transition = self._table.get((self._current, event))
        if transition is None:
            if self.on_illegal is not None:
                self.on_illegal(self._current, event)
            return
        if transition.action is not None:
            transition.action(ctx)
        if self._current in self.on_exit:
            self.on_exit[self._current](ctx)
        self._current = transition.to_state
        if self._current in self.on_enter:
            self.on_enter[self._current](ctx)


@dataclass
class DoorCtx:
    close_timer: float | None = None
    fsm: LifecycleFSM | None = None

    def tick(self, seconds: float) -> None:
        if self.close_timer is None:
            return
        self.close_timer -= seconds
        if self.close_timer <= 0 and self.fsm is not None:
            # "内部事件": 定时器到时后，状态机自己触发一次 fire()。
            self.fsm.fire(DoorEvent.CLOSE_TIMER_EXPIRED, self)


def build_auto_door_fsm() -> LifecycleFSM:
    def start_timer(ctx: DoorCtx) -> None:
        ctx.close_timer = CLOSE_TIMER_DURATION
        print(f"  on_enter[OPEN]: 启动关门倒计时{CLOSE_TIMER_DURATION}秒")

    def cancel_timer(ctx: DoorCtx) -> None:
        ctx.close_timer = None

    transitions = [
        Transition(
            DoorState.CLOSED,
            DoorEvent.PERSON_DETECTED,
            DoorState.OPENING,
            action=lambda ctx: print("  action: 检测到有人，启动开门电机"),
        ),
        Transition(
            DoorState.OPENING,
            DoorEvent.FULLY_OPEN,
            DoorState.OPEN,
            action=lambda ctx: print("  action: 门已全开，停止电机"),
        ),
        Transition(
            DoorState.OPENING,
            DoorEvent.PERSON_DETECTED,
            DoorState.OPENING,
            action=lambda ctx: print("  action: 门已经在开，忽略"),
        ),
        Transition(
            DoorState.OPEN,
            DoorEvent.PERSON_DETECTED,
            DoorState.OPEN,
            action=lambda ctx: print("  action: 重置关门倒计时(经由 on_exit+on_enter 重新触发)"),
        ),
        Transition(
            DoorState.OPEN,
            DoorEvent.CLOSE_TIMER_EXPIRED,
            DoorState.CLOSING,
            action=lambda ctx: print("  action: 关门倒计时到时，启动关门电机  <- 纯由时间驱动"),
        ),
        Transition(
            DoorState.CLOSING,
            DoorEvent.FULLY_CLOSED,
            DoorState.CLOSED,
            action=lambda ctx: print("  action: 门已全关，停止电机"),
        ),
        Transition(
            DoorState.CLOSING,
            DoorEvent.OBSTRUCTION_DETECTED,
            DoorState.OPENING,
            action=lambda ctx: print("  action: 检测到障碍物，反转重新开门"),
        ),
        Transition(
            DoorState.CLOSING,
            DoorEvent.PERSON_DETECTED,
            DoorState.OPENING,
            action=lambda ctx: print("  action: 关门中检测到有人，反转重新开门"),
        ),
    ]

    def log_illegal(state: Enum, event: Enum) -> None:
        print(f"  警告/忽略: state={state.name}, event={event.name} 无对应转移")

    return LifecycleFSM(
        initial_state=DoorState.CLOSED,
        transitions=transitions,
        on_enter={DoorState.OPEN: start_timer},
        on_exit={DoorState.OPEN: cancel_timer},
        on_illegal=log_illegal,
    )


def demo_v4() -> None:
    print("\n--- v4: 通用 FSM 框架片段 (升级版: on_enter/on_exit + 内部事件) ---")
    fsm = build_auto_door_fsm()
    ctx = DoorCtx(fsm=fsm)
    print(f"初始状态: {fsm.current_state.name}")

    fsm.fire(DoorEvent.PERSON_DETECTED, ctx)
    fsm.fire(DoorEvent.FULLY_OPEN, ctx)
    print(f"-> {fsm.current_state.name}, timer={ctx.close_timer}")

    ctx.tick(3)
    print(f"3秒后 -> {fsm.current_state.name}, timer={ctx.close_timer}")

    fsm.fire(DoorEvent.PERSON_DETECTED, ctx)
    print(f"续期后 -> {fsm.current_state.name}, timer={ctx.close_timer}")

    ctx.tick(5)
    print(f"再过5秒 -> {fsm.current_state.name}, timer={ctx.close_timer}")


if __name__ == "__main__":
    demo_v1()
    demo_v2()
    demo_v3()
    demo_v4()
