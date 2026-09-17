print("=== Stack Inspection: lua_gettop ===")
demo_gettop()

print("=== Stack Shrinking: lua_settop ===")
demo_settop()

print("=== Stack Shrinking: lua_pop ===")
demo_pop()

print("=== Stack Growth: lua_pushinteger ===")
demo_pushinteger()

print("=== Stack Growth: lua_pushvalue ===")
demo_pushvalue()

print("=== Stack Read: lua_tointeger (positive/negative index) ===")
demo_read()

print("=== Stack Rearrangement: lua_remove ===")
demo_remove()

print("=== Stack Rearrangement: lua_insert ===")
demo_insert()

print("=== Stack Rearrangement: lua_replace ===")
demo_replace()
