CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := init writer reader deleter verify

.PHONY: all clean run

all: $(BINS)

init: init.c hash_table.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ init.c $(LDLIBS)

writer: writer.c hash_table.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ writer.c $(LDLIBS)

reader: reader.c hash_table.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ reader.c $(LDLIBS)

deleter: deleter.c hash_table.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ deleter.c $(LDLIBS)

verify: verify.c hash_table.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ verify.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_hash_table
