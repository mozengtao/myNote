CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = file_system_type_registry mount_dispatch_demo multi_fs_module_sim

.PHONY: all clean run

all: $(BINS)

file_system_type_registry: file_system_type_registry.c
	$(CC) $(CFLAGS) -o $@ $<

mount_dispatch_demo: mount_dispatch_demo.c
	$(CC) $(CFLAGS) -o $@ $<

multi_fs_module_sim: multi_fs_module_sim.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

run: all
	@echo "==== file_system_type_registry ===="; ./file_system_type_registry
	@echo; echo "==== mount_dispatch_demo ===="; ./mount_dispatch_demo
	@echo; echo "==== multi_fs_module_sim ===="; ./multi_fs_module_sim

clean:
	rm -f $(BINS)
