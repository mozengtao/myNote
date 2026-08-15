"""Lesson 02: 登录流程状态机 (Level 3 - Python工程场景).

配合 02_login_flow.md 阅读。这一课的重点是"扩展状态机"(状态携带
数据 + 守卫条件):失败次数是状态内部的数据,不是独立状态;是否
锁定账户,取决于这个数据是否达到阈值。

四版实现:
    v1: Enum + match-case
    v2: Transition Table,守卫条件放进处理函数内部
    v3: State Pattern,LOCKED 状态显式拒绝一切登录尝试
    v4: 通用FSM框架,支持"转移目标由数据动态决定"(带守卫条件的转移)
"""

from __future__ import annotations

from abc import ABC
from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import UTC, datetime, timedelta
from enum import Enum, auto
from typing import ClassVar


class LoginState(Enum):
    ANONYMOUS = auto()  # 未登录
    AUTHENTICATED = auto()  # 已登录
    LOCKED = auto()  # 已锁定


class LoginEvent(Enum):
    LOGIN_SUCCESS = auto()
    LOGIN_FAILURE = auto()
    LOGOUT = auto()
    LOCK_EXPIRE = auto()
    ADMIN_UNLOCK = auto()


class InvalidTransitionError(Exception):
    """安全类状态机的非法转移必须显式拒绝,且不泄露多余的判断细节。"""

    def __init__(self, state: LoginState, event: LoginEvent) -> None:
        self.state = state
        self.event = event
        super().__init__(
            f"非法转移: 当前状态={state.name}, 事件={event.name} 不允许发生"
        )


LOCK_THRESHOLD = 3
LOCK_DURATION = timedelta(minutes=15)


# ============================================================
# v1: 最简单版本 —— Enum + match-case
#
# 设计理由: 守卫条件("失败次数是否达到阈值")直接用一个 if 写在
# login_failure() 方法内部,直观但不方便审计"结构性转移规则"和
# "业务守卫条件"到底哪部分是硬编码的。
# ============================================================


@dataclass
class AccountV1:
    username: str
    state: LoginState = LoginState.ANONYMOUS
    failure_count: int = 0
    locked_until: datetime | None = None

    def login_success(self) -> None:
        match self.state:
            case LoginState.ANONYMOUS:
                self.state = LoginState.AUTHENTICATED
                self.failure_count = 0
            case _:
                raise InvalidTransitionError(self.state, LoginEvent.LOGIN_SUCCESS)

    def login_failure(self, now: datetime | None = None) -> None:
        match self.state:
            case LoginState.ANONYMOUS:
                self.failure_count += 1
                if self.failure_count >= LOCK_THRESHOLD:
                    self.state = LoginState.LOCKED
                    self.locked_until = (now or datetime.now(UTC)) + LOCK_DURATION
            case _:
                raise InvalidTransitionError(self.state, LoginEvent.LOGIN_FAILURE)

    def logout(self) -> None:
        match self.state:
            case LoginState.AUTHENTICATED:
                self.state = LoginState.ANONYMOUS
                self.failure_count = 0
            case _:
                raise InvalidTransitionError(self.state, LoginEvent.LOGOUT)

    def lock_expire(self) -> None:
        match self.state:
            case LoginState.LOCKED:
                self.state = LoginState.ANONYMOUS
                self.failure_count = 0
                self.locked_until = None
            case _:
                raise InvalidTransitionError(self.state, LoginEvent.LOCK_EXPIRE)

    def admin_unlock(self) -> None:
        match self.state:
            case LoginState.LOCKED:
                self.state = LoginState.ANONYMOUS
                self.failure_count = 0
                self.locked_until = None
            case _:
                raise InvalidTransitionError(self.state, LoginEvent.ADMIN_UNLOCK)


# ============================================================
# v2: Transition Table —— 结构性转移查表,守卫条件放进处理函数
#
# 相比v1的改进: (ANONYMOUS, LOGIN_FAILURE) 在表里始终只有一行,
# 不会因为 LOCK_THRESHOLD 改变而膨胀成多行。表负责回答"这个
# (状态,事件)组合是否存在合法路径",处理函数负责回答"根据当前
# 数据,这次具体走到哪一个后续状态"。两个问题分开,职责更清晰。
# ============================================================


@dataclass
class AccountV2:
    username: str
    state: LoginState = LoginState.ANONYMOUS
    failure_count: int = 0
    locked_until: datetime | None = None
    audit_log: list[str] = field(default_factory=list)


def _v2_login_success(acc: AccountV2, now: datetime) -> None:
    acc.state = LoginState.AUTHENTICATED
    acc.failure_count = 0
    acc.audit_log.append("登录成功")


def _v2_login_failure(acc: AccountV2, now: datetime) -> None:
    acc.failure_count += 1
    if acc.failure_count >= LOCK_THRESHOLD:
        acc.state = LoginState.LOCKED
        acc.locked_until = now + LOCK_DURATION
        acc.audit_log.append(f"连续失败{acc.failure_count}次,账户已锁定")
    else:
        acc.audit_log.append(f"登录失败,当前失败次数={acc.failure_count}")


def _v2_logout(acc: AccountV2, now: datetime) -> None:
    acc.state = LoginState.ANONYMOUS
    acc.failure_count = 0
    acc.audit_log.append("已登出")


def _v2_lock_expire(acc: AccountV2, now: datetime) -> None:
    acc.state = LoginState.ANONYMOUS
    acc.failure_count = 0
    acc.locked_until = None
    acc.audit_log.append("锁定到期,自动解锁")


def _v2_admin_unlock(acc: AccountV2, now: datetime) -> None:
    acc.state = LoginState.ANONYMOUS
    acc.failure_count = 0
    acc.locked_until = None
    acc.audit_log.append("管理员手动解锁")


V2_TRANSITIONS: dict[
    tuple[LoginState, LoginEvent], Callable[[AccountV2, datetime], None]
] = {
    (LoginState.ANONYMOUS, LoginEvent.LOGIN_SUCCESS): _v2_login_success,
    (LoginState.ANONYMOUS, LoginEvent.LOGIN_FAILURE): _v2_login_failure,
    (LoginState.AUTHENTICATED, LoginEvent.LOGOUT): _v2_logout,
    (LoginState.LOCKED, LoginEvent.LOCK_EXPIRE): _v2_lock_expire,
    (LoginState.LOCKED, LoginEvent.ADMIN_UNLOCK): _v2_admin_unlock,
}


def apply_event_v2(
    acc: AccountV2, event: LoginEvent, now: datetime | None = None
) -> None:
    handler = V2_TRANSITIONS.get((acc.state, event))
    if handler is None:
        raise InvalidTransitionError(acc.state, event)
    handler(acc, now or datetime.now(UTC))


# ============================================================
# v3: State Pattern
#
# 适用场景: LockedHandler 可以把"拒绝登录但仍记录攻击线索,且不
# 泄露密码是否正确"这类安全考量完整封装在一个类里,安全审计时
# 只需要读这一个类,不必在扁平的转移表里到处找"锁定期间到底做
# 了什么"。
# ============================================================


@dataclass
class AccountV3:
    username: str
    state: LoginState = LoginState.ANONYMOUS
    failure_count: int = 0
    locked_until: datetime | None = None
    security_events: list[str] = field(default_factory=list)


class LoginStateHandler(ABC):
    state: ClassVar[LoginState]

    def login_success(self, acc: AccountV3, now: datetime) -> None:
        raise InvalidTransitionError(self.state, LoginEvent.LOGIN_SUCCESS)

    def login_failure(self, acc: AccountV3, now: datetime) -> None:
        raise InvalidTransitionError(self.state, LoginEvent.LOGIN_FAILURE)

    def logout(self, acc: AccountV3, now: datetime) -> None:
        raise InvalidTransitionError(self.state, LoginEvent.LOGOUT)

    def lock_expire(self, acc: AccountV3, now: datetime) -> None:
        raise InvalidTransitionError(self.state, LoginEvent.LOCK_EXPIRE)

    def admin_unlock(self, acc: AccountV3, now: datetime) -> None:
        raise InvalidTransitionError(self.state, LoginEvent.ADMIN_UNLOCK)


class _AnonymousHandler(LoginStateHandler):
    state = LoginState.ANONYMOUS

    def login_success(self, acc: AccountV3, now: datetime) -> None:
        acc.state = LoginState.AUTHENTICATED
        acc.failure_count = 0

    def login_failure(self, acc: AccountV3, now: datetime) -> None:
        acc.failure_count += 1
        if acc.failure_count >= LOCK_THRESHOLD:
            acc.state = LoginState.LOCKED
            acc.locked_until = now + LOCK_DURATION
            acc.security_events.append("触发锁定")


class _AuthenticatedHandler(LoginStateHandler):
    state = LoginState.AUTHENTICATED

    def logout(self, acc: AccountV3, now: datetime) -> None:
        acc.state = LoginState.ANONYMOUS
        acc.failure_count = 0


class _LockedHandler(LoginStateHandler):
    """安全重点: 锁定期间的任何登录尝试都必须拒绝,不透露密码正确性。"""

    state = LoginState.LOCKED

    def login_success(self, acc: AccountV3, now: datetime) -> None:
        acc.security_events.append("锁定期间出现一次正确密码的登录尝试(可疑)")
        raise InvalidTransitionError(self.state, LoginEvent.LOGIN_SUCCESS)

    def login_failure(self, acc: AccountV3, now: datetime) -> None:
        acc.security_events.append("锁定期间出现一次登录失败尝试")
        raise InvalidTransitionError(self.state, LoginEvent.LOGIN_FAILURE)

    def lock_expire(self, acc: AccountV3, now: datetime) -> None:
        acc.state = LoginState.ANONYMOUS
        acc.failure_count = 0
        acc.locked_until = None

    def admin_unlock(self, acc: AccountV3, now: datetime) -> None:
        acc.state = LoginState.ANONYMOUS
        acc.failure_count = 0
        acc.locked_until = None
        acc.security_events.append("管理员手动解锁")


_V3_HANDLERS: dict[LoginState, LoginStateHandler] = {
    LoginState.ANONYMOUS: _AnonymousHandler(),
    LoginState.AUTHENTICATED: _AuthenticatedHandler(),
    LoginState.LOCKED: _LockedHandler(),
}


def apply_event_v3(
    acc: AccountV3, event: LoginEvent, now: datetime | None = None
) -> None:
    handler = _V3_HANDLERS[acc.state]
    method: Callable[[AccountV3, datetime], None] = getattr(handler, event.name.lower())
    method(acc, now or datetime.now(UTC))


# ============================================================
# v4: 通用FSM框架 —— 支持"转移目标由数据动态决定"
#
# 和Lesson 01的框架相比,这一版把 Transition.target 从"固定值"
# 放宽成"固定值或者一个根据 context 计算目标状态的函数",这样
# 才能表达"LOGIN_FAILURE 到底跳到 ANONYMOUS 还是 LOCKED,取决于
# failure_count 是否达到阈值"这种带守卫条件的转移,而不需要在表
# 的key里为每个计数值单独开一行。
# ============================================================


@dataclass(frozen=True)
class Transition[S: Enum, E: Enum]:
    source: S
    event: E
    target: S | Callable[[dict[str, object]], S]
    action: Callable[[dict[str, object]], None] | None = None


class GenericFSM[S: Enum, E: Enum]:
    def __init__(self, initial: S, transitions: list[Transition[S, E]]) -> None:
        self._state = initial
        self._table: dict[tuple[S, E], Transition[S, E]] = {
            (t.source, t.event): t for t in transitions
        }
        self.history: list[str] = []

    @property
    def state(self) -> S:
        return self._state

    def fire(self, event: E, context: dict[str, object]) -> S:
        key = (self._state, event)
        transition = self._table.get(key)
        if transition is None:
            raise InvalidTransitionError(self._state, event)  # type: ignore[arg-type]
        if transition.action is not None:
            transition.action(context)
        target = (
            transition.target(context)
            if callable(transition.target)
            else transition.target
        )
        self.history.append(f"{self._state.name} --{event.name}--> {target.name}")
        self._state = target
        return self._state


def build_login_fsm() -> tuple[GenericFSM[LoginState, LoginEvent], dict[str, object]]:
    context: dict[str, object] = {"failure_count": 0, "locked_until": None}

    def on_login_success(ctx: dict[str, object]) -> None:
        ctx["failure_count"] = 0

    def on_login_failure(ctx: dict[str, object]) -> None:
        ctx["failure_count"] = int(ctx["failure_count"]) + 1  # type: ignore[arg-type]
        if int(ctx["failure_count"]) >= LOCK_THRESHOLD:  # type: ignore[arg-type]
            ctx["locked_until"] = datetime.now(UTC) + LOCK_DURATION

    def login_failure_target(ctx: dict[str, object]) -> LoginState:
        return (
            LoginState.LOCKED
            if int(ctx["failure_count"]) >= LOCK_THRESHOLD  # type: ignore[arg-type]
            else LoginState.ANONYMOUS
        )

    def on_unlock(ctx: dict[str, object]) -> None:
        ctx["failure_count"] = 0
        ctx["locked_until"] = None

    transitions = [
        Transition(
            LoginState.ANONYMOUS,
            LoginEvent.LOGIN_SUCCESS,
            LoginState.AUTHENTICATED,
            action=on_login_success,
        ),
        Transition(
            LoginState.ANONYMOUS,
            LoginEvent.LOGIN_FAILURE,
            login_failure_target,
            action=on_login_failure,
        ),
        Transition(
            LoginState.AUTHENTICATED,
            LoginEvent.LOGOUT,
            LoginState.ANONYMOUS,
            action=on_unlock,
        ),
        Transition(
            LoginState.LOCKED,
            LoginEvent.LOCK_EXPIRE,
            LoginState.ANONYMOUS,
            action=on_unlock,
        ),
        Transition(
            LoginState.LOCKED,
            LoginEvent.ADMIN_UNLOCK,
            LoginState.ANONYMOUS,
            action=on_unlock,
        ),
    ]
    return GenericFSM(LoginState.ANONYMOUS, transitions), context


# ============================================================
# 演示
# ============================================================


def _demo_v1() -> None:
    print("--- v1: Enum + match-case ---")
    acc = AccountV1(username="alice")
    acc.login_failure()
    acc.login_failure()
    print(f"两次失败后状态: {acc.state.name}, 失败次数: {acc.failure_count}")
    acc.login_failure()
    print(f"第三次失败后状态: {acc.state.name}(应为LOCKED)")

    try:
        acc.login_success()
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")


def _demo_v2() -> None:
    print("\n--- v2: Transition Table ---")
    acc = AccountV2(username="bob")
    now = datetime.now(UTC)
    apply_event_v2(acc, LoginEvent.LOGIN_FAILURE, now)
    apply_event_v2(acc, LoginEvent.LOGIN_FAILURE, now)
    apply_event_v2(acc, LoginEvent.LOGIN_FAILURE, now)
    print(f"状态: {acc.state.name}, 审计日志: {acc.audit_log}")

    try:
        apply_event_v2(acc, LoginEvent.LOGIN_SUCCESS, now)
    except InvalidTransitionError as exc:
        print(f"锁定期间登录被拒绝(即使密码正确): {exc}")

    apply_event_v2(acc, LoginEvent.ADMIN_UNLOCK, now)
    print(f"管理员解锁后状态: {acc.state.name}, 失败次数: {acc.failure_count}")


def _demo_v3() -> None:
    print("\n--- v3: State Pattern ---")
    acc = AccountV3(username="carol")
    now = datetime.now(UTC)
    apply_event_v3(acc, LoginEvent.LOGIN_FAILURE, now)
    apply_event_v3(acc, LoginEvent.LOGIN_FAILURE, now)
    apply_event_v3(acc, LoginEvent.LOGIN_FAILURE, now)
    print(f"状态: {acc.state.name}")

    try:
        apply_event_v3(acc, LoginEvent.LOGIN_FAILURE, now)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")
    print(f"安全事件记录: {acc.security_events}")


def _demo_v4() -> None:
    print("\n--- v4: 通用FSM框架(带守卫条件的转移) ---")
    fsm, context = build_login_fsm()
    fsm.fire(LoginEvent.LOGIN_FAILURE, context)
    fsm.fire(LoginEvent.LOGIN_FAILURE, context)
    print(f"两次失败后: {fsm.state.name}, failure_count={context['failure_count']}")
    fsm.fire(LoginEvent.LOGIN_FAILURE, context)
    print(f"第三次失败后: {fsm.state.name}(应为LOCKED)")

    try:
        fsm.fire(LoginEvent.LOGIN_SUCCESS, context)
    except InvalidTransitionError as exc:
        print(f"预期的非法转移被拒绝: {exc}")

    fsm.fire(LoginEvent.ADMIN_UNLOCK, context)
    print(f"解锁后: {fsm.state.name}, failure_count={context['failure_count']}")
    print("转移历史:")
    for line in fsm.history:
        print(f"  {line}")


if __name__ == "__main__":
    _demo_v1()
    _demo_v2()
    _demo_v3()
    _demo_v4()
