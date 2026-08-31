CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := capture decode display

.PHONY: all clean run

all: $(BINS)

capture: capture.c pipeline_ring.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ capture.c $(LDLIBS)

decode: decode.c pipeline_ring.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ decode.c $(LDLIBS)

display: display.c pipeline_ring.h ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ display.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
	rm -f /dev/shm/shm_examples_pipeline_stage1 /dev/shm/shm_examples_pipeline_stage2
