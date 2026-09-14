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

TARGET := access_server
SRC := server.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) policy.lua

clean:
	rm -f $(TARGET)

# 编译命令: make
# 运行命令: ./access_server
#
# 示例输出:
#
#   $ make
#   $ ./access_server
#   [policy.lua] 已加载，access policy 规则就位
#   [C Host] Access Policy Server 已启动，共有 4 个模拟请求待处理
#   ---
#   [C Host] 收到请求 #1: user=alice resource=/api/reports
#   [C Host] ALLOW  user=alice  resource=/api/reports  -> 执行请求
#   ---
#   [C Host] 收到请求 #2: user=bob resource=/api/admin
#   [C Host] DENY   user=bob    resource=/api/admin    -> 拒绝, 原因: 资源 '/api/admin' 需要 admin 权限，当前用户 'bob' 不具备
#   ---
#   [C Host] 收到请求 #3: user=eve resource=/etc/passwd
#   [C Host] DENY   user=eve    resource=/etc/passwd   -> 拒绝, 原因: 用户 'eve' 在黑名单中
#   ---
#   [C Host] 收到请求 #4: user=admin resource=/api/admin
#   [C Host] ALLOW  user=admin  resource=/api/admin    -> 执行请求
#   ---
#   [C Host] 已处理完 4 个请求，Lua VM 已释放。
