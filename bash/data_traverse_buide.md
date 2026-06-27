# Shell 脚本数据遍历最佳实践及心智模式

> **核心原则：不要先想"怎么写 `for` 循环"，而要先想"数据是什么形态"。**
>
> Shell 中遍历方式不是只有 `for`，而是根据数据来源和数据结构选择最合适的遍历方式。

---

# 一、Shell 数据遍历的统一心智模型

从数据流的角度看，Shell 中几乎所有遍历都可以抽象为：

```
                Data Source
                     │
      ┌──────────────┼─────────────────┐
      │              │                 │
      ▼              ▼                 ▼
   Array          File/Pipe        Command Output
      │              │                 │
      └──────────────┼─────────────────┘
                     ▼
                Iterator（迭代器）
          ┌──────────┴──────────┐
          │                     │
          ▼                     ▼
        for                 while read
          │                     │
          └──────────┬──────────┘
                     ▼
             Business Logic
                     │
                     ▼
                  Output
```

整个流程实际上就是：

```
Producer
      │
      ▼
Iterator
      │
      ▼
Consumer
```

---

# 二、首先判断数据来源

写循环之前，先问自己：

```
数据来自哪里？
```

```
                    Data Source
                          │
      ┌──────────┬────────┼─────────┬────────────┐
      │          │        │         │            │
      ▼          ▼        ▼         ▼            ▼
    Array      File    Pipeline   Command     Function Args
```

不同的数据来源，对应不同的遍历方式。

---

# 三、场景一：遍历 Bash 数组（★★★★★）

适用于：

- 固定列表
- 配置数据
- 函数内部数据

推荐：

```bash
users=(
    alice
    bob
    charlie
)

for user in "${users[@]}"
do
    greet "$user"
done
```

心智模型：

```
users[]

↓

for

↓

greet
```

永远使用：

```bash
"${array[@]}"
```

不要：

```bash
${array[*]}
```

更不要：

```bash
${array[@]}
```

未加引号。

---

# 四、场景二：遍历函数参数（★★★★★）

推荐：

```bash
process()
{
    local arg

    for arg in "$@"
    do
        echo "$arg"
    done
}
```

调用：

```bash
process a b c
```

心智模型：

```
"$@"

↓

for

↓

process
```

不要：

```bash
for arg in $@
```

---

# 五、场景三：遍历文件（★★★★★）

推荐：

```bash
while IFS= read -r line
do
    echo "$line"
done < file.txt
```

数据流：

```
file

↓

stdin

↓

read

↓

process
```

这是最经典的 Shell 模式。

不要：

```bash
for line in $(cat file)
```

原因：

- Word Splitting
- Globbing
- 空格丢失
- TAB 丢失
- 空行丢失

---

# 六、场景四：遍历命令输出（★★★★★）

推荐：

```bash
find . -type f |
while IFS= read -r file
do
    echo "$file"
done
```

数据流：

```
find

↓

stdout

↓

read

↓

process
```

不要：

```bash
for file in $(find .)
```

---

# 七、场景五：遍历包含空格的文件名（★★★★★）

推荐：

```bash
find . -type f -print0 |
while IFS= read -r -d '' file
do
    echo "$file"
done
```

心智模型：

```
find

↓

NUL

↓

read -d ''

↓

process
```

适用于：

- 空格
- TAB
- 换行
- 特殊字符

这是 Linux 官方推荐方式。

---

# 八、场景六：遍历 Here Document（★★★★★）

例如：

```bash
while IFS= read -r user
do
    greet "$user"
done <<'EOF'
alice
bob
charlie
EOF
```

数据流：

```
Here Document

↓

stdin

↓

read

↓

process
```

适用于：

- 固定长列表
- SQL
- YAML
- JSON
- Hosts

---

# 九、场景七：遍历 CSV（★★★★★）

推荐：

```bash
while IFS=',' read -r name age city
do
    echo "$name"
done < users.csv
```

CSV：

```
alice,18,beijing
bob,20,shanghai
```

数据流：

```
CSV

↓

IFS

↓

Fields

↓

process
```

---

# 十、场景八：遍历 JSON（★★★★★）

例如：

```bash
jq -r '.users[].name' users.json |
while IFS= read -r user
do
    greet "$user"
done
```

数据流：

```
JSON

↓

jq

↓

stdout

↓

read
```

不要自己解析 JSON。

---

# 十一、场景九：遍历目录（★★★★★）

推荐：

```bash
for file in *.txt
do
    echo "$file"
done
```

这是：

```
pathname expansion
```

不是：

```
ls
```

不要：

```bash
for file in $(ls)
```

---

# 十二、场景十：递归遍历目录（★★★★★）

推荐：

```bash
find . -type f |
while IFS= read -r file
do
    echo "$file"
done
```

而不是：

```bash
for file in $(find .)
```

---

# 十三、场景十一：遍历关联数组（★★★★★）

```bash
declare -A users

users[alice]=18
users[bob]=20

for name in "${!users[@]}"
do
    echo "$name ${users[$name]}"
done
```

心智模型：

```
Keys

↓

for

↓

lookup Value
```

---

# 十四、场景十二：遍历数字（★★★★★）

推荐：

```bash
for i in {1..10}
do
    echo "$i"
done
```

或者：

```bash
for ((i=0; i<10; i++))
do
    echo "$i"
done
```

如果需要 C 风格：

```
for (( ))
```

如果需要固定序列：

```
{1..100}
```

---

# 十五、场景十三：遍历命令参数列表

例如：

```bash
set -- a b c

for arg
do
    echo "$arg"
done
```

等价于：

```bash
for arg in "$@"
```

---

# 十六、场景十四：并行遍历（★★★★★）

推荐：

```bash
cat hosts.txt |
xargs -P4 -I{} ssh {} hostname
```

数据流：

```
hosts.txt

↓

xargs

↓

ssh
```

适用于：

- SSH
- curl
- wget
- 编译
- 上传

---

# 十七、场景十五：流式处理日志（★★★★★）

例如：

```bash
tail -f app.log |
while IFS= read -r line
do
    echo "$line"
done
```

数据流：

```
tail

↓

pipe

↓

read

↓

process
```

真正的流处理。

---

# 十八、不同遍历方式的选择

```
                 数据是什么？
                       │
      ┌────────────────┼────────────────┐
      │                │                │
      ▼                ▼                ▼
    Array           Line Stream      File Names
      │                │                │
      ▼                ▼                ▼
     for         while read       read -d ''
```

进一步：

```
                 数据来自哪里？
                        │
        ┌───────────────┼────────────────┐
        │               │                │
        ▼               ▼                ▼
      Array          File/Pipe      Command
        │               │                │
        ▼               ▼                ▼
       for         while read      while read
```

---

# 十九、什么时候用 `for`？什么时候用 `while read`？

## 使用 `for`

适用于：

- Bash 数组
- 函数参数 (`"$@"`)
- Shell 自己生成的数据（如 `{1..10}`、`*.txt`）

例如：

```bash
for file in *.txt
```

```bash
for user in "${users[@]}"
```

```bash
for i in {1..10}
```

---

## 使用 `while read`

适用于：

- 文件
- 管道
- 命令输出
- Here Document
- JSON
- CSV
- 网络数据

例如：

```bash
while IFS= read -r line
```

这是 Shell 中最重要的数据遍历模式。

---

# 二十、不推荐的写法（反模式）

## 1. `for x in $(command)`

```bash
for f in $(find .)
```

❌

---

## 2. `for line in $(cat file)`

```bash
for line in $(cat file)
```

❌

---

## 3. `ls | while read`

```bash
ls |
while read file
```

通常应直接使用：

```bash
for file in *
```

或：

```bash
find .
```

---

## 4. 未引用数组

```bash
for x in ${arr[@]}
```

❌

应使用：

```bash
for x in "${arr[@]}"
```

---

# 二十一、Shell 数据遍历最佳实践总结

| 数据类型 | 推荐方式 | 推荐指数 |
|-----------|----------|----------|
| Bash 数组 | `for x in "${arr[@]}"` | ⭐⭐⭐⭐⭐ |
| 函数参数 | `for x in "$@"` | ⭐⭐⭐⭐⭐ |
| 文件（按行） | `while IFS= read -r line` | ⭐⭐⭐⭐⭐ |
| 管道输出 | `... \| while IFS= read -r` | ⭐⭐⭐⭐⭐ |
| 命令输出 | `while read < <(...)` 或管道 | ⭐⭐⭐⭐⭐ |
| 包含特殊字符的文件名 | `find -print0` + `read -d ''` | ⭐⭐⭐⭐⭐ |
| Here Document | `while read <<EOF` | ⭐⭐⭐⭐⭐ |
| CSV | `IFS=',' read` | ⭐⭐⭐⭐⭐ |
| JSON | `jq` + `while read` | ⭐⭐⭐⭐⭐ |
| 目录文件 | `for f in *.txt` | ⭐⭐⭐⭐⭐ |
| 递归目录 | `find` + `while read` | ⭐⭐⭐⭐⭐ |
| 关联数组 | `for k in "${!map[@]}"` | ⭐⭐⭐⭐⭐ |
| 数字序列 | `{1..N}` 或 `for (( ))` | ⭐⭐⭐⭐⭐ |
| 并行任务 | `xargs -P` | ⭐⭐⭐⭐⭐ |

---

# 二十二、最终心智模型（牢记）

```
                 Data Source
                      │
      ┌───────────────┬──────────────────────┐
      │               │                      │
      ▼               ▼                      ▼
    Array      File / Pipe / Command     Filename Stream
      │               │                      │
      ▼               ▼                      ▼
     for         while IFS= read        read -d ''（NUL）
      │               │                      │
      └───────────────┴──────────────────────┘
                      ▼
             Business Logic（处理逻辑）
                      │
                      ▼
                   Output
```

> **牢记一句话：**
>
> **`for` 适合遍历 Shell 已经拥有的数据（数组、参数、文件名展开、数字序列）；`while IFS= read -r` 适合遍历外部数据流（文件、管道、命令输出、Here Document、网络流、JSON、CSV 等）。先识别数据来源，再选择遍历方式，而不是先决定使用哪种循环语法。**