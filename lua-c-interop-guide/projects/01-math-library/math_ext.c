/*
 * math_ext.c —— Project 1: Math Library
 *
 * 本项目要证明的事情：真正的"库"不是一堆散落的全局函数
 * （像 Chapter 01 的 add 那样用 lua_register 一个个挂到 _G 上），
 * 而是一张 luaL_Reg 数组 + 一次 luaL_setfuncs，把一组相关的 C Function
 * 打包注册进同一个 Lua Table（math_ext），对照 Chapter 20 API Map 里的
 * "G. Function Registration" 知识点。
 *
 * 每个 C Function 内部的动作分类都逃不出 Chapter 26 的五个动词：
 *   luaL_check*   ->  READ    （Stack -> C，校验失败直接 luaL_error）
 *   lua_push*     ->  PUSH    （C -> Stack）
 * 本文件只有一种"进入 C"的场景：Lua 调用 math_ext 里的某个函数，
 * 不涉及 C 反过来调用 Lua，所以没有 CALL 动作。
 */
#include <stdio.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * math_ext.clamp(v, lo, hi)
 *   把浮点数 v 限制在 [lo, hi] 区间内。
 *
 * Stack 进入时：
 *   [1] = v   [2] = lo   [3] = hi
 * 三个都用 luaL_checknumber 校验并转换成 C 的 double —— 这是
 * READ 动作：Stack -> C，任何一个参数类型不对（比如传字符串
 * "abc"），luaL_checknumber 会直接 luaL_error，跳过后面所有代码，
 * 相当于 C 侧提前"拒绝"了这次调用，绝不会用垃圾值继续算下去。
 */
static int l_clamp(lua_State *L)
{
    double v  = luaL_checknumber(L, 1);
    double lo = luaL_checknumber(L, 2);
    double hi = luaL_checknumber(L, 3);

    if (lo > hi) {
        return luaL_error(L, "clamp: lo(%f) 不能大于 hi(%f)", lo, hi);
    }

    double result = v;
    if (result < lo) result = lo;
    if (result > hi) result = hi;

    lua_pushnumber(L, result);   /* PUSH：C -> Stack，栈顶就是这次调用的返回值 */
    return 1;                    /* 告诉 Lua VM：栈顶 1 个值是返回值 */
}

/*
 * math_ext.lerp(a, b, t)
 *   在 a、b 之间按比例 t 做线性插值：a + (b - a) * t。
 *   t 通常落在 [0, 1]，但本函数不强制，允许外插。
 */
static int l_lerp(lua_State *L)
{
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    double t = luaL_checknumber(L, 3);

    double result = a + (b - a) * t;

    lua_pushnumber(L, result);
    return 1;
}

/*
 * math_ext.is_prime(n)
 *   判断整数 n 是否为素数，返回一个 Lua boolean。
 *   注意用 luaL_checkinteger（不是 luaL_checknumber），这样传
 *   "3.5" 之类的非整数浮点数或非数字字符串都会被 luaL_error 挡住。
 */
static int l_is_prime(lua_State *L)
{
    lua_Integer n = luaL_checkinteger(L, 1);

    int prime = 1;
    if (n < 2) {
        prime = 0;
    } else {
        for (lua_Integer i = 2; i * i <= n; i++) {
            if (n % i == 0) {
                prime = 0;
                break;
            }
        }
    }

    lua_pushboolean(L, prime);   /* PUSH：C int(0/1) -> Lua boolean */
    return 1;
}

/*
 * math_ext.gcd(a, b)
 *   欧几里得算法求最大公约数，a、b 都是整数。
 */
static int l_gcd(lua_State *L)
{
    lua_Integer a = luaL_checkinteger(L, 1);
    lua_Integer b = luaL_checkinteger(L, 2);

    if (a < 0) a = -a;
    if (b < 0) b = -b;

    while (b != 0) {
        lua_Integer tmp = b;
        b = a % b;
        a = tmp;
    }

    lua_pushinteger(L, a);
    return 1;
}

/*
 * math_ext.round(v, ndigits)
 *   把浮点数 v 四舍五入到小数点后 ndigits 位，示范"混合整数 + 浮点
 *   参数"的校验：v 用 luaL_checknumber，ndigits 用 luaL_optinteger
 *   （可选参数，缺省时默认为 0，对照 luaL_check* 家族里带"opt"的变体）。
 */
static int l_round(lua_State *L)
{
    double v = luaL_checknumber(L, 1);
    lua_Integer ndigits = luaL_optinteger(L, 2, 0);

    double factor = pow(10.0, (double)ndigits);
    double result = round(v * factor) / factor;

    lua_pushnumber(L, result);
    return 1;
}

/*
 * luaL_Reg 数组：name -> C Function 的映射表，luaL_setfuncs 会
 * 逐条读取，把每一项都以 name 为 key 写进当前栈顶的 Table 里。
 * 数组必须以 {NULL, NULL} 结尾，luaL_setfuncs 靠这个哨兵判断数组长度。
 */
static const luaL_Reg math_ext_funcs[] = {
    {"clamp",    l_clamp},
    {"lerp",     l_lerp},
    {"is_prime", l_is_prime},
    {"gcd",      l_gcd},
    {"round",    l_round},
    {NULL, NULL}
};

/*
 * 把 math_ext_funcs 注册成一个名为 "math_ext" 的全局 Lua Table，
 * 对应 Lua 侧的 math_ext.clamp(...)、math_ext.lerp(...) 等等。
 *
 * 步骤（对照 Chapter 20 API Map "G. Function Registration"）：
 *   1. lua_newtable(L)          在 Stack 顶部新建一张空 Table
 *   2. luaL_setfuncs(L, funcs, 0)
 *      把 math_ext_funcs 里的每一项都以 luaL_Reg.name 为 key，
 *      对应 C Function 为 value，SET 进这张 Table（此刻它还在栈顶）。
 *      第三个参数 0 表示"没有 upvalue 要绑定给这些函数共享"
 *      （Closure/Upvalue 是 Chapter 15 的主题，这里不需要）。
 *   3. lua_setglobal(L, "math_ext")
 *      把栈顶这张已经填满函数的 Table，SET 成全局变量 math_ext，
 *      同时把它从 Stack 上弹出。
 */
static void register_math_ext(lua_State *L)
{
    lua_newtable(L);
    luaL_setfuncs(L, math_ext_funcs, 0);
    lua_setglobal(L, "math_ext");
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    register_math_ext(L);

    if (luaL_dofile(L, "math_ext_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
