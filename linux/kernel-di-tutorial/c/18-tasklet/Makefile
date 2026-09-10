CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = tasklet_schedule_sim tasklet_disable_enable_demo tasklet_vs_workqueue_compare

.PHONY: all clean run

all: $(BINS)

tasklet_schedule_sim: tasklet_schedule_sim.c
	$(CC) $(CFLAGS) -o $@ $<

tasklet_disable_enable_demo: tasklet_disable_enable_demo.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

tasklet_vs_workqueue_compare: tasklet_vs_workqueue_compare.c
	$(CC) $(CFLAGS) -pthread -o $@ $<

run: all
	@echo "==== tasklet_schedule_sim ===="; ./tasklet_schedule_sim
	@echo; echo "==== tasklet_disable_enable_demo ===="; ./tasklet_disable_enable_demo
	@echo; echo "==== tasklet_vs_workqueue_compare ===="; ./tasklet_vs_workqueue_compare

clean:
	rm -f $(BINS)
