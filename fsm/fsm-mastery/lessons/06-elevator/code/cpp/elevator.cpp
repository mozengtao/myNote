#include <cstdio>

enum {
	DoorHold = 3
};

enum class State {
	Idle,
	DoorOpen,
	Moving
};

enum class Event {
	Call,
	DoorClose,
	Arrive,
	Tick,
	TimeoutDoor
};

struct Fsm {
	State state;
	int current_floor;
	int target_floor;
	int door_ticks;
};

/*
 * return a printable name for an elevator state
 */
static const char *
state_name(State state)
{
	if (state == State::DoorOpen)
		return "DOOR_OPEN";
	if (state == State::Moving)
		return "MOVING";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == Event::DoorClose)
		return "DOOR_CLOSE";
	if (event == Event::Arrive)
		return "ARRIVE";
	if (event == Event::Tick)
		return "TICK";
	if (event == Event::TimeoutDoor)
		return "TIMEOUT_DOOR";
	return "CALL";
}

/*
 * begin travel toward the other floor
 */
static void
action_start_move(Fsm *fsm)
{
	std::printf("action: start move to floor %d\n", fsm->target_floor);
}

/*
 * open the door and load the door-wait budget
 */
static void
action_open_door(Fsm *fsm)
{
	std::printf("action: door open at floor %d (door_ticks=%d)\n",
	    fsm->current_floor, fsm->door_ticks);
}

/*
 * close the door
 */
static void
action_close_door(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: door closed\n");
}

/*
 * put the elevator into Idle at floor 0
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Idle;
	fsm->current_floor = 0;
	fsm->target_floor = 0;
	fsm->door_ticks = 0;
}

/*
 * dispatch one event into the elevator FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;

	prev = fsm->state;

	if (event == Event::Tick && fsm->state == State::DoorOpen) {
		if (fsm->door_ticks > 0)
			fsm->door_ticks--;
		std::printf("door_ticks=%d\n", fsm->door_ticks);
		if (fsm->door_ticks > 0)
			return;
		event = Event::TimeoutDoor;
		std::printf("timer expired -> TIMEOUT_DOOR\n");
	}

	switch (fsm->state) {
	case State::Idle:
		if (event == Event::Call) {
			fsm->target_floor = 1 - fsm->current_floor;
			action_start_move(fsm);
			fsm->state = State::Moving;
		}
		break;
	case State::Moving:
		if (event == Event::Arrive) {
			fsm->current_floor = fsm->target_floor;
			fsm->door_ticks = DoorHold;
			action_open_door(fsm);
			fsm->state = State::DoorOpen;
		}
		break;
	case State::DoorOpen:
		if (event == Event::TimeoutDoor || event == Event::DoorClose) {
			action_close_door(fsm);
			fsm->door_ticks = 0;
			fsm->state = State::Idle;
		}
		break;
	}

	if (fsm->state != prev || event == Event::TimeoutDoor ||
	    event == Event::DoorClose || event == Event::Call ||
	    event == Event::Arrive) {
		std::printf("%s --%s--> %s (floor=%d target=%d door_ticks=%d)\n",
		    state_name(prev), event_name(event),
		    state_name(fsm->state), fsm->current_floor,
		    fsm->target_floor, fsm->door_ticks);
	}
}

int
main()
{
	Fsm car{};

	std::printf("=== elevator demo ===\n");
	fsm_init(&car);
	std::printf("init: %s floor=%d\n", state_name(car.state), car.current_floor);

	std::printf("-- trip 0 -> 1, wait for door timeout --\n");
	fsm_dispatch(&car, Event::Call);
	fsm_dispatch(&car, Event::Arrive);
	fsm_dispatch(&car, Event::Tick);
	fsm_dispatch(&car, Event::Tick);
	fsm_dispatch(&car, Event::Tick);

	std::printf("-- trip 1 -> 0, close the door by hand --\n");
	fsm_dispatch(&car, Event::Call);
	fsm_dispatch(&car, Event::Arrive);
	fsm_dispatch(&car, Event::DoorClose);

	return 0;
}
