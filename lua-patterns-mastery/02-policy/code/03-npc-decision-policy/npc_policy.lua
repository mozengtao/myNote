-- npc_policy.lua
--
-- 这是 Engine（npc_engine.c）在启动时加载的 Lua 决策脚本。
-- 它只定义一个全局函数：
--
--   decide_action(distance, hp, enemy_visible) -> "ATTACK" | "FLEE" | "PATROL"
--
-- Engine 已经知道"怎么执行"每一种动作（Mechanism：动画系统、
-- 寻路系统、巡逻路径系统），不知道"这一轮该选哪一种"（Decision），
-- 这个函数只回答后者。
--
-- 决策优先级（从高到低）：
--   1. 血量危险 -> 无论敌人是否可见，优先逃跑（保命优先于战斗）
--   2. 敌人可见且距离足够近 -> 攻击
--   3. 其余情况 -> 巡逻（包括"看到敌人但太远打不到"的情况）

local HP_DANGER_THRESHOLD = 20
local ATTACK_RANGE = 5.0

function decide_action(distance, hp, enemy_visible)
    if hp <= HP_DANGER_THRESHOLD then
        return "FLEE"
    end

    if enemy_visible and distance <= ATTACK_RANGE then
        return "ATTACK"
    end

    return "PATROL"
end

print("[npc_policy.lua] 已加载，NPC decision policy 就位")
