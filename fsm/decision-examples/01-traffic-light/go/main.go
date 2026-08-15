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
	StateRed State = iota
	StateGreen
	StateYellow
)

const (
	EventTick Event = iota
)

const (
	ActionLightRed Action = iota
	ActionLightGreen
	ActionLightYellow
)

func (s State) String() string {
	return [...]string{"RED", "GREEN", "YELLOW"}[s]
}

func (e Event) String() string {
	return [...]string{"TICK"}[e]
}

func (a Action) String() string {
	return [...]string{"LIGHT_RED", "LIGHT_GREEN", "LIGHT_YELLOW"}[a]
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
	{StateRed, EventTick}:    {StateGreen, []Action{ActionLightGreen}},
	{StateGreen, EventTick}:  {StateYellow, []Action{ActionLightYellow}},
	{StateYellow, EventTick}: {StateRed, []Action{ActionLightRed}},
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

func workerLamp(action Action) {
	switch action {
	case ActionLightRed:
		fmt.Println("[EXEC] lamp -> RED")
	case ActionLightGreen:
		fmt.Println("[EXEC] lamp -> GREEN")
	case ActionLightYellow:
		fmt.Println("[EXEC] lamp -> YELLOW")
	}
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, action := range actions {
		fmt.Printf("[DISPATCH] %s\n", action)
		workerLamp(action)
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
	state := StateRed
	fmt.Printf("[INIT] state=%s\n\n", state)
	for _, event := range []Event{EventTick, EventTick, EventTick, EventTick} {
		state = apply(state, event)
		fmt.Println()
	}
}
