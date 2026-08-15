#!/usr/bin/env python3
"""Turnstile: COIN carries cents; workers are coin_box_credit and mechanism_set."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    LOCKED = auto()
    UNLOCKED = auto()


class EventKind(Enum):
    COIN = auto()
    PUSH = auto()


class ActionKind(Enum):
    CREDIT_BOX = auto()
    SET_MECHANISM = auto()


@dataclass(frozen=True)
class Event:
    kind: EventKind
    cents: int = 0


@dataclass(frozen=True)
class Action:
    kind: ActionKind
    cents: int = 0
    locked: int = 0


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
    (State.LOCKED, EventKind.COIN): (
        State.UNLOCKED,
        [ActionKind.CREDIT_BOX, ActionKind.SET_MECHANISM],
    ),
    (State.UNLOCKED, EventKind.PUSH): (
        State.LOCKED,
        [ActionKind.SET_MECHANISM],
    ),
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
        if kind is ActionKind.CREDIT_BOX:
            actions.append(Action(kind=kind, cents=event.cents))
        else:
            locked = 0 if next_state is State.UNLOCKED else 1
            actions.append(Action(kind=kind, locked=locked))
    return Decision(True, next_state, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- parameterized functions; no FSM state
# ---------------------------------------------------------------------------

def coin_box_credit(cents: int) -> None:
    print(f"[EXEC] coin_box_credit({cents})")


def mechanism_set(locked: int) -> None:
    print(f"[EXEC] mechanism_set({locked})")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        if action.kind is ActionKind.CREDIT_BOX:
            print(f"[DISPATCH] CREDIT_BOX(cents={action.cents})")
            coin_box_credit(action.cents)
        elif action.kind is ActionKind.SET_MECHANISM:
            print(f"[DISPATCH] SET_MECHANISM(locked={action.locked})")
            mechanism_set(action.locked)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit ctx, then dispatch
# ---------------------------------------------------------------------------

def format_event(event: Event) -> str:
    if event.kind is EventKind.COIN:
        return f"COIN(cents={event.cents})"
    return "PUSH"


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
    ctx = Ctx(state=State.LOCKED)
    print(f"[INIT] state={ctx.state.name}")
    print()
    events = (
        Event(EventKind.COIN, cents=100),
        Event(EventKind.PUSH),
        Event(EventKind.PUSH),
        Event(EventKind.COIN, cents=100),
        Event(EventKind.COIN, cents=50),
        Event(EventKind.PUSH),
    )
    for event in events:
        apply(ctx, event)
        print()


if __name__ == "__main__":
    main()
