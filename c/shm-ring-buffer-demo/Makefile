CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -g -Iinclude
LDLIBS   := -lpthread -lrt

BUILD_DIR := build
BIN_WRITER := shm_writer
BIN_READER := shm_reader

CORE_SRC := src/shm_ring.c
CORE_OBJ := $(BUILD_DIR)/shm_ring.o

.PHONY: all clean run-writer run-reader

all: $(BIN_WRITER) $(BIN_READER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(CORE_OBJ): $(CORE_SRC) include/shm_ring.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_WRITER): src/writer_main.c $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_READER): src/reader_main.c $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# Convenience targets for a quick manual smoke test:
#   make run-writer   -> creates /demo_ring, pushes 20 messages, 200ms apart
#   make run-reader    -> attaches to /demo_ring and pops until Ctrl-C
run-writer: $(BIN_WRITER)
	./$(BIN_WRITER) /demo_ring 8 128 20 200

run-reader: $(BIN_READER)
	./$(BIN_READER) /demo_ring

clean:
	rm -rf $(BUILD_DIR) $(BIN_WRITER) $(BIN_READER)
	@echo "Note: this does not shm_unlink /demo_ring; if a demo run left it" \
	      "behind, remove it with: rm -f /dev/shm/demo_ring"
