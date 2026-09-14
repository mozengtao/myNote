# 03 · Game Configuration

代码目录：[`code/03-game-configuration/`](code/03-game-configuration/)（`main.c` / `game_config.lua` / `Makefile`）

## 1. Problem（问题定义）

一个用 C 写的游戏引擎需要在启动时读取大量数值型参数：玩家移动速度、跳跃高度、生命值上限，以及每种武器的伤害/攻速/耗蓝。这些参数天然是**结构化、有嵌套关系**的（一个 `weapons` 里有多把武器，每把武器又是一组独立字段），而且策划经常需要"同一份数值表，在不同难度下自动缩放"，如果用纯数据格式（比如 CSV、JSON），要么得为每个难度维护一份完整拷贝，要么得在引擎代码里写死缩放公式——改一次数值平衡就要重新编译整个游戏。

## 2. Why Lua（为什么用 Lua）

游戏数值配置是 Lua Configuration 最常见的落地场景之一，原因是它同时需要：

- **表达嵌套结构**：`weapons.sword.damage`、`weapons.bow.speed`——Lua table 天然支持任意深度嵌套，比大多数静态配置格式更自然；
- **复用计算逻辑**：`scale_by_difficulty()` 这样的函数可以被 `player.speed`、`weapons.bow.damage` 等多处复用，策划改一次缩放公式，所有引用的地方全部生效；
- **条件化**：`max_hp` 在困难模式下直接用一行 `(difficulty >= 3) and 80 or 100` 表达，不需要多维表格。

## 3. Architecture（架构）

```text
main.c (Game Host)
 |
 +-- difficulty              Config API：Host 注入的只读全局变量
 |
 +-- Lua VM
       |
       v
  game_config.lua
       |
       v
 PlayerConfig (C struct)
 WeaponConfig[] (C struct 数组)
       |
       v
   init_game() 初始化游戏
```

## 4. Host responsibilities（Host 的职责）

- 创建/销毁 `lua_State`；
- 注入只读全局变量 `difficulty`（模拟"来自存档/命令行的难度等级"）；
- 加载并执行 `game_config.lua`，处理文件缺失/语法错误/运行时报错；
- 用 `lua_getfield` 读取单层表 `player`；
- 用 `lua_pushnil` + `lua_next` 遍历"表的表" `weapons`，把每个子表转换成 `WeaponConfig` 数组的一项；
- 转换完成后 `lua_close`，用纯 C 数据初始化游戏。

## 5. Lua responsibilities（Lua 的职责）

- 声明 `player` 和 `weapons` 两张全局表；
- 定义并复用一个纯函数 `scale_by_difficulty()`，不产生任何副作用，只是"用于计算的工具函数"；
- 引用 Host 注入的 `difficulty` 做条件判断和缩放计算；
- **不**调用任何会修改 Host 状态的函数——这是它和"游戏逻辑脚本"（Behavior，属于 Extension/Plugin 的范畴）的关键区别。

## 6. Lifecycle（生命周期）

```text
main() 开始
 |
 v
luaL_newstate + luaL_openlibs
 |
 v
lua_setglobal(difficulty) ------------- 注入 Config API
 |
 v
luaL_loadfile(game_config.lua) -------- 失败 -> 报错退出
 |
 v
lua_pcall(0, MULTRET, 0) -------------- 失败 -> 报错退出
 |
 v
load_player_config() ------------------ 读 player 表，失败 -> 报错退出
 |
 v
load_weapons_config() ------------------ 遍历 weapons 表，失败/为空 -> 报错退出
 |
 v
lua_close ------------------------------ 配置已转成纯 C 数据
 |
 v
init_game() ----------------------------- 用纯 C 数据初始化游戏
```

## 7. Data Flow（C -> Lua）

```text
load game_config.lua
 |
 v
read player table (单层表)
 |
 v
read weapons table (表的表，lua_next 遍历)
 |
 v
convert to C struct / struct array
 |
 v
initialize game
```

`weapons` 的读取方式和示例 1 的 `server` 不同：`server` 的字段名是提前已知的（`port`/`workers`），可以逐个 `lua_getfield`；而 `weapons` 里有哪些武器是配置作者决定的、Host 事先不知道，因此必须用 `lua_pushnil` + `lua_next` 做通用遍历，对每个 key/value 对再各自用 `lua_getfield` 读取子字段。

## 8. Control Flow（Lua -> C）

本例里 Lua 到 C 完全没有反向调用——`game_config.lua` 不引用任何 Host 注入的函数（只引用了 Host 注入的变量 `difficulty`），是一个比示例 1 更"纯"的 Configuration：Host 只提供数据输入，不提供计算能力，所有计算都发生在 Lua 内部，用 Lua 自己的 `scale_by_difficulty()` 完成。

## 9. State（状态）

- `game_config.lua` 内部的 `difficulty`（local 遮蔽同名全局变量）、`scale_by_difficulty` 闭包，生命周期仅限于脚本执行期间；
- 转换后的 `PlayerConfig` 和 `WeaponConfig[]` 数组是纯 C 数据，`lua_close` 之后依然完整存在，是游戏后续帧真正会读取的状态。

## 10. Failure（失败处理）

三条独立失败路径，覆盖方式和示例 1 一致（打印明确错误、正确 `lua_close`、非 0 退出码）：

1. **文件不存在 / 语法错误**：`luaL_loadfile` 失败；
2. **脚本执行时报错**：`lua_pcall` 失败；
3. **字段缺失 / 类型错误 / 结构不对**：`load_player_config` 和 `load_weapons_config` 会分别报告"缺少必填字段"、"字段类型错误"、"weapons 表里出现非字符串 key"、"`weapons.xxx` 不是 table"等具体原因；单把武器数据有问题时只会跳过这一把（打印警告），不会让整个程序失败，除非最终一把合法武器都没读到。

## 11. Complete Host Code（完整 Host 代码）

```c
/*
 * main.c
 *
 * Game Host：加载 game_config.lua，读取其中的 player 表（单层表）
 * 和 weapons 表（"表的表"），转换成 C struct / struct 数组，
 * 再用这些数据"初始化游戏"（打印初始化后的玩家/武器参数）。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> 注入只读全局变量 difficulty（模拟存档/命令行传入的难度等级）
 *     -> luaL_loadfile + lua_pcall 加载并执行 game_config.lua
 *     -> lua_getglobal("player")  -> 逐字段 lua_getfield 读取
 *     -> lua_getglobal("weapons") -> lua_pushnil + lua_next 遍历
 *        每个 weapon 子表，转换成 WeaponConfig 数组
 *     -> lua_close 释放 VM
 *     -> 用 C struct 初始化游戏（打印结果）
 *
 * 简化点：lua_next 的遍历顺序由 Lua table 的内部哈希布局决定，
 * 不保证与源码里书写的顺序一致；生产代码如果需要稳定顺序，
 * 应该额外维护一个数组（例如 `weapon_order = {"sword","bow","staff"}`）
 * 或者让配置本身用数组形式书写。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_WEAPONS 16

typedef struct {
    double speed;
    int jump_height;
    int max_hp;
} PlayerConfig;

typedef struct {
    char name[32];
    double damage;
    double speed;
    int has_mana_cost; /* 是否存在 mana_cost 这个可选字段 */
    double mana_cost;
} WeaponConfig;

/* 从 table_idx 指向的 table 中读取一个 number 字段，语义同示例 1 的 get_int_field。 */
static int get_number_field(lua_State *L, int table_idx, const char *field,
                             int required, double default_value, double *out) {
    lua_getfield(L, table_idx, field);
    int type = lua_type(L, -1);

    if (type == LUA_TNIL) {
        lua_pop(L, 1);
        if (required) {
            fprintf(stderr, "[Config] 缺少必填字段: %s\n", field);
            return 0;
        }
        *out = default_value;
        return 0; /* 明确告知调用方：用的是默认值，字段本身不存在 */
    }

    if (type != LUA_TNUMBER) {
        fprintf(stderr, "[Config] 字段类型错误: %s 应为 number，实际是 %s\n",
                field, lua_typename(L, type));
        lua_pop(L, 1);
        *out = default_value;
        return -1; /* 明确的错误，调用方必须视为失败 */
    }

    *out = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return 1; /* 成功读到真实值 */
}

static int get_int_field(lua_State *L, int table_idx, const char *field,
                          int required, int default_value, int *out) {
    double d;
    int rc = get_number_field(L, table_idx, field, required, (double)default_value, &d);
    if (rc < 0) {
        return 0; /* 类型错误 */
    }
    if (rc == 0 && required) {
        return 0; /* 必填但缺失 */
    }
    *out = (int)d;
    return 1;
}

/* 读取 player 表 */
static int load_player_config(lua_State *L, PlayerConfig *out) {
    lua_getglobal(L, "player");
    if (lua_type(L, -1) != LUA_TTABLE) {
        fprintf(stderr, "[Config] 全局变量 'player' 不存在或不是 table\n");
        lua_pop(L, 1);
        return 0;
    }
    int idx = lua_gettop(L);

    int ok = 1;
    double speed = 0;
    int rc = get_number_field(L, idx, "speed", 1, 0, &speed);
    ok = ok && (rc == 1);
    out->speed = speed;

    ok = ok && get_int_field(L, idx, "jump_height", 1, 0, &out->jump_height);
    ok = ok && get_int_field(L, idx, "max_hp", 0, 100, &out->max_hp);

    lua_pop(L, 1); /* 弹出 player table */
    return ok;
}

/*
 * 读取 weapons 表（表的表）：
 *   lua_pushnil 压入一个"起始 key"
 *   每次 lua_next(L, tbl_idx) 弹出旧 key、压入下一对 (key, value)
 *   直到 lua_next 返回 0（表示遍历完毕，且已经把最后的 key 弹出）
 */
static int load_weapons_config(lua_State *L, WeaponConfig *out, int max_out, int *out_count) {
    lua_getglobal(L, "weapons");
    if (lua_type(L, -1) != LUA_TTABLE) {
        fprintf(stderr, "[Config] 全局变量 'weapons' 不存在或不是 table\n");
        lua_pop(L, 1);
        return 0;
    }
    int tbl_idx = lua_gettop(L);

    int ok = 1;
    int count = 0;
    lua_pushnil(L); /* 第一个 key */
    while (lua_next(L, tbl_idx) != 0) {
        /* 栈: ... weapons(tbl_idx) key(-2) value(-1) */
        if (lua_type(L, -2) != LUA_TSTRING) {
            fprintf(stderr, "[Config] weapons 表里出现非字符串 key，已跳过\n");
            lua_pop(L, 1); /* 只弹出 value，把 key 留给下一次 lua_next 当输入 */
            continue;
        }
        const char *wname = lua_tostring(L, -2);

        if (lua_type(L, -1) != LUA_TTABLE) {
            fprintf(stderr, "[Config] weapons.%s 不是 table，已跳过\n", wname);
            lua_pop(L, 1);
            continue;
        }

        if (count >= max_out) {
            fprintf(stderr, "[Config] 武器数量超过上限 %d，'%s' 被丢弃\n", max_out, wname);
            lua_pop(L, 1);
            continue;
        }

        int weapon_idx = lua_gettop(L);
        WeaponConfig *w = &out[count];
        snprintf(w->name, sizeof(w->name), "%s", wname);

        double damage = 0, speed = 0, mana = 0;
        int rc_damage = get_number_field(L, weapon_idx, "damage", 1, 0, &damage);
        int rc_speed = get_number_field(L, weapon_idx, "speed", 1, 0, &speed);
        if (rc_damage != 1 || rc_speed != 1) {
            fprintf(stderr, "[Config] weapons.%s 缺少必填字段或类型错误，已跳过\n", wname);
            lua_pop(L, 1);
            continue;
        }
        w->damage = damage;
        w->speed = speed;

        int rc_mana = get_number_field(L, weapon_idx, "mana_cost", 0, -1, &mana);
        w->has_mana_cost = (rc_mana == 1);
        w->mana_cost = mana;

        count++;
        lua_pop(L, 1); /* 弹出 value（weapon 子表），只留 key 给下一次 lua_next */
    }

    lua_pop(L, 1); /* 弹出 weapons table */
    *out_count = count;
    return ok;
}

static void init_game(const PlayerConfig *player, const WeaponConfig *weapons, int weapon_count) {
    printf("========================================\n");
    printf("[Game] 正在初始化（模拟）...\n");
    printf("[Player] speed=%.2f jump_height=%d max_hp=%d\n",
           player->speed, player->jump_height, player->max_hp);
    printf("[Weapons] 共 %d 件:\n", weapon_count);
    for (int i = 0; i < weapon_count; i++) {
        const WeaponConfig *w = &weapons[i];
        if (w->has_mana_cost) {
            printf("  - %-8s damage=%-6.2f speed=%-4.2f mana_cost=%.2f\n",
                   w->name, w->damage, w->speed, w->mana_cost);
        } else {
            printf("  - %-8s damage=%-6.2f speed=%-4.2f\n", w->name, w->damage, w->speed);
        }
    }
    printf("[Game] 初始化完成。\n");
    printf("========================================\n");
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "game_config.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }
    luaL_openlibs(L);

    /* 2. 注入只读全局变量 difficulty，模拟"来自存档/命令行的难度等级"。
     *    可以用环境变量 GAME_DIFFICULTY 覆盖，默认是 2（普通难度）。 */
    int difficulty = 2;
    const char *diff_env = getenv("GAME_DIFFICULTY");
    if (diff_env) {
        difficulty = atoi(diff_env);
    }
    lua_pushinteger(L, difficulty);
    lua_setglobal(L, "difficulty");

    /* 3. 加载 game_config.lua ——失败路径 1：文件不存在 / 语法错误 */
    if (luaL_loadfile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[Config] 加载失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 4. 执行 game_config.lua ——失败路径 2：脚本运行时报错 */
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[Config] 执行失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 5. 读取 player 表 ——失败路径 3：字段缺失/类型错误 */
    PlayerConfig player;
    if (!load_player_config(L, &player)) {
        fprintf(stderr, "[Config] player 配置校验失败，程序退出\n");
        lua_close(L);
        return 1;
    }

    /* 6. 读取 weapons 表（表的表） */
    WeaponConfig weapons[MAX_WEAPONS];
    int weapon_count = 0;
    if (!load_weapons_config(L, weapons, MAX_WEAPONS, &weapon_count) || weapon_count == 0) {
        fprintf(stderr, "[Config] weapons 配置校验失败或为空，程序退出\n");
        lua_close(L);
        return 1;
    }

    /* 7. 配置已经完全转换成纯 C 数据，释放 Lua VM */
    lua_close(L);

    /* 8. 用 C struct 初始化游戏 */
    init_game(&player, weapons, weapon_count);

    return 0;
}
```

## 12. Complete Lua Code（完整 Lua 代码）

```lua
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
```

## 13. Execution（执行）

```bash
cd 03-configuration/code/03-game-configuration
make
./game_config_demo game_config.lua
```

默认（普通难度，`difficulty=2`）：

```text
[game_config.lua] 已加载配置（难度等级 2），player.speed=10.0, weapons.bow.damage=15.0
========================================
[Game] 正在初始化（模拟）...
[Player] speed=10.00 jump_height=4 max_hp=100
[Weapons] 共 3 件:
  - staff    damage=12.00  speed=0.80 mana_cost=10.00
  - sword    damage=20.00  speed=1.00
  - bow      damage=15.00  speed=0.60
[Game] 初始化完成。
========================================
```

切换到困难模式：

```bash
GAME_DIFFICULTY=3 ./game_config_demo game_config.lua
```

```text
[game_config.lua] 已加载配置（难度等级 3），player.speed=8.0, weapons.bow.damage=12.0
========================================
[Game] 正在初始化（模拟）...
[Player] speed=8.00 jump_height=4 max_hp=80
[Weapons] 共 3 件:
  - sword    damage=20.00  speed=1.00
  - bow      damage=12.00  speed=0.60
  - staff    damage=12.00  speed=0.80 mana_cost=10.00
[Game] 初始化完成。
========================================
```

注意：`weapons` 三件武器的打印顺序在两次运行里不一样（`staff/sword/bow` vs `sword/bow/staff`），这正是第 11 步注释里提到的"简化点"——`lua_next` 遍历顺序由 Lua table 的哈希布局决定，不保证与源码书写顺序一致。

## 14. Step-by-step walkthrough（逐步走读）

1. `main()` 创建 Lua VM，注入 `difficulty`（整数，默认为 2，可用 `GAME_DIFFICULTY` 环境变量覆盖）。
2. `luaL_loadfile` + `lua_pcall` 执行 `game_config.lua`：`local difficulty = difficulty or 1` 这一行用**同名局部变量遮蔽全局变量**，如果 Host 没有注入（比如脚本被独立用 `lua` 命令测试），会退回默认值 1。
3. `scale_by_difficulty` 被定义成一个闭包，捕获了 `difficulty` 这个局部变量。
4. `player = {...}` 求值时，`scale_by_difficulty(10)` 被调用一次，产出 `player.speed`；`max_hp` 由条件表达式算出。
5. `weapons = {...}` 求值时，`sword`/`staff` 是纯字面量，`bow.damage` 再次调用 `scale_by_difficulty(15)`，复用了和 `player.speed` 同一个函数。
6. 回到 C 侧，`load_player_config` 用 `lua_getglobal` + 逐字段 `lua_getfield` 读取 `player`，转换成 `PlayerConfig`。
7. `load_weapons_config` 用 `lua_getglobal("weapons")` 拿到表，然后 `lua_pushnil` + `while (lua_next(...))` 循环遍历：每一轮栈顶是 `(key, value)`，`key` 是武器名字符串，`value` 是这把武器的子表；对子表再各自调用 `get_number_field` 读取 `damage`/`speed`/可选的 `mana_cost`。
8. 每读完一把武器，用 `lua_pop(L, 1)` 弹出 `value`（保留 `key` 供 `lua_next` 下一轮使用）——这是使用 `lua_next` 时最容易出错、也最需要注意的栈平衡细节。
9. 遍历结束后弹出 `weapons` 表本身，`lua_close` 释放整个 VM。
10. `init_game` 用纯 C 数据打印玩家和武器信息，完成"初始化"。

## 15. Mental Model（心智模型）

```text
game_config.lua = 数值表 + 少量可复用的计算函数
Host 不关心配置里有没有用到函数，只关心最终读到的 damage/speed 是不是 number
```

把游戏数值配置想象成"策划用 Lua 写的一张可编程 Excel"：大部分是纯数据（等价于 Excel 里的普通单元格），少数几个字段是"公式"（等价于 Excel 的 `=A1*1.2`），而 Host 只关心"公式算出来的最终结果"，不关心背后用了什么公式。

## 16. Why this is this Pattern（为什么这是 Configuration 而不是别的）

- 它不是 Extension：`game_config.lua` 没有新增游戏引擎原本不具备的能力（比如新的攻击类型的实现逻辑），它只是给已经存在的"武器有伤害值"这件事提供具体数字；
- 它不是 Policy：难度缩放只在启动/读档时算一次，不是"每次战斗结算都要重新问一次 Lua 该造成多少伤害"（如果变成后者，就越界成 Policy 或 Behavior 了）；
- 它不是 Rule Engine：没有"IF 满足什么条件 THEN 触发什么动作"的匹配表，只有值和值之间的计算关系；
- 它不是 Plugin：`game_config.lua` 没有独立的生命周期（没有 `on_enable`/`on_disable`），加载完就可以关闭 VM。

### Advantages（可计算配置的优势）

- **动态**：`difficulty` 由外部输入决定，同一份配置文件在不同难度下自动产出不同数值，例如 `weapons.bow.damage` 在简单/普通/困难下分别是 18.0/15.0/12.0；
- **可计算**：`max_hp = (difficulty >= 3) and 80 or 100` 直接用表达式表达业务规则，不需要额外的多维数值表；
- **可复用**：`scale_by_difficulty()` 被 `player.speed` 和 `weapons.bow.damage` 同时复用，改一次缩放曲线，两处同时生效，不会出现"改了一处忘记改另一处"的不一致；
- **可条件化**：`max_hp` 的困难模式判断本质上是"配置层面的条件逻辑"，不需要在游戏引擎的 C 代码里为"困难模式生命值更低"这件事单独写一个特殊分支。

### Risks（可计算配置的风险）

- **配置变成代码——副作用与死循环**：一旦允许配置文件是可执行代码，就无法从语法上禁止它做"不该做"的事，例如策划不小心在 `game_config.lua` 里写了一个死循环（`while true do end`），或者调用了会产生副作用的函数（如果 Host 不小心暴露了这类 API），后果是配置加载阶段直接卡死或产生难以预料的状态；
- **错误变成 runtime error，不是"解析失败"**：如果把 `weapons.sword.damage` 手误写成 `weapons.sword.demage`，静态配置格式（比如 JSON schema 校验）通常能在"解析阶段"就精确报出"多了一个未知字段/少了一个必需字段"；而 Lua 配置里，`demage` 只是又声明了一个新字段，`damage` 字段本身会被 `get_number_field` 判定为"缺少必填字段"，报错信息不会直接指向"你打错字了"，而是"必填字段缺失"——排查成本比静态格式更高，因为脚本本身"跑得通"，问题是跑完之后的表面结构不对；
- **安全边界更复杂**：如果 `game_config.lua` 来自不可信来源（比如允许玩家自定义的 mod、创意工坊内容），加载它就等于允许执行任意 Lua 代码——它可以访问 `os.execute`、`io.open` 等标准库函数，做远超"改几个数值"的事情。生产环境如果要接受不可信配置，至少需要：
  - **移除危险的全局函数**：加载配置前把 `os`/`io`/`require`/`dofile`/`loadfile` 等从全局表里 `nil` 掉，只保留 `math`/`string`/`table` 等纯计算相关的库；
  - **用 `debug.setmetatable` 或自定义 `_ENV`（Lua 5.2+ 的沙盒环境机制）限制脚本能访问的全局变量集合**，把可执行的配置关进一个只能读写少数几个白名单变量的"沙盒"；
  - **限制资源消耗**：例如通过 debug hook（`lua_sethook`）设置指令计数上限，防止死循环无限占用 CPU。

**如果 Lua 消失，Host 还能做什么？** 游戏引擎完全可以运行，只是数值需要换成一份纯 JSON/二进制表格，缩放逻辑需要写死在 C 代码里（比如做一个 `scale_by_difficulty(float base, int difficulty)` 的 C 函数）；`init_game` 的实现完全不用改，因为它面对的始终是 `PlayerConfig`/`WeaponConfig` 这两个纯 C 类型。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 换成 JSON/YAML：`sword`/`staff` 这类纯数据字段完全没问题，但 `scale_by_difficulty()` 这种"多处复用的计算逻辑"和"困难模式下生命值更低"这种条件分支都做不到，除非把每个难度都存一份完整拷贝（数据量随难度数线性增长，还会出现"改了简单难度忘记改困难难度"的不一致问题）。换成纯 C：计算能力和复用能力都能做到（写一个 C 函数），但每次策划想调一次数值就要重新编译、重新发布客户端，这在游戏行业是不可接受的迭代速度——这正是绝大多数游戏引擎最终选择"用脚本语言表达数值配置"的根本原因。
