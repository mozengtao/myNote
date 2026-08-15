"""Lesson 02: 电梯 (Level 1 - 生活场景).

对应 02_elevator.md。核心训练点: "依赖历史状态"——下一步该往哪走,不能只看
当前处于哪个状态(IDLE / MOVING_UP / MOVING_DOWN / DOOR_OPEN),还要看
持续累积的历史数据: 当前楼层(current_floor)、待处理请求队列
(pending_requests)、上一次运行方向(last_direction)。

    v1: 最简单版本 —— Enum + match-case，数据存成实例属性，方法内部混合判断
    v2: Transition Table 版 —— 带 guard(条件函数)的数据驱动表
    v3: State Pattern 版 —— 每个状态是一个类，持有对上下文的引用
    v4: 通用 FSM 框架片段(升级版) —— Transition 支持 guard 与动态 next_state
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


# ============ v1: 简单版 (Enum + match-case，数据存成实例属性) ============
#
# 设计思路: 状态仍用 Enum 表示，但方法内部除了 match 状态，还要嵌套判断
# current_floor 是否在 pending 队列里、队列里是否存在同向请求等。
# 这一版故意保留"稍微开始变得繁琐"的写法，用来对比 v2 引入 guard 后的改进。


class ElevatorStateV1(Enum):
    IDLE = auto()
    MOVING_UP = auto()
    MOVING_DOWN = auto()
    DOOR_OPEN = auto()


class ElevatorV1:
    """最简单版本: match-case 硬编码，历史数据是裸露的实例属性。"""

    def __init__(self, start_floor: int = 1) -> None:
        self.state: ElevatorStateV1 = ElevatorStateV1.IDLE
        self.current_floor: int = start_floor
        self.pending: set[int] = set()
        self.last_direction: str | None = None  # "UP" / "DOWN" / None

    def request_floor(self, floor: int) -> None:
        match self.state:
            case ElevatorStateV1.IDLE:
                if floor == self.current_floor:
                    self.state = ElevatorStateV1.DOOR_OPEN
                    print(f"  action: 已在{floor}楼，直接开门")
                elif floor > self.current_floor:
                    self.pending.add(floor)
                    self.last_direction = "UP"
                    self.state = ElevatorStateV1.MOVING_UP
                    print(f"  action: 加入请求{floor}楼，开始上行")
                else:
                    self.pending.add(floor)
                    self.last_direction = "DOWN"
                    self.state = ElevatorStateV1.MOVING_DOWN
                    print(f"  action: 加入请求{floor}楼，开始下行")
            case ElevatorStateV1.MOVING_UP | ElevatorStateV1.MOVING_DOWN:
                self.pending.add(floor)
                print(f"  action: 运行中，{floor}楼请求加入队列，继续原方向")
            case ElevatorStateV1.DOOR_OPEN:
                self.pending.add(floor)
                print(f"  action: 门开着，{floor}楼请求加入队列，重置关门倒计时")

    def arrived_at_floor(self, floor: int) -> None:
        self.current_floor = floor
        match self.state:
            case ElevatorStateV1.MOVING_UP | ElevatorStateV1.MOVING_DOWN:
                if floor in self.pending:
                    self.pending.discard(floor)
                    self.state = ElevatorStateV1.DOOR_OPEN
                    print(f"  action: 到达{floor}楼(请求楼层)，停止开门")
                else:
                    print(f"  action: 到达{floor}楼(非请求楼层)，继续行驶")
            case _:
                print(f"  警告: 电梯未在移动，忽略异常的到站事件({floor}楼)")

    def door_timeout(self) -> None:
        if self.state != ElevatorStateV1.DOOR_OPEN:
            print("  警告: 电梯未开门，忽略关门超时事件")
            return
        if not self.pending:
            self.state = ElevatorStateV1.IDLE
            print("  action: 队列为空，关门后进入IDLE")
            return
        same_direction_exists = any(
            (f > self.current_floor) == (self.last_direction == "UP")
            for f in self.pending
        )
        if same_direction_exists:
            self.state = (
                ElevatorStateV1.MOVING_UP
                if self.last_direction == "UP"
                else ElevatorStateV1.MOVING_DOWN
            )
            print(f"  action: 关门，沿原方向({self.last_direction})继续服务")
        else:
            self.last_direction = "DOWN" if self.last_direction == "UP" else "UP"
            self.state = (
                ElevatorStateV1.MOVING_UP
                if self.last_direction == "UP"
                else ElevatorStateV1.MOVING_DOWN
            )
            print(f"  action: 关门，掉头，新方向({self.last_direction})")

    def __repr__(self) -> str:
        return (
            f"ElevatorV1(state={self.state.name}, floor={self.current_floor}, "
            f"pending={sorted(self.pending)}, last_dir={self.last_direction})"
        )


def demo_v1() -> None:
    print("\n--- v1: 简单版 (match-case + 裸露实例属性) ---")
    car = ElevatorV1(start_floor=1)
    print(car)
    car.request_floor(5)
    car.request_floor(8)
    for floor in (2, 3, 4, 5):
        car.arrived_at_floor(floor)
    car.door_timeout()
    print(car)


# ============ v2: Transition Table 版 (带 Guard 的数据驱动表) ============
#
# 设计思路: 与 Lesson 01 不同，同一个 (state, event) 组合在电梯里可能因为
# 历史数据不同而走向不同的 next_state，所以每条规则除了 (state, event)，
# 还带一个 guard(上下文 -> bool)。fire() 按顺序找第一个 guard 成立的规则执行。
# 好处: "依赖历史数据做决策"的地方全部集中在这张表里，一眼可查；
# 新增一条调度策略，只需要加一条规则，不需要改已有代码。


class ElevatorStateV2(Enum):
    IDLE = auto()
    MOVING_UP = auto()
    MOVING_DOWN = auto()
    DOOR_OPEN = auto()


class ElevatorEventV2(Enum):
    REQUEST_FLOOR = auto()
    ARRIVED_AT_FLOOR = auto()
    DOOR_TIMEOUT = auto()


@dataclass
class ElevatorContextV2:
    current_floor: int
    pending: set[int] = field(default_factory=set)
    last_direction: str | None = None
    payload_floor: int | None = None  # 本次事件携带的楼层参数(若有)


@dataclass
class RuleV2:
    state: ElevatorStateV2
    event: ElevatorEventV2
    guard: Callable[[ElevatorContextV2], bool]
    next_state: ElevatorStateV2
    action: Callable[[ElevatorContextV2], None]


def _same_direction_request_exists(ctx: ElevatorContextV2) -> bool:
    if not ctx.pending or ctx.last_direction is None:
        return False
    return any(
        (f > ctx.current_floor) == (ctx.last_direction == "UP") for f in ctx.pending
    )


class ElevatorV2:
    """Transition Table 版: 带 guard 条件的数据驱动表。"""

    def __init__(self, start_floor: int = 1) -> None:
        self.state: ElevatorStateV2 = ElevatorStateV2.IDLE
        self.ctx = ElevatorContextV2(current_floor=start_floor)
        self._rules: list[RuleV2] = self._build_rules()

    def _build_rules(self) -> list[RuleV2]:
        def open_door(ctx: ElevatorContextV2) -> None:
            print(f"  action: 到达/已在{ctx.current_floor}楼，停止开门")
            ctx.pending.discard(ctx.current_floor)

        return [
            RuleV2(
                ElevatorStateV2.IDLE,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: ctx.payload_floor == ctx.current_floor,
                next_state=ElevatorStateV2.DOOR_OPEN,
                action=lambda ctx: print(f"  action: 已在{ctx.current_floor}楼，直接开门"),
            ),
            RuleV2(
                ElevatorStateV2.IDLE,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: ctx.payload_floor is not None
                and ctx.payload_floor > ctx.current_floor,
                next_state=ElevatorStateV2.MOVING_UP,
                action=lambda ctx: (
                    ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                    setattr(ctx, "last_direction", "UP"),
                    print(f"  action: 加入请求{ctx.payload_floor}楼，开始上行"),
                ),
            ),
            RuleV2(
                ElevatorStateV2.IDLE,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: ctx.payload_floor is not None
                and ctx.payload_floor < ctx.current_floor,
                next_state=ElevatorStateV2.MOVING_DOWN,
                action=lambda ctx: (
                    ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                    setattr(ctx, "last_direction", "DOWN"),
                    print(f"  action: 加入请求{ctx.payload_floor}楼，开始下行"),
                ),
            ),
            RuleV2(
                ElevatorStateV2.MOVING_UP,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: True,
                next_state=ElevatorStateV2.MOVING_UP,
                action=lambda ctx: (
                    ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                    print(f"  action: 上行中，{ctx.payload_floor}楼请求加入队列，继续上行"),
                ),
            ),
            RuleV2(
                ElevatorStateV2.MOVING_DOWN,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: True,
                next_state=ElevatorStateV2.MOVING_DOWN,
                action=lambda ctx: (
                    ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                    print(f"  action: 下行中，{ctx.payload_floor}楼请求加入队列，继续下行"),
                ),
            ),
            RuleV2(
                ElevatorStateV2.MOVING_UP,
                ElevatorEventV2.ARRIVED_AT_FLOOR,
                guard=lambda ctx: ctx.current_floor in ctx.pending,
                next_state=ElevatorStateV2.DOOR_OPEN,
                action=open_door,
            ),
            RuleV2(
                ElevatorStateV2.MOVING_UP,
                ElevatorEventV2.ARRIVED_AT_FLOOR,
                guard=lambda ctx: ctx.current_floor not in ctx.pending,
                next_state=ElevatorStateV2.MOVING_UP,
                action=lambda ctx: print(f"  action: 到达{ctx.current_floor}楼(非请求)，继续上行"),
            ),
            RuleV2(
                ElevatorStateV2.MOVING_DOWN,
                ElevatorEventV2.ARRIVED_AT_FLOOR,
                guard=lambda ctx: ctx.current_floor in ctx.pending,
                next_state=ElevatorStateV2.DOOR_OPEN,
                action=open_door,
            ),
            RuleV2(
                ElevatorStateV2.MOVING_DOWN,
                ElevatorEventV2.ARRIVED_AT_FLOOR,
                guard=lambda ctx: ctx.current_floor not in ctx.pending,
                next_state=ElevatorStateV2.MOVING_DOWN,
                action=lambda ctx: print(f"  action: 到达{ctx.current_floor}楼(非请求)，继续下行"),
            ),
            RuleV2(
                ElevatorStateV2.DOOR_OPEN,
                ElevatorEventV2.REQUEST_FLOOR,
                guard=lambda ctx: True,
                next_state=ElevatorStateV2.DOOR_OPEN,
                action=lambda ctx: (
                    ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                    print(f"  action: 门开着，{ctx.payload_floor}楼请求加入队列，重置关门倒计时"),
                ),
            ),
            RuleV2(
                ElevatorStateV2.DOOR_OPEN,
                ElevatorEventV2.DOOR_TIMEOUT,
                guard=lambda ctx: not ctx.pending,
                next_state=ElevatorStateV2.IDLE,
                action=lambda ctx: print("  action: 队列为空，关门后进入IDLE"),
            ),
            RuleV2(
                ElevatorStateV2.DOOR_OPEN,
                ElevatorEventV2.DOOR_TIMEOUT,
                guard=_same_direction_request_exists,
                next_state=ElevatorStateV2.MOVING_UP,  # 占位，实际方向在 action 里按 last_direction 判定
                action=lambda ctx: print(
                    f"  action: 关门，沿原方向({ctx.last_direction})继续服务  <- 依赖历史数据(last_direction)"
                ),
            ),
            RuleV2(
                ElevatorStateV2.DOOR_OPEN,
                ElevatorEventV2.DOOR_TIMEOUT,
                guard=lambda ctx: bool(ctx.pending),
                next_state=ElevatorStateV2.MOVING_UP,  # 占位，实际方向在 action 里翻转 last_direction
                action=lambda ctx: (
                    setattr(
                        ctx, "last_direction", "DOWN" if ctx.last_direction == "UP" else "UP"
                    ),
                    print(f"  action: 关门，掉头，新方向({ctx.last_direction})"),
                ),
            ),
        ]

    def fire(self, event: ElevatorEventV2, floor: int | None = None) -> None:
        self.ctx.payload_floor = floor
        if event == ElevatorEventV2.ARRIVED_AT_FLOOR and floor is not None:
            if self.state not in (ElevatorStateV2.MOVING_UP, ElevatorStateV2.MOVING_DOWN):
                print(f"  警告: 电梯未在移动，忽略异常的到站事件({floor}楼)")
                return
            self.ctx.current_floor = floor
        for rule in self._rules:
            if rule.state == self.state and rule.event == event and rule.guard(self.ctx):
                rule.action(self.ctx)
                # DOOR_TIMEOUT 的方向类规则里 next_state 是占位值，
                # 真正方向以 action 更新后的 last_direction 为准。
                if event == ElevatorEventV2.DOOR_TIMEOUT and rule.next_state != ElevatorStateV2.IDLE:
                    self.state = (
                        ElevatorStateV2.MOVING_UP
                        if self.ctx.last_direction == "UP"
                        else ElevatorStateV2.MOVING_DOWN
                    )
                else:
                    self.state = rule.next_state
                return
        print(f"  忽略未定义转移: state={self.state.name}, event={event.name}")

    def __repr__(self) -> str:
        return (
            f"ElevatorV2(state={self.state.name}, floor={self.ctx.current_floor}, "
            f"pending={sorted(self.ctx.pending)}, last_dir={self.ctx.last_direction})"
        )


def demo_v2() -> None:
    print("\n--- v2: Transition Table 版 (带 Guard 条件) ---")
    car = ElevatorV2(start_floor=1)
    print(car)
    car.fire(ElevatorEventV2.REQUEST_FLOOR, floor=5)
    car.fire(ElevatorEventV2.REQUEST_FLOOR, floor=8)
    for floor in (2, 3, 4, 5):
        car.fire(ElevatorEventV2.ARRIVED_AT_FLOOR, floor=floor)
    car.fire(ElevatorEventV2.DOOR_TIMEOUT)
    print(car)


# ============ v3: State Pattern 版 (每个状态是一个类) ============
#
# 设计思路: IDLE / MOVING_UP / MOVING_DOWN / DOOR_OPEN 各自是一个类，
# 自己实现 on_request / on_arrived / on_timeout。DOOR_OPEN 类的
# on_timeout 逻辑明显最复杂(要查队列、查方向)，IDLE 类最简单——
# 这正符合 State Pattern"各状态复杂度可以不同、互不干扰"的适用场景。


@dataclass
class ElevatorContextV3:
    current_floor: int
    pending: set[int] = field(default_factory=set)
    last_direction: str | None = None
    _state: "ElevatorStateBaseV3" = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._state = IdleStateV3()

    def transition_to(self, state: "ElevatorStateBaseV3") -> None:
        self._state = state

    def request_floor(self, floor: int) -> None:
        self._state.on_request(self, floor)

    def arrived_at_floor(self, floor: int) -> None:
        self._state.on_arrived(self, floor)

    def door_timeout(self) -> None:
        self._state.on_timeout(self)

    @property
    def state_name(self) -> str:
        return self._state.name


class ElevatorStateBaseV3:
    name: str = "AbstractState"

    def on_request(self, ctx: ElevatorContextV3, floor: int) -> None:
        raise NotImplementedError

    def on_arrived(self, ctx: ElevatorContextV3, floor: int) -> None:
        print(f"  警告: [{self.name}] 忽略异常的到站事件({floor}楼)")

    def on_timeout(self, ctx: ElevatorContextV3) -> None:
        print(f"  警告: [{self.name}] 忽略关门超时事件")


class IdleStateV3(ElevatorStateBaseV3):
    name = "IDLE"

    def on_request(self, ctx: ElevatorContextV3, floor: int) -> None:
        if floor == ctx.current_floor:
            print(f"  action: 已在{floor}楼，直接开门")
            ctx.transition_to(DoorOpenStateV3())
        elif floor > ctx.current_floor:
            ctx.pending.add(floor)
            ctx.last_direction = "UP"
            print(f"  action: 加入请求{floor}楼，开始上行")
            ctx.transition_to(MovingUpStateV3())
        else:
            ctx.pending.add(floor)
            ctx.last_direction = "DOWN"
            print(f"  action: 加入请求{floor}楼，开始下行")
            ctx.transition_to(MovingDownStateV3())


class MovingUpStateV3(ElevatorStateBaseV3):
    name = "MOVING_UP"

    def on_request(self, ctx: ElevatorContextV3, floor: int) -> None:
        ctx.pending.add(floor)
        print(f"  action: 运行中，{floor}楼请求加入队列，继续上行")

    def on_arrived(self, ctx: ElevatorContextV3, floor: int) -> None:
        ctx.current_floor = floor
        if floor in ctx.pending:
            ctx.pending.discard(floor)
            print(f"  action: 到达{floor}楼(请求楼层)，停止开门")
            ctx.transition_to(DoorOpenStateV3())
        else:
            print(f"  action: 到达{floor}楼(非请求楼层)，继续上行")


class MovingDownStateV3(ElevatorStateBaseV3):
    name = "MOVING_DOWN"

    def on_request(self, ctx: ElevatorContextV3, floor: int) -> None:
        ctx.pending.add(floor)
        print(f"  action: 运行中，{floor}楼请求加入队列，继续下行")

    def on_arrived(self, ctx: ElevatorContextV3, floor: int) -> None:
        ctx.current_floor = floor
        if floor in ctx.pending:
            ctx.pending.discard(floor)
            print(f"  action: 到达{floor}楼(请求楼层)，停止开门")
            ctx.transition_to(DoorOpenStateV3())
        else:
            print(f"  action: 到达{floor}楼(非请求楼层)，继续下行")


class DoorOpenStateV3(ElevatorStateBaseV3):
    name = "DOOR_OPEN"

    def on_request(self, ctx: ElevatorContextV3, floor: int) -> None:
        ctx.pending.add(floor)
        print(f"  action: 门开着，{floor}楼请求加入队列，重置关门倒计时")

    def on_timeout(self, ctx: ElevatorContextV3) -> None:
        if not ctx.pending:
            print("  action: 队列为空，关门后进入IDLE")
            ctx.transition_to(IdleStateV3())
            return
        same_direction_exists = any(
            (f > ctx.current_floor) == (ctx.last_direction == "UP") for f in ctx.pending
        )
        if not same_direction_exists:
            ctx.last_direction = "DOWN" if ctx.last_direction == "UP" else "UP"
            print(f"  action: 关门，掉头，新方向({ctx.last_direction})")
        else:
            print(f"  action: 关门，沿原方向({ctx.last_direction})继续服务  <- 依赖历史数据")
        ctx.transition_to(MovingUpStateV3() if ctx.last_direction == "UP" else MovingDownStateV3())


def demo_v3() -> None:
    print("\n--- v3: State Pattern 版 (每个状态是一个类) ---")
    ctx = ElevatorContextV3(current_floor=1)
    print(f"初始状态: {ctx.state_name}")
    ctx.request_floor(5)
    ctx.request_floor(8)
    for floor in (2, 3, 4, 5):
        ctx.arrived_at_floor(floor)
    ctx.door_timeout()
    print(f"最终状态: {ctx.state_name}, floor={ctx.current_floor}, pending={sorted(ctx.pending)}")


# ============ v4: 通用 FSM 框架片段 (升级版: 支持 guard) ============
#
# 设计思路: 在 Lesson 01 框架基础上，把 Transition 升级为支持:
#   - guard: 可选的条件函数，决定这条规则是否适用；
#   - next_state: 可以是固定值，也可以是"根据上下文计算"的函数。
# 什么时候要升级框架，而不是每次手写(如 v1)?
#   - 当发现"同一个 (state, event) 组合，因为历史数据不同会走向不同的
#     next_state"，说明简单表(Lesson 01 v4)已经表达不出这种业务逻辑，
#     必须引入 guard；
#   - 这也提示我们: 通用框架应该随着遇到的案例复杂度提升而逐步演化，
#     而不是要求第一版框架就"预见"所有未来可能出现的复杂度。


class ElevatorState(Enum):
    IDLE = auto()
    MOVING_UP = auto()
    MOVING_DOWN = auto()
    DOOR_OPEN = auto()


class ElevatorEvent(Enum):
    REQUEST_FLOOR = auto()
    ARRIVED_AT_FLOOR = auto()
    DOOR_TIMEOUT = auto()


@dataclass
class Context:
    """通用上下文基类语义: 具体案例可以在自己的 dataclass 里扩展字段。"""


@dataclass
class GuardedTransition:
    """一条支持条件判断的转移规则。"""

    from_state: Enum
    event: Enum
    to_state: Enum | Callable[[Context], Enum]
    guard: Callable[[Context], bool] | None = None
    action: Callable[[Context], None] | None = None

    def matches(self, state: Enum, event: Enum, ctx: Context) -> bool:
        if self.from_state != state or self.event != event:
            return False
        return self.guard is None or self.guard(ctx)

    def resolve_next_state(self, ctx: Context) -> Enum:
        return self.to_state(ctx) if callable(self.to_state) else self.to_state


@dataclass
class GuardedFSM:
    """支持 guard 条件的通用 FSM 引擎: 按顺序找第一个 guard 成立的规则。"""

    initial_state: Enum
    transitions: list[GuardedTransition]
    on_illegal: Callable[[Enum, Enum], None] | None = None
    _current: Enum = field(init=False)

    def __post_init__(self) -> None:
        self._current = self.initial_state

    @property
    def current_state(self) -> Enum:
        return self._current

    def fire(self, event: Enum, ctx: Context) -> None:
        for transition in self.transitions:
            if transition.matches(self._current, event, ctx):
                if transition.action is not None:
                    transition.action(ctx)
                self._current = transition.resolve_next_state(ctx)
                return
        if self.on_illegal is not None:
            self.on_illegal(self._current, event)


@dataclass
class ElevatorCtx(Context):
    current_floor: int
    pending: set[int] = field(default_factory=set)
    last_direction: str | None = None
    payload_floor: int | None = None


def build_elevator_fsm() -> GuardedFSM:
    def next_direction_state(ctx: ElevatorCtx) -> ElevatorState:
        return ElevatorState.MOVING_UP if ctx.last_direction == "UP" else ElevatorState.MOVING_DOWN

    def open_door(ctx: ElevatorCtx) -> None:
        print(f"  action: 到达/已在{ctx.current_floor}楼，停止开门")
        ctx.pending.discard(ctx.current_floor)

    def continue_same_direction(ctx: ElevatorCtx) -> None:
        print(f"  action: 关门，沿原方向({ctx.last_direction})继续服务  <- 依赖历史数据")

    def reverse_direction(ctx: ElevatorCtx) -> None:
        ctx.last_direction = "DOWN" if ctx.last_direction == "UP" else "UP"
        print(f"  action: 关门，掉头，新方向({ctx.last_direction})")

    transitions = [
        GuardedTransition(
            ElevatorState.IDLE,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.DOOR_OPEN,
            guard=lambda ctx: ctx.payload_floor == ctx.current_floor,
            action=lambda ctx: print(f"  action: 已在{ctx.current_floor}楼，直接开门"),
        ),
        GuardedTransition(
            ElevatorState.IDLE,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.MOVING_UP,
            guard=lambda ctx: ctx.payload_floor is not None and ctx.payload_floor > ctx.current_floor,
            action=lambda ctx: (
                ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                setattr(ctx, "last_direction", "UP"),
                print(f"  action: 加入请求{ctx.payload_floor}楼，开始上行"),
            ),
        ),
        GuardedTransition(
            ElevatorState.IDLE,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.MOVING_DOWN,
            guard=lambda ctx: ctx.payload_floor is not None and ctx.payload_floor < ctx.current_floor,
            action=lambda ctx: (
                ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                setattr(ctx, "last_direction", "DOWN"),
                print(f"  action: 加入请求{ctx.payload_floor}楼，开始下行"),
            ),
        ),
        GuardedTransition(
            ElevatorState.MOVING_UP,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.MOVING_UP,
            action=lambda ctx: (
                ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                print(f"  action: 上行中，{ctx.payload_floor}楼请求加入队列，继续上行"),
            ),
        ),
        GuardedTransition(
            ElevatorState.MOVING_DOWN,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.MOVING_DOWN,
            action=lambda ctx: (
                ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                print(f"  action: 下行中，{ctx.payload_floor}楼请求加入队列，继续下行"),
            ),
        ),
        GuardedTransition(
            ElevatorState.MOVING_UP,
            ElevatorEvent.ARRIVED_AT_FLOOR,
            to_state=ElevatorState.DOOR_OPEN,
            guard=lambda ctx: ctx.current_floor in ctx.pending,
            action=open_door,
        ),
        GuardedTransition(
            ElevatorState.MOVING_UP,
            ElevatorEvent.ARRIVED_AT_FLOOR,
            to_state=ElevatorState.MOVING_UP,
            guard=lambda ctx: ctx.current_floor not in ctx.pending,
            action=lambda ctx: print(f"  action: 到达{ctx.current_floor}楼(非请求)，继续上行"),
        ),
        GuardedTransition(
            ElevatorState.MOVING_DOWN,
            ElevatorEvent.ARRIVED_AT_FLOOR,
            to_state=ElevatorState.DOOR_OPEN,
            guard=lambda ctx: ctx.current_floor in ctx.pending,
            action=open_door,
        ),
        GuardedTransition(
            ElevatorState.MOVING_DOWN,
            ElevatorEvent.ARRIVED_AT_FLOOR,
            to_state=ElevatorState.MOVING_DOWN,
            guard=lambda ctx: ctx.current_floor not in ctx.pending,
            action=lambda ctx: print(f"  action: 到达{ctx.current_floor}楼(非请求)，继续下行"),
        ),
        GuardedTransition(
            ElevatorState.DOOR_OPEN,
            ElevatorEvent.REQUEST_FLOOR,
            to_state=ElevatorState.DOOR_OPEN,
            action=lambda ctx: (
                ctx.pending.add(ctx.payload_floor),  # type: ignore[func-returns-value]
                print(f"  action: 门开着，{ctx.payload_floor}楼请求加入队列，重置关门倒计时"),
            ),
        ),
        GuardedTransition(
            ElevatorState.DOOR_OPEN,
            ElevatorEvent.DOOR_TIMEOUT,
            to_state=ElevatorState.IDLE,
            guard=lambda ctx: not ctx.pending,
            action=lambda ctx: print("  action: 队列为空，关门后进入IDLE"),
        ),
        GuardedTransition(
            ElevatorState.DOOR_OPEN,
            ElevatorEvent.DOOR_TIMEOUT,
            to_state=next_direction_state,
            guard=_same_direction_exists_generic,
            action=continue_same_direction,
        ),
        GuardedTransition(
            ElevatorState.DOOR_OPEN,
            ElevatorEvent.DOOR_TIMEOUT,
            to_state=next_direction_state,
            guard=lambda ctx: bool(ctx.pending),
            action=reverse_direction,
        ),
    ]

    def log_illegal(state: Enum, event: Enum) -> None:
        print(f"  忽略/异常: state={state.name}, event={event.name}")

    return GuardedFSM(
        initial_state=ElevatorState.IDLE,
        transitions=transitions,
        on_illegal=log_illegal,
    )


def _same_direction_exists_generic(ctx: ElevatorCtx) -> bool:
    if not ctx.pending or ctx.last_direction is None:
        return False
    return any((f > ctx.current_floor) == (ctx.last_direction == "UP") for f in ctx.pending)


def demo_v4() -> None:
    print("\n--- v4: 通用 FSM 框架片段 (升级版: 支持 guard) ---")
    fsm = build_elevator_fsm()
    ctx = ElevatorCtx(current_floor=1)
    print(f"初始状态: {fsm.current_state.name}")

    ctx.payload_floor = 5
    fsm.fire(ElevatorEvent.REQUEST_FLOOR, ctx)
    print(f"-> {fsm.current_state.name}")

    ctx.payload_floor = 8
    fsm.fire(ElevatorEvent.REQUEST_FLOOR, ctx)
    print(f"-> {fsm.current_state.name}")

    for floor in (2, 3, 4, 5):
        ctx.current_floor = floor
        fsm.fire(ElevatorEvent.ARRIVED_AT_FLOOR, ctx)
        print(f"-> {fsm.current_state.name} (floor={ctx.current_floor})")

    fsm.fire(ElevatorEvent.DOOR_TIMEOUT, ctx)
    print(f"-> {fsm.current_state.name} (pending={sorted(ctx.pending)}, dir={ctx.last_direction})")


if __name__ == "__main__":
    demo_v1()
    demo_v2()
    demo_v3()
    demo_v4()
