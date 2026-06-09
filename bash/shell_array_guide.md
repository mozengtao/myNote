# Linux Shell (Bash) 数组最佳实践指南

> 适用范围：
>
> - Bash 4.x/5.x
> - 不适用于 POSIX sh
> - 不适用于 dash
> - 大部分特性适用于 Linux 生产环境

---

# 1. 为什么要使用数组

很多 Shell 新手习惯这样写：

```bash
files=$(find . -type f)
```

后续：

```bash
for f in $files
do
    echo "$f"
done
```

这是典型错误。

原因：

- 空格被拆分
- Tab 被拆分
- 换行被拆分
- 文件名可能损坏

例如：

```text
file1.txt
hello world.txt
test file.txt
```

执行后会变成：

```text
file1.txt
hello
world.txt
test
file.txt
```

---

正确方案：

```bash
mapfile -t files < <(find . -type f)

for f in "${files[@]}"
do
    echo "$f"
done
```

数组保存：

```text
[
  file1.txt,
  hello world.txt,
  test file.txt
]
```

完整保留元素边界。

---

# 2. 数组类型

Bash支持两种数组：

---

## 索引数组（Indexed Array）

类似：

```c
char *argv[];
```

创建：

```bash
arr=("a" "b" "c")
```

访问：

```bash
echo "${arr[0]}"
```

输出：

```text
a
```

---

## 关联数组（Associative Array）

类似：

```go
map[string]string
```

或者：

```cpp
std::unordered_map
```

创建：

```bash
declare -A config
```

赋值：

```bash
config[user]="admin"
config[host]="10.1.1.1"
```

访问：

```bash
echo "${config[user]}"
```

---

# 3. 创建数组

---

## 方式1

```bash
arr=(a b c)
```

结果：

```text
index value
0     a
1     b
2     c
```

---

## 方式2

```bash
arr=(
    a
    b
    c
)
```

推荐。

工程代码可读性最好。

---

## 方式3

动态赋值

```bash
arr[0]="a"
arr[1]="b"
```

---

# 4. 数组长度

---

获取元素数量：

```bash
echo "${#arr[@]}"
```

输出：

```text
3
```

---

获取单个元素长度：

```bash
echo "${#arr[0]}"
```

输出：

```text
1
```

---

# 5. 遍历数组

---

## 推荐方式

```bash
for item in "${arr[@]}"
do
    echo "$item"
done
```

输出：

```text
a
b
c
```

---

## 获取索引

```bash
for i in "${!arr[@]}"
do
    printf '%s -> %s\n' "$i" "${arr[$i]}"
done
```

输出：

```text
0 -> a
1 -> b
2 -> c
```

---

# 6. 追加元素

---

## 单个追加

```bash
arr+=("d")
```

结果：

```text
a b c d
```

---

## 多个追加

```bash
arr+=(
    "d"
    "e"
    "f"
)
```

---

# 7. 删除元素

---

删除指定元素：

```bash
unset 'arr[1]'
```

原数组：

```text
0 a
1 b
2 c
```

结果：

```text
0 a
2 c
```

注意：

Bash不会自动重排索引。

---

重新构建：

```bash
arr=("${arr[@]}")
```

结果：

```text
0 a
1 c
```

---

# 8. 数组切片

类似Python：

---

原数组：

```bash
arr=(a b c d e)
```

---

取前3个：

```bash
echo "${arr[@]:0:3}"
```

输出：

```text
a b c
```

---

取后2个：

```bash
echo "${arr[@]: -2}"
```

输出：

```text
d e
```

注意：

```bash
: -2
```

前面有空格。

---

# 9. 数组展开规则

这是最重要的知识点之一。

---

假设：

```bash
arr=(
    "hello world"
    "foo bar"
)
```

---

## "${arr[@]}"

```bash
for i in "${arr[@]}"
do
    echo "$i"
done
```

输出：

```text
hello world
foo bar
```

保持元素边界。

---

## "${arr[*]}"

```bash
echo "${arr[*]}"
```

输出：

```text
hello world foo bar
```

变成一个字符串。

---

口诀：

```text
[@] = 多个参数

[*] = 一个参数
```

---

# 10. 命令构造最佳实践

这是工程环境最重要的应用。

---

错误：

```bash
cmd="ls -l /tmp"

$cmd
```

问题：

```text
word splitting
globbing
quote丢失
```

---

正确：

```bash
cmd=(
    ls
    -l
    /tmp
)

"${cmd[@]}"
```

---

复杂示例：

```bash
cmd=(
    ssh
    user@host
    "show version"
)

"${cmd[@]}"
```

---

实际工程：

```bash
cmd=(
    nomad
    alloc
    exec
    -task "$task"
    -job "$job"
    ncs_cli
    -u "$user"
)

"${cmd[@]}"
```

这是构造命令的黄金标准。

---

# 11. 接收函数参数

---

全部参数：

```bash
myfunc() {
    local args=("$@")

    printf '%s\n' "${args[@]}"
}
```

调用：

```bash
myfunc a b c
```

---

不要：

```bash
local args=$@
```

因为：

```text
参数边界丢失
```

---

# 12. 保存命令输出到数组

---

错误：

```bash
files=($(find .))
```

问题：

```text
word splitting
```

---

正确：

```bash
mapfile -t files < <(find .)
```

或者：

```bash
readarray -t files < <(find .)
```

---

结果：

```text
每行一个元素
```

---

# 13. 处理 find 输出

生产环境推荐：

```bash
while IFS= read -r -d '' file
do
    files+=("$file")
done < <(find . -type f -print0)
```

---

或者：

```bash
mapfile -d '' -t files < <(
    find . -type f -print0
)
```

---

适用于：

```text
空格
TAB
换行
Unicode
特殊字符
```

---

# 14. 关联数组最佳实践

---

定义：

```bash
declare -A cfg
```

---

赋值：

```bash
cfg[user]=admin
cfg[host]=10.1.1.1
cfg[port]=22
```

---

读取：

```bash
echo "${cfg[user]}"
```

---

遍历key：

```bash
for key in "${!cfg[@]}"
do
    printf '%s=%s\n' \
        "$key" \
        "${cfg[$key]}"
done
```

---

# 15. 实现Set集合

Bash没有Set。

可用关联数组模拟：

```bash
declare -A seen
```

---

插入：

```bash
seen["apple"]=1
```

---

判断存在：

```bash
if [[ -v seen["apple"] ]]
then
    echo found
fi
```

---

去重：

```bash
declare -A seen

for item in "${arr[@]}"
do
    seen["$item"]=1
done
```

---

# 16. 参数构建模式（工程级）

非常常见。

---

动态增加参数：

```bash
args=()
```

---

条件追加：

```bash
[[ -n $user ]] && args+=(
    --user "$user"
)

[[ -n $host ]] && args+=(
    --host "$host"
)
```

---

执行：

```bash
my_program "${args[@]}"
```

等价：

```text
my_program --user admin --host 10.1.1.1
```

---

# 17. getopt替代模式

例如：

```bash
curl
```

构造参数：

```bash
curl_args=(
    --silent
    --fail
)
```

---

按需增加：

```bash
[[ $verbose == yes ]] &&
curl_args+=(--verbose)

[[ -n $header ]] &&
curl_args+=(-H "$header")
```

---

执行：

```bash
curl "${curl_args[@]}" "$url"
```

---

# 18. 数组调试技巧

---

打印索引和值：

```bash
declare -p arr
```

输出：

```bash
declare -a arr=(
    [0]="a"
    [1]="b"
)
```

---

关联数组：

```bash
declare -p cfg
```

输出：

```bash
declare -A cfg=(
    [user]="admin"
)
```

---

生产环境非常推荐。

---

# 19. Shell工程最佳实践总结

## 永远使用数组构造命令

```bash
cmd=(...)
"${cmd[@]}"
```

---

## 永远使用

```bash
"${arr[@]}"
```

不要：

```bash
${arr[@]}
```

---

## 获取命令输出

使用：

```bash
mapfile
readarray
```

不要：

```bash
arr=($(command))
```

---

## 接收参数

使用：

```bash
local args=("$@")
```

不要：

```bash
local args=$@
```

---

## 处理find

使用：

```bash
find -print0
```

配合：

```bash
read -d ''
```

或者：

```bash
mapfile -d ''
```

---

## 动态参数

使用：

```bash
args+=(
    --key value
)
```

不要字符串拼接：

```bash
args="$args --key value"
```

---

## 调试

使用：

```bash
declare -p arr
```

而不是：

```bash
echo "${arr[@]}"
```

---

# 最终心智模型

```text
                Bash Array

                       │
                       │
        ┌──────────────┴──────────────┐
        │                             │
        ▼                             ▼

 Indexed Array               Associative Array

 arr[0]                      map[key]

        │
        │
        ▼

  Safe Element Storage

        │
        ▼

 "${arr[@]}"

        │
        ▼

 Preserve Boundaries

        │
        ▼

 Command Construction

        │
        ▼

 cmd=(
     ssh
     user@host
     "show version"
 )

 "${cmd[@]}"

        │
        ▼

 Production Grade Shell
```