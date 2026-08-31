# 顶层 Makefile：递归构建 / 清理所有示例子目录

SUBDIRS := \
	01_ring_buffer_spsc \
	02_lockfree_queue_mpmc \
	03_object_pool \
	04_hash_table \
	05_pipeline \
	06_shared_config \
	07_snapshot \
	08_double_buffer \
	09_event_notification \
	10_hugepage \
	11_numa_aware \
	12_header_version_magic

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
	@rm -f /dev/shm/shm_examples_* 2>/dev/null || true
