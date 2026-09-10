CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = static_tracepoint_sim tracepoint_multi_probe tracepoint_enable_disable_runtime

.PHONY: all clean run

all: $(BINS)

static_tracepoint_sim: static_tracepoint_sim.c
	$(CC) $(CFLAGS) -o $@ $<

tracepoint_multi_probe: tracepoint_multi_probe.c
	$(CC) $(CFLAGS) -o $@ $<

tracepoint_enable_disable_runtime: tracepoint_enable_disable_runtime.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== static_tracepoint_sim ===="; ./static_tracepoint_sim
	@echo; echo "==== tracepoint_multi_probe ===="; ./tracepoint_multi_probe
	@echo; echo "==== tracepoint_enable_disable_runtime ===="; ./tracepoint_enable_disable_runtime

clean:
	rm -f $(BINS)
