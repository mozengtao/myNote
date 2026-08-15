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
	StateCreated State = iota
	StatePaid
	StateShipped
	StateCancelled
)

const (
	EventPaySuccess Event = iota
	EventShip
	EventCancel
)

const (
	ActionDeductStock Action = iota
	ActionNotifyLogistics
	ActionAddPoints
	ActionSendSms
	ActionReleaseStock
)

func (s State) String() string {
	return [...]string{"CREATED", "PAID", "SHIPPED", "CANCELLED"}[s]
}

func (e Event) String() string {
	return [...]string{"PAY_SUCCESS", "SHIP", "CANCEL"}[e]
}

func (a Action) String() string {
	return [...]string{
		"DEDUCT_STOCK", "NOTIFY_LOGISTICS", "ADD_POINTS",
		"SEND_SMS", "RELEASE_STOCK",
	}[a]
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
	{StateCreated, EventPaySuccess}: {
		StatePaid,
		[]Action{
			ActionDeductStock,
			ActionNotifyLogistics,
			ActionAddPoints,
			ActionSendSms,
		},
	},
	{StatePaid, EventShip}:      {StateShipped, []Action{ActionSendSms}},
	{StateCreated, EventCancel}: {StateCancelled, []Action{ActionSendSms}},
	{StatePaid, EventCancel}: {
		StateCancelled,
		[]Action{ActionReleaseStock, ActionSendSms},
	},
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

func workerInventory(action Action) {
	switch action {
	case ActionDeductStock:
		fmt.Println("[EXEC] inventory -> DEDUCT")
	case ActionReleaseStock:
		fmt.Println("[EXEC] inventory -> RELEASE")
	}
}

func workerLogistics(action Action) {
	if action == ActionNotifyLogistics {
		fmt.Println("[EXEC] logistics -> CREATE_SHIPMENT")
	}
}

func workerPoints(action Action) {
	if action == ActionAddPoints {
		fmt.Println("[EXEC] points -> ADD")
	}
}

func workerSms(action Action) {
	if action == ActionSendSms {
		fmt.Println("[EXEC] sms -> SEND")
	}
}

// ---------------------------------------------------------------------------
// 5. Dispatch
// ---------------------------------------------------------------------------

func dispatch(actions []Action) {
	for _, action := range actions {
		fmt.Printf("[DISPATCH] %s\n", action)
		switch action {
		case ActionDeductStock, ActionReleaseStock:
			workerInventory(action)
		case ActionNotifyLogistics:
			workerLogistics(action)
		case ActionAddPoints:
			workerPoints(action)
		case ActionSendSms:
			workerSms(action)
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

func runScenario(title string, events []Event) {
	state := StateCreated
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
	runScenario("happy-path", []Event{
		EventShip, EventPaySuccess, EventShip, EventCancel,
	})
	runScenario("cancel-created", []Event{EventCancel})
	runScenario("cancel-paid", []Event{EventPaySuccess, EventCancel})
}
