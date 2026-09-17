# Makefile for chapters/24-failure-analysis/code/
#
# 本章 10 个 Failure 案例里，可以安全编译并观察真实运行结果的
# 反例程序集中在这里，每个反例是一个独立的小型宿主程序（自己的
# main()），互相不依赖，也不使用 Makefile.rules 的单 TARGET 约定
# （因为这里天然是"多个 TARGET"）。
#
# 用法：
#   make        # 编译全部反例程序
#   make run    # 依次运行全部反例程序，打印分隔线方便对照阅读
#   make clean

GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common

TARGETS := \
	failure01_wrong_stack_index \
	failure02_forgot_pop \
	failure03_wrong_return_count \
	failure04_wrong_return_position \
	failure06_lua_call_unprotected \
	failure07_userdata_wrong_metatable \
	failure09_stack_index_as_reference \
	failure10_stale_registry_ref

.PHONY: all run clean

all: $(TARGETS)

$(TARGETS): %: %.o
	$(CC) $(CFLAGS) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 注意：failure06（lua_call 无保护）和 failure10（调用已失效的
# handler）故意会以非 0 状态收尾（前者甚至是被信号中止），这正是
# 它们要演示的真实行为。用 `-` 前缀让 make 不因为某一个的非 0
# 退出码而中断，继续跑完剩下的反例。
run: all
	@for t in $(TARGETS); do \
		echo "============================================================" ; \
		echo "== $$t ==" ; \
		echo "============================================================" ; \
		./$$t; echo "(exit code: $$?)" ; \
		echo ; \
	done

clean:
	rm -f $(TARGETS) *.o core.* core
