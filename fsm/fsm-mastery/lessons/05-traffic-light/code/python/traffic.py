#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto


class State(Enum):
    RED = auto()
    GREEN = auto()
    YELLOW = auto()


class Event(Enum):
    TICK = auto()


@dataclass
class Fsm:
    state: State = State.RED
    lamp: str = "RED"


def action_red_on(fsm: Fsm) -> None:
    """turn on the red lamp (transition action)"""
    fsm.lamp = "RED"
    print("RED on")


def action_green_on(fsm: Fsm) -> None:
    """turn on the green lamp (transition action)"""
    fsm.lamp = "GREEN"
    print("GREEN on")


def action_yellow_on(fsm: Fsm) -> None:
    """turn on the yellow lamp (transition action)"""
    fsm.lamp = "YELLOW"
    print("YELLOW on")


def fsm_init(fsm: Fsm) -> None:
    """put the light into RED and turn the red lamp on"""
    fsm.state = State.RED
    action_red_on(fsm)


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event into the traffic-light FSM"""
    prev = fsm.state
    if event is not Event.TICK:
        return

    if fsm.state is State.RED:
        action_green_on(fsm)
        fsm.state = State.GREEN
    elif fsm.state is State.GREEN:
        action_yellow_on(fsm)
        fsm.state = State.YELLOW
    elif fsm.state is State.YELLOW:
        action_red_on(fsm)
        fsm.state = State.RED

    print(f"{prev.name} --{event.name}--> {fsm.state.name} (lamp={fsm.lamp})")


def main() -> None:
    light = Fsm()
    print("=== traffic light demo ===")
    fsm_init(light)
    print(f"init: {light.state.name} (lamp={light.lamp})")

    fsm_dispatch(light, Event.TICK)
    fsm_dispatch(light, Event.TICK)
    fsm_dispatch(light, Event.TICK)
    fsm_dispatch(light, Event.TICK)


if __name__ == "__main__":
    main()
