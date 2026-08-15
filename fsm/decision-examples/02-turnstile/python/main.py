#!/usr/bin/env python3
"""Turnstile: same event means the opposite thing in the other state."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    LOCKED = auto()
    UNLOCKED = auto()


class Event(Enum):
    COIN = auto()
    PUSH = auto()


class Action(Enum):
    UNLOCK = auto()
    LOCK = auto()


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, Event) -> (NextState, ActionList)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, Event], Tuple[State, List[Action]]] = {
    (State.LOCKED, Event.COIN): (State.UNLOCKED, [Action.UNLOCK]),
    (State.UNLOCKED, Event.PUSH): (State.LOCKED, [Action.LOCK]),
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

def worker_mechanism(action: Action) -> None:
    if action is Action.UNLOCK:
        print("[EXEC] mechanism -> UNLOCK")
    elif action is Action.LOCK:
        print("[EXEC] mechanism -> LOCK")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] {action.name}")
        worker_mechanism(action)


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
    state = State.LOCKED
    print(f"[INIT] state={state.name}")
    print()
    events = (
        Event.COIN,
        Event.PUSH,
        Event.PUSH,
        Event.COIN,
        Event.COIN,
        Event.PUSH,
    )
    for event in events:
        state = apply(state, event)
        print()


if __name__ == "__main__":
    main()
