CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = plugin_system event_loop_reactor command_dispatcher

.PHONY: all clean run

all: $(BINS)

plugin_system: plugin_system.c
	$(CC) $(CFLAGS) -o $@ $<

event_loop_reactor: event_loop_reactor.c
	$(CC) $(CFLAGS) -o $@ $<

command_dispatcher: command_dispatcher.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== plugin_system ===="; ./plugin_system
	@echo; echo "==== event_loop_reactor ===="; ./event_loop_reactor
	@echo; echo "==== command_dispatcher ===="; ./command_dispatcher

clean:
	rm -f $(BINS)
