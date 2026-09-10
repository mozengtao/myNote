CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
PTHREAD = -pthread
BINS    = irq_action_chain_sim threaded_irq_demo irq_free_and_race_demo

.PHONY: all clean run

all: $(BINS)

irq_action_chain_sim: irq_action_chain_sim.c
	$(CC) $(CFLAGS) -o $@ $<

threaded_irq_demo: threaded_irq_demo.c
	$(CC) $(CFLAGS) $(PTHREAD) -o $@ $<

irq_free_and_race_demo: irq_free_and_race_demo.c
	$(CC) $(CFLAGS) $(PTHREAD) -o $@ $<

run: all
	@echo "==== irq_action_chain_sim ===="; ./irq_action_chain_sim
	@echo; echo "==== threaded_irq_demo ===="; ./threaded_irq_demo
	@echo; echo "==== irq_free_and_race_demo ===="; ./irq_free_and_race_demo

clean:
	rm -f $(BINS)
