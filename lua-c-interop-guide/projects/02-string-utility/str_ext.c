/*
 * str_ext.c —— Project 2: String Utility Library
 *
 * 本项目要证明的事情：
 *   1. Lua String 不是 C 的 `char *`（以 '\0' 结尾），而是"长度显式的
 *      字节数组"，可能内部包含 '\0'。读它必须用 lua_tolstring（拿到
 *      指针 *and* 长度），只用 lua_tostring/luaL_checkstring 拿指针
 *      而不管长度，遇到内嵌 '\0' 的字符串就会被截断——这是本项目
 *      Failure 部分要示范的坑。
 *   2. C 处理完字符串后，"送回 Lua"分两种典型场景：
 *      - 结果长度已知、且不含内嵌 '\0'（比如 reverse/trim/to_upper 处理
 *        纯文本）：lua_pushlstring(L, buf, len) 是唯一严谨的做法——
 *        依然要显式给长度，不能想着"反正处理完没有 \0 就用 pushstring
 *        也行"，本教程统一用 pushlstring 保持一致、不留隐患。
 *      - 结果是"多个子串"（split）：不是一个字符串，而是一整张
 *        Lua Table，用 lua_newtable + lua_rawseti 按下标 1..N 写入
 *        （对照 Chapter 07"C -> Lua Table"知识点）。
 *   3. C 侧的临时缓冲区来源分两类：栈上定长缓冲区（reverse/to_upper，
 *      长度可预先知道等于输入长度）和 malloc 出来的动态缓冲区
 *      （trim 的结果长度 <= 输入长度，这里选择更保守的 malloc 方案，
 *      并用 split 演示"多次 malloc、用完即 free"）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * str_ext.reverse(s)
 *   把字符串 s 按字节反转。用 lua_tolstring 同时拿到指针 src 和
 *   长度 len——即使 s 内部含有 '\0'，len 依然是"真实字节数"，
 *   不会被 '\0' 截断，反转结果也完整保留这些字节。
 *
 * C 侧缓冲区：用一个栈上（C Call Stack 意义上的栈，不是 Lua Stack）
 * 定长数组即可，因为反转后长度和输入完全相同，不需要 malloc。
 */
#define REVERSE_BUF_MAX 4096

static int l_reverse(lua_State *L)
{
    size_t len;
    const char *src = lua_tolstring(L, 1, &len);
    if (src == NULL) {
        return luaL_error(L, "reverse: 参数 #1 必须是 string");
    }
    if (len >= REVERSE_BUF_MAX) {
        return luaL_error(L, "reverse: 输入长度 %zu 超过示例上限 %d",
                           len, REVERSE_BUF_MAX);
    }

    char buf[REVERSE_BUF_MAX];
    for (size_t i = 0; i < len; i++) {
        buf[i] = src[len - 1 - i];
    }

    lua_pushlstring(L, buf, len); /* PUSH：显式带长度，'\0' 字节原样保留 */
    return 1;
}

/*
 * str_ext.trim(s)
 *   去掉字符串两端的空白字符（空格/\t/\n/\r），中间内容原样保留。
 *   结果长度 <= 输入长度，且要跨函数返回，这里用 malloc 而不是
 *   栈上数组，示范"动态缓冲区处理后 pushlstring、随即 free"的模式：
 *   缓冲区的生命周期完全在 C 原生世界内closed（malloc -> 用 -> free），
 *   Lua 侧拿到的是 lua_pushlstring 复制出来的新字符串（Lua 自己的
 *   GC 堆内存），跟这块 malloc 缓冲区没有任何关系——释放它绝不会
 *   影响 Lua 那边已经拿到的返回值。
 */
static int l_trim(lua_State *L)
{
    size_t len;
    const char *src = lua_tolstring(L, 1, &len);
    if (src == NULL) {
        return luaL_error(L, "trim: 参数 #1 必须是 string");
    }

    size_t start = 0;
    while (start < len && isspace((unsigned char)src[start])) {
        start++;
    }

    size_t end = len; /* [start, end) 是去除两端空白后的区间 */
    while (end > start && isspace((unsigned char)src[end - 1])) {
        end--;
    }

    size_t out_len = end - start;
    char *buf = malloc(out_len > 0 ? out_len : 1);
    if (buf == NULL) {
        return luaL_error(L, "trim: malloc 失败");
    }
    memcpy(buf, src + start, out_len);

    lua_pushlstring(L, buf, out_len);
    free(buf); /* Lua 已经把内容复制进自己的 GC 堆，这块 malloc 内存可以立即释放 */
    return 1;
}

/*
 * str_ext.to_upper(s)
 *   把字符串按字节转成大写（只处理 ASCII 字母，非字母字节原样保留，
 *   包括可能存在的内嵌 '\0'）。和 reverse 一样，输出长度 == 输入长度，
 *   用栈上定长缓冲区即可。
 */
static int l_to_upper(lua_State *L)
{
    size_t len;
    const char *src = lua_tolstring(L, 1, &len);
    if (src == NULL) {
        return luaL_error(L, "to_upper: 参数 #1 必须是 string");
    }
    if (len >= REVERSE_BUF_MAX) {
        return luaL_error(L, "to_upper: 输入长度 %zu 超过示例上限 %d",
                           len, REVERSE_BUF_MAX);
    }

    char buf[REVERSE_BUF_MAX];
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)toupper((unsigned char)src[i]);
    }

    lua_pushlstring(L, buf, len);
    return 1;
}

/*
 * str_ext.split(s, sep)
 *   按单字符分隔符 sep 切分 s，返回一张 Lua Table：{ [1]=part1,
 *   [2]=part2, ... }。这是本项目"C -> Lua Table"的核心示范
 *   （对照 Chapter 07）：
 *
 *   1. lua_newtable(L)：在 Stack 顶部新建一张空 Table（这就是
 *      将来要 return 的那个 Lua Object）。
 *   2. 每切出一段子串：
 *        lua_pushlstring(L, part, part_len)   PUSH：子串进 Stack
 *        lua_rawseti(L, table_idx, i)         SET：Stack 顶部的值
 *                                              写入 table[i]，同时把
 *                                              这个值从 Stack 上弹出
 *      "rawset" 而不是普通 lua_settable，是因为我们确定 table 上
 *      没有 __newindex 之类的 Metamethod 需要触发（Metatable 是
 *      Chapter 11-14 的主题），rawseti 直接写、更快、语义也更清楚。
 *   3. 循环结束后，Stack 顶部正好剩下这张已经填满的 Table，
 *      直接 return 1 交给 Lua。
 */
static int l_split(lua_State *L)
{
    size_t len;
    const char *src = lua_tolstring(L, 1, &len);
    if (src == NULL) {
        return luaL_error(L, "split: 参数 #1 必须是 string");
    }
    size_t sep_len;
    const char *sep = lua_tolstring(L, 2, &sep_len);
    if (sep == NULL || sep_len != 1) {
        return luaL_error(L, "split: 参数 #2 必须是长度为 1 的分隔符 string");
    }
    char sep_ch = sep[0];

    lua_newtable(L);           /* Stack: [1]=s [2]=sep [3]=新 table */
    int table_idx = lua_gettop(L);
    lua_Integer i = 1;          /* Lua Table 下标从 1 开始，是约定，不是强制 */

    const char *part_start = src;
    for (size_t pos = 0; pos <= len; pos++) {
        int at_end = (pos == len);
        int at_sep = (!at_end && src[pos] == sep_ch);
        if (at_end || at_sep) {
            size_t part_len = (size_t)((src + pos) - part_start);
            lua_pushlstring(L, part_start, part_len); /* PUSH：子串 */
            lua_rawseti(L, table_idx, i);              /* SET：table[i] = 子串，并弹出 */
            i++;
            part_start = src + pos + 1;
        }
    }

    /* 此刻 Stack 顶部正是 table_idx 那张 Table：return 1 交给 Lua */
    return 1;
}

static const luaL_Reg str_ext_funcs[] = {
    {"reverse",  l_reverse},
    {"trim",     l_trim},
    {"to_upper", l_to_upper},
    {"split",    l_split},
    {NULL, NULL}
};

/* 注册方式与 Project 1 完全一致：newtable + setfuncs + setglobal，
 * 把上面 4 个 C Function 打包进一张名为 str_ext 的 Lua Table。 */
static void register_str_ext(lua_State *L)
{
    lua_newtable(L);
    luaL_setfuncs(L, str_ext_funcs, 0);
    lua_setglobal(L, "str_ext");
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    register_str_ext(L);

    if (luaL_dofile(L, "str_ext_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
