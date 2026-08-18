#ifndef HSM_H
#define HSM_H

/*
 * Hierarchical state machine skeleton extracted from lesson 13.
 *
 * Lesson code stays self-contained: do not #include this header from
 * lessons/13-hsm. Copy the pattern, then reuse this file in your own
 * programs after you understand parent, entry, exit, and history.
 *
 * A state node has:
 *   name     - printable identity
 *   parent   - NULL for a top-level state
 *   entry    - runs when the node is entered (may be NULL)
 *   exit     - runs when the node is left (may be NULL)
 *   history  - pointer to a slot that stores the last direct child
 *
 * The current state of an Hsm is always a leaf. Composites exist so
 * that one transition can attach to a parent, and so that history
 * can remember which child was last active.
 *
 * External transition order (UML / Statechart):
 *   exit source up to LCA, then optional action, then enter down to target.
 * Internal transition:
 *   run an action, do not exit or reenter the current state.
 */

typedef void (*HsmAction)(void *ctx);

typedef struct HsmState {
	const char *name;
	const struct HsmState *parent;
	HsmAction entry;
	HsmAction exit;
	const struct HsmState **history;
} HsmState;

typedef struct {
	const HsmState *current;
	void *ctx;
} Hsm;

/*
 * enter the path from the root down to initial and set current
 */
void hsm_init(Hsm *hsm, const HsmState *initial, void *ctx);

/*
 * external transition: exit to LCA, then enter to target
 */
void hsm_transition(Hsm *hsm, const HsmState *target);

/*
 * external transition with a transition action between exit and enter
 */
void hsm_transition_act(Hsm *hsm, const HsmState *target, HsmAction action);

/*
 * internal transition: run action, stay in current, no entry/exit
 */
void hsm_internal(Hsm *hsm, HsmAction action);

/*
 * last direct child of a composite, or NULL if never visited
 */
const HsmState *hsm_shallow_history(const HsmState *composite);

/*
 * walk history slots down to a leaf, or NULL if none recorded
 */
const HsmState *hsm_deep_history(const HsmState *composite);

#endif
