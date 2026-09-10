CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = kmem_cache_ctor_demo pluggable_allocator_backend custom_arena_allocator

.PHONY: all clean run

all: $(BINS)

kmem_cache_ctor_demo: kmem_cache_ctor_demo.c
	$(CC) $(CFLAGS) -o $@ $<

pluggable_allocator_backend: pluggable_allocator_backend.c
	$(CC) $(CFLAGS) -o $@ $<

custom_arena_allocator: custom_arena_allocator.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== kmem_cache_ctor_demo ===="; ./kmem_cache_ctor_demo
	@echo; echo "==== pluggable_allocator_backend (slab) ===="; ./pluggable_allocator_backend
	@echo; echo "==== pluggable_allocator_backend (slub) ===="; ./pluggable_allocator_backend slub
	@echo; echo "==== custom_arena_allocator ===="; ./custom_arena_allocator

clean:
	rm -f $(BINS)
