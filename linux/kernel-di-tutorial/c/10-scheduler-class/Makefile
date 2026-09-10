CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = sched_class_chain pluggable_scheduler_policies scheduler_enqueue_dequeue

.PHONY: all clean run

all: $(BINS)

sched_class_chain: sched_class_chain.c
	$(CC) $(CFLAGS) -o $@ $<

pluggable_scheduler_policies: pluggable_scheduler_policies.c
	$(CC) $(CFLAGS) -o $@ $<

scheduler_enqueue_dequeue: scheduler_enqueue_dequeue.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== sched_class_chain ===="; ./sched_class_chain
	@echo; echo "==== pluggable_scheduler_policies ===="; ./pluggable_scheduler_policies
	@echo; echo "==== scheduler_enqueue_dequeue ===="; ./scheduler_enqueue_dequeue

clean:
	rm -f $(BINS)
