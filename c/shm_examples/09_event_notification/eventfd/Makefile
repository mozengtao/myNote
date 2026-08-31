CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := eventfd_demo

.PHONY: all clean run

all: $(BINS)

eventfd_demo: eventfd_demo.c ../../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ eventfd_demo.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_eventfd
