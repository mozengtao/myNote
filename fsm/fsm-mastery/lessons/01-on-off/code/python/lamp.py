#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    OFF = auto()
    ON = auto()


@dataclass
class Fsm:
    state: State = State.OFF


def fsm_init(fsm: Fsm) -> None:
    """put the lamp into OFF"""
    fsm.state = State.OFF


def lamp_turn_on(fsm: Fsm) -> None:
    """occupy ON when the lamp is OFF"""
    if fsm.state is State.OFF:
        fsm.state = State.ON


def lamp_turn_off(fsm: Fsm) -> None:
    """occupy OFF when the lamp is ON"""
    if fsm.state is State.ON:
        fsm.state = State.OFF


def main() -> None:
    lamp = Fsm()
    fsm_init(lamp)
    print(f"init: {lamp.state.name}")

    lamp_turn_on(lamp)
    print(f"after turn on: {lamp.state.name}")

    lamp_turn_on(lamp)
    print(f"turn on again: {lamp.state.name}")

    lamp_turn_off(lamp)
    print(f"after turn off: {lamp.state.name}")


if __name__ == "__main__":
    main()
