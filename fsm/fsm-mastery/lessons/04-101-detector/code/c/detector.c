#include <stdio.h>

typedef enum {
	STATE_S0 = 0,
	STATE_S1,
	STATE_S10,
	STATE_S101
} State;

typedef enum {
	EVENT_BIT0 = 0,
	EVENT_BIT1
} Event;

typedef struct {
	State state;
} Fsm;

/*
 * return a printable name for a detector state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case STATE_S0:
		return "S0";
	case STATE_S1:
		return "S1";
	case STATE_S10:
		return "S10";
	case STATE_S101:
		return "S101";
	}
	return "?";
}

/*
 * return a printable name for a bit event
 */
static const char *
event_name(Event event)
{
	switch (event) {
	case EVENT_BIT0:
		return "BIT0";
	case EVENT_BIT1:
		return "BIT1";
	}
	return "?";
}

/*
 * put the detector into S0
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_S0;
}

/*
 * dispatch one bit; take an arrow only when its bit guard holds
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	switch (fsm->state) {
	case STATE_S0:
		if (event == EVENT_BIT1) {	/* guard: bit == 1 */
			fsm->state = STATE_S1;
		}
		break;
	case STATE_S1:
		if (event == EVENT_BIT0) {	/* guard: bit == 0 */
			fsm->state = STATE_S10;
		}
		/* guard bit==1: stay S1, last 1 still a prefix */
		break;
	case STATE_S10:
		if (event == EVENT_BIT1) {	/* guard: bit == 1 */
			printf("detected 101\n");
			fsm->state = STATE_S101;
		} else {			/* guard: bit == 0 */
			fsm->state = STATE_S0;
		}
		break;
	case STATE_S101:
		if (event == EVENT_BIT1) {	/* guard: bit == 1; overlap */
			fsm->state = STATE_S1;
		} else {			/* guard: bit == 0; overlap */
			fsm->state = STATE_S10;
		}
		break;
	}
}

int
main(void)
{
	static const Event bits[] = {
		EVENT_BIT1,
		EVENT_BIT0,
		EVENT_BIT1,
		EVENT_BIT0,
		EVENT_BIT1,
		EVENT_BIT1
	};
	Fsm detector;
	unsigned i;

	fsm_init(&detector);
	printf("init: %s\n", state_name(detector.state));

	for (i = 0; i < sizeof bits / sizeof bits[0]; i++) {
		fsm_dispatch(&detector, bits[i]);
		printf("after %s: %s\n", event_name(bits[i]),
		    state_name(detector.state));
	}

	return 0;
}
