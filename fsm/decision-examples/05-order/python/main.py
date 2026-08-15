#!/usr/bin/env python3
"""Order fulfillment: one decision emits an action list to many workers."""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Dict, List, Sequence, Tuple


# ---------------------------------------------------------------------------
# 1. Types
# ---------------------------------------------------------------------------

class State(Enum):
    CREATED = auto()
    PAID = auto()
    SHIPPED = auto()
    CANCELLED = auto()


class Event(Enum):
    PAY_SUCCESS = auto()
    SHIP = auto()
    CANCEL = auto()


class Action(Enum):
    DEDUCT_STOCK = auto()
    NOTIFY_LOGISTICS = auto()
    ADD_POINTS = auto()
    SEND_SMS = auto()
    RELEASE_STOCK = auto()


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, Event) -> (NextState, ActionList)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, Event], Tuple[State, List[Action]]] = {
    (State.CREATED, Event.PAY_SUCCESS): (
        State.PAID,
        [
            Action.DEDUCT_STOCK,
            Action.NOTIFY_LOGISTICS,
            Action.ADD_POINTS,
            Action.SEND_SMS,
        ],
    ),
    (State.PAID, Event.SHIP): (State.SHIPPED, [Action.SEND_SMS]),
    (State.CREATED, Event.CANCEL): (State.CANCELLED, [Action.SEND_SMS]),
    (State.PAID, Event.CANCEL): (
        State.CANCELLED,
        [Action.RELEASE_STOCK, Action.SEND_SMS],
    ),
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

def worker_inventory(action: Action) -> None:
    if action is Action.DEDUCT_STOCK:
        print("[EXEC] inventory -> DEDUCT")
    elif action is Action.RELEASE_STOCK:
        print("[EXEC] inventory -> RELEASE")


def worker_logistics(action: Action) -> None:
    if action is Action.NOTIFY_LOGISTICS:
        print("[EXEC] logistics -> CREATE_SHIPMENT")


def worker_points(action: Action) -> None:
    if action is Action.ADD_POINTS:
        print("[EXEC] points -> ADD")


def worker_sms(action: Action) -> None:
    if action is Action.SEND_SMS:
        print("[EXEC] sms -> SEND")


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        print(f"[DISPATCH] {action.name}")
        if action in (Action.DEDUCT_STOCK, Action.RELEASE_STOCK):
            worker_inventory(action)
        elif action is Action.NOTIFY_LOGISTICS:
            worker_logistics(action)
        elif action is Action.ADD_POINTS:
            worker_points(action)
        elif action is Action.SEND_SMS:
            worker_sms(action)


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


def run_scenario(title: str, events: Sequence[Event]) -> None:
    state = State.CREATED
    print(f"=== scenario: {title} ===")
    print(f"[INIT] state={state.name}")
    print()
    for event in events:
        state = apply(state, event)
        print()


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

def main() -> None:
    run_scenario(
        "happy-path",
        (Event.SHIP, Event.PAY_SUCCESS, Event.SHIP, Event.CANCEL),
    )
    run_scenario(
        "cancel-created",
        (Event.CANCEL,),
    )
    run_scenario(
        "cancel-paid",
        (Event.PAY_SUCCESS, Event.CANCEL),
    )


if __name__ == "__main__":
    main()
