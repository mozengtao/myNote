CC = gcc
CFLAGS = -Wall -Wextra -g

all: core_process noncore_process

core_process: core_process.c
	$(CC) $(CFLAGS) -o $@ $<

noncore_process: noncore_process.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f core_process noncore_process
	rm -f /tmp/ipc_data.bin /tmp/ipc_ready

.PHONY: all clean
