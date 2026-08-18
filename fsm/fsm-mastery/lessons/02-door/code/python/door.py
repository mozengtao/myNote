#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    CLOSED = auto()
    OPEN = auto()


@dataclass
class Fsm:
    state: State = State.CLOSED


def fsm_init(fsm: Fsm) -> None:
    """put the door into CLOSED"""
    fsm.state = State.CLOSED


def door_open(fsm: Fsm) -> None:
    """take the CLOSED -> OPEN transition"""
    if fsm.state is State.CLOSED:
        fsm.state = State.OPEN
    elif fsm.state is State.OPEN:
        pass


def door_close(fsm: Fsm) -> None:
    """take the OPEN -> CLOSED transition"""
    if fsm.state is State.OPEN:
        fsm.state = State.CLOSED
    elif fsm.state is State.CLOSED:
        pass


def main() -> None:
    door = Fsm()
    fsm_init(door)
    print(f"init: {door.state.name}")

    door_open(door)
    print(f"after open: {door.state.name}")

    door_open(door)
    print(f"open again: {door.state.name}")

    door_close(door)
    print(f"after close: {door.state.name}")


if __name__ == "__main__":
    main()
