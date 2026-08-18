#include <stdio.h>

typedef enum {
	STATE_CLOSED = 0,
	STATE_OPEN
} State;

typedef struct {
	State state;
} Fsm;

/*
 * return a printable name for a door state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case STATE_OPEN:
		return "OPEN";
	case STATE_CLOSED:
		return "CLOSED";
	}
	return "?";
}

/*
 * put the door into CLOSED
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_CLOSED;
}

/*
 * take the CLOSED -> OPEN transition
 */
void
door_open(Fsm *fsm)
{
	switch (fsm->state) {
	case STATE_CLOSED:
		fsm->state = STATE_OPEN;
		break;
	case STATE_OPEN:
		break;
	}
}

/*
 * take the OPEN -> CLOSED transition
 */
void
door_close(Fsm *fsm)
{
	switch (fsm->state) {
	case STATE_OPEN:
		fsm->state = STATE_CLOSED;
		break;
	case STATE_CLOSED:
		break;
	}
}

int
main(void)
{
	Fsm door;

	fsm_init(&door);
	printf("init: %s\n", state_name(door.state));

	door_open(&door);
	printf("after open: %s\n", state_name(door.state));

	door_open(&door);
	printf("open again: %s\n", state_name(door.state));

	door_close(&door);
	printf("after close: %s\n", state_name(door.state));

	return 0;
}
