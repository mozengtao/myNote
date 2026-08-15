#!/usr/bin/env python3
"""Traffic light: same event, different meaning per state."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


class Event(Enum):
    TICK = auto()


class Action(Enum):
    LIGHT_RED = auto()
    LIGHT_GREEN = auto()
    LIGHT_YELLOW = auto()


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, Event) -> (NextState, ActionList)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, Event], Tuple[State, List[Action]]] = {
    (State.RED, Event.TICK): (State.GREEN, [Action.LIGHT_GREEN]),
    (State.GREEN, Event.TICK): (State.YELLOW, [Action.LIGHT_YELLOW]),
    (State.YELLOW, Event.TICK): (State.RED, [Action.LIGHT_RED]),
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

def worker_lamp(action: Action) -> None:
    if action is Action.LIGHT_RED:
        print("[EXEC] lamp -> RED")
    elif action is Action.LIGHT_GREEN:
        print("[EXEC] lamp -> GREEN")
    elif action is Action.LIGHT_YELLOW:
        print("[EXEC] lamp -> YELLOW")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] {action.name}")
        worker_lamp(action)


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
    state = State.RED
    print(f"[INIT] state={state.name}")
    print()
    for event in (Event.TICK, Event.TICK, Event.TICK, Event.TICK):
        state = apply(state, event)
        print()


if __name__ == "__main__":
    main()
