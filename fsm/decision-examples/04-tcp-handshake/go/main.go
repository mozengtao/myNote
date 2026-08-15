package main

import "fmt"

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type EventKind int
type ActionKind int

const (
	StateClosed State = iota
	StateListen
	StateSynSent
	StateSynRcvd
	StateEstablished
)

const (
	EventPassiveOpen EventKind = iota
	EventActiveOpen
	EventSyn
	EventSynAck
	EventAck
)

const (
	ActionNicSend ActionKind = iota
)

func (s State) String() string {
	return [...]string{"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED"}[s]
}

func (e EventKind) String() string {
	return [...]string{"PASSIVE_OPEN", "ACTIVE_OPEN", "SYN", "SYNACK", "ACK"}[e]
}

type Event struct {
	Kind EventKind
	Seq  uint
	Ack  uint
}

type Action struct {
	Kind  ActionKind
	Flags string
	Seq   uint
	Ack   uint
}

type Ctx struct {
	State     State
	LocalSeq  uint
	RemoteSeq uint
}

type Decision struct {
	Accepted  bool
	NextState State
	LocalSeq  uint
	RemoteSeq uint
	Actions   []Action
}

// ---------------------------------------------------------------------------
// 2. Table  (State, EventKind) -> (NextState, ActionKind list)
// ---------------------------------------------------------------------------

type transKey struct {
	s State
	e EventKind
}

type transVal struct {
	next  State
	kinds []ActionKind
}

var table = map[transKey]transVal{
	{StateClosed, EventPassiveOpen}: {StateListen, []ActionKind{}},
	{StateListen, EventSyn}:         {StateSynRcvd, []ActionKind{ActionNicSend}},
	{StateSynRcvd, EventAck}:        {StateEstablished, []ActionKind{}},
	{StateClosed, EventActiveOpen}:  {StateSynSent, []ActionKind{ActionNicSend}},
	{StateSynSent, EventSynAck}:     {StateEstablished, []ActionKind{ActionNicSend}},
}

func hasNicSend(kinds []ActionKind) bool {
	for _, k := range kinds {
		if k == ActionNicSend {
			return true
		}
	}
	return false
}

// ---------------------------------------------------------------------------
// 3. Engine  -- table lookup + bind seq/ack from Ctx and Event
// ---------------------------------------------------------------------------

func fsmDecide(ctx Ctx, ev Event) Decision {
	hit, ok := table[transKey{ctx.State, ev.Kind}]
	if !ok {
		return Decision{
			Accepted:  false,
			NextState: ctx.State,
			LocalSeq:  ctx.LocalSeq,
			RemoteSeq: ctx.RemoteSeq,
		}
	}
	d := Decision{
		Accepted:  true,
		NextState: hit.next,
		LocalSeq:  ctx.LocalSeq,
		RemoteSeq: ctx.RemoteSeq,
	}
	if !hasNicSend(hit.kinds) {
		return d
	}
	switch ev.Kind {
	case EventSyn:
		d.RemoteSeq = ev.Seq
		d.Actions = []Action{{
			Kind: ActionNicSend, Flags: "SYN+ACK",
			Seq: ctx.LocalSeq, Ack: ev.Seq + 1,
		}}
	case EventActiveOpen:
		d.Actions = []Action{{
			Kind: ActionNicSend, Flags: "SYN",
			Seq: ctx.LocalSeq, Ack: 0,
		}}
	case EventSynAck:
		d.RemoteSeq = ev.Seq
		d.LocalSeq = ctx.LocalSeq + 1
		d.Actions = []Action{{
			Kind: ActionNicSend, Flags: "ACK",
			Seq: d.LocalSeq, Ack: ev.Seq + 1,
		}}
	}
	return d
}

// ---------------------------------------------------------------------------
// 4. Workers  -- parameterized functions; no FSM state
// ---------------------------------------------------------------------------

func nicSend(flags string, seq, ack uint) {
	fmt.Printf("[EXEC] nic_send(%s, %d, %d)\n", flags, seq, ack)
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, a := range actions {
		fmt.Printf("[DISPATCH] NIC_SEND(flags=%s, seq=%d, ack=%d)\n",
			a.Flags, a.Seq, a.Ack)
		nicSend(a.Flags, a.Seq, a.Ack)
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit ctx (including seq), then dispatch
// ---------------------------------------------------------------------------

func formatEvent(ev Event) string {
	switch ev.Kind {
	case EventSyn, EventSynAck, EventAck:
		return fmt.Sprintf("%s(seq=%d, ack=%d)", ev.Kind, ev.Seq, ev.Ack)
	default:
		return ev.Kind.String()
	}
}

func apply(ctx *Ctx, ev Event) {
	fmt.Printf("[EVENT] %s | state=%s\n", formatEvent(ev), ctx.State)
	d := fsmDecide(*ctx, ev)
	if !d.Accepted {
		fmt.Printf("[DECISION] REJECT %s + %s  (state unchanged)\n",
			ctx.State, ev.Kind)
		return
	}
	fmt.Printf("[DECISION] %s + %s -> %s\n", ctx.State, ev.Kind, d.NextState)
	ctx.State = d.NextState
	ctx.LocalSeq = d.LocalSeq
	ctx.RemoteSeq = d.RemoteSeq
	dispatch(d.Actions)
}

func runScenario(title string, localSeq uint, events []Event) {
	ctx := Ctx{State: StateClosed, LocalSeq: localSeq, RemoteSeq: 0}
	fmt.Printf("=== scenario: %s ===\n", title)
	fmt.Printf("[INIT] state=%s local_seq=%d remote_seq=%d\n\n",
		ctx.State, ctx.LocalSeq, ctx.RemoteSeq)
	for _, ev := range events {
		apply(&ctx, ev)
		fmt.Println()
	}
}

// ---------------------------------------------------------------------------
// 7. Main
// ---------------------------------------------------------------------------

func main() {
	runScenario("server", 1000, []Event{
		{Kind: EventPassiveOpen},
		{Kind: EventSyn, Seq: 2000, Ack: 0},
		{Kind: EventAck, Seq: 2001, Ack: 1001},
		{Kind: EventSyn, Seq: 2002, Ack: 1001},
	})
	runScenario("client", 5000, []Event{
		{Kind: EventActiveOpen},
		{Kind: EventSynAck, Seq: 8000, Ack: 5001},
		{Kind: EventAck, Seq: 8001, Ack: 5001},
	})
}
