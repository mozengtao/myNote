CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = toy_bytecode_verifier pluggable_prog_type_ops hook_point_bpf_attach

.PHONY: all clean run

all: $(BINS)

toy_bytecode_verifier: toy_bytecode_verifier.c
	$(CC) $(CFLAGS) -o $@ $<

pluggable_prog_type_ops: pluggable_prog_type_ops.c
	$(CC) $(CFLAGS) -o $@ $<

hook_point_bpf_attach: hook_point_bpf_attach.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== toy_bytecode_verifier ===="; ./toy_bytecode_verifier
	@echo; echo "==== pluggable_prog_type_ops ===="; ./pluggable_prog_type_ops
	@echo; echo "==== hook_point_bpf_attach ===="; ./hook_point_bpf_attach

clean:
	rm -f $(BINS)
