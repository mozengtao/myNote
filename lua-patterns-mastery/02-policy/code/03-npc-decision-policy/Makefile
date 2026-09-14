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

TARGET := npc_engine
SRC := npc_engine.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) npc_policy.lua

clean:
	rm -f $(TARGET)

# 编译命令: make
# 运行命令: ./npc_engine
#
# 示例输出:
#
#   $ make
#   $ ./npc_engine
#   [npc_policy.lua] 已加载，NPC decision policy 就位
#   [Engine] NPC 决策引擎启动，共模拟 6 轮
#   ---
#   第 1 轮状态: distance=10.0 hp=100 enemy_visible=false
#   [Engine] Lua Policy 决策: PATROL
#   [Engine] 第 1 轮: 执行巡逻逻辑，沿预设路径点移动
#   ---
#   第 2 轮状态: distance=3.0 hp=80 enemy_visible=true
#   [Engine] Lua Policy 决策: ATTACK
#   [Engine] 第 2 轮: 执行攻击逻辑，播放 attack.anim，对玩家造成伤害
#   ---
#   第 3 轮状态: distance=2.0 hp=15 enemy_visible=true
#   [Engine] Lua Policy 决策: FLEE
#   [Engine] 第 3 轮: 执行逃跑逻辑，寻路到最近的掩体
#   ---
#   第 4 轮状态: distance=8.0 hp=60 enemy_visible=true
#   [Engine] Lua Policy 决策: PATROL
#   [Engine] 第 4 轮: 执行巡逻逻辑，沿预设路径点移动
#   ---
#   第 5 轮状态: distance=1.5 hp=50 enemy_visible=true
#   [Engine] Lua Policy 决策: ATTACK
#   [Engine] 第 5 轮: 执行攻击逻辑，播放 attack.anim，对玩家造成伤害
#   ---
#   第 6 轮状态: distance=4.0 hp=10 enemy_visible=false
#   [Engine] Lua Policy 决策: FLEE
#   [Engine] 第 6 轮: 执行逃跑逻辑，寻路到最近的掩体
#   ---
#   [Engine] 模拟结束，Lua VM 已释放。
