GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := error_demo
SRCS := error_demo.c
include $(GUIDE_ROOT)/Makefile.rules

# 危险演示专用 target：故意用 lua_call（而不是 lua_pcall）调用一个会出错的
# Lua Function，且外层没有任何保护。会直接中止整个进程，仅用于教学观察，
# 不参与常规 `make run` 验收流程。
.PHONY: run-crash
run-crash: all
	./$(TARGET) --crash-lua-call
