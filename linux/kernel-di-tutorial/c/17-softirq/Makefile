CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
PTHREAD = -pthread
BINS    = softirq_fixed_slots softirq_vs_hardirq_demo percpu_softirq_pending

.PHONY: all clean run

all: $(BINS)

softirq_fixed_slots: softirq_fixed_slots.c
	$(CC) $(CFLAGS) -o $@ $<

softirq_vs_hardirq_demo: softirq_vs_hardirq_demo.c
	$(CC) $(CFLAGS) -o $@ $<

percpu_softirq_pending: percpu_softirq_pending.c
	$(CC) $(CFLAGS) $(PTHREAD) -o $@ $<

run: all
	@echo "==== softirq_fixed_slots ===="; ./softirq_fixed_slots
	@echo; echo "==== softirq_vs_hardirq_demo ===="; ./softirq_vs_hardirq_demo
	@echo; echo "==== percpu_softirq_pending ===="; ./percpu_softirq_pending

clean:
	rm -f $(BINS)
