print("=== 示例 1: positive index 读固定参数 ===")
demo_fixed_args(11, 22, 33)

print("=== 示例 2: 参数个数不确定时，用 negative index 取回刚 push 的结果 ===")
local s1 = demo_dynamic_result(1, 2, 3)
print("sum(1,2,3) =", s1)
local s2 = demo_dynamic_result(5, 6, 7, 8, 9)
print("sum(5,6,7,8,9) =", s2)

print("=== 示例 3: 循环里反复 push，始终用 -1 取最新值 ===")
demo_loop_latest()
