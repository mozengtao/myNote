#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * describe_value(v)：Lua -> C 方向。
 * C 侧先用 lua_type 判断 Stack 上第 1 个参数的类型，
 * 再用对应的 lua_to* / lua_is* 把值读出来打印。
 *
 * 注意：Lua 5.4 里 integer 和 number(float) 共享同一个 lua_type()
 * 返回值 LUA_TNUMBER，要用 lua_isinteger 再细分子类型——这是 Lua 5.4
 * 才有的区分（Lua 5.1/5.2/5.3 早期没有独立的 integer 子类型）。
 */
static int l_describe_value(lua_State *L) {
    int t = lua_type(L, 1);

    switch (t) {
    case LUA_TNIL:
        printf("[describe_value] type=nil          lua_isnil=true\n");
        break;

    case LUA_TBOOLEAN: {
        int b = lua_toboolean(L, 1);
        printf("[describe_value] type=boolean      lua_toboolean=%s\n",
               b ? "true" : "false");
        break;
    }

    case LUA_TNUMBER:
        if (lua_isinteger(L, 1)) {
            lua_Integer i = lua_tointeger(L, 1);
            printf("[describe_value] type=number(integer 子类型)  lua_tointeger=%lld\n",
                   (long long)i);
        } else {
            lua_Number n = lua_tonumber(L, 1);
            printf("[describe_value] type=number(float 子类型)    lua_tonumber=%g\n", n);
        }
        break;

    case LUA_TSTRING: {
        size_t len = 0;
        const char *s = lua_tolstring(L, 1, &len);
        printf("[describe_value] type=string       lua_tolstring=\"%s\" (len=%zu)\n",
               s, len);
        break;
    }

    default:
        printf("[describe_value] type=%s（本章不覆盖，见 Chapter 06 起）\n",
               lua_typename(L, t));
        break;
    }

    return 0; /* 栈顶 0 个返回值：describe_value 只打印，不返回任何东西给 Lua */
}

/*
 * make_sample()：C -> Lua 方向。
 * 用 lua_push* 依次把 nil / boolean / integer / number / string
 * 五种基本类型的值 Push 到 Stack 上，最后 return 5 告诉 Lua VM：
 * "栈顶 5 个值就是这次调用的返回值"。
 */
static int l_make_sample(lua_State *L) {
    lua_pushnil(L);                              /* Stack: [nil] */
    lua_pushboolean(L, 1);                        /* Stack: [nil, true] */
    lua_pushinteger(L, 123);                       /* Stack: [nil, true, 123] */
    lua_pushnumber(L, 2.5);                        /* Stack: [nil, true, 123, 2.5] */
    lua_pushlstring(L, "C 端字符串", sizeof("C 端字符串") - 1); /* 字节长度（UTF-8），不是字符数 */
    /* Stack: [nil, true, 123, 2.5, "C 端字符串"] */

    return 5; /* 栈顶 5 个值是返回值，Lua 侧用 local a,b,c,d,e = make_sample() 接收 */
}

/*
 * checknumber_strict(v)：演示 luaL_check* 系列的"校验失败即报错"行为。
 * 与 lua_to* 系列（静默转换失败）形成对照——这是本章 Failure 部分的核心例子。
 */
static int l_checknumber_strict(lua_State *L) {
    double v = luaL_checknumber(L, 1); /* 无法转换成数字 -> 直接 luaL_error，下一行不会执行 */
    lua_pushnumber(L, v * 2);
    return 1;
}

/*
 * demo_pitfalls()：集中演示两个"不报错但结果是错的"陷阱：
 *   1. lua_tointeger 对无法转换的字符串静默返回 0
 *   2. lua_tostring 对 boolean 返回 NULL
 * 这两个都不会让程序崩溃或报错，但如果调用者不检查，很容易产生
 * "看起来正常但实际是错的" 结果——这正是 lua_to*（Read/To 系列）
 * 与 luaL_check*（校验系列）行为不同的地方。
 */
static int l_demo_pitfalls(lua_State *L) {
    printf("\n--- Failure 1: lua_tointeger 对无法转换的字符串静默返回 0 ---\n");
    lua_pushstring(L, "hello");              /* Stack: [..., "hello"] */
    lua_Integer bad = lua_tointeger(L, -1);  /* "hello" 无法转数字 -> 静默返回 0 */
    printf("lua_tointeger(\"hello\") = %lld  "
           "(没有任何报错，看起来像一个合法的 0！)\n",
           (long long)bad);
    lua_pop(L, 1);                            /* Stack: [...] */

    printf("\n--- Failure 2: lua_tostring 对 boolean 返回 NULL ---\n");
    lua_pushboolean(L, 1);                    /* Stack: [..., true] */
    const char *s = lua_tostring(L, -1);      /* boolean 不能隐式转换为字符串 */
    if (s == NULL) {
        printf("lua_tostring(true) 返回 NULL（boolean 无法转换为字符串）\n");
        printf("若不判空直接 printf(\"%%s\", s) 会解引用空指针，属于未定义行为\n");
    } else {
        printf("lua_tostring(true) = %s\n", s);
    }
    lua_pop(L, 1);                             /* Stack: [...] */

    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "describe_value", l_describe_value);
    lua_register(L, "make_sample", l_make_sample);
    lua_register(L, "checknumber_strict", l_checknumber_strict);
    lua_register(L, "demo_pitfalls", l_demo_pitfalls);

    if (luaL_dofile(L, "types_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
