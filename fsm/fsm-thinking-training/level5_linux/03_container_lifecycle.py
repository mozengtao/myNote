"""
Lesson 03: 容器生命周期 (Level 5 - Linux系统)

教学用简化模型,模拟OCI Runtime Specification + runc/libcontainer视角的
容器runtime状态机(creating/created/running/paused/stopped),
以及"delete"作为终结FSM实例而非转移到新状态的特殊语义。
不是runc的真实实现,只提炼这一课的核心教学点。

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
#   把"已销毁"也放进同一个Enum当成一个终态处理,这是"教学清晰度"和
#   "实现简洁性"之间的妥协——概念上delete不是真正的状态,但实现上
#   给它一个显式的DESTROYED值,比用None表示"已销毁"更不容易和
#   "FSM尚未初始化"混淆。
# ---------------------------------------------------------------------------


class ContainerState(Enum):
    CREATING = auto()
    CREATED = auto()
    RUNNING = auto()
    PAUSED = auto()
    STOPPED = auto()
    DESTROYED = auto()  # 概念上不是真正的状态,是"FSM已终结"的实现标记


class ContainerEvent(Enum):
    CREATE = auto()
    CREATE_DONE = auto()
    CREATE_FAILED = auto()
    START = auto()
    PAUSE = auto()
    RESUME = auto()
    PROCESS_EXITED = auto()
    HOOK_FAILED = auto()
    DELETE = auto()


class ContainerV1:
    """最直观的写法,delete被硬编码成走向DESTROYED这个终态。"""

    def __init__(self, container_id: str) -> None:
        self.container_id = container_id
        self.state = ContainerState.CREATING

    def handle(self, event: ContainerEvent) -> None:
        match self.state, event:
            case ContainerState.CREATING, ContainerEvent.CREATE_DONE:
                self._go(ContainerState.CREATED)
            case ContainerState.CREATING, (ContainerEvent.CREATE_FAILED | ContainerEvent.HOOK_FAILED):
                self._go(ContainerState.DESTROYED)
            case ContainerState.CREATED, ContainerEvent.START:
                self._go(ContainerState.RUNNING)
            case ContainerState.CREATED, (ContainerEvent.HOOK_FAILED | ContainerEvent.DELETE):
                self._go(ContainerState.DESTROYED)
            case ContainerState.RUNNING, ContainerEvent.PAUSE:
                self._go(ContainerState.PAUSED)
            case ContainerState.PAUSED, ContainerEvent.RESUME:
                self._go(ContainerState.RUNNING)
            case ContainerState.RUNNING, ContainerEvent.PROCESS_EXITED:
                self._go(ContainerState.STOPPED)
            case ContainerState.STOPPED, ContainerEvent.DELETE:
                self._go(ContainerState.DESTROYED)
            case ContainerState.DESTROYED, _:
                print(f"[{self.container_id}] 已销毁,拒绝任何后续操作 {event.name}")
            case _:
                print(f"[{self.container_id}] {self.state.name}状态下不允许 {event.name}")

    def _go(self, next_state: ContainerState) -> None:
        print(f"[{self.container_id}] {self.state.name} -> {next_state.name}")
        self.state = next_state


# ---------------------------------------------------------------------------
# v2: Transition Table版(数据驱动) —— 非法转移改为报错而非忽略
# ---------------------------------------------------------------------------
# 设计取舍:
#   OCI规范要求"对created/running/paused/stopped执行不合法的操作必须报错,
#   且不能对容器产生任何效果"。所以这一版查表失败时抛异常,
#   而不是像第1课交通灯那样"忽略并保持原状态"——
#   两种系统对"非法输入"的正确处理方式是不同的,取决于系统本身的设计目标。
# ---------------------------------------------------------------------------


class InvalidTransitionError(RuntimeError):
    """对应OCI规范里"操作非法时必须报错"的约束。"""


@dataclass(frozen=True)
class TransitionV2:
    next_state: ContainerState
    action_desc: str = ""


TABLE_V2: dict[tuple[ContainerState, ContainerEvent], TransitionV2] = {
    (ContainerState.CREATING, ContainerEvent.CREATE_DONE): TransitionV2(
        ContainerState.CREATED, "写入state.json,记录init进程pid"
    ),
    (ContainerState.CREATED, ContainerEvent.START): TransitionV2(
        ContainerState.RUNNING, "执行startContainer hooks,exec用户程序"
    ),
    (ContainerState.RUNNING, ContainerEvent.PAUSE): TransitionV2(
        ContainerState.PAUSED, "写cgroup freezer.state=FROZEN"
    ),
    (ContainerState.PAUSED, ContainerEvent.RESUME): TransitionV2(
        ContainerState.RUNNING, "写cgroup freezer.state=THAWED"
    ),
    (ContainerState.RUNNING, ContainerEvent.PROCESS_EXITED): TransitionV2(
        ContainerState.STOPPED, "记录退出码,回收部分资源"
    ),
}

# DELETE / CREATE_FAILED / HOOK_FAILED 会终结FSM,单独处理而不塞进TABLE_V2,
# 因为它们的"next_state"不是一个真正的状态,是"实例终结"这一特殊结果。
_DELETABLE_STATES = {ContainerState.CREATED, ContainerState.STOPPED}
_DESTROY_ON_FAILURE_STATES = {ContainerState.CREATING, ContainerState.CREATED}


class ContainerV2:
    def __init__(self, container_id: str) -> None:
        self.container_id = container_id
        self.state = ContainerState.CREATING
        self.destroyed = False

    def handle(self, event: ContainerEvent) -> None:
        if self.destroyed:
            raise InvalidTransitionError(f"{self.container_id} 已被销毁,不能再处理任何事件")

        if event is ContainerEvent.DELETE:
            if self.state not in _DELETABLE_STATES:
                raise InvalidTransitionError(
                    f"不能在{self.state.name}状态下delete,必须先转为stopped"
                )
            print(f"[{self.container_id}] {self.state.name} -> (销毁,清理cgroup/namespace)")
            self.destroyed = True
            return

        if event in (ContainerEvent.CREATE_FAILED, ContainerEvent.HOOK_FAILED):
            if self.state not in _DESTROY_ON_FAILURE_STATES:
                raise InvalidTransitionError(
                    f"{event.name}只应发生在creating/created阶段,当前是{self.state.name}"
                )
            print(f"[{self.container_id}] {self.state.name} -> (销毁,{event.name}触发清理)")
            self.destroyed = True
            return

        transition = TABLE_V2.get((self.state, event))
        if transition is None:
            raise InvalidTransitionError(
                f"{self.container_id}: 不允许在{self.state.name}状态下执行{event.name}"
            )
        if transition.action_desc:
            print(f"[{self.container_id}] 动作: {transition.action_desc}")
        print(f"[{self.container_id}] {self.state.name} -> {transition.next_state.name}")
        self.state = transition.next_state


# ---------------------------------------------------------------------------
# v3: State Pattern版 —— 对照libcontainer真实结构:以"目标状态"为判断输入
# ---------------------------------------------------------------------------
# 设计取舍:
#   贴近runc源码state_linux.go的实际设计:每个状态类实现
#   transition(next_state) -> bool,判断"我能不能变成这个新状态",
#   而不是"收到某个事件该怎么办"。runtime场景下调用者往往是
#   "我要让容器变成running"这种目标明确的调用,这种角度更直白。
# ---------------------------------------------------------------------------


class ContainerStateHandler:
    status: ContainerState

    def can_transition_to(self, target: ContainerState) -> bool:
        raise NotImplementedError

    def destroy(self, ctx: "ContainerV3") -> None:
        raise InvalidTransitionError(f"{self.status.name}状态下不允许destroy")


class CreatedStateHandler(ContainerStateHandler):
    status = ContainerState.CREATED

    def can_transition_to(self, target: ContainerState) -> bool:
        return target in (ContainerState.RUNNING,)

    def destroy(self, ctx: "ContainerV3") -> None:
        print(f"[{ctx.container_id}] created -> (销毁,尚未start,直接清理)")
        ctx.destroyed = True


class RunningStateHandler(ContainerStateHandler):
    status = ContainerState.RUNNING

    def can_transition_to(self, target: ContainerState) -> bool:
        return target in (ContainerState.PAUSED, ContainerState.STOPPED)


class PausedStateHandler(ContainerStateHandler):
    status = ContainerState.PAUSED

    def can_transition_to(self, target: ContainerState) -> bool:
        # 对照libcontainer: paused只能回到running,不能直接跳到stopped
        # (必须先resume再走正常退出流程)。
        return target in (ContainerState.RUNNING,)

    def destroy(self, ctx: "ContainerV3") -> None:
        raise InvalidTransitionError("paused状态不能直接destroy,必须先resume")


class StoppedStateHandler(ContainerStateHandler):
    status = ContainerState.STOPPED

    def can_transition_to(self, target: ContainerState) -> bool:
        return False  # stopped是运行时状态机的终态,只能被delete,不能再transition

    def destroy(self, ctx: "ContainerV3") -> None:
        print(f"[{ctx.container_id}] stopped -> (销毁,清理cgroup/namespace残留)")
        ctx.destroyed = True


_STATE_HANDLERS: dict[ContainerState, type[ContainerStateHandler]] = {
    ContainerState.CREATED: CreatedStateHandler,
    ContainerState.RUNNING: RunningStateHandler,
    ContainerState.PAUSED: PausedStateHandler,
    ContainerState.STOPPED: StoppedStateHandler,
}


class ContainerV3:
    """构造函数直接跳过creating(视为瞬间完成),聚焦created起始的四态。"""

    def __init__(self, container_id: str) -> None:
        self.container_id = container_id
        self.destroyed = False
        self._handler: ContainerStateHandler = CreatedStateHandler()
        print(f"[{self.container_id}] creating -> created")

    @property
    def status(self) -> ContainerState:
        return self._handler.status

    def transition_to(self, target: ContainerState) -> None:
        if self.destroyed:
            raise InvalidTransitionError(f"{self.container_id} 已销毁")
        if not self._handler.can_transition_to(target):
            raise InvalidTransitionError(
                f"{self.container_id}: 不允许从{self.status.name}转移到{target.name}"
            )
        print(f"[{self.container_id}] {self.status.name} -> {target.name}")
        self._handler = _STATE_HANDLERS[target]()

    def delete(self) -> None:
        if self.destroyed:
            raise InvalidTransitionError(f"{self.container_id} 已销毁")
        self._handler.destroy(self)


# ---------------------------------------------------------------------------
# v4: 可复用FSM框架 —— 显式区分"转移到状态"与"终结实例"
# ---------------------------------------------------------------------------
# 设计取舍:
#   next_state为None时表示"这条规则会终结整台FSM",终结后的实例
#   会拒绝任何后续dispatch并给出明确提示,而不是第1、2课那样
#   静默地"忽略未定义事件"——因为"已销毁的对象还能响应操作"本身
#   就是一个需要在框架层面显式建模的新场景。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Rule:
    state: ContainerState
    event: ContainerEvent
    next_state: ContainerState | None  # None表示终结FSM,而非转移到某个状态
    action: Callable[["GenericFSM"], None] | None = None


@dataclass
class GenericFSM:
    name: str
    state: ContainerState
    rules: list[Rule] = field(default_factory=list)
    terminated: bool = False

    def add_rule(self, rule: Rule) -> None:
        self.rules.append(rule)

    def dispatch(self, event: ContainerEvent) -> None:
        if self.terminated:
            print(f"[{self.name}] FSM已终结,拒绝处理事件 {event.name}")
            return
        for rule in self.rules:
            if rule.state == self.state and rule.event == event:
                if rule.action is not None:
                    rule.action(self)
                if rule.next_state is None:
                    print(f"[{self.name}] {self.state.name} -> (终结)")
                    self.terminated = True
                else:
                    print(f"[{self.name}] {self.state.name} -> {rule.next_state.name}")
                    self.state = rule.next_state
                return
        print(f"[{self.name}] {self.state.name}状态下不允许事件 {event.name}(未定义转移)")


def build_container_fsm(container_id: str) -> GenericFSM:
    fsm = GenericFSM(name=container_id, state=ContainerState.CREATING)
    fsm.add_rule(Rule(ContainerState.CREATING, ContainerEvent.CREATE_DONE, ContainerState.CREATED))
    fsm.add_rule(Rule(ContainerState.CREATING, ContainerEvent.HOOK_FAILED, None))
    fsm.add_rule(Rule(ContainerState.CREATED, ContainerEvent.START, ContainerState.RUNNING))
    fsm.add_rule(Rule(ContainerState.CREATED, ContainerEvent.HOOK_FAILED, None))
    fsm.add_rule(Rule(ContainerState.CREATED, ContainerEvent.DELETE, None))
    fsm.add_rule(Rule(ContainerState.RUNNING, ContainerEvent.PAUSE, ContainerState.PAUSED))
    fsm.add_rule(Rule(ContainerState.PAUSED, ContainerEvent.RESUME, ContainerState.RUNNING))
    fsm.add_rule(Rule(ContainerState.RUNNING, ContainerEvent.PROCESS_EXITED, ContainerState.STOPPED))
    fsm.add_rule(Rule(ContainerState.STOPPED, ContainerEvent.DELETE, None))
    return fsm


if __name__ == "__main__":
    print("=== v1: 简单版(Enum + match-case) ===")
    c1 = ContainerV1("nginx-a1b2")
    for evt in (
        ContainerEvent.CREATE_DONE,
        ContainerEvent.START,
        ContainerEvent.PAUSE,
        ContainerEvent.RESUME,
        ContainerEvent.PROCESS_EXITED,
        ContainerEvent.DELETE,
        ContainerEvent.START,  # 已销毁,应被拒绝
    ):
        c1.handle(evt)

    print("\n=== v2: Transition Table版(非法操作报错而非忽略) ===")
    c2 = ContainerV2("redis-c3d4")
    c2.handle(ContainerEvent.CREATE_DONE)
    c2.handle(ContainerEvent.START)
    c2.handle(ContainerEvent.PROCESS_EXITED)
    c2.handle(ContainerEvent.DELETE)
    try:
        c2.handle(ContainerEvent.START)
    except InvalidTransitionError as exc:
        print(f"[预期内的错误] {exc}")

    print("\n=== v3: State Pattern版(以目标状态为判断输入,对照libcontainer) ===")
    c3 = ContainerV3("postgres-e5f6")
    c3.transition_to(ContainerState.RUNNING)
    c3.transition_to(ContainerState.PAUSED)
    try:
        c3.delete()
    except InvalidTransitionError as exc:
        print(f"[预期内的错误] {exc}")
    c3.transition_to(ContainerState.RUNNING)
    c3.transition_to(ContainerState.STOPPED)
    c3.delete()

    print("\n=== v4: 可复用FSM框架(显式区分'转移'与'终结') ===")
    c4 = build_container_fsm("worker-g7h8")
    c4.dispatch(ContainerEvent.CREATE_DONE)
    c4.dispatch(ContainerEvent.START)
    c4.dispatch(ContainerEvent.PAUSE)
    c4.dispatch(ContainerEvent.RESUME)
    c4.dispatch(ContainerEvent.PROCESS_EXITED)
    c4.dispatch(ContainerEvent.DELETE)
    c4.dispatch(ContainerEvent.START)  # 已终结,应被拒绝
