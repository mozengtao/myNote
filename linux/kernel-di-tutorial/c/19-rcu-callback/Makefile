CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = call_rcu_grace_period_sim rcu_read_lock_replace_demo rcu_vs_refcount_compare

.PHONY: all clean run

all: $(BINS)

call_rcu_grace_period_sim: call_rcu_grace_period_sim.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

rcu_read_lock_replace_demo: rcu_read_lock_replace_demo.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

rcu_vs_refcount_compare: rcu_vs_refcount_compare.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

run: all
	@echo "==== call_rcu_grace_period_sim ===="; ./call_rcu_grace_period_sim
	@echo; echo "==== rcu_read_lock_replace_demo ===="; ./rcu_read_lock_replace_demo
	@echo; echo "==== rcu_vs_refcount_compare ===="; ./rcu_vs_refcount_compare

clean:
	rm -f $(BINS)
