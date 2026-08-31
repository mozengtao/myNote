CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

BINS := hugepage_demo

.PHONY: all clean run

all: $(BINS)

hugepage_demo: hugepage_demo.c ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ hugepage_demo.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
