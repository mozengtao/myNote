CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt

all: core_process noncore_process

core_process: core_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

noncore_process: noncore_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f core_process noncore_process
	rm -f /dev/mqueue/ipc_demo_mq

.PHONY: all clean
