#include <stdio.h>

typedef enum {
	STATE_RED = 0,
	STATE_GREEN,
	STATE_YELLOW
} State;

typedef enum {
	EVENT_TICK = 0
} Event;

typedef struct {
	State state;
	const char *lamp;
} Fsm;

/*
 * return a printable name for a traffic-light state
 */
static const char *
state_name(State state)
{
	if (state == STATE_GREEN)
		return "GREEN";
	if (state == STATE_YELLOW)
		return "YELLOW";
	return "RED";
}

/*
 * return a printable name for an event
 */
static const char *
event_name(Event event)
{
	(void)event;
	return "TICK";
}

/*
 * turn on the red lamp (transition action)
 */
static void
action_red_on(Fsm *fsm)
{
	fsm->lamp = "RED";
	printf("RED on\n");
}

/*
 * turn on the green lamp (transition action)
 */
static void
action_green_on(Fsm *fsm)
{
	fsm->lamp = "GREEN";
	printf("GREEN on\n");
}

/*
 * turn on the yellow lamp (transition action)
 */
static void
action_yellow_on(Fsm *fsm)
{
	fsm->lamp = "YELLOW";
	printf("YELLOW on\n");
}

/*
 * put the light into RED and turn the red lamp on
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_RED;
	action_red_on(fsm);
}

/*
 * dispatch one event into the traffic-light FSM
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;

	prev = fsm->state;
	if (event != EVENT_TICK)
		return;

	switch (fsm->state) {
	case STATE_RED:
		action_green_on(fsm);
		fsm->state = STATE_GREEN;
		break;
	case STATE_GREEN:
		action_yellow_on(fsm);
		fsm->state = STATE_YELLOW;
		break;
	case STATE_YELLOW:
		action_red_on(fsm);
		fsm->state = STATE_RED;
		break;
	}

	printf("%s --%s--> %s (lamp=%s)\n",
	    state_name(prev), event_name(event),
	    state_name(fsm->state), fsm->lamp);
}

int
main(void)
{
	Fsm light;

	printf("=== traffic light demo ===\n");
	fsm_init(&light);
	printf("init: %s (lamp=%s)\n", state_name(light.state), light.lamp);

	fsm_dispatch(&light, EVENT_TICK);
	fsm_dispatch(&light, EVENT_TICK);
	fsm_dispatch(&light, EVENT_TICK);
	fsm_dispatch(&light, EVENT_TICK);

	return 0;
}
