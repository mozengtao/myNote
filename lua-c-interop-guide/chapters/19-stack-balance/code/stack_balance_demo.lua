print("== 示例 2a: push 了 2 个值却 return 1 ==")
local bad_sum = bad_over_push_under_return(3, 4)
print("bad_over_push_under_return(3,4) 只拿到 1 个返回值 =", bad_sum,
      "（期望的『和』7 被悄悄丢弃，这其实是『积』12）")
local good_sum, good_product = good_over_push_under_return(3, 4)
print("good_over_push_under_return(3,4) =", good_sum, good_product, "（和, 积 都拿到了）")

print()
print("== 示例 2b: 只 push 了 1 个值却 return 2 ==")
local x, y = bad_under_push_over_return(5)
print("bad_under_push_over_return(5) =", x, y,
      "（第一个返回值 x 其实是参数 a 本身，不是有意 push 的结果！）")
local gx, gy = good_under_push_over_return(5)
print("good_under_push_over_return(5) =", gx, gy)

print()
print("== 示例 3: lua_error 之后的代码是死代码 ==")
local ok1, err1 = pcall(bad_after_error, -1)
print("pcall(bad_after_error, -1) -> ok=", ok1, "err=", err1)
print("（注意上面没有打印『这一行绝不会被打印』，证明 luaL_error 之后的代码确实没有执行）")
local ok2, r2 = pcall(bad_after_error, 3)
print("pcall(bad_after_error, 3) -> ok=", ok2, "result=", r2)
local ok3, err3 = pcall(good_after_error, -1)
print("pcall(good_after_error, -1) -> ok=", ok3, "err=", err3)

print()
print("== 示例 4: 把 Stack index 存成全局变量 ==")
bad_stash_index({ value = "first-call-table" })
bad_use_stashed_index(42)
good_store_ref({ value = "stable-value" })
good_use_ref(42) -- 传什么参数都不影响取回结果，因为用的是 Registry Reference 而不是 Stack index

print()
print("== 示例 5: lua_next 循环忘记 pop ==")
local t = { a = 1, b = 2, c = 3 }
local ok5, err5 = pcall(bad_lua_next_leak, t)
print("pcall(bad_lua_next_leak, t) -> ok=", ok5, "err=", err5)
print("（第 1 轮之后栈顶变成了 value 而不是 key，第 2 次 lua_next 触发 'invalid key to next' 错误）")
good_lua_next_ok(t)
