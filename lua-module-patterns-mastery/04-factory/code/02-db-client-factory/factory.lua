-- factory.lua
--
-- Factory Module：唯一知道有哪些具体数据库驱动的地方。
-- Application/Framework 不应该、也不需要直接依赖某一个具体的数据库实现——
-- 换数据库（sqlite -> postgres）应该只是改一个配置字符串,
-- 不应该触发 Application 代码里任何一行的修改。

local SqliteDriver = require("drivers.sqlite")
local PostgresDriver = require("drivers.postgres")
local MemoryDriver = require("drivers.memory")

local M = {}

local creators = {
    sqlite = SqliteDriver.new,
    postgres = PostgresDriver.new,
    memory = MemoryDriver.new,
}

--- create(kind, config) -> db client 实例 | (nil, err)
function M.create(kind, config)
    local creator = creators[kind]
    if not creator then
        return nil, string.format("factory: unknown db driver kind '%s'", tostring(kind))
    end
    return creator(config)
end

return M
