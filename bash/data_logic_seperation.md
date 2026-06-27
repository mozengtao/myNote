# Shell 脚本最佳实践 —— "数据"与"处理逻辑"分离

## 一、核心思想

优秀的 Shell 脚本，不是把数据直接写进逻辑，而是：

> **Data（数据）与 Logic（处理逻辑）分离。**

换句话说：

```
                Bad

for user in user1 user2 user3
do
    greet "$user"
done
```

应该思考为：

```
          Data Source
               │
               ▼
        Processing Logic
               │
               ▼
            Result
```

Shell 本质上也是一种"数据流语言"：

```
         Data
           │
           ▼
      read / for
           │
           ▼
     command/function
           │
           ▼
         Output
```

因此：

> **循环应该消费数据，而不是保存数据。**

---

# 二、推荐建立的统一心智模型

所有 Shell 数据都可以抽象成：

```
                Data Source
                     │
      ┌──────────────┼─────────────────┐
      │              │                 │
      ▼              ▼                 ▼
   Array        Here Document      File
      │              │                 │
      └──────────────┼─────────────────┘
                     ▼
               while read
                     或
                    for
                     │
                     ▼
             Processing Logic
                     │
                     ▼
                  Output
```

以后写脚本时，先考虑：

> **数据来自哪里？**

而不是：

> **for 后面应该写什么？**

---

# 三、各种数据来源及最佳实践

---
## 示例
```bash
不要把 for 看成"保存数据"的地方，而是把它看成消费者（Consumer）
                 Data Source
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
      Array      Here Document     File
        │             │             │
        └─────────────┼─────────────┘
                      ▼
               while read / for
                      │
                      ▼
                 greet "$user"

# 使用数组
users=(
    user1
    user2
    user3
)

users+=("user4")

for user in "${users[@]}"; do
    greet "$user"
done

users[]
   │
   ▼
for
   │
   ▼
greet

# 使用 Here Document
while IFS= read -r user; do
    greet "$user"
done <<'EOF'
user1
user2
user3
user4
user5
EOF

Here doc
   │
   ▼
  STDIN
   │
   ▼
for
   │
   ▼
greet

# 读取文件
while IFS= read -r user; do
    greet "$user"
done < users.txt

users.txt
     │
     ▼
   read
     │
     ▼
  greet

# 数组 + 函数参数
users=(
    user1
    user2
)

process_users() {
    local user

    for user in "$@"; do
        greet "$user"
    done
}

process_users "${users[@]}"
process_users "${admins[@]}"
process_users "${developers[@]}"

# 命令输出
get_users() {
    printf '%s\n' \
        user1 \
        user2 \
        user3
}

while IFS= read -r user; do
    greet "$user"
done < <(get_users)
```

## 场景一：固定少量数据（★★★★★）

例如：

```
user1
user2
user3
```

推荐：

```bash
users=(
    user1
    user2
    user3
)

for user in "${users[@]}"
do
    greet "$user"
done
```

心智模型：

```
users[]
   │
   ▼
 for
   │
   ▼
greet
```

优点：

- 数据独立
- 易维护
- 易修改
- 支持空格

不要：

```bash
for user in user1 user2 user3
```

因为：

```
Data

和

Logic

混在一起
```

---

## 场景二：固定长列表（★★★★★）

例如几十行配置：

推荐：

```bash
while IFS= read -r user
do
    greet "$user"
done <<'EOF'
user1
user2
user3
user4
user5
EOF
```

数据：

```
user1
user2
...
```

逻辑：

```
greet
```

完全分离。

优点：

- diff 清晰
- 一行一个元素
- 可注释
- 易复制

---

## 场景三：配置文件（★★★★★）

例如：

users.txt

```
user1
user2
user3
```

脚本：

```bash
while IFS= read -r user
do
    greet "$user"
done < users.txt
```

心智模型：

```
users.txt
      │
      ▼
   read line
      │
      ▼
    greet
```

符合 Unix：

```
Everything is a file
```

---

## 场景四：命令输出（★★★★★）

例如：

```bash
getent passwd
```

或者：

```bash
kubectl get pod
```

推荐：

```bash
kubectl get pod -o name |
while IFS= read -r pod
do
    echo "$pod"
done
```

或者：

```bash
while IFS= read -r pod
do
    echo "$pod"
done < <(
    kubectl get pod -o name
)
```

心智模型：

```
Command

↓

stdout

↓

read

↓

process
```

而不是：

```bash
for pod in $(kubectl ...)
```

---

## 场景五：函数参数（★★★★★）

例如：

```bash
process_users()
{
    local user

    for user in "$@"
    do
        greet "$user"
    done
}
```

调用：

```bash
process_users \
    user1 \
    user2 \
    user3
```

或者：

```bash
process_users "${users[@]}"
```

函数完全不知道：

```
数据来自哪里
```

符合：

```
High Cohesion

Low Coupling
```

---

## 场景六：Pipeline（★★★★★）

例如：

```bash
grep ERROR app.log |
while IFS= read -r line
do
    echo "$line"
done
```

心智模型：

```
grep

↓

stdout

↓

while read

↓

process
```

典型 Unix 风格。

---

## 场景七：JSON 数据（★★★★★）

例如：

```bash
jq -r '.users[].name' users.json |
while IFS= read -r user
do
    greet "$user"
done
```

数据来源：

```
JSON
```

处理逻辑：

```
greet
```

完全解耦。

---

## 场景八：find 输出（★★★★★）

推荐：

```bash
find . -type f -print0 |
while IFS= read -r -d '' file
do
    echo "$file"
done
```

而不是：

```bash
for file in $(find ...)
```

原因：

```
find

↓

stdout

↓

read

↓

process
```

能够正确处理：

- 空格
- 换行
- 特殊字符

---

## 场景九：CSV 数据（★★★★★）

例如：

```
alice,18
bob,20
```

推荐：

```bash
while IFS=',' read -r name age
do
    echo "$name : $age"
done < users.csv
```

数据：

```
CSV
```

逻辑：

```
echo
```

天然分离。

---

## 场景十：SSH 多主机（★★★★★）

例如：

hosts.txt

```
host1
host2
host3
```

脚本：

```bash
while IFS= read -r host
do
    ssh "$host" hostname
done < hosts.txt
```

不要：

```bash
for host in host1 host2 host3
```

---

## 场景十一：xargs 并行（★★★★★）

例如：

```
hosts.txt

↓

xargs

↓

ssh

↓

hostname
```

```
cat hosts.txt |
xargs -P4 -I{} ssh {} hostname
```

数据：

```
hosts.txt
```

逻辑：

```
ssh hostname
```

完全分离。

---

# 四、数据来源的统一抽象

工程上，可以把所有数据统一抽象成：

```
                   Data Source
                        │
        ┌───────────────┼──────────────────┐
        │               │                  │
        ▼               ▼                  ▼
     Array            File             Command
        │               │                  │
        ├───────────────┼──────────────────┤
        ▼               ▼                  ▼
   Here Document     JSON/CSV         Database/API
                        │
                        ▼
                  Standard Input
                        │
                        ▼
                  while read
                        │
                        ▼
                 Business Logic
                        │
                        ▼
                     Output
```

数据来源可以随时变化。

处理逻辑保持不变。

---

# 五、为什么推荐 while read？

Shell 中真正的数据流模型：

```
Producer

↓

stdout

↓

stdin

↓

Consumer
```

例如：

```
find

↓

while read

↓

rm
```

而：

```
for in $(...)
```

其实是：

```
Producer

↓

Command Substitution

↓

Word Splitting

↓

for
```

中间经历：

```
字符串

↓

拆词

↓

glob

↓

变量展开
```

容易出问题。

所以：

```
Producer

↓

read

↓

Consumer
```

才是 Unix 的原生模式。

---

# 六、最佳实践总结

| 数据来源 | 推荐方式 | 推荐指数 |
|----------|----------|----------|
| 少量固定数据 | Bash Array | ⭐⭐⭐⭐⭐ |
| 长列表（脚本内） | Here Document + while read | ⭐⭐⭐⭐⭐ |
| 配置文件 | while read < file | ⭐⭐⭐⭐⭐ |
| JSON | jq + while read | ⭐⭐⭐⭐⭐ |
| CSV | IFS=',' read | ⭐⭐⭐⭐⭐ |
| find | -print0 + read -d '' | ⭐⭐⭐⭐⭐ |
| Pipeline | while read | ⭐⭐⭐⭐⭐ |
| SSH Hosts | hosts.txt + while read | ⭐⭐⭐⭐⭐ |
| 函数参数 | "$@" | ⭐⭐⭐⭐⭐ |
| xargs | stdin + xargs | ⭐⭐⭐⭐⭐ |

---

# 七、不推荐的写法

## 1. 数据写死在 for 中

```bash
for user in user1 user2 user3
```

原因：

```
Data

和

Logic

耦合
```

---

## 2. 使用 `for x in $(command)`

```bash
for file in $(find .)
```

问题：

- Word Splitting
- Globbing
- 无法处理空格
- 无法处理换行

推荐：

```bash
find . -print0 |
while IFS= read -r -d '' file
do
    ...
done
```

---

## 3. 使用 echo 解析数据

例如：

```bash
echo "$users" | while ...
```

如果数据本身已经在文件、数组或命令输出中，优先直接消费数据源，而不是绕一圈再用 `echo` 生成。

---

# 八、最终推荐的 Shell 数据流心智模型（牢记）

```
                    Shell Script

                         │
                         ▼
                  Data Source（数据）
                         │
      ┌──────────────────┼───────────────────┐
      │                  │                   │
      ▼                  ▼                   ▼
    Array             File              Command
      │                  │                   │
      │                  │                   │
      └──────────────────┬───────────────────┘
                         ▼
                  Standard Input（stdin）
                         │
                         ▼
              while read / for "$@"
                         │
                         ▼
              Business Logic（处理逻辑）
                         │
                         ▼
                      Output（输出）
```

---

# 九、一句话总结

> **Shell 是一种数据流驱动（Data Flow）的语言，而不是控制流驱动（Control Flow）的语言。优秀的 Shell 脚本应该始终遵循"数据源（Array/File/Command/Here Document）→ 消费者（while read / for "$@"）→ 处理逻辑（函数/命令）→ 输出"的模式，使数据与处理逻辑解耦，从而获得更好的可维护性、可复用性和可扩展性。**