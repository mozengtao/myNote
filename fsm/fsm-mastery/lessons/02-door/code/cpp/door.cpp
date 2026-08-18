#include <cstdio>

enum class State {
	Closed,
	Open
};

struct Fsm {
	State state;
};

/*
 * return a printable name for a door state
 */
static const char *
state_name(State state)
{
	switch (state) {
	case State::Open:
		return "OPEN";
	case State::Closed:
		return "CLOSED";
	}
	return "?";
}

/*
 * put the door into Closed
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Closed;
}

/*
 * take the Closed -> Open transition
 */
void
door_open(Fsm *fsm)
{
	switch (fsm->state) {
	case State::Closed:
		fsm->state = State::Open;
		break;
	case State::Open:
		break;
	}
}

/*
 * take the Open -> Closed transition
 */
void
door_close(Fsm *fsm)
{
	switch (fsm->state) {
	case State::Open:
		fsm->state = State::Closed;
		break;
	case State::Closed:
		break;
	}
}

int
main()
{
	Fsm door{};

	fsm_init(&door);
	std::printf("init: %s\n", state_name(door.state));

	door_open(&door);
	std::printf("after open: %s\n", state_name(door.state));

	door_open(&door);
	std::printf("open again: %s\n", state_name(door.state));

	door_close(&door);
	std::printf("after close: %s\n", state_name(door.state));

	return 0;
}
