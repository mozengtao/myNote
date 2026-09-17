GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := table_iteration_demo
SRCS := table_iteration_demo.c
include $(GUIDE_ROOT)/Makefile.rules

# 下面是 README「Failure」小节用到的反例（lua_next 循环里漏掉 lua_pop），
# 独立于 make run 默认目标，按需用 `make run-fail` 编译运行。
FAIL_TARGET := table_iteration_fail_demo
FAIL_OBJS := $(FAIL_TARGET:=.o)

$(FAIL_TARGET): $(FAIL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(FAIL_OBJS) $(LDFLAGS) $(LDLIBS)

run-fail: $(FAIL_TARGET)
	-./$(FAIL_TARGET)

clean-fail:
	rm -f $(FAIL_OBJS) $(FAIL_TARGET)

.PHONY: run-fail clean-fail
