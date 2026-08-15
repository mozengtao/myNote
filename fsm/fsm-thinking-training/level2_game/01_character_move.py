"""Lesson 01: 人物移动 (Level 2 - 小游戏).

对应文档: 01_character_move.md

本文件包含同一个"人物移动状态机"问题的四种实现方式,从简单到通用:

    v1: Enum + match-case,逻辑手写在一个类里
    v2: Transition Table,数据驱动的转移表
    v3: State Pattern,每个状态是一个独立的类
    v4: 可复用的通用FSM框架片段(State/Event/Transition/FSM)

四版都能独立运行,文件末尾的 ``if __name__ == "__main__":`` 依次演示每一版。
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional, Protocol


# ============================================================
# v1: 最简单版本 —— Enum + match-case,逻辑写在一个类里
# ============================================================


class MoveStateV1(Enum):
    """角色移动状态(五个互斥且穷尽的姿态)。"""

    IDLE = auto()
    WALKING = auto()
    RUNNING = auto()
    JUMPING = auto()
    FALLING = auto()


class MoveEventV1(Enum):
    """驱动移动状态切换的事件。"""

    MOVE_PRESSED = auto()
    MOVE_RELEASED = auto()
    RUN_MODIFIER_ON = auto()
    RUN_MODIFIER_OFF = auto()
    JUMP_PRESSED = auto()
    APEX_REACHED = auto()
    GROUND_LOST = auto()
    LANDED = auto()


class CharacterV1:
    """v1: 所有转移逻辑手写在一个大的match语句里。

    优点: 直观,能立刻跑起来。
    缺点: 转移规则和代码逻辑混在一起,没有一份能脱离代码单独审查的
    "转移清单",状态/事件一多,这个方法会持续膨胀。
    """

    def __init__(self) -> None:
        self.state: MoveStateV1 = MoveStateV1.IDLE
        self.move_key_held: bool = False
        self.run_key_held: bool = False

    def handle_event(self, event: MoveEventV1) -> None:
        match (self.state, event):
            case (MoveStateV1.IDLE, MoveEventV1.MOVE_PRESSED):
                self.move_key_held = True
                self.state = MoveStateV1.WALKING
                print("[v1] Idle -> Walking (播放走路动画)")

            case (MoveStateV1.IDLE, MoveEventV1.JUMP_PRESSED):
                self.state = MoveStateV1.JUMPING
                print("[v1] Idle -> Jumping (施加向上速度)")

            case (MoveStateV1.WALKING, MoveEventV1.MOVE_RELEASED):
                self.move_key_held = False
                self.state = MoveStateV1.IDLE
                print("[v1] Walking -> Idle")

            case (MoveStateV1.WALKING, MoveEventV1.RUN_MODIFIER_ON):
                self.run_key_held = True
                self.state = MoveStateV1.RUNNING
                print("[v1] Walking -> Running")

            case (MoveStateV1.WALKING, MoveEventV1.JUMP_PRESSED):
                self.state = MoveStateV1.JUMPING
                print("[v1] Walking -> Jumping (保留水平速度)")

            case (MoveStateV1.WALKING, MoveEventV1.GROUND_LOST):
                self.state = MoveStateV1.FALLING
                print("[v1] Walking -> Falling (走出平台边缘)")

            case (MoveStateV1.RUNNING, MoveEventV1.RUN_MODIFIER_OFF):
                self.run_key_held = False
                self.state = MoveStateV1.WALKING
                print("[v1] Running -> Walking")

            case (MoveStateV1.RUNNING, MoveEventV1.MOVE_RELEASED):
                self.move_key_held = False
                self.run_key_held = False
                self.state = MoveStateV1.IDLE
                print("[v1] Running -> Idle")

            case (MoveStateV1.RUNNING, MoveEventV1.JUMP_PRESSED):
                self.state = MoveStateV1.JUMPING
                print("[v1] Running -> Jumping (带水平冲量)")

            case (MoveStateV1.RUNNING, MoveEventV1.GROUND_LOST):
                self.state = MoveStateV1.FALLING
                print("[v1] Running -> Falling (冲出平台边缘)")

            case (MoveStateV1.JUMPING, MoveEventV1.APEX_REACHED):
                self.state = MoveStateV1.FALLING
                print("[v1] Jumping -> Falling (到达抛体顶点)")

            case (MoveStateV1.JUMPING, MoveEventV1.JUMP_PRESSED):
                print("[v1] Jumping 收到 jump_pressed,忽略(不支持二段跳)")

            case (MoveStateV1.FALLING, MoveEventV1.LANDED):
                if self.move_key_held and self.run_key_held:
                    self.state = MoveStateV1.RUNNING
                elif self.move_key_held:
                    self.state = MoveStateV1.WALKING
                else:
                    self.state = MoveStateV1.IDLE
                print(f"[v1] Falling -> {self.state.name} (落地,按落地瞬间输入分流)")

            case _:
                print(f"[v1] {self.state.name} 收到 {event.name},忽略(无合法转移)")


# ============================================================
# v2: Transition Table —— 数据驱动的转移表
# ============================================================
#
# 相比v1的改进: 转移规则第一次变成了"数据"而不是"代码逻辑"。
# 新增一个状态/事件只需要往表里加一行,不需要再改分支逻辑本身;
# 也可以在测试里直接遍历这张表,断言每个 (state, event) 组合的行为
# 是否符合Step 4文档里的转移表。


MoveStateV2 = MoveStateV1
MoveEventV2 = MoveEventV1


@dataclass
class TransitionV2:
    """一条转移规则: 目标状态 + 转移时执行的动作。"""

    target: MoveStateV2
    action: Callable[["CharacterV2"], None]


def _noop(_: "CharacterV2") -> None:
    return None


def _set_move_held(character: "CharacterV2") -> None:
    character.move_key_held = True


def _clear_move_held(character: "CharacterV2") -> None:
    character.move_key_held = False


def _set_run_held(character: "CharacterV2") -> None:
    character.run_key_held = True


def _clear_run_held(character: "CharacterV2") -> None:
    character.run_key_held = False


class CharacterV2:
    """v2: 用一张显式的转移表驱动状态切换。"""

    #: (当前状态, 事件) -> 转移规则。落地(LANDED)的分流逻辑因为依赖
    #: 按键这一"守卫数据",单独在 handle_event 里处理,不放进这张表。
    TABLE: dict[tuple[MoveStateV2, MoveEventV2], TransitionV2] = {
        (MoveStateV2.IDLE, MoveEventV2.MOVE_PRESSED): TransitionV2(
            MoveStateV2.WALKING, _set_move_held
        ),
        (MoveStateV2.IDLE, MoveEventV2.JUMP_PRESSED): TransitionV2(
            MoveStateV2.JUMPING, _noop
        ),
        (MoveStateV2.WALKING, MoveEventV2.MOVE_RELEASED): TransitionV2(
            MoveStateV2.IDLE, _clear_move_held
        ),
        (MoveStateV2.WALKING, MoveEventV2.RUN_MODIFIER_ON): TransitionV2(
            MoveStateV2.RUNNING, _set_run_held
        ),
        (MoveStateV2.WALKING, MoveEventV2.JUMP_PRESSED): TransitionV2(
            MoveStateV2.JUMPING, _noop
        ),
        (MoveStateV2.WALKING, MoveEventV2.GROUND_LOST): TransitionV2(
            MoveStateV2.FALLING, _noop
        ),
        (MoveStateV2.RUNNING, MoveEventV2.RUN_MODIFIER_OFF): TransitionV2(
            MoveStateV2.WALKING, _clear_run_held
        ),
        (MoveStateV2.RUNNING, MoveEventV2.MOVE_RELEASED): TransitionV2(
            MoveStateV2.IDLE, lambda c: (_clear_move_held(c), _clear_run_held(c))
        ),
        (MoveStateV2.RUNNING, MoveEventV2.JUMP_PRESSED): TransitionV2(
            MoveStateV2.JUMPING, _noop
        ),
        (MoveStateV2.RUNNING, MoveEventV2.GROUND_LOST): TransitionV2(
            MoveStateV2.FALLING, _noop
        ),
        (MoveStateV2.JUMPING, MoveEventV2.APEX_REACHED): TransitionV2(
            MoveStateV2.FALLING, _noop
        ),
    }

    def __init__(self) -> None:
        self.state: MoveStateV2 = MoveStateV2.IDLE
        self.move_key_held: bool = False
        self.run_key_held: bool = False

    def handle_event(self, event: MoveEventV2) -> None:
        if self.state is MoveStateV2.FALLING and event is MoveEventV2.LANDED:
            self._land()
            return

        transition = self.TABLE.get((self.state, event))
        if transition is None:
            print(f"[v2] {self.state.name} 收到 {event.name},查表未命中,忽略")
            return

        transition.action(self)
        old_state = self.state
        self.state = transition.target
        print(f"[v2] {old_state.name} -> {self.state.name}")

    def _land(self) -> None:
        if self.move_key_held and self.run_key_held:
            self.state = MoveStateV2.RUNNING
        elif self.move_key_held:
            self.state = MoveStateV2.WALKING
        else:
            self.state = MoveStateV2.IDLE
        print(f"[v2] Falling -> {self.state.name} (落地,按落地瞬间输入分流)")


# ============================================================
# v3: State Pattern —— 每个状态是一个独立的类
# ============================================================
#
# 适用场景: 每个状态的 enter/exit 副作用(播放动画、锁定输入、开关
# 碰撞体积)本身就很复杂,继续塞进一张扁平表格的"action"列会失去
# 可读性。State Pattern把这些副作用下放到各自的状态类里,是绝大多数
# 游戏引擎(Unity Animator、Unreal AnimGraph)里角色状态机的实际做法。


class MoveStateV3(Protocol):
    """状态对象的统一接口。"""

    name: str

    def on_enter(self, character: "CharacterV3") -> None: ...

    def on_exit(self, character: "CharacterV3") -> None: ...

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        """返回下一个状态对象;返回None表示忽略该事件,停留在当前状态。"""
        ...


@dataclass
class IdleStateV3:
    name: str = "Idle"

    def on_enter(self, character: "CharacterV3") -> None:
        print("[v3] 进入 Idle,播放站立动画")

    def on_exit(self, character: "CharacterV3") -> None:
        pass

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        match event:
            case MoveEventV2.MOVE_PRESSED:
                character.move_key_held = True
                return WalkingStateV3()
            case MoveEventV2.JUMP_PRESSED:
                return JumpingStateV3()
            case _:
                return None


@dataclass
class WalkingStateV3:
    name: str = "Walking"

    def on_enter(self, character: "CharacterV3") -> None:
        print("[v3] 进入 Walking,播放走路动画")

    def on_exit(self, character: "CharacterV3") -> None:
        pass

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        match event:
            case MoveEventV2.MOVE_RELEASED:
                character.move_key_held = False
                return IdleStateV3()
            case MoveEventV2.RUN_MODIFIER_ON:
                character.run_key_held = True
                return RunningStateV3()
            case MoveEventV2.JUMP_PRESSED:
                return JumpingStateV3()
            case MoveEventV2.GROUND_LOST:
                return FallingStateV3()
            case _:
                return None


@dataclass
class RunningStateV3:
    name: str = "Running"

    def on_enter(self, character: "CharacterV3") -> None:
        print("[v3] 进入 Running,播放奔跑动画")

    def on_exit(self, character: "CharacterV3") -> None:
        pass

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        match event:
            case MoveEventV2.RUN_MODIFIER_OFF:
                character.run_key_held = False
                return WalkingStateV3()
            case MoveEventV2.MOVE_RELEASED:
                character.move_key_held = False
                character.run_key_held = False
                return IdleStateV3()
            case MoveEventV2.JUMP_PRESSED:
                return JumpingStateV3()
            case MoveEventV2.GROUND_LOST:
                return FallingStateV3()
            case _:
                return None


@dataclass
class JumpingStateV3:
    name: str = "Jumping"

    def on_enter(self, character: "CharacterV3") -> None:
        print("[v3] 进入 Jumping,施加向上初始速度")

    def on_exit(self, character: "CharacterV3") -> None:
        pass

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        match event:
            case MoveEventV2.APEX_REACHED:
                return FallingStateV3()
            case MoveEventV2.JUMP_PRESSED:
                print("[v3] Jumping 忽略 jump_pressed(不支持二段跳)")
                return None
            case _:
                return None


@dataclass
class FallingStateV3:
    name: str = "Falling"

    def on_enter(self, character: "CharacterV3") -> None:
        print("[v3] 进入 Falling,播放下落动画")

    def on_exit(self, character: "CharacterV3") -> None:
        pass

    def handle_event(
        self, character: "CharacterV3", event: MoveEventV2
    ) -> Optional["MoveStateV3"]:
        match event:
            case MoveEventV2.LANDED:
                if character.move_key_held and character.run_key_held:
                    return RunningStateV3()
                if character.move_key_held:
                    return WalkingStateV3()
                return IdleStateV3()
            case _:
                return None


class CharacterV3:
    """v3: 持有当前状态对象,把事件转发给它处理。"""

    def __init__(self) -> None:
        self.move_key_held: bool = False
        self.run_key_held: bool = False
        self.current: MoveStateV3 = IdleStateV3()
        self.current.on_enter(self)

    def handle_event(self, event: MoveEventV2) -> None:
        next_state = self.current.handle_event(self, event)
        if next_state is None:
            return
        self.current.on_exit(self)
        old_name = self.current.name
        self.current = next_state
        self.current.on_enter(self)
        print(f"[v3] {old_name} -> {self.current.name}")


# ============================================================
# v4: 可复用的通用FSM框架片段
# ============================================================
#
# 抽出与"移动"这个具体业务无关的概念: State / Event / Transition / FSM。
# 之后的案例(攻击、NPC、Boss)可以直接复用这套框架,只需要提供各自的
# 状态清单、事件清单和转移表,不需要重新发明"查表 -> 检查guard ->
# 执行action -> 切换state"这套引擎本身。
#
# 什么时候值得引入框架而不是每次手写? 当你发现自己在第二个、第三个
# 案例里又重新写了一遍这套调度逻辑时,说明它已经和具体业务无关,
# 值得往上提一层。反之,只有一个孤立的小状态机时,框架是过度设计。


@dataclass(frozen=True)
class GenericTransition:
    """通用转移规则。

    guard: 可选的守卫函数,返回False时即使 (state, event) 匹配也不会转移
           (对应Step 4里"落地时依据按键分流"这种需要额外数据的场景)。
    action: 转移发生时执行的副作用。
    """

    target: Enum
    guard: Optional[Callable[[object], bool]] = None
    action: Optional[Callable[[object], None]] = None


class GenericFSM:
    """极简的通用有限状态机引擎。"""

    def __init__(
        self,
        initial: Enum,
        table: dict[tuple[Enum, Enum], list[GenericTransition]],
    ) -> None:
        self.state: Enum = initial
        self._table = table

    def handle_event(self, event: Enum, context: object) -> bool:
        """处理一个事件,返回是否发生了状态转移。"""
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
class MoveContextV4:
    """v4演示用的上下文数据(相当于v1~v3里CharacterV*的字段)。"""

    move_key_held: bool = False
    run_key_held: bool = False


def _guard_land_run(ctx: MoveContextV4) -> bool:
    return ctx.move_key_held and ctx.run_key_held


def _guard_land_walk(ctx: MoveContextV4) -> bool:
    return ctx.move_key_held and not ctx.run_key_held


def _guard_land_idle(ctx: MoveContextV4) -> bool:
    return not ctx.move_key_held


def build_move_fsm_v4() -> GenericFSM:
    """用通用框架搭建和v1~v3等价的人物移动状态机。"""

    S = MoveStateV2
    E = MoveEventV2
    table: dict[tuple[Enum, Enum], list[GenericTransition]] = {
        (S.IDLE, E.MOVE_PRESSED): [
            GenericTransition(S.WALKING, action=_set_move_held)
        ],
        (S.IDLE, E.JUMP_PRESSED): [GenericTransition(S.JUMPING)],
        (S.WALKING, E.MOVE_RELEASED): [
            GenericTransition(S.IDLE, action=_clear_move_held)
        ],
        (S.WALKING, E.RUN_MODIFIER_ON): [
            GenericTransition(S.RUNNING, action=_set_run_held)
        ],
        (S.WALKING, E.JUMP_PRESSED): [GenericTransition(S.JUMPING)],
        (S.WALKING, E.GROUND_LOST): [GenericTransition(S.FALLING)],
        (S.RUNNING, E.RUN_MODIFIER_OFF): [
            GenericTransition(S.WALKING, action=_clear_run_held)
        ],
        (S.RUNNING, E.MOVE_RELEASED): [
            GenericTransition(
                S.IDLE,
                action=lambda c: (_clear_move_held(c), _clear_run_held(c)),
            )
        ],
        (S.RUNNING, E.JUMP_PRESSED): [GenericTransition(S.JUMPING)],
        (S.RUNNING, E.GROUND_LOST): [GenericTransition(S.FALLING)],
        (S.JUMPING, E.APEX_REACHED): [GenericTransition(S.FALLING)],
        # LANDED 用三条按guard顺序尝试的候选转移,实现Step 4里的分流。
        (S.FALLING, E.LANDED): [
            GenericTransition(S.RUNNING, guard=_guard_land_run),
            GenericTransition(S.WALKING, guard=_guard_land_walk),
            GenericTransition(S.IDLE, guard=_guard_land_idle),
        ],
    }
    return GenericFSM(initial=S.IDLE, table=table)


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("\n===== v1: Enum + match-case =====")
    character = CharacterV1()
    for event in (
        MoveEventV1.MOVE_PRESSED,
        MoveEventV1.RUN_MODIFIER_ON,
        MoveEventV1.JUMP_PRESSED,
        MoveEventV1.JUMP_PRESSED,  # 应被忽略(不支持二段跳)
        MoveEventV1.APEX_REACHED,
        MoveEventV1.LANDED,
    ):
        character.handle_event(event)
    print(f"最终状态: {character.state.name}")


def _demo_v2() -> None:
    print("\n===== v2: Transition Table =====")
    character = CharacterV2()
    for event in (
        MoveEventV2.MOVE_PRESSED,
        MoveEventV2.GROUND_LOST,
        MoveEventV2.LANDED,
    ):
        character.handle_event(event)
    print(f"最终状态: {character.state.name}")


def _demo_v3() -> None:
    print("\n===== v3: State Pattern =====")
    character = CharacterV3()
    for event in (
        MoveEventV2.MOVE_PRESSED,
        MoveEventV2.RUN_MODIFIER_ON,
        MoveEventV2.GROUND_LOST,
        MoveEventV2.LANDED,
    ):
        character.handle_event(event)
    print(f"最终状态: {character.current.name}")


def _demo_v4() -> None:
    print("\n===== v4: 通用FSM框架 =====")
    fsm = build_move_fsm_v4()
    ctx = MoveContextV4()
    for event in (
        MoveEventV2.MOVE_PRESSED,
        MoveEventV2.RUN_MODIFIER_ON,
        MoveEventV2.JUMP_PRESSED,
        MoveEventV2.APEX_REACHED,
        MoveEventV2.LANDED,
    ):
        moved = fsm.handle_event(event, ctx)
        print(f"[v4] 事件={event.name} 转移={moved} 当前状态={fsm.state.name}")
    print(f"最终状态: {fsm.state.name}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
