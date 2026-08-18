#include <cstdio>

enum class State {
	Init = 0,
	Selecting,
	Requesting,
	Bound,
	Renewing
};

enum {
	StateCount = 5
};

enum class Event {
	DiscoverSent = 0,
	Offer,
	RequestSent,
	Ack,
	Nak,
	T1,
	T2,
	LeaseExpired
};

enum {
	EventCount = 8
};

struct Fsm {
	State state;
};

using ActionFn = void (*)(Fsm *fsm);

struct Cell {
	ActionFn action;
	State next;
	int defined;
};

static Cell table[StateCount][EventCount];
static int table_ready;

/*
 * return a printable name for a DHCP teaching state
 */
static const char *
state_name(State state)
{
	if (state == State::Selecting)
		return "SELECTING";
	if (state == State::Requesting)
		return "REQUESTING";
	if (state == State::Bound)
		return "BOUND";
	if (state == State::Renewing)
		return "RENEWING";
	return "INIT";
}

/*
 * return a printable name for a DHCP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == Event::Offer)
		return "OFFER";
	if (event == Event::RequestSent)
		return "REQUEST_SENT";
	if (event == Event::Ack)
		return "ACK";
	if (event == Event::Nak)
		return "NAK";
	if (event == Event::T1)
		return "T1";
	if (event == Event::T2)
		return "T2";
	if (event == Event::LeaseExpired)
		return "LEASE_EXPIRED";
	return "DISCOVER_SENT";
}

/*
 * client broadcasts DHCPDISCOVER
 */
static void
action_send_discover(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send DISCOVER\n");
}

/*
 * client chose an OFFER and sends DHCPREQUEST
 */
static void
action_send_request(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send REQUEST\n");
}

/*
 * REQUEST is on the wire; still waiting for ACK
 */
static void
action_request_on_wire(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: REQUEST on wire\n");
}

/*
 * lease is bound
 */
static void
action_bound(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: bound\n");
}

/*
 * T1 fired; unicast renew to the original server
 */
static void
action_send_renew(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: send renew REQUEST\n");
}

/*
 * renew ACK; keep the same lease
 */
static void
action_renewed(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: renewed\n");
}

/*
 * server refused the request
 */
static void
action_nak(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: NAK, restart\n");
}

/*
 * T2 would enter REBINDING; teaching path goes back to INIT
 */
static void
action_omit_rebinding(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: T2, omit REBINDING, back to INIT\n");
}

/*
 * lease clock ran out
 */
static void
action_lease_dead(Fsm *fsm)
{
	(void)fsm;
	std::printf("action: lease expired\n");
}

/*
 * install one table cell
 */
static void
table_set(State state, Event event, ActionFn action, State next)
{
	table[static_cast<int>(state)][static_cast<int>(event)] = {
		action, next, 1
	};
}

/*
 * fill the sparse transition table once
 */
static void
ensure_table(void)
{
	if (table_ready)
		return;
	table_set(State::Init, Event::DiscoverSent, action_send_discover,
	    State::Selecting);
	table_set(State::Selecting, Event::Offer, action_send_request,
	    State::Requesting);
	table_set(State::Requesting, Event::RequestSent, action_request_on_wire,
	    State::Requesting);
	table_set(State::Requesting, Event::Ack, action_bound, State::Bound);
	table_set(State::Requesting, Event::Nak, action_nak, State::Init);
	table_set(State::Bound, Event::T1, action_send_renew, State::Renewing);
	table_set(State::Bound, Event::LeaseExpired, action_lease_dead,
	    State::Init);
	table_set(State::Renewing, Event::Ack, action_renewed, State::Bound);
	table_set(State::Renewing, Event::Nak, action_nak, State::Init);
	table_set(State::Renewing, Event::T2, action_omit_rebinding, State::Init);
	table_set(State::Renewing, Event::LeaseExpired, action_lease_dead,
	    State::Init);
	table_ready = 1;
}

/*
 * put the teaching DHCP FSM into INIT
 */
void
fsm_init(Fsm *fsm)
{
	ensure_table();
	fsm->state = State::Init;
}

/*
 * dispatch one event by table lookup: table[state][event]
 */
void
fsm_dispatch(Fsm *fsm, Event event)
{
	const Cell *cell;
	State prev;

	prev = fsm->state;
	cell = &table[static_cast<int>(fsm->state)][static_cast<int>(event)];
	if (!cell->defined) {
		std::printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	std::printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

int
main(void)
{
	Fsm dhcp;

	std::printf("=== dhcp teaching FSM ===\n");

	std::printf("-- acquire lease --\n");
	fsm_init(&dhcp);
	std::printf("init: %s\n", state_name(dhcp.state));
	fsm_dispatch(&dhcp, Event::DiscoverSent);
	fsm_dispatch(&dhcp, Event::Offer);
	fsm_dispatch(&dhcp, Event::RequestSent);
	fsm_dispatch(&dhcp, Event::Ack);

	std::printf("-- T1 renew then ACK --\n");
	fsm_dispatch(&dhcp, Event::T1);
	fsm_dispatch(&dhcp, Event::Ack);

	std::printf("-- NAK restarts --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, Event::DiscoverSent);
	fsm_dispatch(&dhcp, Event::Offer);
	fsm_dispatch(&dhcp, Event::Nak);

	std::printf("-- T2 omits REBINDING --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, Event::DiscoverSent);
	fsm_dispatch(&dhcp, Event::Offer);
	fsm_dispatch(&dhcp, Event::Ack);
	fsm_dispatch(&dhcp, Event::T1);
	fsm_dispatch(&dhcp, Event::T2);

	std::printf("-- lease expired from BOUND --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, Event::DiscoverSent);
	fsm_dispatch(&dhcp, Event::Offer);
	fsm_dispatch(&dhcp, Event::Ack);
	fsm_dispatch(&dhcp, Event::LeaseExpired);

	std::printf("-- illegal input is a missing table cell --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, Event::Ack);

	return 0;
}
