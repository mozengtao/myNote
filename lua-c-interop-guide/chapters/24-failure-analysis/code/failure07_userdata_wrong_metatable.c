/*
 * failure07_userdata_wrong_metatable.c
 *
 * Failure 7（Chapter 24）：userdata 类型错误。
 *
 * new_widget() 创建的 Full Userdata 挂的是名为 "Widget" 的 metatable。
 * use_as_gadget() 却拿错误的 metatable 名字 "Gadget" 去调用
 * luaL_checkudata——观察真实报出来的 Lua Error 文本长什么样。
 */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef struct {
    int value;
} Widget;

static int l_new_widget(lua_State *L) {
    Widget *w = (Widget *)lua_newuserdata(L, sizeof(Widget));
    w->value = 42;
    luaL_getmetatable(L, "Widget");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_use_as_gadget(lua_State *L) {
    /* BUG：这个 userdata 实际挂的 metatable 名字是 "Widget"，
     * 这里却错误地用 "Gadget" 去检查/取出它。 */
    Widget *w = (Widget *)luaL_checkudata(L, 1, "Gadget");
    printf("[C] value = %d\n", w->value);
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, "Widget");
    lua_pop(L, 1);
    luaL_newmetatable(L, "Gadget");
    lua_pop(L, 1);

    lua_register(L, "new_widget", l_new_widget);
    lua_register(L, "use_as_gadget", l_use_as_gadget);

    const char *src =
        "local w = new_widget()\n"
        "use_as_gadget(w)\n";

    if (luaL_dostring(L, src) != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
