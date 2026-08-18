#include <stdio.h>

#define DOOR_HOLD 3

typedef enum {
	STATE_IDLE = 0,
	STATE_DOOR_OPEN,
	STATE_MOVING
} State;

typedef enum {
	EVENT_CALL = 0,
	EVENT_DOOR_CLOSE,
	EVENT_ARRIVE,
	EVENT_TICK,
	EVENT_TIMEOUT_DOOR
} Event;

typedef struct {
	State state;
	int current_floor;
	int target_floor;
	int door_ticks;
} Fsm;

/*
 * return a printable name for an elevator state
 */
static const char *
state_name(State state)
{
	if (state == STATE_DOOR_OPEN)
		return "DOOR_OPEN";
	if (state == STATE_MOVING)
		return "MOVING";
	return "IDLE";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_DOOR_CLOSE)
		return "DOOR_CLOSE";
	if (event == EVENT_ARRIVE)
		return "ARRIVE";
	if (event == EVENT_TICK)
		return "TICK";
	if (event == EVENT_TIMEOUT_DOOR)
		return "TIMEOUT_DOOR";
	return "CALL";
}

/*
 * begin travel toward the other floor
 */
static void
action_start_move(Fsm *fsm)
{
	printf("action: start move to floor %d\n", fsm->target_floor);
}

/*
 * open the door and load the door-wait budget
 */
static void
action_open_door(Fsm *fsm)
{
	printf("action: door open at floor %d (door_ticks=%d)\n",
	    fsm->current_floor, fsm->door_ticks);
}

/*
 * close the door
 */
static void
action_close_door(Fsm *fsm)
{
	(void)fsm;
	printf("action: door closed\n");
}

/*
 * put the elevator into IDLE at floor 0
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_IDLE;
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

	if (event == EVENT_TICK && fsm->state == STATE_DOOR_OPEN) {
		if (fsm->door_ticks > 0)
			fsm->door_ticks--;
		printf("door_ticks=%d\n", fsm->door_ticks);
		if (fsm->door_ticks > 0)
			return;
		event = EVENT_TIMEOUT_DOOR;
		printf("timer expired -> TIMEOUT_DOOR\n");
	}

	switch (fsm->state) {
	case STATE_IDLE:
		if (event == EVENT_CALL) {
			fsm->target_floor = 1 - fsm->current_floor;
			action_start_move(fsm);
			fsm->state = STATE_MOVING;
		}
		break;
	case STATE_MOVING:
		if (event == EVENT_ARRIVE) {
			fsm->current_floor = fsm->target_floor;
			fsm->door_ticks = DOOR_HOLD;
			action_open_door(fsm);
			fsm->state = STATE_DOOR_OPEN;
		}
		break;
	case STATE_DOOR_OPEN:
		if (event == EVENT_TIMEOUT_DOOR || event == EVENT_DOOR_CLOSE) {
			action_close_door(fsm);
			fsm->door_ticks = 0;
			fsm->state = STATE_IDLE;
		}
		break;
	}

	if (fsm->state != prev || event == EVENT_TIMEOUT_DOOR ||
	    event == EVENT_DOOR_CLOSE || event == EVENT_CALL ||
	    event == EVENT_ARRIVE) {
		printf("%s --%s--> %s (floor=%d target=%d door_ticks=%d)\n",
		    state_name(prev), event_name(event),
		    state_name(fsm->state), fsm->current_floor,
		    fsm->target_floor, fsm->door_ticks);
	}
}

int
main(void)
{
	Fsm car;

	printf("=== elevator demo ===\n");
	fsm_init(&car);
	printf("init: %s floor=%d\n", state_name(car.state), car.current_floor);

	printf("-- trip 0 -> 1, wait for door timeout --\n");
	fsm_dispatch(&car, EVENT_CALL);
	fsm_dispatch(&car, EVENT_ARRIVE);
	fsm_dispatch(&car, EVENT_TICK);
	fsm_dispatch(&car, EVENT_TICK);
	fsm_dispatch(&car, EVENT_TICK);

	printf("-- trip 1 -> 0, close the door by hand --\n");
	fsm_dispatch(&car, EVENT_CALL);
	fsm_dispatch(&car, EVENT_ARRIVE);
	fsm_dispatch(&car, EVENT_DOOR_CLOSE);

	return 0;
}
