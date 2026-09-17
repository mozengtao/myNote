#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* 打印当前 Lua Stack 的完整内容，label 用来区分 "调用前/调用后" */
static void dump_stack(lua_State *L, const char *label) {
    int top = lua_gettop(L);
    printf("  %s (top=%d): ", label, top);
    if (top == 0) {
        printf("<empty>");
    }
    for (int i = 1; i <= top; i++) {
        int t = lua_type(L, i);
        if (t == LUA_TNIL) {
            printf("[%d]=nil ", i);
        } else if (t == LUA_TNUMBER) {
            printf("[%d]=%g ", i, lua_tonumber(L, i));
        } else if (t == LUA_TSTRING) {
            printf("[%d]=\"%s\" ", i, lua_tostring(L, i));
        } else {
            printf("[%d]=%s ", i, lua_typename(L, t));
        }
    }
    printf("\n");
}

/* ---------- Stack Inspection ---------- */

static int demo_gettop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_gettop(L) --\n");
    dump_stack(L, "before");
    int n = lua_gettop(L);
    printf("  lua_gettop(L) = %d\n", n);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Shrinking ---------- */

static int demo_settop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    lua_pushinteger(L, 40);
    printf("-- lua_settop(L, 2)  [shrink] --\n");
    dump_stack(L, "before");
    lua_settop(L, 2);
    dump_stack(L, "after ");

    printf("-- lua_settop(L, 5)  [grow, 用 nil 填充] --\n");
    dump_stack(L, "before");
    lua_settop(L, 5);
    dump_stack(L, "after ");
    return 0;
}

static int demo_pop(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    lua_pushinteger(L, 40);
    lua_pushinteger(L, 50);
    printf("-- lua_pop(L, 2) --\n");
    dump_stack(L, "before");
    lua_pop(L, 2);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Growth ---------- */

static int demo_pushinteger(lua_State *L) {
    lua_settop(L, 0);
    printf("-- lua_pushinteger(L, 10) --\n");
    dump_stack(L, "before");
    lua_pushinteger(L, 10);
    dump_stack(L, "after ");
    return 0;
}

static int demo_pushvalue(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_pushvalue(L, 1)  [复制已有值到栈顶] --\n");
    dump_stack(L, "before");
    lua_pushvalue(L, 1);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Read ---------- */

static int demo_read(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 100);
    lua_pushinteger(L, 200);
    lua_pushinteger(L, 300);
    printf("-- lua_tointeger(L, 1) / lua_tointeger(L, -1) --\n");
    dump_stack(L, "before");
    lua_Integer first = lua_tointeger(L, 1);
    lua_Integer last  = lua_tointeger(L, -1);
    printf("  lua_tointeger(L, 1)  = %lld  (positive index -> 从 stack bottom 数)\n",
           (long long)first);
    printf("  lua_tointeger(L, -1) = %lld  (negative index -> 从 stack top 数)\n",
           (long long)last);
    dump_stack(L, "after ");
    return 0;
}

/* ---------- Stack Rearrangement ---------- */

static int demo_remove(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_remove(L, 1) --\n");
    dump_stack(L, "before");
    lua_remove(L, 1);
    dump_stack(L, "after ");
    return 0;
}

static int demo_insert(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_insert(L, 1) --\n");
    dump_stack(L, "before");
    lua_insert(L, 1);
    dump_stack(L, "after ");
    return 0;
}

static int demo_replace(lua_State *L) {
    lua_settop(L, 0);
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_pushinteger(L, 30);
    printf("-- lua_replace(L, 1) --\n");
    dump_stack(L, "before");
    lua_replace(L, 1);
    dump_stack(L, "after ");
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "demo_gettop", demo_gettop);
    lua_register(L, "demo_settop", demo_settop);
    lua_register(L, "demo_pop", demo_pop);
    lua_register(L, "demo_pushinteger", demo_pushinteger);
    lua_register(L, "demo_pushvalue", demo_pushvalue);
    lua_register(L, "demo_read", demo_read);
    lua_register(L, "demo_remove", demo_remove);
    lua_register(L, "demo_insert", demo_insert);
    lua_register(L, "demo_replace", demo_replace);

    if (luaL_dofile(L, "stack_basics_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
