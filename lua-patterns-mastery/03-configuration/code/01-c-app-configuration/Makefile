# Makefile — C Application + Lua Configuration 示例
#
# 依次尝试 pkg-config 已知的 lua5.4 / lua5.3 / lua5.1 / lua，
# 兼容不同发行版把 Lua 开发包命名成不同 pkg-config 名字的情况。

CC      ?= cc
CFLAGS  ?= -Wall -Wextra -std=c99 -g

LUA_PKG := $(shell for p in lua5.4 lua5.3 lua5.1 lua; do \
              pkg-config --exists $$p 2>/dev/null && echo $$p && break; \
           done)

ifeq ($(LUA_PKG),)
$(error 找不到任何 Lua 开发包，请安装 liblua5.4-dev（或对应发行版的 lua-devel）后重试)
endif

CFLAGS  += $(shell pkg-config --cflags $(LUA_PKG))
LDFLAGS += $(shell pkg-config --libs $(LUA_PKG))

TARGET := app_config_demo
SRC    := main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) config.lua

clean:
	rm -f $(TARGET)
