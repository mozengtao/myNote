#include <stdio.h>

typedef enum {
	STATE_INIT = 0,
	STATE_SELECTING,
	STATE_REQUESTING,
	STATE_BOUND,
	STATE_RENEWING,
	STATE_COUNT
} State;

typedef enum {
	EVENT_DISCOVER_SENT = 0,
	EVENT_OFFER,
	EVENT_REQUEST_SENT,
	EVENT_ACK,
	EVENT_NAK,
	EVENT_T1,
	EVENT_T2,
	EVENT_LEASE_EXPIRED,
	EVENT_COUNT
} Event;

typedef struct {
	State state;
} Fsm;

typedef void (*ActionFn)(Fsm *fsm);

typedef struct {
	ActionFn action;
	State next;
	int defined;
} Cell;

/*
 * return a printable name for a DHCP teaching state
 */
static const char *
state_name(State state)
{
	if (state == STATE_SELECTING)
		return "SELECTING";
	if (state == STATE_REQUESTING)
		return "REQUESTING";
	if (state == STATE_BOUND)
		return "BOUND";
	if (state == STATE_RENEWING)
		return "RENEWING";
	return "INIT";
}

/*
 * return a printable name for a DHCP teaching event
 */
static const char *
event_name(Event event)
{
	if (event == EVENT_OFFER)
		return "OFFER";
	if (event == EVENT_REQUEST_SENT)
		return "REQUEST_SENT";
	if (event == EVENT_ACK)
		return "ACK";
	if (event == EVENT_NAK)
		return "NAK";
	if (event == EVENT_T1)
		return "T1";
	if (event == EVENT_T2)
		return "T2";
	if (event == EVENT_LEASE_EXPIRED)
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
	printf("action: send DISCOVER\n");
}

/*
 * client chose an OFFER and sends DHCPREQUEST
 */
static void
action_send_request(Fsm *fsm)
{
	(void)fsm;
	printf("action: send REQUEST\n");
}

/*
 * REQUEST is on the wire; still waiting for ACK
 */
static void
action_request_on_wire(Fsm *fsm)
{
	(void)fsm;
	printf("action: REQUEST on wire\n");
}

/*
 * lease is bound
 */
static void
action_bound(Fsm *fsm)
{
	(void)fsm;
	printf("action: bound\n");
}

/*
 * T1 fired; unicast renew to the original server
 */
static void
action_send_renew(Fsm *fsm)
{
	(void)fsm;
	printf("action: send renew REQUEST\n");
}

/*
 * renew ACK; keep the same lease
 */
static void
action_renewed(Fsm *fsm)
{
	(void)fsm;
	printf("action: renewed\n");
}

/*
 * server refused the request
 */
static void
action_nak(Fsm *fsm)
{
	(void)fsm;
	printf("action: NAK, restart\n");
}

/*
 * T2 would enter REBINDING; teaching path goes back to INIT
 */
static void
action_omit_rebinding(Fsm *fsm)
{
	(void)fsm;
	printf("action: T2, omit REBINDING, back to INIT\n");
}

/*
 * lease clock ran out
 */
static void
action_lease_dead(Fsm *fsm)
{
	(void)fsm;
	printf("action: lease expired\n");
}

static const Cell table[STATE_COUNT][EVENT_COUNT] = {
	[STATE_INIT][EVENT_DISCOVER_SENT] = {
		action_send_discover, STATE_SELECTING, 1
	},
	[STATE_SELECTING][EVENT_OFFER] = {
		action_send_request, STATE_REQUESTING, 1
	},
	[STATE_REQUESTING][EVENT_REQUEST_SENT] = {
		action_request_on_wire, STATE_REQUESTING, 1
	},
	[STATE_REQUESTING][EVENT_ACK] = {
		action_bound, STATE_BOUND, 1
	},
	[STATE_REQUESTING][EVENT_NAK] = {
		action_nak, STATE_INIT, 1
	},
	[STATE_BOUND][EVENT_T1] = {
		action_send_renew, STATE_RENEWING, 1
	},
	[STATE_BOUND][EVENT_LEASE_EXPIRED] = {
		action_lease_dead, STATE_INIT, 1
	},
	[STATE_RENEWING][EVENT_ACK] = {
		action_renewed, STATE_BOUND, 1
	},
	[STATE_RENEWING][EVENT_NAK] = {
		action_nak, STATE_INIT, 1
	},
	[STATE_RENEWING][EVENT_T2] = {
		action_omit_rebinding, STATE_INIT, 1
	},
	[STATE_RENEWING][EVENT_LEASE_EXPIRED] = {
		action_lease_dead, STATE_INIT, 1
	},
};

/*
 * put the teaching DHCP FSM into INIT
 */
void
fsm_init(Fsm *fsm)
{
	fsm->state = STATE_INIT;
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
	cell = &table[fsm->state][event];
	if (!cell->defined) {
		printf("ignore: %s + %s\n", state_name(prev),
		    event_name(event));
		return;
	}
	if (cell->action)
		cell->action(fsm);
	fsm->state = cell->next;
	printf("%s --%s--> %s\n", state_name(prev), event_name(event),
	    state_name(fsm->state));
}

int
main(void)
{
	Fsm dhcp;

	printf("=== dhcp teaching FSM ===\n");

	printf("-- acquire lease --\n");
	fsm_init(&dhcp);
	printf("init: %s\n", state_name(dhcp.state));
	fsm_dispatch(&dhcp, EVENT_DISCOVER_SENT);
	fsm_dispatch(&dhcp, EVENT_OFFER);
	fsm_dispatch(&dhcp, EVENT_REQUEST_SENT);
	fsm_dispatch(&dhcp, EVENT_ACK);

	printf("-- T1 renew then ACK --\n");
	fsm_dispatch(&dhcp, EVENT_T1);
	fsm_dispatch(&dhcp, EVENT_ACK);

	printf("-- NAK restarts --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, EVENT_DISCOVER_SENT);
	fsm_dispatch(&dhcp, EVENT_OFFER);
	fsm_dispatch(&dhcp, EVENT_NAK);

	printf("-- T2 omits REBINDING --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, EVENT_DISCOVER_SENT);
	fsm_dispatch(&dhcp, EVENT_OFFER);
	fsm_dispatch(&dhcp, EVENT_ACK);
	fsm_dispatch(&dhcp, EVENT_T1);
	fsm_dispatch(&dhcp, EVENT_T2);

	printf("-- lease expired from BOUND --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, EVENT_DISCOVER_SENT);
	fsm_dispatch(&dhcp, EVENT_OFFER);
	fsm_dispatch(&dhcp, EVENT_ACK);
	fsm_dispatch(&dhcp, EVENT_LEASE_EXPIRED);

	printf("-- illegal input is a missing table cell --\n");
	fsm_init(&dhcp);
	fsm_dispatch(&dhcp, EVENT_ACK);

	return 0;
}
