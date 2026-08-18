#include <cstdio>

enum class State {
	Idle,
	HasCoin
};

enum class Event {
	Coin,
	Select,
	Refund
};

struct Fsm {
	State state;
};

/*
 * return a printable name for a vending state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case State::HasCoin:
		return "HAS_COIN";
	case State::Idle:
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
	case Event::Coin:
		return "COIN";
	case Event::Select:
		return "SELECT";
	case Event::Refund:
		return "REFUND";
	}
	return "?";
}

/*
 * put the machine into Idle
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Idle;
}

/*
 * dispatch one named input; ignore combinations with no arrow
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	switch (fsm->state) {
	case State::Idle:
		switch (event) {
		case Event::Coin:
			fsm->state = State::HasCoin;
			break;
		case Event::Select:
		case Event::Refund:
			break;
		}
		break;
	case State::HasCoin:
		switch (event) {
		case Event::Select:
			std::printf("dispense\n");
			fsm->state = State::Idle;
			break;
		case Event::Refund:
			std::printf("refund\n");
			fsm->state = State::Idle;
			break;
		case Event::Coin:
			break;
		}
		break;
	}
}

int
main()
{
	Fsm machine{};

	fsm_init(&machine);
	std::printf("init: %s\n", state_name(machine.state));

	fsm_dispatch(&machine, Event::Coin);
	std::printf("after %s: %s\n", event_name(Event::Coin),
	    state_name(machine.state));

	fsm_dispatch(&machine, Event::Select);
	std::printf("after %s: %s\n", event_name(Event::Select),
	    state_name(machine.state));

	fsm_dispatch(&machine, Event::Coin);
	std::printf("after %s: %s\n", event_name(Event::Coin),
	    state_name(machine.state));

	fsm_dispatch(&machine, Event::Refund);
	std::printf("after %s: %s\n", event_name(Event::Refund),
	    state_name(machine.state));

	fsm_dispatch(&machine, Event::Select);
	std::printf("after %s: %s\n", event_name(Event::Select),
	    state_name(machine.state));

	return 0;
}
