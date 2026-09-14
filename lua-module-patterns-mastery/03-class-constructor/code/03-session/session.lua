-- session.lua
--
-- Class / Constructor Module：Session 是类型（Class Table），只装
-- login/logout/is_authenticated 三个方法。每个 Session.new(user)
-- 产生一个独立 Object，拥有自己的 user / password（这里用一个固定的
-- "正确密码"模拟真实校验）和 authenticated 标志。

local Session = {}
Session.__index = Session

-- 模拟的"正确密码"数据源：真实系统里这会来自用户数据库，
-- 这里简化成常量，只是为了让 login() 里有真实的成功/失败分支。
local VALID_PASSWORD = "correct-password"

function Session.new(user)
    local self = setmetatable({}, Session)
    self.user = user
    self.authenticated = false   -- Object State：只属于这一个 Session 实例
    return self
end

--- session:login(password) -> boolean, string|nil
-- 密码正确才把 self.authenticated 置为 true；这个赋值只发生在
-- 当前实例的 Instance Table 上，不会影响任何其他 Session 实例。
function Session:login(password)
    if password ~= VALID_PASSWORD then
        self.authenticated = false
        return false, "invalid password"
    end
    self.authenticated = true
    return true
end

function Session:logout()
    self.authenticated = false
end

function Session:is_authenticated()
    return self.authenticated
end

function Session:describe()
    return string.format(
        "Session(user=%s, authenticated=%s)",
        self.user, tostring(self.authenticated)
    )
end

return Session
