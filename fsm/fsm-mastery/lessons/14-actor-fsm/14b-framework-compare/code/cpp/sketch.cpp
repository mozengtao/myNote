#include <cstdio>
#include <deque>

/*
 * Conceptual pipeline only. Not Akka, CAF, XState, or SObjectizer.
 *
 * message in -> mailbox -> dispatch -> FSM transition
 *
 * Akka:        ref.tell(msg)
 * CAF:         self->send(actor, msg)
 * XState:      actor.send(event)
 * SObjectizer: so_5::send<Msg>(mbox)
 * lesson 14a:  actor_send(to, event)
 */

enum class State {
	Closed,
	SynSent,
	Established,
	Error
};

enum class Event {
	Connect,
	Synack,
	Fail,
	Restart
};

struct Fsm {
	State state;
};

struct Actor {
	const char *name;
	Fsm fsm;
	std::deque<Event> mailbox;
};

static const char *
state_name(State state)
{
	if (state == State::SynSent)
		return "SYN_SENT";
	if (state == State::Established)
		return "ESTABLISHED";
	if (state == State::Error)
		return "ERROR";
	return "CLOSED";
}

static const char *
event_name(Event event)
{
	if (event == Event::Synack)
		return "SYNACK";
	if (event == Event::Fail)
		return "FAIL";
	if (event == Event::Restart)
		return "RESTART";
	return "CONNECT";
}

/*
 * message in -> mailbox
 */
static void
tell(Actor *actor, Event event)
{
	actor->mailbox.push_back(event);
	std::printf("tell %s %s\n", actor->name, event_name(event));
}

/*
 * dispatch -> FSM transition
 */
static void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;
	State next;
	int found;

	prev = fsm->state;
	found = 1;
	if (prev == State::Closed && event == Event::Connect)
		next = State::SynSent;
	else if (prev == State::SynSent && event == Event::Synack)
		next = State::Established;
	else if (prev == State::SynSent && event == Event::Fail)
		next = State::Error;
	else if (prev == State::Established && event == Event::Fail)
		next = State::Error;
	else if (prev == State::Error && event == Event::Restart)
		next = State::Closed;
	else
		found = 0;
	if (!found) {
		std::printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	fsm->state = next;
	std::printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

/*
 * mailbox -> dispatch, one message at a time
 */
static void
drain(Actor *actor)
{
	while (!actor->mailbox.empty()) {
		Event event = actor->mailbox.front();
		actor->mailbox.pop_front();
		std::printf("dispatch %s %s\n", actor->name, event_name(event));
		fsm_dispatch(&actor->fsm, event);
	}
}

int
main(void)
{
	Actor conn;

	conn.name = "conn";
	conn.fsm.state = State::Closed;

	std::printf("=== conceptual sequence: message -> mailbox -> dispatch -> FSM ===\n");
	tell(&conn, Event::Connect);
	tell(&conn, Event::Synack);
	drain(&conn);
	tell(&conn, Event::Fail);
	drain(&conn);
	tell(&conn, Event::Restart);
	drain(&conn);
	std::printf("final: %s\n", state_name(conn.fsm.state));
	return 0;
}
