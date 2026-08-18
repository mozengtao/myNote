#include <stdio.h>

#define ACTOR_COUNT 2
#define MAILBOX_CAP 8

typedef enum {
	STATE_CLOSED = 0,
	STATE_SYN_SENT,
	STATE_ESTABLISHED,
	STATE_ERROR
} State;

typedef enum {
	EVENT_CONNECT = 0,
	EVENT_SYNACK,
	EVENT_DATA,
	EVENT_FAIL,
	EVENT_RESTART
} Event;

typedef struct {
	State state;
} Fsm;

typedef struct {
	Event items[MAILBOX_CAP];
	int head;
	int count;
} Mailbox;

typedef struct {
	const char *name;
	Fsm fsm;
	Mailbox mailbox;
} Actor;

static Actor actors[ACTOR_COUNT];

/*
 * return a printable name for a connection teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_SYN_SENT)
		return "SYN_SENT";
	if (state == STATE_ESTABLISHED)
		return "ESTABLISHED";
	if (state == STATE_ERROR)
		return "ERROR";
	return "CLOSED";
}

/*
 * return a printable name for a mailbox message
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_SYNACK)
		return "SYNACK";
	if (event == EVENT_DATA)
		return "DATA";
	if (event == EVENT_FAIL)
		return "FAIL";
	if (event == EVENT_RESTART)
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

	if (to->mailbox.count >= MAILBOX_CAP) {
		printf("mailbox full, drop %s -> %s\n", event_name(event),
		    to->name);
		return;
	}
	tail = (to->mailbox.head + to->mailbox.count) % MAILBOX_CAP;
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
	m->head = (m->head + 1) % MAILBOX_CAP;
	m->count--;
	return 1;
}

/*
 * put one connection FSM into CLOSED
 */
static void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_CLOSED;
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
	if (prev == STATE_CLOSED && event == EVENT_CONNECT) {
		printf("action: send SYN\n");
		next = STATE_SYN_SENT;
	} else if (prev == STATE_SYN_SENT && event == EVENT_SYNACK) {
		printf("action: send ACK\n");
		next = STATE_ESTABLISHED;
	} else if (prev == STATE_ESTABLISHED && event == EVENT_DATA) {
		printf("action: recv DATA\n");
		next = STATE_ESTABLISHED;
	} else if ((prev == STATE_SYN_SENT || prev == STATE_ESTABLISHED) &&
	    event == EVENT_FAIL) {
		printf("action: connection failed\n");
		next = STATE_ERROR;
	} else if (prev == STATE_ERROR && event == EVENT_RESTART) {
		printf("action: reset connection\n");
		next = STATE_CLOSED;
	} else {
		printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	fsm->state = next;
	printf("%s --%s--> %s\n", state_name(prev), event_name(event),
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
	printf("supervisor: restart %s (enqueue RESTART)\n", actor->name);
	actor_send(actor, EVENT_RESTART);
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
		for (i = 0; i < ACTOR_COUNT; i++) {
			if (!mailbox_dequeue(&actors[i].mailbox, &event))
				continue;
			progress = 1;
			printf("sched: %s dequeue %s\n", actors[i].name,
			    event_name(event));
			fsm_dispatch(&actors[i].fsm, event);
			if (actors[i].fsm.state == STATE_ERROR)
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
	printf("snapshot %s: %s=%s %s=%s\n", wave,
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

	printf("=== actor + mailbox teaching runtime ===\n");
	snapshot("init");

	printf("-- wave: CONNECT both --\n");
	actor_send(a, EVENT_CONNECT);
	actor_send(b, EVENT_CONNECT);
	scheduler_run();
	snapshot("after CONNECT");

	printf("-- wave: SYNACK both --\n");
	actor_send(a, EVENT_SYNACK);
	actor_send(b, EVENT_SYNACK);
	scheduler_run();
	snapshot("after SYNACK");

	printf("-- wave: DATA both --\n");
	actor_send(a, EVENT_DATA);
	actor_send(b, EVENT_DATA);
	scheduler_run();
	snapshot("after DATA");

	printf("-- wave: FAIL A and DATA B (isolation + recovery) --\n");
	actor_send(a, EVENT_FAIL);
	actor_send(b, EVENT_DATA);
	scheduler_run();
	snapshot("after FAIL A");

	printf("-- wave: A reconnects; B still ESTABLISHED --\n");
	actor_send(a, EVENT_CONNECT);
	actor_send(a, EVENT_SYNACK);
	actor_send(b, EVENT_DATA);
	scheduler_run();
	snapshot("after A recovered");

	return 0;
}
