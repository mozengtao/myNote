/*
 * config_parser_demo.c
 *
 * Project 3 · Configuration Parser
 *
 * 在 Chapter 06（table_lua_to_c_demo.c 里的 configure()，只读一层
 * 扁平字段）的基础上，做成一个更接近"真实应用"的配置解析器：
 *
 *   Lua Table（嵌套结构 + 数组部分）
 *       v  load_config()
 *   C configuration object（struct app_config，嵌套 struct）
 *       v  逐字段校验（luaL_error 报出清晰的字段路径）
 *   validation
 *       v  校验全部通过
 *   C application（这里简化为 print_app_config，证明 C 侧真的拿到了
 *                  正确的数据，而不是只是"没报错"）
 *
 * 关于"递归处理嵌套 table"：
 *   本例的嵌套只有一层（app.server / app.limits），所以没有写成一个
 *   自己调用自己的 C 函数。但 parse_server_config / parse_limits_config
 *   两个函数各自重复了**完全相同的三步套路**：
 *     lua_getfield（Get）-> 类型校验/取值（Read）-> lua_pop（清理）
 *   这正是"处理嵌套 table"的通用模式：每多一层嵌套，就是再套一次这三步。
 *   如果配置的嵌套深度是不固定的（比如任意深度的 JSON 风格配置），
 *   自然的做法就是把这三步写进一个真正递归调用自己的 C 函数，
 *   用一个 "path 前缀字符串" 参数一路拼出 "a.b.c" 这样的错误路径。
 *
 * 关于数组部分为什么选 lua_rawgeti 而不是 lua_next：
 *   - `features` 在语义上就是"一个有序列表"，不是"任意键值对集合"。
 *   - lua_rawlen() 直接给出这类纯数组 table 的长度（border），
 *     配合 lua_rawgeti(L, idx, i) 按 1..n 遍历，顺序确定、代码直白，
 *     且天然跳过了 hash 部分（如果脚本手误往 features 里塞了非数字键，
 *     会被安静忽略，而不是被当成"数组的一部分"误读）。
 *   - lua_next 更适合"不知道有哪些 key、需要遍历整张 table"的场景
 *     （Chapter 08），但这里我们已经知道要的是"数组部分"，用 lua_next
 *     反而要多写代码去过滤非整数 key，还要处理遍历顺序不保证的问题。
 */
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_HOST_LEN    64
#define MAX_FEATURE_LEN 32
#define MAX_FEATURES    8

/* ---- C configuration object：嵌套 struct，与 Lua Table 的嵌套结构一一对应 ---- */

typedef struct {
    char host[MAX_HOST_LEN];
    int  port;
} server_config_t;

typedef struct {
    int max_connections;
    int timeout_ms;
} limits_config_t;

typedef struct {
    server_config_t server;
    limits_config_t limits;
    char features[MAX_FEATURES][MAX_FEATURE_LEN];
    int  feature_count;
} app_config_t;

/*
 * parse_server_config: 处理 outer_idx 位置的 table 里的 "server" 子 table。
 *
 * 进入前 Stack：... [outer_idx]=外层 table ...（栈顶不一定是 outer_idx）
 * 离开后 Stack：与进入前完全一致（Stack Balance）——
 *   这是本函数的 Stack Contract：无论内部经过多少次 push/pop，
 *   净效果必须是 0。
 */
static void parse_server_config(lua_State *L, int outer_idx, server_config_t *out)
{
    lua_getfield(L, outer_idx, "server");            /* GET: outer -> Stack */
    if (!lua_istable(L, -1)) {
        luaL_error(L, "server must be a table (got %s)", luaL_typename(L, -1));
    }
    int server_idx = lua_gettop(L);

    /* --- server.host ---
     * 同理，这里也用 lua_type() == LUA_TSTRING 严格判断，而不是
     * lua_isstring()：lua_isstring() 对数字也会返回真（数字总能转换
     * 成字符串），严格校验配置字段类型时同样不希望有这种隐式转换。 */
    lua_getfield(L, server_idx, "host");             /* GET: server -> Stack */
    if (lua_type(L, -1) != LUA_TSTRING) {
        luaL_error(L, "server.host must be a string (got %s)", luaL_typename(L, -1));
    }
    const char *host = lua_tostring(L, -1);
    size_t hostlen = strlen(host);
    if (hostlen == 0 || hostlen >= MAX_HOST_LEN) {
        luaL_error(L, "server.host must be 1..%d characters (got %zu)",
                   MAX_HOST_LEN - 1, hostlen);
    }
    strncpy(out->host, host, MAX_HOST_LEN - 1);
    out->host[MAX_HOST_LEN - 1] = '\0';
    lua_pop(L, 1);                                    /* 清理 host 临时值 */

    /* --- server.port ---
     * 注意：这里故意用 lua_type(L, -1) == LUA_TNUMBER 做严格类型判断，
     * 而不是 lua_isnumber()。lua_isnumber() 对"能转换成数字的字符串"
     * （比如 "8080"）也会返回真——这是 Lua 的隐式强转规则，写配置校验时
     * 几乎从来不是我们想要的行为（否则 port = "8080" 这种拼写错误就会
     * 被悄悄接受）。这个坑会在本项目 README 的 Failure 部分展开讲。 */
    lua_getfield(L, server_idx, "port");              /* GET: server -> Stack */
    if (lua_type(L, -1) != LUA_TNUMBER) {
        luaL_error(L, "server.port must be a number (got %s)", luaL_typename(L, -1));
    }
    lua_Number port = lua_tonumber(L, -1);
    if (port < 1 || port > 65535) {
        luaL_error(L, "server.port must be between 1 and 65535 (got %f)", (double)port);
    }
    out->port = (int)port;
    lua_pop(L, 1);                                     /* 清理 port 临时值 */

    lua_pop(L, 1);                                     /* 清理 server table 本身 */
}

/*
 * parse_limits_config: 与 parse_server_config 完全同构的第二次"进入嵌套 table"，
 * 有意重复同样的 Get -> Check -> Pop 套路，强调"处理嵌套 table 的模式是统一的，
 * 与具体字段无关"。
 */
static void parse_limits_config(lua_State *L, int outer_idx, limits_config_t *out)
{
    lua_getfield(L, outer_idx, "limits");
    if (!lua_istable(L, -1)) {
        luaL_error(L, "limits must be a table (got %s)", luaL_typename(L, -1));
    }
    int limits_idx = lua_gettop(L);

    lua_getfield(L, limits_idx, "max_connections");
    if (lua_type(L, -1) != LUA_TNUMBER) {
        luaL_error(L, "limits.max_connections must be a number (got %s)", luaL_typename(L, -1));
    }
    lua_Number max_conn = lua_tonumber(L, -1);
    if (max_conn <= 0 || max_conn > 1000000) {
        luaL_error(L, "limits.max_connections must be a positive integer (got %f)",
                   (double)max_conn);
    }
    out->max_connections = (int)max_conn;
    lua_pop(L, 1);

    lua_getfield(L, limits_idx, "timeout_ms");
    if (lua_type(L, -1) != LUA_TNUMBER) {
        luaL_error(L, "limits.timeout_ms must be a number (got %s)", luaL_typename(L, -1));
    }
    lua_Number timeout = lua_tonumber(L, -1);
    if (timeout <= 0) {
        luaL_error(L, "limits.timeout_ms must be a positive number (got %f)", (double)timeout);
    }
    out->timeout_ms = (int)timeout;
    lua_pop(L, 1);

    lua_pop(L, 1);
}

/*
 * parse_features: 处理数组部分。features 字段是可选的
 * （不写就当作空列表），但如果写了就必须是一个纯数组 table。
 */
static void parse_features(lua_State *L, int outer_idx, app_config_t *out)
{
    lua_getfield(L, outer_idx, "features");
    if (lua_isnil(L, -1)) {
        out->feature_count = 0;
        lua_pop(L, 1);
        return;
    }
    if (!lua_istable(L, -1)) {
        luaL_error(L, "features must be a table (got %s)", luaL_typename(L, -1));
    }

    int features_idx = lua_gettop(L);
    size_t n = lua_rawlen(L, features_idx);   /* 数组部分长度（border） */
    if (n > (size_t)MAX_FEATURES) {
        luaL_error(L, "features must have at most %d entries (got %zu)",
                   MAX_FEATURES, n);
    }

    for (size_t i = 1; i <= n; i++) {
        lua_rawgeti(L, features_idx, (lua_Integer)i);  /* GET: features[i] -> Stack */
        if (lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "features[%zu] must be a string (got %s)", i, luaL_typename(L, -1));
        }
        const char *feat = lua_tostring(L, -1);
        size_t flen = strlen(feat);
        if (flen == 0 || flen >= MAX_FEATURE_LEN) {
            luaL_error(L, "features[%zu] must be 1..%d characters (got %zu)",
                       i, MAX_FEATURE_LEN - 1, flen);
        }
        strncpy(out->features[i - 1], feat, MAX_FEATURE_LEN - 1);
        out->features[i - 1][MAX_FEATURE_LEN - 1] = '\0';
        lua_pop(L, 1);                                  /* 清理 features[i] 临时值 */
    }
    out->feature_count = (int)n;

    lua_pop(L, 1);                                       /* 清理 features table 本身 */
}

static void print_app_config(const app_config_t *cfg)
{
    printf("[C] load_config: 校验全部通过，app_config 内容如下：\n");
    printf("[C]   server.host             = %s\n", cfg->server.host);
    printf("[C]   server.port             = %d\n", cfg->server.port);
    printf("[C]   limits.max_connections  = %d\n", cfg->limits.max_connections);
    printf("[C]   limits.timeout_ms       = %d\n", cfg->limits.timeout_ms);
    printf("[C]   features (%d)           = [", cfg->feature_count);
    for (int i = 0; i < cfg->feature_count; i++) {
        printf("%s\"%s\"", i > 0 ? ", " : "", cfg->features[i]);
    }
    printf("]\n");
}

/*
 * l_load_config: Lua 侧唯一入口。
 *   load_config({ server = {...}, limits = {...}, features = {...} })
 *
 * Stack 进入时只有一个参数：[1] = 那张顶层 table。
 * 三个 parse_* 函数各自保证自己净 Stack Balance 为 0，
 * 所以 l_load_config 本身完全不需要关心中间过程用了多少临时槛位。
 */
static int l_load_config(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    app_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    parse_server_config(L, 1, &cfg.server);
    parse_limits_config(L, 1, &cfg.limits);
    parse_features(L, 1, &cfg);

    /* validation 全部通过之后才走到这里：C application 拿到的是
     * 完全校验过的数据，不需要再次判空/判范围。 */
    print_app_config(&cfg);

    return 0; /* 告诉 Lua VM：这次调用没有返回值 */
}

int main(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "load_config", l_load_config);

    if (luaL_dofile(L, "config_parser_demo.lua") != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
