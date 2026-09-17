#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * 示例 1：函数签名固定是 (a, b, c)，用 positive index 读取参数。
 * 正索引从 Stack bottom 数，只要参数在调用时的顺序不变，
 * [1]/[2]/[3] 永远稳定对应 a/b/c —— 即便函数后面又往 Stack 上
 * push 了别的临时值，也不会影响 [1]/[2]/[3] 的含义。
 */
static int demo_fixed_args(lua_State *L) {
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);
    lua_Integer c = luaL_checkinteger(L, 3);
    printf("  positive index 读参数: a=[1]=%lld b=[2]=%lld c=[3]=%lld\n",
           (long long)a, (long long)b, (long long)c);

    /* 制造一次"栈顶变化"：压入一个临时和 */
    lua_pushinteger(L, a + b + c);
    printf("  push 临时和之后:\n");
    printf("    [1] 依然 = %lld  (positive index 不受影响，还是参数 a)\n",
           (long long)luaL_checkinteger(L, 1));
    printf("    [-1] 现在 = %lld  (negative index 变了！现在指向新 push 的临时和，"
           "不再是参数 c 了)\n",
           (long long)lua_tointeger(L, -1));
    return 0;
}

/*
 * 示例 2：调用方传入的参数个数不确定（类似 vararg），
 * 函数算完结果后 push 到栈顶，用 negative index (-1) 取回。
 * 好处：不管调用时传了几个参数（n 是运行时才知道的），
 * "刚 push 的结果"永远是 -1，不需要先算出 n+1 这个 positive index。
 */
static int demo_dynamic_result(lua_State *L) {
    int n = lua_gettop(L); /* 参数个数，编译期未知，运行时才确定 */
    lua_Integer sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += luaL_checkinteger(L, i);
    }
    lua_pushinteger(L, sum); /* 结果落在第几个 positive index 取决于 n，我们不关心 */
    lua_Integer via_negative = lua_tointeger(L, -1); /* 永远正确：栈顶就是刚 push 的结果 */
    printf("  n=%d 个参数, sum=%lld, 用 lua_tointeger(L, -1) 取回 = %lld\n",
           n, (long long)sum, (long long)via_negative);
    return 1;
}

/*
 * 示例 3：循环里反复 push，每一轮都想拿到"刚 push 的那个值"。
 * 用 positive index 的话，每轮都要重新调用 lua_gettop(L) 才知道
 * 最新值在哪个位置（1, 2, 3, 4, 5 ... 一直在变）；
 * 用 negative index 的话，永远是 -1，不需要关心已经循环了几轮。
 */
static int demo_loop_latest(lua_State *L) {
    lua_settop(L, 0);
    for (int i = 1; i <= 5; i++) {
        lua_pushinteger(L, i * 10);
        lua_Integer latest = lua_tointeger(L, -1);
        printf("  iteration %d: pushed %d, lua_tointeger(L, -1) = %lld, "
               "lua_gettop(L) = %d (这个 positive 位置每轮都在变)\n",
               i, i * 10, (long long)latest, lua_gettop(L));
    }
    return 0;
}

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "demo_fixed_args", demo_fixed_args);
    lua_register(L, "demo_dynamic_result", demo_dynamic_result);
    lua_register(L, "demo_loop_latest", demo_loop_latest);

    if (luaL_dofile(L, "stack_index_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}
