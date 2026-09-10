CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = bus_driver_match_demo plugin_registry_hotplug multi_driver_probe

.PHONY: all clean run

all: $(BINS)

bus_driver_match_demo: bus_driver_match_demo.c
	$(CC) $(CFLAGS) -o $@ $<

plugin_registry_hotplug: plugin_registry_hotplug.c
	$(CC) $(CFLAGS) -o $@ $<

multi_driver_probe: multi_driver_probe.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== bus_driver_match_demo ===="; ./bus_driver_match_demo
	@echo; echo "==== plugin_registry_hotplug ===="; ./plugin_registry_hotplug
	@echo; echo "==== multi_driver_probe ===="; ./multi_driver_probe

clean:
	rm -f $(BINS)
