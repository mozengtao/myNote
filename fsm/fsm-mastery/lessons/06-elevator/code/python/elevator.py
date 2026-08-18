#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto

DOOR_HOLD = 3


class State(Enum):
    IDLE = auto()
    DOOR_OPEN = auto()
    MOVING = auto()


class Event(Enum):
    CALL = auto()
    DOOR_CLOSE = auto()
    ARRIVE = auto()
    TICK = auto()
    TIMEOUT_DOOR = auto()


@dataclass
class Fsm:
    state: State = State.IDLE
    current_floor: int = 0
    target_floor: int = 0
    door_ticks: int = 0


def action_start_move(fsm: Fsm) -> None:
    """begin travel toward the other floor"""
    print(f"action: start move to floor {fsm.target_floor}")


def action_open_door(fsm: Fsm) -> None:
    """open the door and load the door-wait budget"""
    print(
        f"action: door open at floor {fsm.current_floor} "
        f"(door_ticks={fsm.door_ticks})"
    )


def action_close_door(fsm: Fsm) -> None:
    """close the door"""
    print("action: door closed")


def fsm_init(fsm: Fsm) -> None:
    """put the elevator into IDLE at floor 0"""
    fsm.state = State.IDLE
    fsm.current_floor = 0
    fsm.target_floor = 0
    fsm.door_ticks = 0


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event into the elevator FSM"""
    prev = fsm.state

    if event is Event.TICK and fsm.state is State.DOOR_OPEN:
        if fsm.door_ticks > 0:
            fsm.door_ticks -= 1
        print(f"door_ticks={fsm.door_ticks}")
        if fsm.door_ticks > 0:
            return
        event = Event.TIMEOUT_DOOR
        print("timer expired -> TIMEOUT_DOOR")

    if fsm.state is State.IDLE and event is Event.CALL:
        fsm.target_floor = 1 - fsm.current_floor
        action_start_move(fsm)
        fsm.state = State.MOVING
    elif fsm.state is State.MOVING and event is Event.ARRIVE:
        fsm.current_floor = fsm.target_floor
        fsm.door_ticks = DOOR_HOLD
        action_open_door(fsm)
        fsm.state = State.DOOR_OPEN
    elif fsm.state is State.DOOR_OPEN and event in (
        Event.TIMEOUT_DOOR,
        Event.DOOR_CLOSE,
    ):
        action_close_door(fsm)
        fsm.door_ticks = 0
        fsm.state = State.IDLE

    if fsm.state is not prev or event in (
        Event.TIMEOUT_DOOR,
        Event.DOOR_CLOSE,
        Event.CALL,
        Event.ARRIVE,
    ):
        print(
            f"{prev.name} --{event.name}--> {fsm.state.name} "
            f"(floor={fsm.current_floor} target={fsm.target_floor} "
            f"door_ticks={fsm.door_ticks})"
        )


def main() -> None:
    car = Fsm()
    print("=== elevator demo ===")
    fsm_init(car)
    print(f"init: {car.state.name} floor={car.current_floor}")

    print("-- trip 0 -> 1, wait for door timeout --")
    fsm_dispatch(car, Event.CALL)
    fsm_dispatch(car, Event.ARRIVE)
    fsm_dispatch(car, Event.TICK)
    fsm_dispatch(car, Event.TICK)
    fsm_dispatch(car, Event.TICK)

    print("-- trip 1 -> 0, close the door by hand --")
    fsm_dispatch(car, Event.CALL)
    fsm_dispatch(car, Event.ARRIVE)
    fsm_dispatch(car, Event.DOOR_CLOSE)


if __name__ == "__main__":
    main()
