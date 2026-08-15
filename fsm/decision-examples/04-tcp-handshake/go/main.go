package main

import (
	"fmt"
	"strings"
)

// ---------------------------------------------------------------------------
// 1. Types
// ---------------------------------------------------------------------------

type State int
type Event int
type Action int

const (
	StateClosed State = iota
	StateListen
	StateSynSent
	StateSynRcvd
	StateEstablished
)

const (
	EventPassiveOpen Event = iota
	EventActiveOpen
	EventSyn
	EventSynAck
	EventAck
)

const (
	ActionSendSyn Action = iota
	ActionSendSynAck
	ActionSendAck
)

func (s State) String() string {
	return [...]string{"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED"}[s]
}

func (e Event) String() string {
	return [...]string{"PASSIVE_OPEN", "ACTIVE_OPEN", "SYN", "SYNACK", "ACK"}[e]
}

func (a Action) String() string {
	return [...]string{"SEND_SYN", "SEND_SYNACK", "SEND_ACK"}[a]
}

type Decision struct {
	Accepted  bool
	NextState State
	Actions   []Action
}

// ---------------------------------------------------------------------------
// 2. Table  (State, Event) -> (NextState, ActionList)
// ---------------------------------------------------------------------------

type transKey struct {
	s State
	e Event
}

type transVal struct {
	next    State
	actions []Action
}

var table = map[transKey]transVal{
	{StateClosed, EventPassiveOpen}: {StateListen, []Action{}},
	{StateListen, EventSyn}:         {StateSynRcvd, []Action{ActionSendSynAck}},
	{StateSynRcvd, EventAck}:        {StateEstablished, []Action{}},
	{StateClosed, EventActiveOpen}:  {StateSynSent, []Action{ActionSendSyn}},
	{StateSynSent, EventSynAck}:     {StateEstablished, []Action{ActionSendAck}},
}

// ---------------------------------------------------------------------------
// 3. Engine  -- table lookup only, zero side effects
// ---------------------------------------------------------------------------

func fsmDecide(state State, event Event) Decision {
	hit, ok := table[transKey{state, event}]
	if !ok {
		return Decision{Accepted: false, NextState: state}
	}
	return Decision{Accepted: true, NextState: hit.next, Actions: hit.actions}
}

// ---------------------------------------------------------------------------
// 4. Workers  -- execution; workers do not know current state
// ---------------------------------------------------------------------------

func workerNic(action Action) {
	switch action {
	case ActionSendSyn:
		fmt.Println("[EXEC] nic -> SEND SYN")
	case ActionSendSynAck:
		fmt.Println("[EXEC] nic -> SEND SYN+ACK")
	case ActionSendAck:
		fmt.Println("[EXEC] nic -> SEND ACK")
	}
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, action := range actions {
		fmt.Printf("[DISPATCH] %s\n", action)
		workerNic(action)
	}
}

// ---------------------------------------------------------------------------
// 6. Apply  -- commit next state, then dispatch
// ---------------------------------------------------------------------------

func formatActions(actions []Action) string {
	names := make([]string, len(actions))
	for i, a := range actions {
		names[i] = a.String()
	}
	return strings.Join(names, ", ")
}

func apply(state State, event Event) State {
	fmt.Printf("[EVENT] %s | state=%s\n", event, state)
	d := fsmDecide(state, event)
	if !d.Accepted {
		fmt.Printf("[DECISION] REJECT %s + %s  (state unchanged)\n", state, event)
		return state
	}
	fmt.Printf("[DECISION] %s + %s -> %s  actions=[%s]\n",
		state, event, d.NextState, formatActions(d.Actions))
	dispatch(d.Actions)
	return d.NextState
}

func runScenario(title string, events []Event) {
	state := StateClosed
	fmt.Printf("=== scenario: %s ===\n", title)
	fmt.Printf("[INIT] state=%s\n\n", state)
	for _, event := range events {
		state = apply(state, event)
		fmt.Println()
	}
}

// ---------------------------------------------------------------------------
// 7. Main
// ---------------------------------------------------------------------------

func main() {
	runScenario("server", []Event{
		EventPassiveOpen, EventSyn, EventAck, EventSyn,
	})
	runScenario("client", []Event{
		EventActiveOpen, EventSynAck, EventAck,
	})
}
