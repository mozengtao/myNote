CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O0 -g
SRCS    := $(wildcard [0-9]*.c)
BINS    := $(SRCS:.c=)

.PHONY: all run clean

all: $(BINS)

%: %.c kobj.h
	$(CC) $(CFLAGS) -o $@ $< -lm

run: all
	@for b in $(BINS); do \
		echo "========== $$b =========="; \
		./$$b; \
		echo; \
	done

clean:
	rm -f $(BINS)
