#!/usr/bin/env python3
"""Order fulfillment: one decision fills many worker calls from order data."""

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


class EventKind(Enum):
    PAY_SUCCESS = auto()
    SHIP = auto()
    CANCEL = auto()


class ActionKind(Enum):
    DEDUCT_STOCK = auto()
    NOTIFY_LOGISTICS = auto()
    ADD_POINTS = auto()
    SEND_SMS = auto()
    RELEASE_STOCK = auto()


@dataclass(frozen=True)
class Event:
    kind: EventKind
    order_id: int = 0
    sku: str = ""
    qty: int = 0
    amount: int = 0
    phone: str = ""


@dataclass(frozen=True)
class Action:
    kind: ActionKind
    sku: str = ""
    qty: int = 0
    order_id: int = 0
    amount: int = 0
    phone: str = ""
    msg: str = ""


@dataclass
class Ctx:
    state: State
    order_id: int
    sku: str
    qty: int
    amount: int
    phone: str


@dataclass(frozen=True)
class Decision:
    accepted: bool
    next_state: State
    actions: Tuple[Action, ...]


# ---------------------------------------------------------------------------
# 2. Table  (State, EventKind) -> (NextState, ActionKind list)
# ---------------------------------------------------------------------------

TABLE: Dict[Tuple[State, EventKind], Tuple[State, List[ActionKind]]] = {
    (State.CREATED, EventKind.PAY_SUCCESS): (
        State.PAID,
        [
            ActionKind.DEDUCT_STOCK,
            ActionKind.NOTIFY_LOGISTICS,
            ActionKind.ADD_POINTS,
            ActionKind.SEND_SMS,
        ],
    ),
    (State.PAID, EventKind.SHIP): (State.SHIPPED, [ActionKind.SEND_SMS]),
    (State.CREATED, EventKind.CANCEL): (State.CANCELLED, [ActionKind.SEND_SMS]),
    (State.PAID, EventKind.CANCEL): (
        State.CANCELLED,
        [ActionKind.RELEASE_STOCK, ActionKind.SEND_SMS],
    ),
}

SMS_MSG = {
    EventKind.PAY_SUCCESS: "paid",
    EventKind.SHIP: "shipped",
    EventKind.CANCEL: "cancelled",
}


# ---------------------------------------------------------------------------
# 3. Engine  -- table lookup + bind args from Event or Ctx
# ---------------------------------------------------------------------------

def fsm_decide(ctx: Ctx, event: Event) -> Decision:
    hit = TABLE.get((ctx.state, event.kind))
    if hit is None:
        return Decision(False, ctx.state, ())
    next_state, kinds = hit
    if event.kind is EventKind.PAY_SUCCESS:
        order_id, sku, qty, amount, phone = (
            event.order_id, event.sku, event.qty, event.amount, event.phone
        )
    else:
        order_id, sku, qty, amount, phone = (
            ctx.order_id, ctx.sku, ctx.qty, ctx.amount, ctx.phone
        )
    actions = []
    for kind in kinds:
        if kind is ActionKind.DEDUCT_STOCK:
            actions.append(Action(kind=kind, sku=sku, qty=qty))
        elif kind is ActionKind.NOTIFY_LOGISTICS:
            actions.append(Action(kind=kind, order_id=order_id, sku=sku))
        elif kind is ActionKind.ADD_POINTS:
            actions.append(Action(kind=kind, order_id=order_id, amount=amount))
        elif kind is ActionKind.SEND_SMS:
            actions.append(
                Action(kind=kind, phone=phone, msg=SMS_MSG[event.kind])
            )
        elif kind is ActionKind.RELEASE_STOCK:
            actions.append(Action(kind=kind, sku=sku, qty=qty))
    return Decision(True, next_state, tuple(actions))


# ---------------------------------------------------------------------------
# 4. Workers  -- parameterized functions; no FSM state
# ---------------------------------------------------------------------------

def inventory_deduct(sku: str, qty: int) -> None:
    print(f'[EXEC] inventory_deduct("{sku}", {qty})')


def inventory_release(sku: str, qty: int) -> None:
    print(f'[EXEC] inventory_release("{sku}", {qty})')


def logistics_create(order_id: int, sku: str) -> None:
    print(f'[EXEC] logistics_create({order_id}, "{sku}")')


def points_add(order_id: int, amount: int) -> None:
    print(f"[EXEC] points_add({order_id}, {amount})")


def sms_send(phone: str, msg: str) -> None:
    print(f'[EXEC] sms_send("{phone}", "{msg}")')


# ---------------------------------------------------------------------------
# 5. Dispatch
# ---------------------------------------------------------------------------

def dispatch(actions: Tuple[Action, ...]) -> None:
    for action in actions:
        if action.kind is ActionKind.DEDUCT_STOCK:
            print(f"[DISPATCH] DEDUCT_STOCK(sku={action.sku}, qty={action.qty})")
            inventory_deduct(action.sku, action.qty)
        elif action.kind is ActionKind.NOTIFY_LOGISTICS:
            print(
                f"[DISPATCH] NOTIFY_LOGISTICS(order_id={action.order_id}, "
                f"sku={action.sku})"
            )
            logistics_create(action.order_id, action.sku)
        elif action.kind is ActionKind.ADD_POINTS:
            print(
                f"[DISPATCH] ADD_POINTS(order_id={action.order_id}, "
                f"amount={action.amount})"
            )
            points_add(action.order_id, action.amount)
        elif action.kind is ActionKind.SEND_SMS:
            print(f"[DISPATCH] SEND_SMS(phone={action.phone}, msg={action.msg})")
            sms_send(action.phone, action.msg)
        elif action.kind is ActionKind.RELEASE_STOCK:
            print(
                f"[DISPATCH] RELEASE_STOCK(sku={action.sku}, qty={action.qty})"
            )
            inventory_release(action.sku, action.qty)


# ---------------------------------------------------------------------------
# 6. Apply  -- commit ctx, then dispatch
# ---------------------------------------------------------------------------

def format_event(event: Event) -> str:
    if event.kind is EventKind.PAY_SUCCESS:
        return (
            f"PAY_SUCCESS(order_id={event.order_id}, sku={event.sku}, "
            f"qty={event.qty}, amount={event.amount}, phone={event.phone})"
        )
    return event.kind.name


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
    if event.kind is EventKind.PAY_SUCCESS:
        ctx.order_id = event.order_id
        ctx.sku = event.sku
        ctx.qty = event.qty
        ctx.amount = event.amount
        ctx.phone = event.phone
    dispatch(decision.actions)


def new_order() -> Ctx:
    return Ctx(
        state=State.CREATED,
        order_id=1001,
        sku="SKU-7",
        qty=2,
        amount=99,
        phone="13800001111",
    )


def run_scenario(title: str, events: Sequence[Event]) -> None:
    ctx = new_order()
    print(f"=== scenario: {title} ===")
    print(
        f"[INIT] state={ctx.state.name} order_id={ctx.order_id} "
        f"sku={ctx.sku} qty={ctx.qty} amount={ctx.amount} phone={ctx.phone}"
    )
    print()
    for event in events:
        apply(ctx, event)
        print()


# ---------------------------------------------------------------------------
# 7. Main
# ---------------------------------------------------------------------------

PAY = Event(
    EventKind.PAY_SUCCESS,
    order_id=1001,
    sku="SKU-7",
    qty=2,
    amount=99,
    phone="13800001111",
)


def main() -> None:
    run_scenario(
        "happy-path",
        (Event(EventKind.SHIP), PAY, Event(EventKind.SHIP), Event(EventKind.CANCEL)),
    )
    run_scenario("cancel-created", (Event(EventKind.CANCEL),))
    run_scenario("cancel-paid", (PAY, Event(EventKind.CANCEL)))


if __name__ == "__main__":
    main()
