-- game_config.lua
--
-- 游戏配置：两张表
--   player  -- 单层表，玩家基础属性
--   weapons -- "表的表"：每个 key 是武器名，value 是该武器的属性表
--
-- 为了体现"配置具备计算能力"，加入：
--   1. difficulty：由 Host 注入的只读全局变量（例如从存档/命令行读到的难度等级）
--   2. scale_by_difficulty()：一个本地函数，根据难度对数值做缩放，
--      被多个字段复用，体现"可复用"
--   3. weapons.bow：伤害值由函数计算得出，而不是写死的常量

-- 简化点：真实项目里 difficulty 可能来自存档文件或命令行参数，
-- 这里用 Host 注入的全局变量模拟，若 Host 未注入则退回默认值 1（普通难度）。
local difficulty = difficulty or 1

-- 根据难度等级对基础数值做缩放：难度越高，敌人/挑战相关数值收益越低。
-- 这是一个会被多处配置复用的"计算函数"，纯静态配置无法表达这种复用。
local function scale_by_difficulty(base)
    -- 难度 1 = 简单，2 = 普通，3 = 困难
    local factor = ({ [1] = 1.2, [2] = 1.0, [3] = 0.8 })[difficulty] or 1.0
    return base * factor
end

player = {
    speed = scale_by_difficulty(10),
    jump_height = 4,
    -- 条件分支：困难模式下生命值更低
    max_hp = (difficulty >= 3) and 80 or 100,
}

weapons = {
    sword = {
        damage = 20,
        speed = 1.0,
    },
    bow = {
        -- 数值由函数计算得出：基础伤害 15，随难度缩放
        damage = scale_by_difficulty(15),
        speed = 0.6,
    },
    staff = {
        damage = 12,
        speed = 0.8,
        mana_cost = 10,
    },
}

print(string.format(
    "[game_config.lua] 已加载配置（难度等级 %d），player.speed=%.1f, weapons.bow.damage=%.1f",
    difficulty, player.speed, weapons.bow.damage
))
