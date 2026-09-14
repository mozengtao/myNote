--
-- parser.lua
--
-- Pattern 2: Private + Public Module (Encapsulation)
--
-- Public API : parse(str), validate(str)
-- Private impl: tokenize(str), parse_value(tokens, pos), parse_object(tokens, pos),
--               parse_array(tokens, pos)   (array support is a small extra private helper)
--
-- Callers only ever see M.parse / M.validate. The tokenizer and the
-- recursive-descent helpers are `local function`, closed over by this file's
-- upvalues, and never reachable from outside -- this is real encapsulation,
-- not a naming convention.
--

local M = {}

--------------------------------------------------------------------------
-- Private Implementation
--------------------------------------------------------------------------

-- tokenize: turn a raw JSON-like string into a flat array of tokens.
-- Token shape: { type = "STRING"|"NUMBER"|"BOOL"|"NULL"|"{"|"}"|"["|"]"|":"|",",
--                value = <lua value, only meaningful for STRING/NUMBER/BOOL> }
local function tokenize(str)
    local tokens = {}
    local i = 1
    local n = #str

    local function skip_ws()
        while i <= n do
            local c = str:sub(i, i)
            if c == " " or c == "\t" or c == "\n" or c == "\r" then
                i = i + 1
            else
                break
            end
        end
    end

    while true do
        skip_ws()
        if i > n then
            break
        end

        local c = str:sub(i, i)

        if c == "{" or c == "}" or c == "[" or c == "]" or c == ":" or c == "," then
            tokens[#tokens + 1] = { type = c }
            i = i + 1

        elseif c == "\"" then
            local j = i + 1
            local buf = {}
            local closed = false
            while j <= n do
                local cj = str:sub(j, j)
                if cj == "\"" then
                    closed = true
                    break
                elseif cj == "\\" and j < n then
                    -- Minimal escape handling: \" \\ \n \t
                    local nextc = str:sub(j + 1, j + 1)
                    if nextc == "n" then
                        buf[#buf + 1] = "\n"
                    elseif nextc == "t" then
                        buf[#buf + 1] = "\t"
                    else
                        buf[#buf + 1] = nextc
                    end
                    j = j + 2
                else
                    buf[#buf + 1] = cj
                    j = j + 1
                end
            end
            if not closed then
                error("tokenize: unterminated string starting at position " .. i)
            end
            tokens[#tokens + 1] = { type = "STRING", value = table.concat(buf) }
            i = j + 1

        elseif c:match("[%-%d]") then
            local j = i
            while j <= n and str:sub(j, j):match("[%-%d%.eE%+]") do
                j = j + 1
            end
            local numstr = str:sub(i, j - 1)
            local num = tonumber(numstr)
            if not num then
                error("tokenize: invalid number literal '" .. numstr .. "'")
            end
            tokens[#tokens + 1] = { type = "NUMBER", value = num }
            i = j

        elseif str:sub(i, i + 3) == "true" then
            tokens[#tokens + 1] = { type = "BOOL", value = true }
            i = i + 4

        elseif str:sub(i, i + 4) == "false" then
            tokens[#tokens + 1] = { type = "BOOL", value = false }
            i = i + 5

        elseif str:sub(i, i + 3) == "null" then
            tokens[#tokens + 1] = { type = "NULL" }
            i = i + 4

        else
            error("tokenize: unexpected character '" .. c .. "' at position " .. i)
        end
    end

    return tokens
end

-- Forward declaration: parse_value and parse_object/parse_array are mutually
-- recursive (an object's value can itself be an object/array, and vice versa).
local parse_value
local parse_object
local parse_array

-- parse_object: assumes tokens[pos].type == "{" (checked by parse_value,
-- the only caller). Private helpers may assume already-validated input --
-- validating "is this really an object" is parse_value's job, not this
-- function's job.
parse_object = function(tokens, pos)
    local obj = {}
    pos = pos + 1 -- consume "{"

    if tokens[pos] and tokens[pos].type == "}" then
        return obj, pos + 1
    end

    while true do
        local key_tok = tokens[pos]
        if not key_tok or key_tok.type ~= "STRING" then
            error("parse_object: expected string key at token " .. tostring(pos))
        end
        local key = key_tok.value
        pos = pos + 1

        local colon_tok = tokens[pos]
        if not colon_tok or colon_tok.type ~= ":" then
            error("parse_object: expected ':' after key '" .. key .. "'")
        end
        pos = pos + 1

        local value, next_pos = parse_value(tokens, pos)
        obj[key] = value
        pos = next_pos

        local sep = tokens[pos]
        if sep and sep.type == "," then
            pos = pos + 1
        elseif sep and sep.type == "}" then
            pos = pos + 1
            break
        else
            error("parse_object: expected ',' or '}' at token " .. tostring(pos))
        end
    end

    return obj, pos
end

-- parse_array: same trust contract as parse_object -- only ever called by
-- parse_value after it has already confirmed tokens[pos].type == "[".
parse_array = function(tokens, pos)
    local arr = {}
    pos = pos + 1 -- consume "["

    if tokens[pos] and tokens[pos].type == "]" then
        return arr, pos + 1
    end

    while true do
        local value, next_pos = parse_value(tokens, pos)
        arr[#arr + 1] = value
        pos = next_pos

        local sep = tokens[pos]
        if sep and sep.type == "," then
            pos = pos + 1
        elseif sep and sep.type == "]" then
            pos = pos + 1
            break
        else
            error("parse_array: expected ',' or ']' at token " .. tostring(pos))
        end
    end

    return arr, pos
end

-- parse_value: the dispatcher. This is the only function allowed to look at
-- a token and decide "what kind of value is this" -- parse_object/parse_array
-- never re-check the token type they were dispatched on.
parse_value = function(tokens, pos)
    local tok = tokens[pos]
    if not tok then
        error("parse_value: unexpected end of input")
    end

    if tok.type == "{" then
        return parse_object(tokens, pos)
    elseif tok.type == "[" then
        return parse_array(tokens, pos)
    elseif tok.type == "STRING" or tok.type == "NUMBER" or tok.type == "BOOL" then
        return tok.value, pos + 1
    elseif tok.type == "NULL" then
        return nil, pos + 1
    else
        error("parse_value: unexpected token '" .. tostring(tok.type) .. "'")
    end
end

--------------------------------------------------------------------------
-- Public API
--------------------------------------------------------------------------

-- M.parse: the module's only entry point for turning a string into data.
-- Responsible for input validation (is `str` even a string?) before handing
-- off to the private helpers, which assume that contract is already met.
function M.parse(str)
    if type(str) ~= "string" then
        return nil, "parse: expected a string, got " .. type(str)
    end

    local ok, tokens_or_err = pcall(tokenize, str)
    if not ok then
        return nil, tokens_or_err
    end
    local tokens = tokens_or_err

    if #tokens == 0 then
        return nil, "parse: empty input"
    end

    local ok2, value_or_err, pos_or_nil = pcall(parse_value, tokens, 1)
    if not ok2 then
        return nil, value_or_err
    end

    local value, pos = value_or_err, pos_or_nil
    if pos <= #tokens then
        return nil, "parse: trailing tokens after position " .. pos
    end

    return value
end

-- M.validate: built on top of M.parse, not on the private helpers directly.
-- Even the module's own public functions prefer to call other public
-- functions when possible -- this is what keeps the private surface free
-- to change.
function M.validate(str)
    local value, err = M.parse(str)
    if err then
        return false, err
    end
    return true
end

return M
