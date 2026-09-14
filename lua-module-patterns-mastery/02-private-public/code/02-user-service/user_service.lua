--
-- user_service.lua
--
-- Pattern 2: Private + Public Module (Encapsulation)
--
-- Public API : create(input), find(id), delete(id)
-- Private impl: validate(input), normalize(input), generate_id()
--
-- Module-level State: `users` and `next_id` below are created exactly once,
-- the first time this module is require()'d, and are shared by every
-- caller that does `require("user_service")` for the rest of the process
-- lifetime (Lua's `require` cache returns the same table M on every call,
-- so it is also the same closure over `users`/`next_id`). This is exactly
-- the "Module 级 State" foreshadowed in the Pattern 2 architecture doc --
-- full treatment of require()'s caching behavior belongs to the separate
-- 06-require-and-state-mental-model.md document, not here.
--

local M = {}

--------------------------------------------------------------------------
-- Module-level State
--------------------------------------------------------------------------

local users = {}   -- id (number) -> { id = ..., name = ..., email = ... }
local next_id = 1

--------------------------------------------------------------------------
-- Private Implementation
--------------------------------------------------------------------------

-- validate: the gatekeeper. Every private helper downstream of validate()
-- is allowed to assume `input.name` and `input.email` are already
-- well-formed strings -- validate() is where that guarantee is established,
-- and it is only ever reached through the public M.create() path.
local function validate(input)
    if type(input) ~= "table" then
        return false, "input must be a table"
    end
    if type(input.name) ~= "string" or #input.name == 0 then
        return false, "name must be a non-empty string"
    end
    if type(input.email) ~= "string" or not input.email:match("^[^@%s]+@[^@%s]+%.[^@%s]+$") then
        return false, "email must look like user@host.tld"
    end
    return true
end

-- normalize: assumes validate() already ran. It does not re-check types --
-- if it were called with malformed input directly (which cannot happen from
-- outside the module, since normalize is not on M), it would fail with a
-- raw Lua error instead of a clean user-facing message. That asymmetry is
-- intentional: normalize's contract is "input already passed validate()".
local function normalize(input)
    return {
        name = input.name:gsub("^%s+", ""):gsub("%s+$", ""),
        email = input.email:lower(),
    }
end

-- generate_id: pure module-internal counter logic. Nothing about "how ids
-- are generated" is part of the module's public contract -- callers only
-- ever see the resulting id on the returned user record.
local function generate_id()
    local id = next_id
    next_id = next_id + 1
    return id
end

--------------------------------------------------------------------------
-- Public API
--------------------------------------------------------------------------

-- M.create: validates first, normalizes second, then touches State.
-- This ordering is the module's real responsibility -- private helpers
-- are not required to defend themselves against bad input because
-- M.create is the only door they can be reached through.
function M.create(input)
    local ok, err = validate(input)
    if not ok then
        return nil, err
    end

    local clean = normalize(input)
    local id = generate_id()

    local user = {
        id = id,
        name = clean.name,
        email = clean.email,
    }
    users[id] = user

    return user
end

function M.find(id)
    local user = users[id]
    if not user then
        return nil, "no user with id " .. tostring(id)
    end
    return user
end

function M.delete(id)
    if not users[id] then
        return false, "no user with id " .. tostring(id)
    end
    users[id] = nil
    return true
end

return M
