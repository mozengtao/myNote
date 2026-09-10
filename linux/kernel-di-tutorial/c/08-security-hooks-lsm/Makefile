CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = lsm_hook_chain policy_injection_acl audit_hook_pipeline

.PHONY: all clean run

all: $(BINS)

lsm_hook_chain: lsm_hook_chain.c
	$(CC) $(CFLAGS) -o $@ $<

policy_injection_acl: policy_injection_acl.c
	$(CC) $(CFLAGS) -o $@ $<

audit_hook_pipeline: audit_hook_pipeline.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== lsm_hook_chain ===="; ./lsm_hook_chain
	@echo; echo "==== policy_injection_acl ===="; ./policy_injection_acl
	@echo; echo "==== audit_hook_pipeline ===="; ./audit_hook_pipeline

clean:
	rm -f $(BINS)
