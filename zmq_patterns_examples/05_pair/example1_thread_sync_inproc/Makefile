CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lzmq -lpthread

TARGETS = main

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
