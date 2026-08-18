#include <stdio.h>

typedef enum {
	STATE_IDLE = 0,
	STATE_HAS_COIN
} State;

typedef enum {
	EVENT_COIN = 0,
	EVENT_SELECT,
	EVENT_REFUND
} Event;

typedef struct {
	State state;
} Fsm;

/*
 * return a printable name for a vending state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case STATE_HAS_COIN:
		return "HAS_COIN";
	case STATE_IDLE:
		return "IDLE";
	}
	return "?";
}

/*
 * return a printable name for a vending event
 */
static const char *
event_name(Event event)
{
	switch (event) {
	case EVENT_COIN:
		return "COIN";
	case EVENT_SELECT:
		return "SELECT";
	case EVENT_REFUND:
		return "REFUND";
	}
	return "?";
}

/*
 * put the machine into IDLE
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_IDLE;
}

/*
 * dispatch one named input; ignore combinations with no arrow
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	switch (fsm->state) {
	case STATE_IDLE:
		switch (event) {
		case EVENT_COIN:
			fsm->state = STATE_HAS_COIN;
			break;
		case EVENT_SELECT:
		case EVENT_REFUND:
			break;
		}
		break;
	case STATE_HAS_COIN:
		switch (event) {
		case EVENT_SELECT:
			printf("dispense\n");
			fsm->state = STATE_IDLE;
			break;
		case EVENT_REFUND:
			printf("refund\n");
			fsm->state = STATE_IDLE;
			break;
		case EVENT_COIN:
			break;
		}
		break;
	}
}

int
main(void)
{
	Fsm machine;

	fsm_init(&machine);
	printf("init: %s\n", state_name(machine.state));

	fsm_dispatch(&machine, EVENT_COIN);
	printf("after %s: %s\n", event_name(EVENT_COIN),
	    state_name(machine.state));

	fsm_dispatch(&machine, EVENT_SELECT);
	printf("after %s: %s\n", event_name(EVENT_SELECT),
	    state_name(machine.state));

	fsm_dispatch(&machine, EVENT_COIN);
	printf("after %s: %s\n", event_name(EVENT_COIN),
	    state_name(machine.state));

	fsm_dispatch(&machine, EVENT_REFUND);
	printf("after %s: %s\n", event_name(EVENT_REFUND),
	    state_name(machine.state));

	fsm_dispatch(&machine, EVENT_SELECT);
	printf("after %s: %s\n", event_name(EVENT_SELECT),
	    state_name(machine.state));

	return 0;
}
