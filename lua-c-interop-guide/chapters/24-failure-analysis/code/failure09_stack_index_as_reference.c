/*
 * failure09_stack_index_as_reference.c
 *
 * Failure 9（Chapter 24）：把 Stack index 当成长期 Object Reference。
 *
 * remember_bad() 里把"参数 1 所在的 Stack 位置"（也就是整数 1）存进
 * 一个 C 全局变量，以为这样就"记住了"那次调用传进来的那个值。
 * 下一次、完全独立的一次"进入 C"（use_remembered_bad）里，Stack 位置 1
 * 早已经是别的调用的别的参数——但 saved_index 这个整数本身完全没变。
 *
 * 实际编译运行，观察第二次调用时到底读到了什么。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* BUG：这里存的是一个"Stack 位置编号"，不是 Registry Reference，
 * 也不是任何形式的对象句柄——它只在"当时那一次调用"里有意义。 */
static int saved_index = 0;

static int l_remember_bad(lua_State *L) {
    saved_index = 1; /* 以为"记住了参数1的位置"就等于"记住了这个值" */
    printf("[C] remember_bad: 记下 saved_index = %d，此刻这个位置的值 = %s\n",
           saved_index, lua_tostring(L, saved_index));
    return 0;
}

static int l_use_remembered_bad(lua_State *L) {
    const char *s = lua_tostring(L, saved_index);
    printf("[C] use_remembered_bad: Stack 位置 %d 现在的类型 = %s, "
           "lua_tostring 结果 = %s\n",
           saved_index, luaL_typename(L, saved_index), s ? s : "(NULL)");
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "remember_bad", l_remember_bad);
    lua_register(L, "use_remembered_bad", l_use_remembered_bad);

    const char *src =
        "remember_bad(\"first-call-value\")\n"
        "use_remembered_bad(123)\n"
        "use_remembered_bad(true)\n";

    if (luaL_dostring(L, src) != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
