#!/usr/bin/env python3
"""Automatic door: PERSON/TIMEOUT carry data; motor_run(dir, duty) moves the door."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    CLOSED = auto()
    OPENING = auto()
    OPEN = auto()
    CLOSING = auto()


class EventKind(Enum):
    PERSON = auto()
    TIMEOUT = auto()


class ActionKind(Enum):
    MOTOR_RUN = auto()
    LATCH_SET = auto()


class Dir(Enum):
    OPEN = auto()
    CLOSE = auto()


@dataclass(frozen=True)
class Event:
    kind: EventKind
    sensor_id: int = 0
    elapsed_ms: int = 0


@dataclass(frozen=True)
class Action:
    kind: ActionKind
    direction: Dir = Dir.OPEN
    duty: int = 0
    open: int = 0


@dataclass
class Ctx:
    state: State


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, EventKind) -> (NextState, ActionKind list)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, EventKind], Tuple[State, List[ActionKind]]] = {
    (State.CLOSED, EventKind.PERSON): (State.OPENING, [ActionKind.MOTOR_RUN]),
    (State.OPENING, EventKind.TIMEOUT): (State.OPEN, [ActionKind.LATCH_SET]),
    (State.OPEN, EventKind.PERSON): (State.OPEN, []),
    (State.OPEN, EventKind.TIMEOUT): (State.CLOSING, [ActionKind.MOTOR_RUN]),
    (State.CLOSING, EventKind.PERSON): (State.OPENING, [ActionKind.MOTOR_RUN]),
    (State.CLOSING, EventKind.TIMEOUT): (State.CLOSED, [ActionKind.LATCH_SET]),
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup + bind args; zero side effects
# ---------------------------------------------------------------------------

def fsm_decide(ctx: Ctx, event: Event) -> Decision:
    hit = TABLE.get((ctx.state, event.kind))
    if hit is None:
        return Decision(False, ctx.state, ())
    next_state, kinds = hit
    actions = []
    for kind in kinds:
        if kind is ActionKind.MOTOR_RUN:
            if next_state is State.OPENING:
                actions.append(Action(kind=kind, direction=Dir.OPEN, duty=80))
            else:
                actions.append(Action(kind=kind, direction=Dir.CLOSE, duty=80))
        else:
            opened = 1 if next_state is State.OPEN else 0
            actions.append(Action(kind=kind, open=opened))
    return Decision(True, next_state, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- parameterized functions; no FSM state
# ---------------------------------------------------------------------------

def motor_run(direction: Dir, duty: int) -> None:
    print(f"[EXEC] motor_run({direction.name}, {duty})")


def latch_set(open_flag: int) -> None:
    print(f"[EXEC] latch_set({open_flag})")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        if action.kind is ActionKind.MOTOR_RUN:
            print(
                f"[DISPATCH] MOTOR_RUN(dir={action.direction.name}, "
                f"duty={action.duty})"
            )
            motor_run(action.direction, action.duty)
        else:
            print(f"[DISPATCH] LATCH_SET(open={action.open})")
            latch_set(action.open)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit ctx, then dispatch
# ---------------------------------------------------------------------------

def format_event(event: Event) -> str:
    if event.kind is EventKind.PERSON:
        return f"PERSON(sensor_id={event.sensor_id})"
    return f"TIMEOUT(elapsed_ms={event.elapsed_ms})"


def apply(ctx: Ctx, event: Event) -> None:
    print(f"[EVENT] {format_event(event)} | state={ctx.state.name}")
    decision = fsm_decide(ctx, event)
    if not decision.accepted:
        print(
            f"[DECISION] REJECT {ctx.state.name} + {event.kind.name}  "
            "(state unchanged)"
        )
        return
    print(
        f"[DECISION] {ctx.state.name} + {event.kind.name} -> "
        f"{decision.next_state.name}"
    )
    ctx.state = decision.next_state
    dispatch(decision.actions)


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

def main() -> None:
    ctx = Ctx(state=State.CLOSED)
    print(f"[INIT] state={ctx.state.name}")
    print()
    events = (
        Event(EventKind.TIMEOUT, elapsed_ms=500),
        Event(EventKind.PERSON, sensor_id=1),
        Event(EventKind.TIMEOUT, elapsed_ms=2000),
        Event(EventKind.PERSON, sensor_id=1),
        Event(EventKind.TIMEOUT, elapsed_ms=5000),
        Event(EventKind.PERSON, sensor_id=2),
        Event(EventKind.TIMEOUT, elapsed_ms=2000),
        Event(EventKind.TIMEOUT, elapsed_ms=5000),
        Event(EventKind.TIMEOUT, elapsed_ms=2000),
    )
    for event in events:
        apply(ctx, event)
        print()


if __name__ == "__main__":
    main()
