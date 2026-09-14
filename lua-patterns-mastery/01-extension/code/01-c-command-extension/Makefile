# Makefile
#
# 用 pkg-config 探测 lua5.4 的头文件路径和链接参数。
# 如果当前环境没有 lua5.4.pc（例如系统只装了 lua5.1/liblua），
# 可以把下面这一行替换成对应版本：
#   LUA_PKG = lua5.3   或者   LUA_PKG = lua-5.4   或者   LUA_PKG = lua
# 也可以完全不用 pkg-config，手动指定：
#   CFLAGS += -I/usr/include/lua5.4
#   LDLIBS += -llua5.4
LUA_PKG := lua5.4

CC := cc
CFLAGS := -Wall -Wextra -std=c11 $(shell pkg-config --cflags $(LUA_PKG))
LDLIBS := $(shell pkg-config --libs $(LUA_PKG))

TARGET := cmdhost
SRC := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) extension.lua

clean:
	rm -f $(TARGET)

# 编译命令: make
# 运行命令: ./cmdhost
#
# 示例交互输出:
#
#   $ make
#   $ ./cmdhost
#   [C Host] 已注册命令: hello
#   [C Host] 已注册命令: add
#   [C Host] 已注册命令: boom
#   [C Host] 已注册命令: state
#   [extension.lua] 已加载，注册了 4 个命令：hello / add / boom / state
#   [C Host] 命令行已就绪，共有 4 个命令。输入 'quit' 退出。
#   > hello world
#   Hello, world! (来自 Lua 的问候)
#   > add 3 5
#   3 + 5 = 8
#   > add foo bar
#   [C Host] 命令 'add' 执行失败: extension.lua:24: add 需要两个数字参数，例如: add 3 5
#   > boom
#   [C Host] 命令 'boom' 执行失败: extension.lua:33: boom 命令故意抛出的错误：模拟脚本 bug
#   > state
#   state 命令被调用了 1 次
#   > state
#   state 命令被调用了 2 次
#   > nosuch
#   [C Host] 错误: 命令不存在: nosuch
#   > quit
#   [C Host] 已退出，Lua VM 已释放。
