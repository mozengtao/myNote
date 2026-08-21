CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lzmq

TARGETS = peer_a peer_b

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
