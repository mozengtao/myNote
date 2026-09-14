# Makefile —— Firewall Rule Engine
#
# 依赖系统安装的 Lua 5.4 开发包（liblua5.4-dev / lua5.4-devel，视发行版而定）。
# 如果 pkg-config 里的包名不同，按需修改 LUA_PKG。

LUA_PKG ?= lua5.4

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -g $(shell pkg-config --cflags $(LUA_PKG))
LDFLAGS += $(shell pkg-config --libs $(LUA_PKG))

TARGET  := firewall
SRCS    := main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
