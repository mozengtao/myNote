-- policy.lua
--
-- 运行环境说明：本脚本需要 OpenResty（或 Nginx + lua-nginx-module）才能真正运行，
-- 通过 access_by_lua_file 挂载到某个 location 的 access 阶段。
-- 本文只验证脚本逻辑正确性（语法、ngx.* API 用法、决策分支是否完整），
-- 不在本机搭建真实的 Nginx/OpenResty 环境执行它。
--
-- 职责边界（这正是本示例要说明的 Policy 本质）：
--   Nginx Core（C）已经拥有 socket、event loop、HTTP 协议解析、
--   连接管理这些 Mechanism —— 请求怎么被接收、怎么被解析成
--   method/uri/headers，全部由 C 完成，这个脚本运行的时候，
--   一个结构完整的 HTTP 请求已经摆在 ngx.var / ngx.req 里了。
--   这个脚本唯一要做的事情，是在"access"这个阶段回答一个
--   yes/no 问题："这个请求该不该被继续处理？"
--   它不解析协议、不管理连接、不新增任何 Nginx 原本没有的能力，
--   只是在已有能力（继续处理 / 提前终止）之间做一次选择。

-- 规则 1：只允许 GET / POST 两种方法
local allowed_methods = {
    GET = true,
    POST = true,
}

-- 规则 2：路径黑名单，命中直接拒绝（常见的敏感路径探测/扫描器路径）
local denied_paths = {
    ["/admin"] = true,
    ["/.env"] = true,
    ["/wp-login.php"] = true,
}

-- 规则 3：必须携带的自定义 Header（简化的 API Key 校验，
-- 真实场景通常还要校验值是否合法，这里只检查是否存在）
local required_header = "X-Api-Key"

-- ========== 1. check method ==========
local method = ngx.var.request_method
if not allowed_methods[method] then
    ngx.log(ngx.WARN, "policy: method not allowed: ", method)
    return ngx.exit(ngx.HTTP_NOT_ALLOWED)
end

-- ========== 2. check path（黑名单） ==========
local uri = ngx.var.uri
if denied_paths[uri] then
    ngx.log(ngx.WARN, "policy: path denied: ", uri)
    return ngx.exit(ngx.HTTP_FORBIDDEN)
end

-- ========== 3. check headers（必须携带 X-Api-Key） ==========
local headers = ngx.req.get_headers()
if not headers[required_header] then
    ngx.log(ngx.WARN, "policy: missing required header: ", required_header)
    return ngx.exit(ngx.HTTP_FORBIDDEN)
end

-- ========== 4. allow ==========
-- 不调用 ngx.exit，意味着 access 阶段正常结束，
-- 请求会继续往下走到后续阶段（content / proxy_pass 等），
-- 这就是"放行"在 access_by_lua 里的具体含义：什么都不做，让它继续。
ngx.log(ngx.INFO, "policy: request allowed: ", method, " ", uri)
