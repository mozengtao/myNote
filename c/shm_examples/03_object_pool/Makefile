CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := init worker verify

.PHONY: all clean run

all: $(BINS)

init: init.c object_pool.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ init.c $(LDLIBS)

worker: worker.c object_pool.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ worker.c $(LDLIBS)

verify: verify.c object_pool.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ verify.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_object_pool
