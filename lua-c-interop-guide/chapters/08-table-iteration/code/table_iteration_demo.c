#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * l_process: 遍历一张 Table 的所有 key/value，不事先知道字段名。
 *
 * lua_next(L, idx) 的契约：
 *   - idx 处必须是那张要遍历的 table。
 *   - 栈顶（idx 的正上方）必须是"上一次找到的 key"（第一次用 nil 表示"从头开始"）。
 *   - 找到下一对就把 key、value 依次 Push 到栈顶，返回非 0；
 *     没有更多元素了就什么都不 Push，返回 0。
 *
 * 因此每一轮循环处理完 value 之后，必须只 lua_pop(L, 1)（只弹 value），
 * 把 key 留在栈顶——它是下一次 lua_next 的输入。
 */
static int l_process(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_pushnil(L); /* [1]=table [2]=nil，nil 表示"从第一个 key 开始找" */

    while (lua_next(L, 1) != 0) {
        /* 此刻栈上：[1]=table [2]=key [3]=value */
        const char *key = lua_tostring(L, -2);
        int vtype = lua_type(L, -1);

        switch (vtype) {
        case LUA_TSTRING:
            printf("[C]   %s = \"%s\" (string)\n", key, lua_tostring(L, -1));
            break;
        case LUA_TBOOLEAN:
            printf("[C]   %s = %s (boolean)\n", key,
                   lua_toboolean(L, -1) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("[C]   %s = %g (number)\n", key, lua_tonumber(L, -1));
            break;
        default:
            printf("[C]   %s = <%s>\n", key, lua_typename(L, vtype));
            break;
        }

        /* 只弹 value（栈顶那 1 个），把 key（[2]）留给下一次 lua_next 当输入 */
        lua_pop(L, 1);
    }

    /* 循环正常结束后，栈上只剩 [1]=table，Stack Balance 是干净的 */
    return 0;
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "process", l_process);

    if (luaL_dofile(L, "table_iteration_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
