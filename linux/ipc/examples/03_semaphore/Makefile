CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

all: core_process noncore_process

core_process: core_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

noncore_process: noncore_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f core_process noncore_process
	rm -f /tmp/ipc_sem_data.bin
	rm -f /dev/shm/sem.ipc_demo_sem

.PHONY: all clean
