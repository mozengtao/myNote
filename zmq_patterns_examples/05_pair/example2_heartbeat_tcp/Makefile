CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lzmq

TARGETS = node_a node_b

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
