#include <stdio.h>

typedef enum {
	STATE_OFF = 0,
	STATE_ON
} State;

typedef struct {
	State state;
} Fsm;

/*
 * return a printable name for a lamp state
 */
static const char *
state_name(State state)
{
	if (state == STATE_ON)
		return "ON";
	return "OFF";
}

/*
 * put the lamp into OFF
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_OFF;
}

/*
 * occupy ON when the lamp is OFF
 */
void
lamp_turn_on(Fsm *fsm)
{
	if (fsm->state == STATE_OFF)
		fsm->state = STATE_ON;
}

/*
 * occupy OFF when the lamp is ON
 */
void
lamp_turn_off(Fsm *fsm)
{
	if (fsm->state == STATE_ON)
		fsm->state = STATE_OFF;
}

int
main(void)
{
	Fsm lamp;

	fsm_init(&lamp);
	printf("init: %s\n", state_name(lamp.state));

	lamp_turn_on(&lamp);
	printf("after turn on: %s\n", state_name(lamp.state));

	lamp_turn_on(&lamp);
	printf("turn on again: %s\n", state_name(lamp.state));

	lamp_turn_off(&lamp);
	printf("after turn off: %s\n", state_name(lamp.state));

	return 0;
}
