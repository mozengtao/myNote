-- http.lua
--
-- Table Module：HTTP 协议相关的常量集合 + 少量配套的纯函数。
-- 对应一个 C header 里 `#define GET "GET"` / `#define STATUS_OK 200`
-- 这类常量定义，外加几个和这些常量搭配使用的工具函数。
-- 不持有任何运行时状态，METHODS/STATUS 都是加载时就固定的只读数据。

local M = {}

--- HTTP 方法常量
M.METHODS = {
    GET = "GET",
    POST = "POST",
    PUT = "PUT",
    DELETE = "DELETE",
    PATCH = "PATCH",
}

--- HTTP 状态码 -> 状态短语
M.STATUS = {
    [200] = "OK",
    [201] = "Created",
    [204] = "No Content",
    [301] = "Moved Permanently",
    [400] = "Bad Request",
    [401] = "Unauthorized",
    [404] = "Not Found",
    [500] = "Internal Server Error",
}

--- build_header(name, value) -> string
-- 拼出一行合法的 HTTP header 文本。
-- 例如 build_header("Content-Type", "application/json")
-- 返回 "Content-Type: application/json"
function M.build_header(name, value)
    return string.format("%s: %s", name, value)
end

--- status_text(code) -> string
-- 查状态码对应的短语，查不到已知状态码时返回 "Unknown Status"。
function M.status_text(code)
    return M.STATUS[code] or "Unknown Status"
end

--- is_success(code) -> boolean
-- 判断状态码是否落在 2xx 区间。
function M.is_success(code)
    return code >= 200 and code < 300
end

return M
