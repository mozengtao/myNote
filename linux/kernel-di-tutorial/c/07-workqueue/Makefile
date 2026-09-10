CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = workqueue_from_scratch delayed_work_timer thread_pool_task_queue

.PHONY: all clean run

all: $(BINS)

workqueue_from_scratch: workqueue_from_scratch.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

delayed_work_timer: delayed_work_timer.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

thread_pool_task_queue: thread_pool_task_queue.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

run: all
	@echo "==== workqueue_from_scratch ===="; ./workqueue_from_scratch
	@echo; echo "==== delayed_work_timer ===="; ./delayed_work_timer
	@echo; echo "==== thread_pool_task_queue ===="; ./thread_pool_task_queue

clean:
	rm -f $(BINS)
