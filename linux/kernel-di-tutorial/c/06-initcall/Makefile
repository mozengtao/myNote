CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = linker_section_ctor_sim module_registry_levels self_registering_tests

.PHONY: all clean run

all: $(BINS)

linker_section_ctor_sim: linker_section_ctor_sim.c
	$(CC) $(CFLAGS) -o $@ $<

module_registry_levels: module_registry_levels.c
	$(CC) $(CFLAGS) -o $@ $<

self_registering_tests: self_registering_tests.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== linker_section_ctor_sim ===="; ./linker_section_ctor_sim
	@echo; echo "==== module_registry_levels ===="; ./module_registry_levels
	@echo; echo "==== self_registering_tests ===="; ./self_registering_tests || true

clean:
	rm -f $(BINS)
