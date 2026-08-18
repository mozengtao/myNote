#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    S0 = auto()
    S1 = auto()
    S10 = auto()
    S101 = auto()


class Event(Enum):
    BIT0 = auto()
    BIT1 = auto()


@dataclass
class Fsm:
    state: State = State.S0


def fsm_init(fsm: Fsm) -> None:
    """put the detector into S0"""
    fsm.state = State.S0


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one bit; take an arrow only when its bit guard holds"""
    if fsm.state is State.S0:
        if event is Event.BIT1:  # guard: bit == 1
            fsm.state = State.S1
    elif fsm.state is State.S1:
        if event is Event.BIT0:  # guard: bit == 0
            fsm.state = State.S10
        # guard bit==1: stay S1, last 1 still a prefix
    elif fsm.state is State.S10:
        if event is Event.BIT1:  # guard: bit == 1
            print("detected 101")
            fsm.state = State.S101
        else:  # guard: bit == 0
            fsm.state = State.S0
    elif fsm.state is State.S101:
        if event is Event.BIT1:  # guard: bit == 1; overlap
            fsm.state = State.S1
        else:  # guard: bit == 0; overlap
            fsm.state = State.S10


def main() -> None:
    bits = (
        Event.BIT1,
        Event.BIT0,
        Event.BIT1,
        Event.BIT0,
        Event.BIT1,
        Event.BIT1,
    )
    detector = Fsm()
    fsm_init(detector)
    print(f"init: {detector.state.name}")

    for bit in bits:
        fsm_dispatch(detector, bit)
        print(f"after {bit.name}: {detector.state.name}")


if __name__ == "__main__":
    main()
