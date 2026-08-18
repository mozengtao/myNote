#!/usr/bin/env python3

from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Optional

QUEUE_CAP = 16
HOLD_BUDGET = 5
KEEPALIVE_BUDGET = 2
CONNECTRETRY_BUDGET = 3


class State(Enum):
    IDLE = auto()
    CONNECT = auto()
    OPENSENT = auto()
    OPENCONFIRM = auto()
    ESTABLISHED = auto()


class Event(Enum):
    START = auto()
    STOP = auto()
    TCP_OPEN = auto()
    TCP_FAIL = auto()
    OPEN_MSG = auto()
    KEEPALIVE_MSG = auto()
    HOLD_EXPIRED = auto()
    KEEPALIVE_EXPIRED = auto()
    CONNECTRETRY_EXPIRED = auto()


@dataclass
class Fsm:
    state: State = State.IDLE
    queue: deque = field(default_factory=deque)
    hold_ticks: int = 0
    keepalive_ticks: int = 0
    connectretry_ticks: int = 0


ActionFn = Callable[["Fsm"], None]


@dataclass
class Cell:
    action: Optional[ActionFn]
    next: State


def enqueue(fsm: Fsm, event: Event) -> None:
    """enqueue one event; drop if the teaching queue is full"""
    if len(fsm.queue) >= QUEUE_CAP:
        print(f"queue full, drop {event.name}")
        return
    fsm.queue.append(event)


def dequeue(fsm: Fsm) -> Optional[Event]:
    """dequeue one event or None"""
    if not fsm.queue:
        return None
    return fsm.queue.popleft()


def clear_timers(fsm: Fsm) -> None:
    """stop all three teaching timers"""
    fsm.hold_ticks = 0
    fsm.keepalive_ticks = 0
    fsm.connectretry_ticks = 0


def arm_connectretry(fsm: Fsm) -> None:
    """arm ConnectRetry after entering CONNECT"""
    fsm.connectretry_ticks = CONNECTRETRY_BUDGET
    print(f"timer: arm ConnectRetry={fsm.connectretry_ticks}")


def arm_session_timers(fsm: Fsm) -> None:
    """arm Hold and Keepalive after the OPEN handshake is in progress"""
    fsm.hold_ticks = HOLD_BUDGET
    fsm.keepalive_ticks = KEEPALIVE_BUDGET
    fsm.connectretry_ticks = 0
    print(f"timer: arm Hold={fsm.hold_ticks} Keepalive={fsm.keepalive_ticks}")


def action_start_connect(fsm: Fsm) -> None:
    """application asks the session to start"""
    print("action: open TCP")
    arm_connectretry(fsm)


def action_send_open(fsm: Fsm) -> None:
    """TCP is up; send OPEN"""
    print("action: send OPEN")
    fsm.connectretry_ticks = 0
    fsm.hold_ticks = HOLD_BUDGET
    print(f"timer: arm Hold={fsm.hold_ticks}")


def action_send_keepalive_openconfirm(fsm: Fsm) -> None:
    """peer OPEN accepted; send KEEPALIVE and wait for theirs"""
    print("action: send KEEPALIVE")
    arm_session_timers(fsm)


def action_established(fsm: Fsm) -> None:
    """session is established"""
    print("action: session ESTABLISHED")
    arm_session_timers(fsm)


def action_reset_hold(fsm: Fsm) -> None:
    """peer keepalive resets Hold"""
    fsm.hold_ticks = HOLD_BUDGET
    print(f"action: recv KEEPALIVE, reset Hold={fsm.hold_ticks}")


def action_send_keepalive(fsm: Fsm) -> None:
    """local Keepalive timer expired; send and re-arm"""
    print("action: send KEEPALIVE")
    fsm.keepalive_ticks = KEEPALIVE_BUDGET
    print(f"timer: re-arm Keepalive={fsm.keepalive_ticks}")


def action_drop(fsm: Fsm) -> None:
    """drop the session and return to IDLE"""
    print("action: drop session")
    clear_timers(fsm)


def action_retry_connect(fsm: Fsm) -> None:
    """ConnectRetry expired; try TCP again"""
    print("action: retry open TCP")
    arm_connectretry(fsm)


table: dict[tuple[State, Event], Cell] = {
    (State.IDLE, Event.START): Cell(action_start_connect, State.CONNECT),
    (State.CONNECT, Event.TCP_OPEN): Cell(action_send_open, State.OPENSENT),
    (State.CONNECT, Event.TCP_FAIL): Cell(action_drop, State.IDLE),
    (State.CONNECT, Event.STOP): Cell(action_drop, State.IDLE),
    (State.CONNECT, Event.CONNECTRETRY_EXPIRED): Cell(
        action_retry_connect, State.CONNECT
    ),
    (State.OPENSENT, Event.OPEN_MSG): Cell(
        action_send_keepalive_openconfirm, State.OPENCONFIRM
    ),
    (State.OPENSENT, Event.TCP_FAIL): Cell(action_drop, State.IDLE),
    (State.OPENSENT, Event.STOP): Cell(action_drop, State.IDLE),
    (State.OPENSENT, Event.HOLD_EXPIRED): Cell(action_drop, State.IDLE),
    (State.OPENCONFIRM, Event.KEEPALIVE_MSG): Cell(
        action_established, State.ESTABLISHED
    ),
    (State.OPENCONFIRM, Event.HOLD_EXPIRED): Cell(action_drop, State.IDLE),
    (State.OPENCONFIRM, Event.STOP): Cell(action_drop, State.IDLE),
    (State.OPENCONFIRM, Event.TCP_FAIL): Cell(action_drop, State.IDLE),
    (State.ESTABLISHED, Event.KEEPALIVE_MSG): Cell(
        action_reset_hold, State.ESTABLISHED
    ),
    (State.ESTABLISHED, Event.KEEPALIVE_EXPIRED): Cell(
        action_send_keepalive, State.ESTABLISHED
    ),
    (State.ESTABLISHED, Event.HOLD_EXPIRED): Cell(action_drop, State.IDLE),
    (State.ESTABLISHED, Event.STOP): Cell(action_drop, State.IDLE),
    (State.ESTABLISHED, Event.TCP_FAIL): Cell(action_drop, State.IDLE),
}


def fsm_init(fsm: Fsm) -> None:
    """put the teaching BGP FSM into IDLE with an empty queue"""
    fsm.state = State.IDLE
    fsm.queue.clear()
    clear_timers(fsm)


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event by table lookup"""
    prev = fsm.state
    cell = table.get((fsm.state, event))
    if cell is None:
        print(f"ignore: {prev.name} + {event.name}")
        return
    if cell.action is not None:
        cell.action(fsm)
    fsm.state = cell.next
    print(f"{prev.name} --{event.name}--> {fsm.state.name}")


def fsm_post(fsm: Fsm, event: Event) -> None:
    """post an event onto the queue; it is not dispatched yet"""
    print(f"post: {event.name}")
    enqueue(fsm, event)


def fsm_run(fsm: Fsm) -> None:
    """drain the queue: while dequeue, dispatch"""
    while True:
        event = dequeue(fsm)
        if event is None:
            break
        fsm_dispatch(fsm, event)


def fsm_tick(fsm: Fsm) -> None:
    """decrement armed timers; expiry becomes a queued event"""
    print(
        f"tick: hold={fsm.hold_ticks} keepalive={fsm.keepalive_ticks} "
        f"connectretry={fsm.connectretry_ticks}"
    )
    if fsm.connectretry_ticks > 0:
        fsm.connectretry_ticks -= 1
        if fsm.connectretry_ticks == 0:
            print("timer: ConnectRetry expired")
            enqueue(fsm, Event.CONNECTRETRY_EXPIRED)
    if fsm.hold_ticks > 0:
        fsm.hold_ticks -= 1
        if fsm.hold_ticks == 0:
            print("timer: Hold expired")
            enqueue(fsm, Event.HOLD_EXPIRED)
    if fsm.keepalive_ticks > 0:
        fsm.keepalive_ticks -= 1
        if fsm.keepalive_ticks == 0:
            print("timer: Keepalive expired")
            enqueue(fsm, Event.KEEPALIVE_EXPIRED)


def main() -> None:
    bgp = Fsm()
    print("=== bgp teaching FSM ===")

    print("-- session up, keepalive fire, then hold expire --")
    fsm_init(bgp)
    print(f"init: {bgp.state.name}")
    fsm_post(bgp, Event.START)
    fsm_run(bgp)
    fsm_post(bgp, Event.TCP_OPEN)
    fsm_run(bgp)
    fsm_post(bgp, Event.OPEN_MSG)
    fsm_run(bgp)
    fsm_post(bgp, Event.KEEPALIVE_MSG)
    fsm_run(bgp)
    for _ in range(HOLD_BUDGET):
        fsm_tick(bgp)
        fsm_run(bgp)

    print("-- connectretry when TCP never comes up --")
    fsm_init(bgp)
    print(f"init: {bgp.state.name}")
    fsm_post(bgp, Event.START)
    fsm_run(bgp)
    for _ in range(CONNECTRETRY_BUDGET):
        fsm_tick(bgp)
        fsm_run(bgp)


if __name__ == "__main__":
    main()
