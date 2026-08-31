CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := config_writer worker

.PHONY: all clean run

all: $(BINS)

config_writer: config_writer.c shared_config.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ config_writer.c $(LDLIBS)

worker: worker.c shared_config.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ worker.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_shared_config
