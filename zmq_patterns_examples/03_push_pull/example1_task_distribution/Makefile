CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lzmq

TARGETS = producer worker

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
