CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
LDLIBS  = -lm
BINS    = vtable_shape_polymorphism mini_vfs_filesystem strategy_pattern_ops

.PHONY: all clean run

all: $(BINS)

vtable_shape_polymorphism: vtable_shape_polymorphism.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

mini_vfs_filesystem: mini_vfs_filesystem.c
	$(CC) $(CFLAGS) -o $@ $<

strategy_pattern_ops: strategy_pattern_ops.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== vtable_shape_polymorphism ===="; ./vtable_shape_polymorphism
	@echo; echo "==== mini_vfs_filesystem ===="; ./mini_vfs_filesystem
	@echo; echo "==== strategy_pattern_ops ===="; ./strategy_pattern_ops

clean:
	rm -f $(BINS)
