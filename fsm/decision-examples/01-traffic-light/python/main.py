#!/usr/bin/env python3
"""Traffic light: TICK carries time; lamp_set(color) is chosen by Decision."""

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


class EventKind(Enum):
    TICK = auto()


class ActionKind(Enum):
    SET_LAMP = auto()


class Color(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


@dataclass(frozen=True)
class Event:
    kind: EventKind
    now_ms: int = 0


@dataclass(frozen=True)
class Action:
    kind: ActionKind
    color: Color


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
    (State.RED, EventKind.TICK): (State.GREEN, [ActionKind.SET_LAMP]),
    (State.GREEN, EventKind.TICK): (State.YELLOW, [ActionKind.SET_LAMP]),
    (State.YELLOW, EventKind.TICK): (State.RED, [ActionKind.SET_LAMP]),
}

COLOR_OF = {
    State.RED: Color.RED,
    State.GREEN: Color.GREEN,
    State.YELLOW: Color.YELLOW,
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup + bind args; zero side effects
# ---------------------------------------------------------------------------

def fsm_decide(ctx: Ctx, event: Event) -> Decision:
    hit = TABLE.get((ctx.state, event.kind))
    if hit is None:
        return Decision(False, ctx.state, ())
    next_state, kinds = hit
    actions = tuple(
        Action(kind=ActionKind.SET_LAMP, color=COLOR_OF[next_state])
        for _ in kinds
    )
    return Decision(True, next_state, actions)


# ---------------------------------------------------------------------------
# 4. Workers  -- parameterized functions; no FSM state
# ---------------------------------------------------------------------------

def lamp_set(color: Color) -> None:
    print(f"[EXEC] lamp_set({color.name})")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] SET_LAMP(color={action.color.name})")
        lamp_set(action.color)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit ctx, then dispatch
# ---------------------------------------------------------------------------

def format_event(event: Event) -> str:
    return f"TICK(now_ms={event.now_ms})"


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
    ctx = Ctx(state=State.RED)
    print(f"[INIT] state={ctx.state.name}")
    print()
    for now_ms in (1000, 2000, 3000, 4000):
        apply(ctx, Event(EventKind.TICK, now_ms=now_ms))
        print()


if __name__ == "__main__":
    main()
