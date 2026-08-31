CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := writer reader corrupt_tool

.PHONY: all clean run

all: $(BINS)

writer: writer.c shm_header.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ writer.c $(LDLIBS)

reader: reader.c shm_header.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ reader.c $(LDLIBS)

corrupt_tool: corrupt_tool.c shm_header.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ corrupt_tool.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_header_magic
