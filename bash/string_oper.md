```bash
# 基础变量用于所有测试
str1="hello_world"
str2="hello_standby"
str3="rpdsim_001"
empty_str=""
num_str="123abc"

# ====================== 1. 空字符串判断 ======================
# -z：字符串长度为0 为空则true
[[ -z $empty_str ]]
echo $?  # 0
[[ -z $str1 ]]
echo $?  # 1

# -n：字符串非空有内容则true
[[ -n $empty_str ]]
echo $?  # 1
[[ -n $str1 ]]
echo $?  # 0

# 简写：[[ $var ]] 等价 [[ -n $var ]]
[[ $empty_str ]]
echo $?  # 1
[[ $str1 ]]
echo $?  # 0

# 取反 !
[[ ! $str1 ]]
echo $?  # 1

# ====================== 2. 字符串完全相等 == / = ======================
# == 与 = 在 [[ ]] 内完全等价
[[ $str1 == "hello_world" ]]
echo $?  # 0
[[ $str1 = "hello_world" ]]
echo $?  # 0

[[ $str1 == "hello" ]]
echo $?  # 1

# ====================== 3. 字符串不等于 != ======================
[[ $str1 != "hello" ]]
echo $?  # 0
[[ $str1 != "hello_world" ]]
echo $?  # 1

# ====================== 4. Glob 通配符模糊匹配（* ? []，非正则） ======================
# 包含指定子串
[[ $str2 == *standby* ]]
echo $?  # 0
[[ $str1 == *standby* ]]
echo $?  # 1

# 以前缀开头
[[ $str3 == rpdsim* ]]
echo $?  # 0

# 以后缀结尾
[[ $str2 == *standby ]]
echo $?  # 0

# 单字符通配 ?
[[ "abc" == a?c ]]
echo $?  # 0

# 字符集 [] 匹配单个字符
[[ "a5" == a[0-9] ]]
echo $?  # 0

# 通配表达式加引号会变成纯字面匹配，* 失去通配含义
[[ $str1 == "*world" ]]
echo $?  # 1

# ====================== 5. 字典序大小比较 < > （ASCII 顺序） ======================
[[ "apple" < "banana" ]]
echo $?  # 0
[[ "zoo" > "apple" ]]
echo $?  # 0
[[ "123" < "45" ]]
echo $?  # 0

# ====================== 6. 正则匹配 =~ 核心用法 ======================
# 6.1 简单或逻辑匹配 包含 standby 或 rpdsim
[[ $str2 =~ standby|rpdsim ]]
echo $?  # 0
[[ $str2 =~ standby || $str2 =~ rpdsim ]]
echo $?  # 0
[[ $str3 =~ standby|rpdsim ]]
echo $?  # 0
[[ $str1 =~ standby|rpdsim ]]
echo $?  # 1

# 6.2 ^ 开头锚定：以 rpdsim 起始
[[ $str3 =~ ^rpdsim ]]
echo $?  # 0

# 6.3 $ 结尾锚定：以 standby 结尾
[[ $str2 =~ standby$ ]]
echo $?  # 0

# 6.4 捕获分组，匹配内容存入 BASH_REMATCH 数组
test_txt="vmc_rpdsim_99"
[[ $test_txt =~ (rpdsim)_[0-9]+ ]]
echo ${BASH_REMATCH[0]}  # rpdsim_99
echo ${BASH_REMATCH[1]}  # rpdsim

# 6.5 正则存入变量再匹配（推荐复杂正则写法）
reg="standby|rpdsim"
[[ $str2 =~ $reg ]]
echo $?  # 0

# 6.6 取反不匹配
[[ ! $str1 =~ standby|rpdsim ]]
echo $?  # 0

# ⚠️ 禁忌：右侧正则整体不能双引号包裹，否则失效
[[ $str2 =~ "standby|rpdsim" ]]
echo $?  # 1

# ====================== 7. 多条件逻辑组合 && || ! ======================
# 逻辑与 &&
[[ $str2 == *standby* && $empty_str == "" ]]
echo $?  # 0

# 逻辑或 ||
[[ $str1 == *world* || $str3 == *rpdsim* ]]
echo $?  # 0

# 混合取反
[[ ! $str1 =~ rpdsim && $str2 =~ standby ]]
echo $?  # 0
```