#include <cstdio>

enum class State {
	S0,
	S1,
	S10,
	S101
};

enum class Event {
	Bit0,
	Bit1
};

struct Fsm {
	State state;
};

/*
 * return a printable name for a detector state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case State::S0:
		return "S0";
	case State::S1:
		return "S1";
	case State::S10:
		return "S10";
	case State::S101:
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
	case Event::Bit0:
		return "BIT0";
	case Event::Bit1:
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
	fsm->state = State::S0;
}

/*
 * dispatch one bit; take an arrow only when its bit guard holds
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	switch (fsm->state) {
	case State::S0:
		if (event == Event::Bit1) {	/* guard: bit == 1 */
			fsm->state = State::S1;
		}
		break;
	case State::S1:
		if (event == Event::Bit0) {	/* guard: bit == 0 */
			fsm->state = State::S10;
		}
		/* guard bit==1: stay S1, last 1 still a prefix */
		break;
	case State::S10:
		if (event == Event::Bit1) {	/* guard: bit == 1 */
			std::printf("detected 101\n");
			fsm->state = State::S101;
		} else {			/* guard: bit == 0 */
			fsm->state = State::S0;
		}
		break;
	case State::S101:
		if (event == Event::Bit1) {	/* guard: bit == 1; overlap */
			fsm->state = State::S1;
		} else {			/* guard: bit == 0; overlap */
			fsm->state = State::S10;
		}
		break;
	}
}

int
main()
{
	static const Event bits[] = {
		Event::Bit1,
		Event::Bit0,
		Event::Bit1,
		Event::Bit0,
		Event::Bit1,
		Event::Bit1
	};
	Fsm detector{};

	fsm_init(&detector);
	std::printf("init: %s\n", state_name(detector.state));

	for (Event bit : bits) {
		fsm_dispatch(&detector, bit);
		std::printf("after %s: %s\n", event_name(bit),
		    state_name(detector.state));
	}

	return 0;
}
