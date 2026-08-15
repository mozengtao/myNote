"""Lesson 02: 角色攻击 (Level 2 - 小游戏).

对应文档: 02_character_attack.md

本文件围绕"不可打断状态"与"输入忽略 vs 输入排队"这两个核心概念,
提供四版实现:

    v1: Enum + match-case,逻辑手写在一个类里
    v2: Transition Table,显式区分"忽略"(表里无条目)和"排队"(自环转移+action)
    v3: State Pattern,连击窗口/排队标志下放到Attacking状态对象内部
    v4: 复用通用FSM框架,并演示"高优先级打断事件"的批量注册技巧
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Any, Callable, Optional, Protocol


# ============================================================
# v1: 最简单版本 —— Enum + match-case,逻辑写在一个类里
# ============================================================


class AttackStateV1(Enum):
    IDLE = auto()
    MOVING = auto()
    ATTACKING = auto()
    STAGGERED = auto()
    DEAD = auto()


class AttackEventV1(Enum):
    MOVE_PRESSED = auto()
    MOVE_RELEASED = auto()
    ATTACK_PRESSED = auto()
    ATTACK_FINISHED = auto()
    HIT_TAKEN = auto()
    STAGGER_FINISHED = auto()
    HP_ZERO = auto()


MAX_COMBO_STEP = 3


class FighterV1:
    """v1: 所有转移逻辑手写在一个大的match语句里。"""

    def __init__(self) -> None:
        self.state: AttackStateV1 = AttackStateV1.IDLE
        self.combo_step: int = 0
        self.combo_window_open: bool = False
        self.combo_queued: bool = False

    def handle_event(self, event: AttackEventV1) -> None:
        # hp_zero 优先级最高,可以从任意非Dead状态直接进入Dead。
        if event is AttackEventV1.HP_ZERO and self.state is not AttackStateV1.DEAD:
            self.state = AttackStateV1.DEAD
            print("[v1] * -> Dead (hp_zero,最高优先级打断)")
            return

        match (self.state, event):
            case (AttackStateV1.IDLE, AttackEventV1.MOVE_PRESSED):
                self.state = AttackStateV1.MOVING
                print("[v1] Idle -> Moving")

            case (AttackStateV1.IDLE, AttackEventV1.ATTACK_PRESSED):
                self._start_attack()

            case (AttackStateV1.IDLE, AttackEventV1.HIT_TAKEN):
                self.state = AttackStateV1.STAGGERED
                print("[v1] Idle -> Staggered")

            case (AttackStateV1.MOVING, AttackEventV1.MOVE_RELEASED):
                self.state = AttackStateV1.IDLE
                print("[v1] Moving -> Idle")

            case (AttackStateV1.MOVING, AttackEventV1.ATTACK_PRESSED):
                self._start_attack()

            case (AttackStateV1.MOVING, AttackEventV1.HIT_TAKEN):
                self.state = AttackStateV1.STAGGERED
                print("[v1] Moving -> Staggered")

            case (AttackStateV1.ATTACKING, AttackEventV1.ATTACK_PRESSED):
                if self.combo_window_open:
                    self.combo_queued = True
                    print("[v1] Attacking 收到 attack_pressed,连击窗口内 -> 排队")
                else:
                    print("[v1] Attacking 收到 attack_pressed,窗口外 -> 忽略")

            case (AttackStateV1.ATTACKING, AttackEventV1.MOVE_PRESSED):
                print("[v1] Attacking 收到 move_pressed -> 忽略(位置锁定)")

            case (AttackStateV1.ATTACKING, AttackEventV1.MOVE_RELEASED):
                print("[v1] Attacking 收到 move_released -> 忽略(位置锁定)")

            case (AttackStateV1.ATTACKING, AttackEventV1.ATTACK_FINISHED):
                if self.combo_queued and self.combo_step < MAX_COMBO_STEP:
                    self.combo_queued = False
                    self.combo_step += 1
                    self.combo_window_open = False
                    print(f"[v1] Attacking 续第{self.combo_step}段连击 (仍是Attacking)")
                else:
                    self.state = AttackStateV1.IDLE
                    self.combo_step = 0
                    self.combo_queued = False
                    self.combo_window_open = False
                    print("[v1] Attacking -> Idle (攻击结束,无排队)")

            case (AttackStateV1.ATTACKING, AttackEventV1.HIT_TAKEN):
                self.state = AttackStateV1.STAGGERED
                self.combo_step = 0
                self.combo_queued = False
                self.combo_window_open = False
                print("[v1] Attacking -> Staggered (被打断)")

            case (AttackStateV1.STAGGERED, AttackEventV1.STAGGER_FINISHED):
                self.state = AttackStateV1.IDLE
                print("[v1] Staggered -> Idle")

            case (AttackStateV1.STAGGERED, AttackEventV1.HIT_TAKEN):
                print("[v1] Staggered 再次被击中 -> 忽略状态切换(可选:刷新硬直计时)")

            case (AttackStateV1.STAGGERED, _):
                print("[v1] Staggered 忽略常规输入")

            case (AttackStateV1.DEAD, _):
                print("[v1] Dead 忽略一切事件(终态)")

            case _:
                print(f"[v1] {self.state.name} 收到 {event.name},忽略(无合法转移)")

    def _start_attack(self) -> None:
        self.state = AttackStateV1.ATTACKING
        self.combo_step = 1
        self.combo_window_open = True
        self.combo_queued = False
        print("[v1] -> Attacking (第1段攻击,锁定移动)")

    def open_combo_window(self) -> None:
        """模拟动画播放到指定进度时打开连击窗口(通常由动画事件驱动)。"""
        self.combo_window_open = True


# ============================================================
# v2: Transition Table —— 显式区分"忽略"与"排队"
# ============================================================

AttackStateV2 = AttackStateV1
AttackEventV2 = AttackEventV1


@dataclass
class TransitionV2:
    target: AttackStateV2
    action: Callable[["FighterV2"], None]


def _noop_v2(_: "FighterV2") -> None:
    return None


def _queue_combo(fighter: "FighterV2") -> None:
    fighter.combo_queued = True
    print("[v2] attack_pressed 在连击窗口内 -> 排队(自环转移,状态不变)")


def _finish_attack(fighter: "FighterV2") -> None:
    if fighter.combo_queued and fighter.combo_step < MAX_COMBO_STEP:
        fighter.combo_queued = False
        fighter.combo_step += 1
        fighter.combo_window_open = False
        print(f"[v2] 续第{fighter.combo_step}段连击")
    else:
        fighter.combo_step = 0
        fighter.combo_queued = False
        fighter.combo_window_open = False


def _reset_combo_and_stagger(fighter: "FighterV2") -> None:
    fighter.combo_step = 0
    fighter.combo_queued = False
    fighter.combo_window_open = False


def _start_attack_action(fighter: "FighterV2") -> None:
    fighter.combo_step = 1
    fighter.combo_window_open = True
    fighter.combo_queued = False


class FighterV2:
    """v2: 用一张显式转移表驱动,"忽略"用表里无条目表示,"排队"用自环转移表示。"""

    #: attack_finished 在Attacking下的目标状态需要根据combo_queued决定,
    #: 因此用一个特殊值None标记"运行时再决定",在handle_event里单独处理。
    TABLE: dict[tuple[AttackStateV2, AttackEventV2], TransitionV2] = {
        (AttackStateV2.IDLE, AttackEventV2.MOVE_PRESSED): TransitionV2(
            AttackStateV2.MOVING, _noop_v2
        ),
        (AttackStateV2.IDLE, AttackEventV2.ATTACK_PRESSED): TransitionV2(
            AttackStateV2.ATTACKING, _start_attack_action
        ),
        (AttackStateV2.IDLE, AttackEventV2.HIT_TAKEN): TransitionV2(
            AttackStateV2.STAGGERED, _noop_v2
        ),
        (AttackStateV2.MOVING, AttackEventV2.MOVE_RELEASED): TransitionV2(
            AttackStateV2.IDLE, _noop_v2
        ),
        (AttackStateV2.MOVING, AttackEventV2.ATTACK_PRESSED): TransitionV2(
            AttackStateV2.ATTACKING, _start_attack_action
        ),
        (AttackStateV2.MOVING, AttackEventV2.HIT_TAKEN): TransitionV2(
            AttackStateV2.STAGGERED, _noop_v2
        ),
        # 注意: (ATTACKING, ATTACK_PRESSED) 不放进这张静态表,因为它的行为
        # (排队 vs 忽略)取决于 combo_window_open 这个运行时数据,属于
        # "guard条件决定分支"的场景,和Lesson 1的landed分流是同一类模式,
        # 在 handle_event 里单独处理更清晰。
        (AttackStateV2.ATTACKING, AttackEventV2.HIT_TAKEN): TransitionV2(
            AttackStateV2.STAGGERED, _reset_combo_and_stagger
        ),
        (AttackStateV2.STAGGERED, AttackEventV2.STAGGER_FINISHED): TransitionV2(
            AttackStateV2.IDLE, _noop_v2
        ),
    }

    def __init__(self) -> None:
        self.state: AttackStateV2 = AttackStateV2.IDLE
        self.combo_step: int = 0
        self.combo_window_open: bool = False
        self.combo_queued: bool = False

    def handle_event(self, event: AttackEventV2) -> None:
        if event is AttackEventV2.HP_ZERO and self.state is not AttackStateV2.DEAD:
            self.state = AttackStateV2.DEAD
            print("[v2] * -> Dead (最高优先级)")
            return

        if self.state is AttackStateV2.DEAD:
            print("[v2] Dead 忽略一切事件")
            return

        if self.state is AttackStateV2.ATTACKING and event is AttackEventV2.ATTACK_PRESSED:
            if self.combo_window_open:
                _queue_combo(self)
            else:
                print("[v2] Attacking 窗口外收到 attack_pressed -> 忽略")
            return

        if self.state is AttackStateV2.ATTACKING and event in (
            AttackEventV2.MOVE_PRESSED,
            AttackEventV2.MOVE_RELEASED,
        ):
            print(f"[v2] Attacking 收到 {event.name} -> 忽略(位置锁定)")
            return

        if self.state is AttackStateV2.ATTACKING and event is AttackEventV2.ATTACK_FINISHED:
            _finish_attack(self)
            if self.combo_step > 0:
                print(f"[v2] Attacking 续第{self.combo_step}段连击 (仍是Attacking)")
            else:
                self.state = AttackStateV2.IDLE
                print("[v2] Attacking -> Idle")
            return

        if self.state is AttackStateV2.STAGGERED and event not in (
            AttackEventV2.STAGGER_FINISHED,
            AttackEventV2.HIT_TAKEN,
        ):
            print(f"[v2] Staggered 忽略 {event.name}")
            return

        transition = self.TABLE.get((self.state, event))
        if transition is None:
            print(f"[v2] {self.state.name} 收到 {event.name},查表未命中,忽略")
            return

        transition.action(self)
        old_state = self.state
        self.state = transition.target
        print(f"[v2] {old_state.name} -> {self.state.name}")


# ============================================================
# v3: State Pattern —— 连击窗口/排队标志下放到Attacking状态内部
# ============================================================


class AttackStateV3Base(Protocol):
    name: str

    def on_enter(self, fighter: "FighterV3") -> None: ...

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional["AttackStateV3Base"]: ...


@dataclass
class IdleStateV3:
    name: str = "Idle"

    def on_enter(self, fighter: "FighterV3") -> None:
        print("[v3] 进入 Idle")

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional[AttackStateV3Base]:
        match event:
            case AttackEventV2.MOVE_PRESSED:
                return MovingStateV3()
            case AttackEventV2.ATTACK_PRESSED:
                return AttackingStateV3()
            case AttackEventV2.HIT_TAKEN:
                return StaggeredStateV3()
            case _:
                return None


@dataclass
class MovingStateV3:
    name: str = "Moving"

    def on_enter(self, fighter: "FighterV3") -> None:
        print("[v3] 进入 Moving")

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional[AttackStateV3Base]:
        match event:
            case AttackEventV2.MOVE_RELEASED:
                return IdleStateV3()
            case AttackEventV2.ATTACK_PRESSED:
                return AttackingStateV3()
            case AttackEventV2.HIT_TAKEN:
                return StaggeredStateV3()
            case _:
                return None


@dataclass
class AttackingStateV3:
    """连击窗口、连击段数、排队标志都是这个状态对象自己的内部数据,
    随着这个状态对象被销毁(离开Attacking)而自然消失,不会残留在
    Fighter对象的顶层字段里污染其他状态的逻辑。
    """

    name: str = "Attacking"
    combo_step: int = 1
    combo_window_open: bool = True
    queued: bool = False

    def on_enter(self, fighter: "FighterV3") -> None:
        print(f"[v3] 进入 Attacking (第{self.combo_step}段),锁定移动")

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional[AttackStateV3Base]:
        match event:
            case AttackEventV2.ATTACK_PRESSED if self.combo_window_open:
                self.queued = True
                print("[v3] 连击窗口内 -> 排队")
                return None
            case AttackEventV2.ATTACK_PRESSED:
                print("[v3] 连击窗口外 -> 忽略")
                return None
            case AttackEventV2.MOVE_PRESSED | AttackEventV2.MOVE_RELEASED:
                print("[v3] 位置锁定,忽略移动输入")
                return None
            case AttackEventV2.ATTACK_FINISHED:
                if self.queued and self.combo_step < MAX_COMBO_STEP:
                    print(f"[v3] 续第{self.combo_step + 1}段连击")
                    return AttackingStateV3(combo_step=self.combo_step + 1)
                return IdleStateV3()
            case AttackEventV2.HIT_TAKEN:
                return StaggeredStateV3()
            case _:
                return None


@dataclass
class StaggeredStateV3:
    name: str = "Staggered"

    def on_enter(self, fighter: "FighterV3") -> None:
        print("[v3] 进入 Staggered")

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional[AttackStateV3Base]:
        match event:
            case AttackEventV2.STAGGER_FINISHED:
                return IdleStateV3()
            case AttackEventV2.HIT_TAKEN:
                print("[v3] Staggered 内再次被击中,刷新硬直(不切状态)")
                return None
            case _:
                print(f"[v3] Staggered 忽略 {event.name}")
                return None


@dataclass
class DeadStateV3:
    name: str = "Dead"

    def on_enter(self, fighter: "FighterV3") -> None:
        print("[v3] 进入 Dead (终态)")

    def handle_event(
        self, fighter: "FighterV3", event: AttackEventV2
    ) -> Optional[AttackStateV3Base]:
        return None


class FighterV3:
    def __init__(self) -> None:
        self.current: AttackStateV3Base = IdleStateV3()
        self.current.on_enter(self)

    def handle_event(self, event: AttackEventV2) -> None:
        if event is AttackEventV2.HP_ZERO and not isinstance(self.current, DeadStateV3):
            self.current = DeadStateV3()
            self.current.on_enter(self)
            print("[v3] * -> Dead (最高优先级打断)")
            return

        next_state = self.current.handle_event(self, event)
        if next_state is None:
            return
        old_name = self.current.name
        self.current = next_state
        self.current.on_enter(self)
        print(f"[v3] {old_name} -> {self.current.name}")


# ============================================================
# v4: 复用通用FSM框架 + 高优先级打断事件的批量注册
# ============================================================

@dataclass(frozen=True)
class GenericTransition:
    target: Enum
    guard: Optional[Callable[[Any], bool]] = None
    action: Optional[Callable[[Any], None]] = None


class GenericFSM:
    def __init__(
        self,
        initial: Enum,
        table: dict[tuple[Enum, Enum], list[GenericTransition]],
    ) -> None:
        self.state: Enum = initial
        self._table = table

    def handle_event(self, event: Enum, context: object) -> bool:
        candidates = self._table.get((self.state, event), [])
        for transition in candidates:
            if transition.guard is not None and not transition.guard(context):
                continue
            if transition.action is not None:
                transition.action(context)
            self.state = transition.target
            return True
        return False


@dataclass
class AttackContextV4:
    combo_step: int = 0
    combo_window_open: bool = False
    combo_queued: bool = False


def _guard_combo_window_open(ctx: AttackContextV4) -> bool:
    return ctx.combo_window_open


def _guard_combo_window_closed(ctx: AttackContextV4) -> bool:
    return not ctx.combo_window_open


def _queue_combo_v4(ctx: AttackContextV4) -> None:
    ctx.combo_queued = True


def _start_attack_v4(ctx: AttackContextV4) -> None:
    ctx.combo_step = 1
    ctx.combo_window_open = True
    ctx.combo_queued = False


def _reset_combo_v4(ctx: AttackContextV4) -> None:
    ctx.combo_step = 0
    ctx.combo_window_open = False
    ctx.combo_queued = False


def build_attack_fsm_v4() -> GenericFSM:
    S = AttackStateV2
    E = AttackEventV2
    table: dict[tuple[Enum, Enum], list[GenericTransition]] = {
        (S.IDLE, E.MOVE_PRESSED): [GenericTransition(S.MOVING)],
        (S.IDLE, E.ATTACK_PRESSED): [
            GenericTransition(S.ATTACKING, action=_start_attack_v4)
        ],
        (S.MOVING, E.MOVE_RELEASED): [GenericTransition(S.IDLE)],
        (S.MOVING, E.ATTACK_PRESSED): [
            GenericTransition(S.ATTACKING, action=_start_attack_v4)
        ],
        (S.ATTACKING, E.ATTACK_PRESSED): [
            GenericTransition(
                S.ATTACKING, guard=_guard_combo_window_open, action=_queue_combo_v4
            ),
            # 窗口关闭时落到下面这条"什么都不做"的自环,等价于忽略。
            GenericTransition(S.ATTACKING, guard=_guard_combo_window_closed),
        ],
        (S.ATTACKING, E.ATTACK_FINISHED): [
            GenericTransition(S.IDLE, action=_reset_combo_v4),
        ],
        (S.STAGGERED, E.STAGGER_FINISHED): [GenericTransition(S.IDLE)],
    }

    # 高优先级打断事件: hit_taken 可以从Idle/Moving/Attacking打断到Staggered,
    # hp_zero 可以从任意非Dead状态打断到Dead。用循环批量注册,
    # 避免像手写v1那样为每个状态重复写一遍相同的转移——
    # 这正是Step 6提到的"批量生成表项"技巧,也是分层FSM价值的预告。
    interruptible_by_hit = (S.IDLE, S.MOVING, S.ATTACKING)
    for state in interruptible_by_hit:
        table[(state, E.HIT_TAKEN)] = [
            GenericTransition(S.STAGGERED, action=_reset_combo_v4)
        ]

    all_alive_states = (S.IDLE, S.MOVING, S.ATTACKING, S.STAGGERED)
    for state in all_alive_states:
        table[(state, E.HP_ZERO)] = [GenericTransition(S.DEAD, action=_reset_combo_v4)]

    return GenericFSM(initial=S.IDLE, table=table)


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("\n===== v1: Enum + match-case =====")
    fighter = FighterV1()
    fighter.handle_event(AttackEventV1.ATTACK_PRESSED)
    fighter.open_combo_window()
    fighter.handle_event(AttackEventV1.ATTACK_PRESSED)  # 排队
    fighter.handle_event(AttackEventV1.MOVE_PRESSED)  # 忽略
    fighter.handle_event(AttackEventV1.ATTACK_FINISHED)  # 续第2段
    fighter.handle_event(AttackEventV1.HIT_TAKEN)  # 打断
    fighter.handle_event(AttackEventV1.STAGGER_FINISHED)
    print(f"最终状态: {fighter.state.name}")


def _demo_v2() -> None:
    print("\n===== v2: Transition Table =====")
    fighter = FighterV2()
    fighter.handle_event(AttackEventV2.ATTACK_PRESSED)
    fighter.combo_window_open = True
    fighter.handle_event(AttackEventV2.ATTACK_PRESSED)  # 排队
    fighter.handle_event(AttackEventV2.ATTACK_FINISHED)  # 续第2段
    fighter.handle_event(AttackEventV2.HP_ZERO)  # 最高优先级
    print(f"最终状态: {fighter.state.name}")


def _demo_v3() -> None:
    print("\n===== v3: State Pattern =====")
    fighter = FighterV3()
    fighter.handle_event(AttackEventV2.ATTACK_PRESSED)
    assert isinstance(fighter.current, AttackingStateV3)
    fighter.current.combo_window_open = True
    fighter.handle_event(AttackEventV2.ATTACK_PRESSED)  # 排队
    fighter.handle_event(AttackEventV2.ATTACK_FINISHED)  # 续第2段
    fighter.handle_event(AttackEventV2.HIT_TAKEN)  # 打断
    fighter.handle_event(AttackEventV2.STAGGER_FINISHED)
    print(f"最终状态: {fighter.current.name}")


def _demo_v4() -> None:
    print("\n===== v4: 通用FSM框架 + 批量注册高优先级打断 =====")
    fsm = build_attack_fsm_v4()
    ctx = AttackContextV4()
    events = [
        AttackEventV2.ATTACK_PRESSED,
        AttackEventV2.ATTACK_PRESSED,  # 窗口尚未手动打开时会走"关闭"分支,忽略
        AttackEventV2.HIT_TAKEN,  # 打断
    ]
    for event in events:
        fsm.handle_event(event, ctx)
        print(f"[v4] 事件={event.name} 当前状态={fsm.state.name}")
    print(f"最终状态: {fsm.state.name}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
