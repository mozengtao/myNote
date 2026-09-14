--
-- app.lua
--
-- Framework/Caller role: only ever touches parser.parse / parser.validate.
-- Never touches parser.tokenize / parser.parse_value / parser.parse_object
-- because those names simply do not exist on the returned table M.
--

local parser = require("parser")

-- A small manual pretty-printer -- no external library needed. This lives
-- in app.lua (the caller), not in parser.lua, because "how to print a
-- result" is not the module's concern.
local function pretty(value, indent)
    indent = indent or ""
    local t = type(value)

    if t == "table" then
        -- Distinguish "array-ish" (1..n integer keys) from "object-ish".
        local is_array = true
        local n = 0
        for k in pairs(value) do
            n = n + 1
            if type(k) ~= "number" then
                is_array = false
            end
        end
        if is_array and n == #value then
            if n == 0 then
                io.write("[]")
                return
            end
            io.write("[\n")
            for i, v in ipairs(value) do
                io.write(indent .. "  ")
                pretty(v, indent .. "  ")
                if i < n then io.write(",") end
                io.write("\n")
            end
            io.write(indent .. "]")
        else
            local keys = {}
            for k in pairs(value) do keys[#keys + 1] = k end
            table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
            if #keys == 0 then
                io.write("{}")
                return
            end
            io.write("{\n")
            for idx, k in ipairs(keys) do
                io.write(indent .. "  " .. tostring(k) .. " = ")
                pretty(value[k], indent .. "  ")
                if idx < #keys then io.write(",") end
                io.write("\n")
            end
            io.write(indent .. "}")
        end
    elseif t == "string" then
        io.write(string.format("%q", value))
    elseif t == "nil" then
        io.write("null")
    else
        io.write(tostring(value))
    end
end

local sample = [[
{
    "name": "sensor-01",
    "active": true,
    "reading": -12.5,
    "tags": ["temp", "outdoor"],
    "calibration": null,
    "meta": {
        "vendor": "Acme",
        "version": 3
    }
}
]]

print("== parsing sample JSON ==")
local value, err = parser.parse(sample)
if not value then
    print("parse failed: " .. tostring(err))
    os.exit(1)
end

pretty(value)
print()

print()
print("== validate() checks ==")
print("valid sample     ->", parser.validate(sample))
print("valid broken json ->", parser.validate("{ \"a\": }"))

-- Only M.parse / M.validate exist on the returned table. Uncommenting the
-- line below would fail with "attempt to call a nil value (field
-- 'tokenize')" -- proof that the private helpers are not part of the
-- module's public surface:
--
-- parser.tokenize(sample)
