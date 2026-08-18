#!/usr/bin/env python3

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Optional

RETRY_BUDGET = 3


class State(Enum):
    IDLE = auto()
    SCANNING = auto()
    AUTHENTICATING = auto()
    ASSOCIATING = auto()
    CONNECTED = auto()
    DISCONNECTED = auto()


class Event(Enum):
    START = auto()
    SCAN_DONE = auto()
    AUTH_OK = auto()
    AUTH_FAIL = auto()
    ASSOC_OK = auto()
    ASSOC_FAIL = auto()
    DATA_OK = auto()
    LOST = auto()
    TIMEOUT = auto()
    RETRY = auto()


@dataclass
class Context:
    retries_left: int = 0
    timer_armed: int = 0


@dataclass
class Fsm:
    state: State = State.IDLE
    ctx: Context = field(default_factory=Context)


ActionFn = Callable[[Fsm], None]


def print_transition(prev: State, event: Event, fsm: Fsm) -> None:
    """print one transition and the retry budget"""
    print(
        f"{prev.name} --{event.name}--> {fsm.state.name} "
        f"(retries left={fsm.ctx.retries_left})"
    )


def action_start_scan(fsm: Fsm) -> None:
    """begin a session: arm retry budget and start scan"""
    fsm.ctx.retries_left = RETRY_BUDGET
    fsm.ctx.timer_armed = 1
    print(f"start scan (retries left={fsm.ctx.retries_left})")


def action_begin_auth(fsm: Fsm) -> None:
    """scan found a candidate BSS; begin 802.11 authentication"""
    fsm.ctx.timer_armed = 1
    print("scan done, begin auth")


def action_begin_assoc(fsm: Fsm) -> None:
    """authentication succeeded; begin association"""
    fsm.ctx.timer_armed = 1
    print("auth ok, begin assoc")


def action_connected(fsm: Fsm) -> None:
    """association succeeded; session is up (4-way handshake omitted)"""
    fsm.ctx.retries_left = RETRY_BUDGET
    fsm.ctx.timer_armed = 0
    print("assoc ok, connected")


def action_data_ok(fsm: Fsm) -> None:
    """data path is alive"""
    print("data ok")


def action_session_fail(fsm: Fsm) -> None:
    """auth, assoc, scan wait, or link loss: go to the recovery state"""
    if fsm.ctx.retries_left > 0:
        fsm.ctx.retries_left -= 1
    fsm.ctx.timer_armed = 1
    print(f"session fail, retries left={fsm.ctx.retries_left}")


def action_retry_scan(fsm: Fsm) -> None:
    """still have budget: return to SCANNING"""
    fsm.ctx.timer_armed = 1
    print(f"retry scan, retries left={fsm.ctx.retries_left}")


def action_give_up(fsm: Fsm) -> None:
    """budget exhausted: stop recovering"""
    fsm.ctx.timer_armed = 0
    print("give up, back to idle")


TABLE: dict[tuple[State, Event], tuple[Optional[ActionFn], State]] = {
    (State.IDLE, Event.START): (action_start_scan, State.SCANNING),
    (State.SCANNING, Event.SCAN_DONE): (
        action_begin_auth,
        State.AUTHENTICATING,
    ),
    (State.SCANNING, Event.TIMEOUT): (
        action_session_fail,
        State.DISCONNECTED,
    ),
    (State.AUTHENTICATING, Event.AUTH_OK): (
        action_begin_assoc,
        State.ASSOCIATING,
    ),
    (State.AUTHENTICATING, Event.AUTH_FAIL): (
        action_session_fail,
        State.DISCONNECTED,
    ),
    (State.AUTHENTICATING, Event.TIMEOUT): (
        action_session_fail,
        State.DISCONNECTED,
    ),
    (State.ASSOCIATING, Event.ASSOC_OK): (action_connected, State.CONNECTED),
    (State.ASSOCIATING, Event.ASSOC_FAIL): (
        action_session_fail,
        State.DISCONNECTED,
    ),
    (State.ASSOCIATING, Event.TIMEOUT): (
        action_session_fail,
        State.DISCONNECTED,
    ),
    (State.CONNECTED, Event.DATA_OK): (action_data_ok, State.CONNECTED),
    (State.CONNECTED, Event.LOST): (action_session_fail, State.DISCONNECTED),
}


def fsm_init(fsm: Fsm) -> None:
    """put the wifi FSM into IDLE with a clean context"""
    fsm.state = State.IDLE
    fsm.ctx = Context()


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event into the wifi session FSM"""
    prev = fsm.state

    if fsm.state is State.DISCONNECTED and event in {
        Event.RETRY,
        Event.TIMEOUT,
    }:
        if fsm.ctx.retries_left > 0:
            action_retry_scan(fsm)
            fsm.state = State.SCANNING
        else:
            action_give_up(fsm)
            fsm.state = State.IDLE
        print_transition(prev, event, fsm)
        return

    cell = TABLE.get((fsm.state, event))
    if cell is None:
        return

    action, nxt = cell
    if action is not None:
        action(fsm)
    fsm.state = nxt
    print_transition(prev, event, fsm)


def main() -> None:
    wifi = Fsm()
    print("=== wifi session demo ===")

    print("-- 1. happy path association --")
    fsm_init(wifi)
    print(f"init: {wifi.state.name}")
    fsm_dispatch(wifi, Event.START)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_OK)
    fsm_dispatch(wifi, Event.DATA_OK)

    print("-- 2. auth fail, then retry, then success --")
    fsm_init(wifi)
    fsm_dispatch(wifi, Event.START)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_FAIL)
    fsm_dispatch(wifi, Event.RETRY)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_OK)

    print("-- 3. assoc fail until budget exhausted --")
    fsm_init(wifi)
    fsm_dispatch(wifi, Event.START)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_FAIL)
    fsm_dispatch(wifi, Event.RETRY)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_FAIL)
    fsm_dispatch(wifi, Event.RETRY)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_FAIL)
    fsm_dispatch(wifi, Event.RETRY)

    print("-- 4. connected then lost, timeout recovers to scan --")
    fsm_init(wifi)
    fsm_dispatch(wifi, Event.START)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_OK)
    fsm_dispatch(wifi, Event.LOST)
    fsm_dispatch(wifi, Event.TIMEOUT)
    fsm_dispatch(wifi, Event.SCAN_DONE)
    fsm_dispatch(wifi, Event.AUTH_OK)
    fsm_dispatch(wifi, Event.ASSOC_OK)

    print("-- 5. scan timeout then give up after budget --")
    fsm_init(wifi)
    fsm_dispatch(wifi, Event.START)
    fsm_dispatch(wifi, Event.TIMEOUT)
    fsm_dispatch(wifi, Event.RETRY)
    fsm_dispatch(wifi, Event.TIMEOUT)
    fsm_dispatch(wifi, Event.RETRY)
    fsm_dispatch(wifi, Event.TIMEOUT)
    fsm_dispatch(wifi, Event.RETRY)


if __name__ == "__main__":
    main()
