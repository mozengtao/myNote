CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = generic_bus_core multi_bus_types_demo uevent_hotplug_notify

.PHONY: all clean run

all: $(BINS)

generic_bus_core: generic_bus_core.c
	$(CC) $(CFLAGS) -o $@ $<

multi_bus_types_demo: multi_bus_types_demo.c
	$(CC) $(CFLAGS) -o $@ $<

uevent_hotplug_notify: uevent_hotplug_notify.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== generic_bus_core ===="; ./generic_bus_core
	@echo; echo "==== multi_bus_types_demo ===="; ./multi_bus_types_demo
	@echo; echo "==== uevent_hotplug_notify ===="; ./uevent_hotplug_notify

clean:
	rm -f $(BINS)
