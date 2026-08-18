#include <cstdio>

enum class State {
	Red,
	Green,
	Yellow
};

enum class Event {
	Tick
};

struct Fsm {
	State state;
	const char *lamp;
};

/*
 * return a printable name for a traffic-light state
 */
static const char *
state_name(State state)
{
	if (state == State::Green)
		return "GREEN";
	if (state == State::Yellow)
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
	std::printf("RED on\n");
}

/*
 * turn on the green lamp (transition action)
 */
static void
action_green_on(Fsm *fsm)
{
	fsm->lamp = "GREEN";
	std::printf("GREEN on\n");
}

/*
 * turn on the yellow lamp (transition action)
 */
static void
action_yellow_on(Fsm *fsm)
{
	fsm->lamp = "YELLOW";
	std::printf("YELLOW on\n");
}

/*
 * put the light into Red and turn the red lamp on
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Red;
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
	if (event != Event::Tick)
		return;

	switch (fsm->state) {
	case State::Red:
		action_green_on(fsm);
		fsm->state = State::Green;
		break;
	case State::Green:
		action_yellow_on(fsm);
		fsm->state = State::Yellow;
		break;
	case State::Yellow:
		action_red_on(fsm);
		fsm->state = State::Red;
		break;
	}

	std::printf("%s --%s--> %s (lamp=%s)\n",
	    state_name(prev), event_name(event),
	    state_name(fsm->state), fsm->lamp);
}

int
main()
{
	Fsm light{};

	std::printf("=== traffic light demo ===\n");
	fsm_init(&light);
	std::printf("init: %s (lamp=%s)\n", state_name(light.state), light.lamp);

	fsm_dispatch(&light, Event::Tick);
	fsm_dispatch(&light, Event::Tick);
	fsm_dispatch(&light, Event::Tick);
	fsm_dispatch(&light, Event::Tick);

	return 0;
}
