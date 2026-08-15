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
	StateOpening
	StateOpen
	StateClosing
)

const (
	EventPerson Event = iota
	EventTimeout
)

const (
	ActionMotorOpen Action = iota
	ActionMotorClose
	ActionLatchOpen
	ActionLatchClosed
)

func (s State) String() string {
	return [...]string{"CLOSED", "OPENING", "OPEN", "CLOSING"}[s]
}

func (e Event) String() string {
	return [...]string{"PERSON", "TIMEOUT"}[e]
}

func (a Action) String() string {
	return [...]string{"MOTOR_OPEN", "MOTOR_CLOSE", "LATCH_OPEN", "LATCH_CLOSED"}[a]
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
	{StateClosed, EventPerson}:   {StateOpening, []Action{ActionMotorOpen}},
	{StateOpening, EventTimeout}: {StateOpen, []Action{ActionLatchOpen}},
	{StateOpen, EventPerson}:     {StateOpen, []Action{}},
	{StateOpen, EventTimeout}:    {StateClosing, []Action{ActionMotorClose}},
	{StateClosing, EventPerson}:  {StateOpening, []Action{ActionMotorOpen}},
	{StateClosing, EventTimeout}: {StateClosed, []Action{ActionLatchClosed}},
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

func workerMotor(action Action) {
	switch action {
	case ActionMotorOpen:
		fmt.Println("[EXEC] motor -> OPEN")
	case ActionMotorClose:
		fmt.Println("[EXEC] motor -> CLOSE")
	}
}

func workerLatch(action Action) {
	switch action {
	case ActionLatchOpen:
		fmt.Println("[EXEC] latch -> OPEN")
	case ActionLatchClosed:
		fmt.Println("[EXEC] latch -> CLOSED")
	}
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, action := range actions {
		fmt.Printf("[DISPATCH] %s\n", action)
		switch action {
		case ActionMotorOpen, ActionMotorClose:
			workerMotor(action)
		default:
			workerLatch(action)
		}
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

// ---------------------------------------------------------------------------
// 7. Main
// ---------------------------------------------------------------------------

func main() {
	state := StateClosed
	fmt.Printf("[INIT] state=%s\n\n", state)
	events := []Event{
		EventTimeout,
		EventPerson,
		EventTimeout,
		EventPerson,
		EventTimeout,
		EventPerson,
		EventTimeout,
		EventTimeout,
		EventTimeout,
	}
	for _, event := range events {
		state = apply(state, event)
		fmt.Println()
	}
}
