CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt -lpthread

all: core_process noncore_process

core_process: core_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

noncore_process: noncore_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f core_process noncore_process
	rm -f /dev/shm/ipc_demo_shm
	rm -f /dev/shm/sem.ipc_shm_empty /dev/shm/sem.ipc_shm_full

.PHONY: all clean
