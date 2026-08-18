#include <cstdio>

enum class State {
	Off,
	On
};

struct Fsm {
	State state;
};

/*
 * return a printable name for a lamp state
 */
static const char *
state_name(State state)
{
	if (state == State::On)
		return "ON";
	return "OFF";
}

/*
 * put the lamp into Off
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Off;
}

/*
 * occupy On when the lamp is Off
 */
void
lamp_turn_on(Fsm *fsm)
{
	if (fsm->state == State::Off)
		fsm->state = State::On;
}

/*
 * occupy Off when the lamp is On
 */
void
lamp_turn_off(Fsm *fsm)
{
	if (fsm->state == State::On)
		fsm->state = State::Off;
}

int
main()
{
	Fsm lamp{};

	fsm_init(&lamp);
	std::printf("init: %s\n", state_name(lamp.state));

	lamp_turn_on(&lamp);
	std::printf("after turn on: %s\n", state_name(lamp.state));

	lamp_turn_on(&lamp);
	std::printf("turn on again: %s\n", state_name(lamp.state));

	lamp_turn_off(&lamp);
	std::printf("after turn off: %s\n", state_name(lamp.state));

	return 0;
}
