#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum, auto

RETRY_BUDGET = 3


class State(Enum):
    STOPPED = auto()
    CONNECTING = auto()
    PLAYING = auto()
    BUFFERING = auto()


class Event(Enum):
    PLAY = auto()
    STOP = auto()
    CONNECTED = auto()
    DATA_STARVE = auto()
    DATA_OK = auto()
    TIMEOUT = auto()
    TICK = auto()
    GIVE_UP = auto()


@dataclass
class Fsm:
    state: State = State.STOPPED
    retries_left: int = 0
    backoff_ticks: int = 1
    wait_ticks: int = 0


def action_start_connect(fsm: Fsm) -> None:
    """begin a connect attempt"""
    print(f"action: start connect (retries left={fsm.retries_left})")


def action_start_play(fsm: Fsm) -> None:
    """begin playback"""
    print("action: start play")


def action_wait_data(fsm: Fsm) -> None:
    """wait for more stream data"""
    print("action: wait data")


def action_resume_play(fsm: Fsm) -> None:
    """resume playback after buffering"""
    print("action: resume play")


def action_stop(fsm: Fsm) -> None:
    """stop playback and drop the session"""
    print("action: stop")


def clear_budget(fsm: Fsm) -> None:
    """clear retry budget and backoff wait"""
    fsm.retries_left = 0
    fsm.backoff_ticks = 1
    fsm.wait_ticks = 0


def fsm_init(fsm: Fsm) -> None:
    """put the player into STOPPED"""
    fsm.state = State.STOPPED
    clear_budget(fsm)


def fsm_dispatch(fsm: Fsm, event: Event) -> None:
    """dispatch one event into the player FSM"""
    prev = fsm.state

    if (
        event is Event.TICK
        and fsm.state is State.CONNECTING
        and fsm.wait_ticks > 0
    ):
        fsm.wait_ticks -= 1
        print(f"backoff wait={fsm.wait_ticks}")
        if fsm.wait_ticks == 0:
            print(f"retry connect, retries left={fsm.retries_left}")
        return

    if event is Event.TIMEOUT and fsm.state is State.CONNECTING:
        if fsm.wait_ticks > 0:
            return
        fsm.retries_left -= 1
        print(f"retries left={fsm.retries_left}")
        if fsm.retries_left <= 0:
            event = Event.GIVE_UP
        else:
            fsm.wait_ticks = fsm.backoff_ticks
            print(f"backoff wait: {fsm.wait_ticks}")
            fsm.backoff_ticks *= 2
            print(
                f"{prev.name} --TIMEOUT--> {fsm.state.name} "
                f"(retries left={fsm.retries_left})"
            )
            return

    if fsm.state is State.STOPPED and event is Event.PLAY:
        fsm.retries_left = RETRY_BUDGET
        fsm.backoff_ticks = 1
        fsm.wait_ticks = 0
        action_start_connect(fsm)
        fsm.state = State.CONNECTING
    elif fsm.state is State.CONNECTING:
        if event is Event.CONNECTED:
            fsm.wait_ticks = 0
            action_start_play(fsm)
            fsm.state = State.PLAYING
        elif event is Event.GIVE_UP:
            print("give up")
            action_stop(fsm)
            clear_budget(fsm)
            fsm.state = State.STOPPED
        elif event is Event.STOP:
            action_stop(fsm)
            clear_budget(fsm)
            fsm.state = State.STOPPED
    elif fsm.state is State.PLAYING:
        if event is Event.DATA_STARVE:
            action_wait_data(fsm)
            fsm.state = State.BUFFERING
        elif event is Event.STOP:
            action_stop(fsm)
            clear_budget(fsm)
            fsm.state = State.STOPPED
    elif fsm.state is State.BUFFERING:
        if event is Event.DATA_OK:
            action_resume_play(fsm)
            fsm.state = State.PLAYING
        elif event is Event.STOP:
            action_stop(fsm)
            clear_budget(fsm)
            fsm.state = State.STOPPED

    if fsm.state is not prev:
        print(
            f"{prev.name} --{event.name}--> {fsm.state.name} "
            f"(retries left={fsm.retries_left} wait={fsm.wait_ticks})"
        )


def main() -> None:
    player = Fsm()
    print("=== player demo ===")
    fsm_init(player)
    print(f"init: {player.state.name}")

    print("-- connect fails three times, then give up --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.TIMEOUT)
    fsm_dispatch(player, Event.TICK)
    fsm_dispatch(player, Event.TIMEOUT)
    fsm_dispatch(player, Event.TICK)
    fsm_dispatch(player, Event.TICK)
    fsm_dispatch(player, Event.TIMEOUT)

    print("-- connect, starve, recover, stop --")
    fsm_dispatch(player, Event.PLAY)
    fsm_dispatch(player, Event.CONNECTED)
    fsm_dispatch(player, Event.DATA_STARVE)
    fsm_dispatch(player, Event.DATA_OK)
    fsm_dispatch(player, Event.STOP)


if __name__ == "__main__":
    main()
