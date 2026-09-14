--
-- app.lua
--
-- Framework/Caller role: only ever calls user_service.create / .find / .delete.
-- validate / normalize / generate_id do not exist on the returned table.
--

local user_service = require("user_service")

print("== create: success path ==")
local alice, err = user_service.create({ name = "  Alice  ", email = "Alice@Example.com" })
if not alice then
    print("unexpected failure: " .. tostring(err))
    os.exit(1)
end
print(string.format("created user id=%d name=%q email=%q", alice.id, alice.name, alice.email))

print()
print("== create: failure path (bad email) ==")
local bad, bad_err = user_service.create({ name = "Bob", email = "not-an-email" })
print("bad, err ->", bad, bad_err)

print()
print("== create: failure path (missing name) ==")
local bad2, bad2_err = user_service.create({ email = "carol@example.com" })
print("bad2, err ->", bad2, bad2_err)

print()
print("== find: success and failure ==")
local found, find_err = user_service.find(alice.id)
print("find(alice.id) ->", found and found.name, find_err)

local missing, missing_err = user_service.find(999)
print("find(999)      ->", missing, missing_err)

print()
print("== module-level State: create another user, both share the same `users` table ==")
local dave = user_service.create({ name = "Dave", email = "dave@example.com" })
print(string.format("created user id=%d name=%q", dave.id, dave.name))
local alice_again = user_service.find(alice.id)
print("find(alice.id) still works ->", alice_again.name)

print()
print("== delete: success and failure ==")
local deleted, delete_err = user_service.delete(alice.id)
print("delete(alice.id) ->", deleted, delete_err)

local not_found, not_found_err = user_service.delete(alice.id)
print("delete(alice.id) again ->", not_found, not_found_err)

print("find(alice.id) after delete ->", user_service.find(alice.id))

-- Only M.create / M.find / M.delete exist on the returned table. Uncommenting
-- the line below would fail with "attempt to call a nil value (field
-- 'validate')" -- validate/normalize/generate_id are truly unreachable:
--
-- user_service.validate({})
