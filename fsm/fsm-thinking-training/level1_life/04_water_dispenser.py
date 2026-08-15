"""Lesson 04: 饮水机 (Level 1 - 生活场景).

对应 04_water_dispenser.md。核心训练点: 区分"真正的状态"、"标志位(guard数据)"
和"应该拆成正交状态机的另一个维度"。

    主状态机(出水控制): STANDBY / DISPENSING_HOT / DISPENSING_COLD
    标志位(不是状态): water_low(缺水)
    正交的第二个状态机(加热子系统，仅在 v4 中完整展示): HEATING / KEEPING_WARM

    v1: 最简单版本 —— Enum + match-case，标志位是裸露的布尔属性
    v2: Transition Table 版 —— guard 读取标志位，状态数量不因标志位膨胀
    v3: State Pattern 版 —— 每个真正的状态是一个类，不为标志位单独建类
    v4: 通用 FSM 框架 + 独立的加热子状态机(正交状态机组合示范)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable


# ============ v1: 简单版 (Enum + match-case，标志位是裸露的布尔属性) ============
#
# 设计思路: 主状态用 Enum 表示；water_low、heater_ready 是普通的布尔实例
# 属性，在 match 分支内部用 if 读取它们做判断。它们从不出现在 match 的
# case 目标里，只是被"读取"，这一点和"状态"的用法形成对比。


class DispenserStateV1(Enum):
    STANDBY = auto()
    DISPENSING_HOT = auto()
    DISPENSING_COLD = auto()


class WaterDispenserV1:
    def __init__(self) -> None:
        self.state: DispenserStateV1 = DispenserStateV1.STANDBY
        self.water_low: bool = False
        self.heater_ready: bool = True  # 简化: 默认已烧开一次，可以出热水

    def hot_button_pressed(self) -> None:
        match self.state:
            case DispenserStateV1.STANDBY:
                if self.water_low:
                    print("  忽略: 缺水，蜂鸣提示")
                elif not self.heater_ready:
                    print("  忽略: 水温未达标，请稍候")
                else:
                    self.state = DispenserStateV1.DISPENSING_HOT
                    print("  action: 打开热水阀")
            case _:
                print("  忽略: 出水中，重复按下无新效果")

    def cold_button_pressed(self) -> None:
        match self.state:
            case DispenserStateV1.STANDBY:
                if self.water_low:
                    print("  忽略: 缺水，蜂鸣提示")
                else:
                    self.state = DispenserStateV1.DISPENSING_COLD
                    print("  action: 打开冷水阀")
            case _:
                print("  忽略: 出水中，重复按下无新效果")

    def button_released(self) -> None:
        match self.state:
            case DispenserStateV1.DISPENSING_HOT:
                self.state = DispenserStateV1.STANDBY
                print("  action: 关闭热水阀")
            case DispenserStateV1.DISPENSING_COLD:
                self.state = DispenserStateV1.STANDBY
                print("  action: 关闭冷水阀")
            case _:
                print("  忽略: 当前并未出水")

    def water_low_detected(self) -> None:
        self.water_low = True
        match self.state:
            case DispenserStateV1.STANDBY:
                print("  action: 仅设置缺水标志位，点亮指示灯")
            case _:
                self.state = DispenserStateV1.STANDBY
                print("  action: 出水中检测到缺水，强制关阀并报警(安全优先)")

    def water_refilled(self) -> None:
        self.water_low = False
        print("  action: 清除缺水标志位，熄灭指示灯（不自动恢复出水）")

    def __repr__(self) -> str:
        return (
            f"WaterDispenserV1(state={self.state.name}, "
            f"water_low={self.water_low}, heater_ready={self.heater_ready})"
        )


def demo_v1() -> None:
    print("\n--- v1: 简单版 (match-case + 裸露布尔属性) ---")
    d = WaterDispenserV1()
    print(d)
    d.hot_button_pressed()
    d.button_released()
    d.cold_button_pressed()
    d.water_low_detected()  # 出水中缺水 -> 强制停止
    print(d)
    d.hot_button_pressed()  # 应被guard拒绝(缺水)
    d.water_refilled()
    d.hot_button_pressed()  # 现在应该可以出热水了
    print(d)


# ============ v2: Transition Table 版 (guard 读取标志位) ============
#
# 设计思路: 转移表的 from_state/to_state 永远只在三个真正的状态之间
# 取值，guard 函数负责读取 water_low / heater_ready 这些标志位来决定
# 走哪条规则——状态数量不会因为标志位组合的增加而膨胀。


class DispenserStateV2(Enum):
    STANDBY = auto()
    DISPENSING_HOT = auto()
    DISPENSING_COLD = auto()


class DispenserEventV2(Enum):
    HOT_BUTTON_PRESSED = auto()
    COLD_BUTTON_PRESSED = auto()
    BUTTON_RELEASED = auto()
    WATER_LOW_DETECTED = auto()
    WATER_REFILLED = auto()


@dataclass
class DispenserCtxV2:
    water_low: bool = False
    heater_ready: bool = True


@dataclass
class RuleV2:
    state: DispenserStateV2
    event: DispenserEventV2
    guard: Callable[[DispenserCtxV2], bool]
    next_state: DispenserStateV2
    action: Callable[[DispenserCtxV2], None]


class WaterDispenserV2:
    def __init__(self) -> None:
        self.state: DispenserStateV2 = DispenserStateV2.STANDBY
        self.ctx = DispenserCtxV2()
        self._rules: list[RuleV2] = self._build_rules()

    def _build_rules(self) -> list[RuleV2]:
        return [
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.HOT_BUTTON_PRESSED,
                guard=lambda c: not c.water_low and c.heater_ready,
                next_state=DispenserStateV2.DISPENSING_HOT,
                action=lambda c: print("  action: 打开热水阀"),
            ),
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.HOT_BUTTON_PRESSED,
                guard=lambda c: c.water_low,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  忽略: 缺水，蜂鸣提示"),
            ),
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.HOT_BUTTON_PRESSED,
                guard=lambda c: not c.water_low and not c.heater_ready,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  忽略: 水温未达标，请稍候"),
            ),
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.COLD_BUTTON_PRESSED,
                guard=lambda c: not c.water_low,
                next_state=DispenserStateV2.DISPENSING_COLD,
                action=lambda c: print("  action: 打开冷水阀"),
            ),
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.COLD_BUTTON_PRESSED,
                guard=lambda c: c.water_low,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  忽略: 缺水，蜂鸣提示"),
            ),
            RuleV2(
                DispenserStateV2.DISPENSING_HOT,
                DispenserEventV2.BUTTON_RELEASED,
                guard=lambda c: True,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  action: 关闭热水阀"),
            ),
            RuleV2(
                DispenserStateV2.DISPENSING_COLD,
                DispenserEventV2.BUTTON_RELEASED,
                guard=lambda c: True,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  action: 关闭冷水阀"),
            ),
            RuleV2(
                DispenserStateV2.DISPENSING_HOT,
                DispenserEventV2.WATER_LOW_DETECTED,
                guard=lambda c: True,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  action: 出水中缺水，强制关阀并报警"),
            ),
            RuleV2(
                DispenserStateV2.DISPENSING_COLD,
                DispenserEventV2.WATER_LOW_DETECTED,
                guard=lambda c: True,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  action: 出水中缺水，强制关阀并报警"),
            ),
            RuleV2(
                DispenserStateV2.STANDBY,
                DispenserEventV2.WATER_LOW_DETECTED,
                guard=lambda c: True,
                next_state=DispenserStateV2.STANDBY,
                action=lambda c: print("  action: 仅设置缺水标志位，点亮指示灯"),
            ),
        ]

    def fire(self, event: DispenserEventV2) -> None:
        # 标志位在事件到达之前先更新(WATER_LOW_DETECTED / WATER_REFILLED 本身就是在更新标志位)。
        if event is DispenserEventV2.WATER_LOW_DETECTED:
            self.ctx.water_low = True
        elif event is DispenserEventV2.WATER_REFILLED:
            self.ctx.water_low = False
            print("  action: 清除缺水标志位，熄灭指示灯（不自动恢复出水）")
            return
        for rule in self._rules:
            if rule.state == self.state and rule.event == event and rule.guard(self.ctx):
                rule.action(self.ctx)
                self.state = rule.next_state
                return
        print(f"  忽略未定义转移: state={self.state.name}, event={event.name}")

    def __repr__(self) -> str:
        return (
            f"WaterDispenserV2(state={self.state.name}, "
            f"water_low={self.ctx.water_low}, heater_ready={self.ctx.heater_ready})"
        )


def demo_v2() -> None:
    print("\n--- v2: Transition Table 版 (guard 读取标志位) ---")
    d = WaterDispenserV2()
    print(d)
    d.fire(DispenserEventV2.HOT_BUTTON_PRESSED)
    d.fire(DispenserEventV2.BUTTON_RELEASED)
    d.fire(DispenserEventV2.COLD_BUTTON_PRESSED)
    d.fire(DispenserEventV2.WATER_LOW_DETECTED)
    print(d)
    d.fire(DispenserEventV2.HOT_BUTTON_PRESSED)
    d.fire(DispenserEventV2.WATER_REFILLED)
    d.fire(DispenserEventV2.HOT_BUTTON_PRESSED)
    print(d)


# ============ v3: State Pattern 版 (真正的状态才建类，标志位不建类) ============
#
# 设计思路: STANDBY / DISPENSING_HOT / DISPENSING_COLD 各自是一个类。
# water_low 和 heater_ready 存在共享上下文里，状态类的方法读取它们做
# 判断，但没有为"缺水"单独写一个 State 类——它没有独立的行为分支。


@dataclass
class DispenserContextV3:
    water_low: bool = False
    heater_ready: bool = True
    _state: "DispenserStateBaseV3" = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._state = StandbyStateV3()

    def transition_to(self, state: "DispenserStateBaseV3") -> None:
        self._state = state

    def hot_button_pressed(self) -> None:
        self._state.on_hot_button(self)

    def cold_button_pressed(self) -> None:
        self._state.on_cold_button(self)

    def button_released(self) -> None:
        self._state.on_release(self)

    def water_low_detected(self) -> None:
        self.water_low = True
        self._state.on_water_low(self)

    def water_refilled(self) -> None:
        self.water_low = False
        print("  action: 清除缺水标志位，熄灭指示灯（不自动恢复出水）")

    @property
    def state_name(self) -> str:
        return self._state.name


class DispenserStateBaseV3:
    name = "AbstractState"

    def on_hot_button(self, ctx: DispenserContextV3) -> None:
        print(f"  [{self.name}] 忽略: 出水中，重复按下无新效果")

    def on_cold_button(self, ctx: DispenserContextV3) -> None:
        print(f"  [{self.name}] 忽略: 出水中，重复按下无新效果")

    def on_release(self, ctx: DispenserContextV3) -> None:
        print(f"  [{self.name}] 忽略: 当前并未出水")

    def on_water_low(self, ctx: DispenserContextV3) -> None:
        ctx.transition_to(StandbyStateV3())
        print("  action: 出水中缺水，强制关阀并报警")


class StandbyStateV3(DispenserStateBaseV3):
    name = "STANDBY"

    def on_hot_button(self, ctx: DispenserContextV3) -> None:
        if ctx.water_low:
            print("  忽略: 缺水，蜂鸣提示")
        elif not ctx.heater_ready:
            print("  忽略: 水温未达标，请稍候")
        else:
            ctx.transition_to(DispensingHotStateV3())
            print("  action: 打开热水阀")

    def on_cold_button(self, ctx: DispenserContextV3) -> None:
        if ctx.water_low:
            print("  忽略: 缺水，蜂鸣提示")
        else:
            ctx.transition_to(DispensingColdStateV3())
            print("  action: 打开冷水阀")

    def on_water_low(self, ctx: DispenserContextV3) -> None:
        print("  action: 仅设置缺水标志位，点亮指示灯")


class DispensingHotStateV3(DispenserStateBaseV3):
    name = "DISPENSING_HOT"

    def on_release(self, ctx: DispenserContextV3) -> None:
        ctx.transition_to(StandbyStateV3())
        print("  action: 关闭热水阀")


class DispensingColdStateV3(DispenserStateBaseV3):
    name = "DISPENSING_COLD"

    def on_release(self, ctx: DispenserContextV3) -> None:
        ctx.transition_to(StandbyStateV3())
        print("  action: 关闭冷水阀")


def demo_v3() -> None:
    print("\n--- v3: State Pattern 版 (真正的状态才建类) ---")
    d = DispenserContextV3()
    print(f"初始状态: {d.state_name}")
    d.hot_button_pressed()
    d.button_released()
    d.cold_button_pressed()
    d.water_low_detected()
    print(f"当前: {d.state_name}, water_low={d.water_low}")
    d.hot_button_pressed()
    d.water_refilled()
    d.hot_button_pressed()
    print(f"最终: {d.state_name}")


# ============ v4: 通用 FSM 框架 + 独立加热子状态机 (正交状态机组合) ============
#
# 设计思路: 复用 Lesson 02/03 演化出的带 guard 的通用 FSM 引擎，实例化
# 两个完全独立的 FSM —— 一个是主状态机(出水)，另一个是加热子系统
# (HEATING / KEEPING_WARM)。两者互不干扰，只通过一个只读的 guard
# 查询("加热子系统当前是否为 KEEPING_WARM")产生联系。这就是"正交状态
# 机"的组合方式: 当系统存在多个互不排斥、各自独立演变的行为维度时，
# 分别建模、用少量标志位连接，而不是合并成一个状态数暴涨的大状态机。


class DispenserState(Enum):
    STANDBY = auto()
    DISPENSING_HOT = auto()
    DISPENSING_COLD = auto()


class DispenserEvent(Enum):
    HOT_BUTTON_PRESSED = auto()
    COLD_BUTTON_PRESSED = auto()
    BUTTON_RELEASED = auto()
    WATER_LOW_DETECTED = auto()
    WATER_REFILLED = auto()


class HeaterState(Enum):
    HEATING = auto()
    KEEPING_WARM = auto()


class HeaterEvent(Enum):
    TEMP_REACHED_TARGET = auto()
    TEMP_DROPPED_BELOW_TARGET = auto()


@dataclass
class Transition:
    from_state: Enum
    event: Enum
    to_state: Enum
    guard: Callable[["SharedCtx"], bool] | None = None
    action: Callable[["SharedCtx"], None] | None = None


@dataclass
class FSM:
    """复用自 Lesson 02/03 的通用引擎: 支持可选 guard 的转移表。"""

    initial_state: Enum
    transitions: list[Transition]
    on_illegal: Callable[[Enum, Enum], None] | None = None
    _current: Enum = field(init=False)

    def __post_init__(self) -> None:
        self._current = self.initial_state

    @property
    def current_state(self) -> Enum:
        return self._current

    def fire(self, event: Enum, ctx: "SharedCtx") -> None:
        for t in self.transitions:
            if t.from_state == self._current and t.event == event:
                if t.guard is not None and not t.guard(ctx):
                    continue
                if t.action is not None:
                    t.action(ctx)
                self._current = t.to_state
                return
        if self.on_illegal is not None:
            self.on_illegal(self._current, event)


@dataclass
class SharedCtx:
    """主状态机和加热子状态机共享的上下文，只存放标志位数据。"""

    water_low: bool = False
    heater_fsm: FSM | None = None

    @property
    def heater_ready(self) -> bool:
        return self.heater_fsm is not None and self.heater_fsm.current_state == HeaterState.KEEPING_WARM


def build_heater_fsm() -> FSM:
    """加热子系统: 与出水模式完全正交，独立演变。"""
    return FSM(
        initial_state=HeaterState.HEATING,
        transitions=[
            Transition(
                HeaterState.HEATING,
                HeaterEvent.TEMP_REACHED_TARGET,
                HeaterState.KEEPING_WARM,
                action=lambda ctx: print("  [加热子系统] action: 达到目标温度，切换到保温"),
            ),
            Transition(
                HeaterState.KEEPING_WARM,
                HeaterEvent.TEMP_DROPPED_BELOW_TARGET,
                HeaterState.HEATING,
                action=lambda ctx: print("  [加热子系统] action: 温度回落，重新加热"),
            ),
        ],
    )


def build_dispenser_fsm() -> FSM:
    """主状态机: 出水控制，通过 guard 查询 ctx.heater_ready(来自另一个FSM)。"""

    def log_illegal(state: Enum, event: Enum) -> None:
        print(f"  忽略未定义转移: state={state.name}, event={event.name}")

    return FSM(
        initial_state=DispenserState.STANDBY,
        transitions=[
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.HOT_BUTTON_PRESSED,
                DispenserState.DISPENSING_HOT,
                guard=lambda ctx: not ctx.water_low and ctx.heater_ready,
                action=lambda ctx: print("  action: 打开热水阀"),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.HOT_BUTTON_PRESSED,
                DispenserState.STANDBY,
                guard=lambda ctx: ctx.water_low,
                action=lambda ctx: print("  忽略: 缺水，蜂鸣提示"),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.HOT_BUTTON_PRESSED,
                DispenserState.STANDBY,
                guard=lambda ctx: not ctx.water_low and not ctx.heater_ready,
                action=lambda ctx: print("  忽略: 水温未达标(加热子系统仍在HEATING)，请稍候"),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.COLD_BUTTON_PRESSED,
                DispenserState.DISPENSING_COLD,
                guard=lambda ctx: not ctx.water_low,
                action=lambda ctx: print("  action: 打开冷水阀（不受加热子系统状态影响）"),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.COLD_BUTTON_PRESSED,
                DispenserState.STANDBY,
                guard=lambda ctx: ctx.water_low,
                action=lambda ctx: print("  忽略: 缺水，蜂鸣提示"),
            ),
            Transition(
                DispenserState.DISPENSING_HOT,
                DispenserEvent.BUTTON_RELEASED,
                DispenserState.STANDBY,
                action=lambda ctx: print("  action: 关闭热水阀"),
            ),
            Transition(
                DispenserState.DISPENSING_COLD,
                DispenserEvent.BUTTON_RELEASED,
                DispenserState.STANDBY,
                action=lambda ctx: print("  action: 关闭冷水阀"),
            ),
            Transition(
                DispenserState.DISPENSING_HOT,
                DispenserEvent.WATER_LOW_DETECTED,
                DispenserState.STANDBY,
                action=lambda ctx: (
                    setattr(ctx, "water_low", True),
                    print("  action: 出水中缺水，强制关阀并报警"),
                ),
            ),
            Transition(
                DispenserState.DISPENSING_COLD,
                DispenserEvent.WATER_LOW_DETECTED,
                DispenserState.STANDBY,
                action=lambda ctx: (
                    setattr(ctx, "water_low", True),
                    print("  action: 出水中缺水，强制关阀并报警"),
                ),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.WATER_LOW_DETECTED,
                DispenserState.STANDBY,
                action=lambda ctx: (
                    setattr(ctx, "water_low", True),
                    print("  action: 仅设置缺水标志位，点亮指示灯"),
                ),
            ),
            Transition(
                DispenserState.STANDBY,
                DispenserEvent.WATER_REFILLED,
                DispenserState.STANDBY,
                action=lambda ctx: (
                    setattr(ctx, "water_low", False),
                    print("  action: 清除缺水标志位，熄灭指示灯（不自动恢复出水）"),
                ),
            ),
        ],
        on_illegal=log_illegal,
    )


def demo_v4() -> None:
    print("\n--- v4: 通用FSM框架 + 独立加热子状态机 (正交状态机组合) ---")
    ctx = SharedCtx()
    heater = build_heater_fsm()
    dispenser = build_dispenser_fsm()
    ctx.heater_fsm = heater

    print(f"初始: dispenser={dispenser.current_state.name}, heater={heater.current_state.name}")

    # 加热子系统独立演变，与出水模式完全无关。
    heater.fire(HeaterEvent.TEMP_REACHED_TARGET, ctx)
    print(f"heater -> {heater.current_state.name} (heater_ready={ctx.heater_ready})")

    dispenser.fire(DispenserEvent.HOT_BUTTON_PRESSED, ctx)
    print(f"dispenser -> {dispenser.current_state.name}")
    dispenser.fire(DispenserEvent.BUTTON_RELEASED, ctx)

    # 出冷水完全不受加热子系统状态影响: 即使加热子系统正在重新加热，也能出冷水。
    heater.fire(HeaterEvent.TEMP_DROPPED_BELOW_TARGET, ctx)
    print(f"heater -> {heater.current_state.name} (heater_ready={ctx.heater_ready})")
    dispenser.fire(DispenserEvent.COLD_BUTTON_PRESSED, ctx)
    print(f"dispenser -> {dispenser.current_state.name} (出冷水时加热子系统={heater.current_state.name})")
    dispenser.fire(DispenserEvent.WATER_LOW_DETECTED, ctx)
    print(f"dispenser -> {dispenser.current_state.name} (water_low={ctx.water_low})")


if __name__ == "__main__":
    demo_v1()
    demo_v2()
    demo_v3()
    demo_v4()
