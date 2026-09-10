CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = notifier_chain_impl pubsub_event_bus gui_event_broadcast

.PHONY: all clean run

all: $(BINS)

notifier_chain_impl: notifier_chain_impl.c
	$(CC) $(CFLAGS) -o $@ $<

pubsub_event_bus: pubsub_event_bus.c
	$(CC) $(CFLAGS) -o $@ $<

gui_event_broadcast: gui_event_broadcast.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== notifier_chain_impl ===="; ./notifier_chain_impl
	@echo; echo "==== pubsub_event_bus ===="; ./pubsub_event_bus
	@echo; echo "==== gui_event_broadcast ===="; ./gui_event_broadcast

clean:
	rm -f $(BINS)
