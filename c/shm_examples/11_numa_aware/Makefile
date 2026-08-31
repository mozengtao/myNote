CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS  := -lrt -lpthread

# 自动探测 libnuma 是否可用（同时具备 numa.h 头文件和 -lnuma 库）：
# 用一段最小程序尝试编译+链接，成功则认为可用。本机环境未安装
# libnuma，这里会解析为 0，走 Makefile 里的 fallback 分支。
HAVE_LIBNUMA := $(shell printf '#include <numa.h>\nint main(void){return numa_available();}\n' | \
	$(CC) -xc - -lnuma -o /tmp/.numa_probe_$$$$ 2>/dev/null && \
	rm -f /tmp/.numa_probe_$$$$ && echo 1 || echo 0)

ifeq ($(HAVE_LIBNUMA),1)
CFLAGS += -DHAVE_LIBNUMA
LDLIBS += -lnuma
endif

BINS := numa_demo

.PHONY: all clean run

all: $(BINS)
	@if [ "$(HAVE_LIBNUMA)" = "1" ]; then \
		echo "[Makefile] libnuma 可用，已启用 -DHAVE_LIBNUMA"; \
	else \
		echo "[Makefile] 未检测到 libnuma，使用 malloc fallback 编译"; \
	fi

numa_demo: numa_demo.c ../common/shm_common.h
	$(CC) $(CFLAGS) -o $@ numa_demo.c $(LDLIBS)

run: all
	./run_demo.sh

clean:
	rm -f $(BINS)
