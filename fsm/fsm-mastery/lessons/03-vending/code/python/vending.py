#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    IDLE = auto()
    HAS_COIN = auto()


class Event(Enum):
    COIN = auto()
    SELECT = auto()
    REFUND = auto()


@dataclass
class Fsm:
    state: State = State.IDLE


def fsm_init(fsm: Fsm) -> None:
    """put the machine into IDLE"""
    fsm.state = State.IDLE


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one named input; ignore combinations with no arrow"""
    if fsm.state is State.IDLE:
        if event is Event.COIN:
            fsm.state = State.HAS_COIN
    elif fsm.state is State.HAS_COIN:
        if event is Event.SELECT:
            print("dispense")
            fsm.state = State.IDLE
        elif event is Event.REFUND:
            print("refund")
            fsm.state = State.IDLE


def main() -> None:
    machine = Fsm()
    fsm_init(machine)
    print(f"init: {machine.state.name}")

    fsm_dispatch(machine, Event.COIN)
    print(f"after {Event.COIN.name}: {machine.state.name}")

    fsm_dispatch(machine, Event.SELECT)
    print(f"after {Event.SELECT.name}: {machine.state.name}")

    fsm_dispatch(machine, Event.COIN)
    print(f"after {Event.COIN.name}: {machine.state.name}")

    fsm_dispatch(machine, Event.REFUND)
    print(f"after {Event.REFUND.name}: {machine.state.name}")

    fsm_dispatch(machine, Event.SELECT)
    print(f"after {Event.SELECT.name}: {machine.state.name}")


if __name__ == "__main__":
    main()
