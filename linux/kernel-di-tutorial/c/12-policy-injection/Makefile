CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
LDLIBS  = -lm
BINS    = io_scheduler_policy congestion_control_policy sysctl_tunable_policy_switch

.PHONY: all clean run

all: $(BINS)

io_scheduler_policy: io_scheduler_policy.c
	$(CC) $(CFLAGS) -o $@ $<

congestion_control_policy: congestion_control_policy.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

sysctl_tunable_policy_switch: sysctl_tunable_policy_switch.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== io_scheduler_policy ===="; ./io_scheduler_policy
	@echo; echo "==== congestion_control_policy ===="; ./congestion_control_policy
	@echo; echo "==== sysctl_tunable_policy_switch ===="; ./sysctl_tunable_policy_switch || true

clean:
	rm -f $(BINS)
