CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := init producer consumer verify

.PHONY: all clean run

all: $(BINS)

init: init.c mpmc_queue.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ init.c $(LDLIBS)

producer: producer.c mpmc_queue.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ producer.c $(LDLIBS)

consumer: consumer.c mpmc_queue.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ consumer.c $(LDLIBS)

verify: verify.c mpmc_queue.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ verify.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_mpmc_queue
