CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
SRCDIR  := examples
BINDIR  := bin

SRCS := $(wildcard $(SRCDIR)/*.c)
BINS := $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(SRCS))

.PHONY: all run clean

all: $(BINS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/%: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@for b in $(BINS); do \
		echo "== $$b =="; \
		./$$b || exit 1; \
		echo; \
	done

clean:
	rm -rf $(BINDIR)
