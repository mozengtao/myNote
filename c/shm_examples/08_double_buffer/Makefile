CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := producer consumer

.PHONY: all clean run

all: $(BINS)

producer: producer.c double_buffer.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ producer.c $(LDLIBS)

consumer: consumer.c double_buffer.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ consumer.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_double_buffer
