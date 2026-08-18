#include <cstdio>

enum {
	ActorCount = 2,
	MailboxCap = 8
};

enum class State {
	Closed = 0,
	SynSent,
	Established,
	Error
};

enum class Event {
	Connect = 0,
	Synack,
	Data,
	Fail,
	Restart
};

struct Fsm {
	State state;
};

struct Mailbox {
	Event items[MailboxCap];
	int head;
	int count;
};

struct Actor {
	const char *name;
	Fsm fsm;
	Mailbox mailbox;
};

static Actor actors[ActorCount];

/*
 * return a printable name for a connection teaching state
 */
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

/*
 * return a printable name for a mailbox message
 */
static const char *
event_name(Event event)
{
	if (event == Event::Synack)
		return "SYNACK";
	if (event == Event::Data)
		return "DATA";
	if (event == Event::Fail)
		return "FAIL";
	if (event == Event::Restart)
		return "RESTART";
	return "CONNECT";
}

/*
 * enqueue one message; drop if the teaching mailbox is full
 */
static void
actor_send(Actor *to, Event event)
{
	int tail;

	if (to->mailbox.count >= MailboxCap) {
		std::printf("mailbox full, drop %s -> %s\n", event_name(event),
		    to->name);
		return;
	}
	tail = (to->mailbox.head + to->mailbox.count) % MailboxCap;
	to->mailbox.items[tail] = event;
	to->mailbox.count++;
}

/*
 * dequeue one message; return 1 if a message was written
 */
static int
mailbox_dequeue(Mailbox *m, Event *event)
{
	if (m->count <= 0)
		return 0;
	*event = m->items[m->head];
	m->head = (m->head + 1) % MailboxCap;
	m->count--;
	return 1;
}

/*
 * put one connection FSM into CLOSED
 */
static void
fsm_init(Fsm *fsm)
{
	fsm->state = State::Closed;
}

/*
 * dispatch one mailbox message into this actor's FSM only
 */
static void
fsm_dispatch(Fsm *fsm, Event event)
{
	State prev;
	State next;

	prev = fsm->state;
	next = prev;
	if (prev == State::Closed && event == Event::Connect) {
		std::printf("action: send SYN\n");
		next = State::SynSent;
	} else if (prev == State::SynSent && event == Event::Synack) {
		std::printf("action: send ACK\n");
		next = State::Established;
	} else if (prev == State::Established && event == Event::Data) {
		std::printf("action: recv DATA\n");
		next = State::Established;
	} else if ((prev == State::SynSent || prev == State::Established) &&
	    event == Event::Fail) {
		std::printf("action: connection failed\n");
		next = State::Error;
	} else if (prev == State::Error && event == Event::Restart) {
		std::printf("action: reset connection\n");
		next = State::Closed;
	} else {
		std::printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	fsm->state = next;
	std::printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

/*
 * give one actor a name, empty mailbox, and CLOSED FSM
 */
static void
actor_init(Actor *actor, const char *name)
{
	actor->name = name;
	actor->mailbox.head = 0;
	actor->mailbox.count = 0;
	fsm_init(&actor->fsm);
}

/*
 * supervisor: restart THIS actor by message, never by writing state
 */
static void
supervisor_on_error(Actor *actor)
{
	std::printf("supervisor: restart %s (enqueue RESTART)\n", actor->name);
	actor_send(actor, Event::Restart);
}

/*
 * round-robin: one message per actor per turn, until all mailboxes empty
 */
static void
scheduler_run(void)
{
	int progress;
	int i;
	Event event;

	do {
		progress = 0;
		for (i = 0; i < ActorCount; i++) {
			if (!mailbox_dequeue(&actors[i].mailbox, &event))
				continue;
			progress = 1;
			std::printf("sched: %s dequeue %s\n", actors[i].name,
			    event_name(event));
			fsm_dispatch(&actors[i].fsm, event);
			if (actors[i].fsm.state == State::Error)
				supervisor_on_error(&actors[i]);
		}
	} while (progress);
}

/*
 * print both actors' isolated state fields
 */
static void
snapshot(const char *wave)
{
	std::printf("snapshot %s: %s=%s %s=%s\n", wave,
	    actors[0].name, state_name(actors[0].fsm.state),
	    actors[1].name, state_name(actors[1].fsm.state));
}

int
main(void)
{
	Actor *a;
	Actor *b;

	a = &actors[0];
	b = &actors[1];
	actor_init(a, "A");
	actor_init(b, "B");

	std::printf("=== actor + mailbox teaching runtime ===\n");
	snapshot("init");

	std::printf("-- wave: CONNECT both --\n");
	actor_send(a, Event::Connect);
	actor_send(b, Event::Connect);
	scheduler_run();
	snapshot("after CONNECT");

	std::printf("-- wave: SYNACK both --\n");
	actor_send(a, Event::Synack);
	actor_send(b, Event::Synack);
	scheduler_run();
	snapshot("after SYNACK");

	std::printf("-- wave: DATA both --\n");
	actor_send(a, Event::Data);
	actor_send(b, Event::Data);
	scheduler_run();
	snapshot("after DATA");

	std::printf("-- wave: FAIL A and DATA B (isolation + recovery) --\n");
	actor_send(a, Event::Fail);
	actor_send(b, Event::Data);
	scheduler_run();
	snapshot("after FAIL A");

	std::printf("-- wave: A reconnects; B still ESTABLISHED --\n");
	actor_send(a, Event::Connect);
	actor_send(a, Event::Synack);
	actor_send(b, Event::Data);
	scheduler_run();
	snapshot("after A recovered");

	return 0;
}
