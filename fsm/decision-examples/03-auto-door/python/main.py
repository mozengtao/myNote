#!/usr/bin/env python3
"""Automatic door: FSM decides motor commands; the motor actually moves."""

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


class Event(Enum):
    PERSON = auto()
    TIMEOUT = auto()


class Action(Enum):
    MOTOR_OPEN = auto()
    MOTOR_CLOSE = auto()
    LATCH_OPEN = auto()
    LATCH_CLOSED = auto()


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, Event) -> (NextState, ActionList)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, Event], Tuple[State, List[Action]]] = {
    (State.CLOSED, Event.PERSON): (State.OPENING, [Action.MOTOR_OPEN]),
    (State.OPENING, Event.TIMEOUT): (State.OPEN, [Action.LATCH_OPEN]),
    (State.OPEN, Event.PERSON): (State.OPEN, []),
    (State.OPEN, Event.TIMEOUT): (State.CLOSING, [Action.MOTOR_CLOSE]),
    (State.CLOSING, Event.PERSON): (State.OPENING, [Action.MOTOR_OPEN]),
    (State.CLOSING, Event.TIMEOUT): (State.CLOSED, [Action.LATCH_CLOSED]),
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup only, zero side effects
# ---------------------------------------------------------------------------

def fsm_decide(state: State, event: Event) -> Decision:
    hit = TABLE.get((state, event))
    if hit is None:
        return Decision(False, state, ())
    next_state, actions = hit
    return Decision(True, next_state, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- execution; workers do not know current state
# ---------------------------------------------------------------------------

def worker_motor(action: Action) -> None:
    if action is Action.MOTOR_OPEN:
        print("[EXEC] motor -> OPEN")
    elif action is Action.MOTOR_CLOSE:
        print("[EXEC] motor -> CLOSE")


def worker_latch(action: Action) -> None:
    if action is Action.LATCH_OPEN:
        print("[EXEC] latch -> OPEN")
    elif action is Action.LATCH_CLOSED:
        print("[EXEC] latch -> CLOSED")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] {action.name}")
        if action in (Action.MOTOR_OPEN, Action.MOTOR_CLOSE):
            worker_motor(action)
        else:
            worker_latch(action)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit next state, then dispatch
# ---------------------------------------------------------------------------

def apply(state: State, event: Event) -> State:
    print(f"[EVENT] {event.name} | state={state.name}")
    decision = fsm_decide(state, event)
    if not decision.accepted:
        print(
            f"[DECISION] REJECT {state.name} + {event.name}  "
            "(state unchanged)"
        )
        return state
    names = ", ".join(a.name for a in decision.actions)
    print(
        f"[DECISION] {state.name} + {event.name} -> "
        f"{decision.next_state.name}  actions=[{names}]"
    )
    dispatch(decision.actions)
    return decision.next_state


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

def main() -> None:
    state = State.CLOSED
    print(f"[INIT] state={state.name}")
    print()
    events = (
        Event.TIMEOUT,
        Event.PERSON,
        Event.TIMEOUT,
        Event.PERSON,
        Event.TIMEOUT,
        Event.PERSON,
        Event.TIMEOUT,
        Event.TIMEOUT,
        Event.TIMEOUT,
    )
    for event in events:
        state = apply(state, event)
        print()


if __name__ == "__main__":
    main()
