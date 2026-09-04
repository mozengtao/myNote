#
# Makefile - C 语言共享内存 Seqlock 工程构建系统
#

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror -pedantic -std=c11 -pthread -lrt -D_GNU_SOURCE
INCLUDES = -I./include

BIN_DIR = bin
SRC_DIR = src

TARGETS = \
	$(BIN_DIR)/01_simplest_seqlock \
	$(BIN_DIR)/02_multi_reader_stats \
	$(BIN_DIR)/03_posix_shm_writer \
	$(BIN_DIR)/03_posix_shm_reader \
	$(BIN_DIR)/04_dpdk_telemetry \
	$(BIN_DIR)/05_kernel_seqcount

.PHONY: all clean test

all: $(BIN_DIR) $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/01_simplest_seqlock: $(SRC_DIR)/01_simplest_seqlock.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/02_multi_reader_stats: $(SRC_DIR)/02_multi_reader_stats.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/03_posix_shm_writer: $(SRC_DIR)/03_posix_shm_writer.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/03_posix_shm_reader: $(SRC_DIR)/03_posix_shm_reader.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/04_dpdk_telemetry: $(SRC_DIR)/04_dpdk_telemetry.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/05_kernel_seqcount: $(SRC_DIR)/05_kernel_seqcount.c include/seqlock.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

test: all
	@chmod +x run_tests.sh
	@./run_tests.sh

clean:
	rm -rf $(BIN_DIR)
	@rm -f /dev/shm/shm_seqlock_demo
	@echo "清理完成。"
