CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = container_of_demo worker_thread_pool timer_context_demo

.PHONY: all clean run

all: $(BINS)

container_of_demo: container_of_demo.c
	$(CC) $(CFLAGS) -o $@ $<

worker_thread_pool: worker_thread_pool.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

timer_context_demo: timer_context_demo.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== container_of_demo ===="; ./container_of_demo
	@echo; echo "==== worker_thread_pool ===="; ./worker_thread_pool
	@echo; echo "==== timer_context_demo ===="; ./timer_context_demo

clean:
	rm -f $(BINS)
