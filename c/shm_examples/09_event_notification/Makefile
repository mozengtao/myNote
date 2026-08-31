# 递归构建 / 清理 09 下的三个子示例（eventfd / sem / futex）

SUBDIRS := eventfd sem futex

.PHONY: all clean run $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

run: all
	./run_demo.sh

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
