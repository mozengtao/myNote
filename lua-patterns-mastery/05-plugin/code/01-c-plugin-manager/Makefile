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

TARGET := pluginmgr
SRC := plugin_manager.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

# 注意：discover_plugins() 用 readdir(".") 扫描"当前工作目录"，
# 所以必须在 code/01-c-plugin-manager/ 目录下运行，
# 这样才能扫到同目录下的 plugin_a.lua / plugin_b.lua。
run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

# 编译命令: make
# 运行命令: ./pluginmgr
#
# 示例输出:
#
#   == Discovery ==
#   [Manager] 发现 2 个插件文件:
#     - plugin_a.lua
#     - plugin_b.lua
#
#   == Load + Register ==
#   [Host] <plugin_a> plugin_init 被调用，host 传入的标识: plugin_a
#   [Manager] 插件 plugin_a 加载并初始化成功，状态 -> RUNNING
#   [Host] <plugin_b> plugin_init 被调用，host 传入的标识: plugin_b，计数器已初始化为 0
#   [Manager] 插件 plugin_b 加载并初始化成功，状态 -> RUNNING
#
#   [Manager] 当前插件表 (2 个):
#     - plugin_a     state=RUNNING
#     - plugin_b     state=RUNNING
#
#   == Event ==
#
#   [Manager] ==> dispatch_event(name="tick", payload="1")
#   [Host] <plugin_a> 收到事件 on_event(tick, 1)，Plugin A 只打印日志，不维护状态
#   [Host] <plugin_b> 收到事件 on_event(tick, 1)，这是第 1 次收到事件
#
#   [Manager] ==> dispatch_event(name="boom", payload="oops")
#   [Host] <plugin_a> 收到事件 on_event(boom, oops)，Plugin A 只打印日志，不维护状态
#   [Manager] 插件 plugin_b 的 on_event 执行失败: plugin_b.lua:27: Plugin B 故意在 on_event 里抛出的错误，用于演示 Manager 的错误隔离
#
#   [Manager] ==> dispatch_event(name="tick", payload="2")
#   [Host] <plugin_a> 收到事件 on_event(tick, 2)，Plugin A 只打印日志，不维护状态
#   [Host] <plugin_b> 收到事件 on_event(tick, 2)，这是第 2 次收到事件
#
#   == Unload ==
#   [Host] <plugin_a> plugin_shutdown 被调用，Plugin A 即将卸载
#   [Manager] 插件 plugin_a 已卸载 (lua_State 已释放)
#   [Host] <plugin_b> plugin_shutdown 被调用，Plugin B 共处理了 2 次事件后卸载
#   [Manager] 插件 plugin_b 已卸载 (lua_State 已释放)
#
#   [Manager] 全部完成。
