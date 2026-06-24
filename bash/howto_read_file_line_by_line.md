# Shell 下读取文件并逐行解析的最佳实践

很多 Shell 初学者会写出这样的代码：

```bash
for line in $(cat file.txt)
do
    echo "$line"
done
```

这是 Shell 编程中最经典的错误之一。

原因：

```text
Shell 本质上是基于字符串和单词分割(word splitting)工作的。

$(...) 的结果会经过：

1. Parameter Expansion
2. Word Splitting
3. Filename Expansion(globbing)

导致：
    空格丢失
    TAB丢失
    引号丢失
    特殊字符损坏
```

例如：

文件：

```text
hello world
abc def
```

错误代码：

```bash
for line in $(cat file.txt)
do
    echo "[$line]"
done
```

输出：

```text
[hello]
[world]
[abc]
[def]
```

已经不是按行处理了。

---

# 一、最佳实践总原则

## 永远使用

```bash
while IFS= read -r line
do
    ...
done < file
```

这是 POSIX Shell 中最安全的方式。

---

# 二、核心心智模型

文件：

```text
line1
line2
line3
```

读取过程：

```text
file
 │
 ▼

stdin

 │
 ▼

read()

 │
 ▼

Shell Variable
```

即：

```text
Kernel File
    │
    ▼
read()
    │
    ▼
line variable
```

---

# 三、标准模板

```bash
while IFS= read -r line
do
    echo "$line"
done < file.txt
```

---

# 四、为什么是 IFS=

## 默认情况

Shell：

```bash
read line
```

等价：

```bash
IFS=$' \t\n'
read line
```

即：

```text
前导空格
后导空格
TAB
```

都会被处理掉。

---

例如：

文件：

```text
    hello
```

---

读取：

```bash
read line
```

结果：

```text
hello
```

前面的空格没了。

---

使用：

```bash
IFS=
read line
```

结果：

```text
    hello
```

完整保留。

---

# 五、为什么是 -r

## 默认read

```bash
read line
```

会把：

```text
\
```

当作转义符。

---

文件：

```text
C:\Windows
```

---

结果：

```text
C:Windows
```

被修改了。

---

使用：

```bash
read -r
```

表示：

```text
Raw Mode
```

不处理反斜杠。

---

因此：

```bash
IFS= read -r line
```

是标准写法。

---

# 六、推荐模板

## 模板1：逐行读取

```bash
while IFS= read -r line
do
    echo "$line"
done < file.txt
```

---

# 七、读取最后一行没有换行符的文件

很多配置文件：

```text
line1
line2
line3
```

最后一行没有：

```text
\n
```

---

普通写法：

```bash
while IFS= read -r line
do
    echo "$line"
done < file
```

最后一行可能丢失。

---

最佳实践：

```bash
while IFS= read -r line || [[ -n "$line" ]]
do
    echo "$line"
done < file
```

---

心智模型：

```text
EOF
  +
line非空

仍然处理一次
```

---

# 八、同时读取多个字段

文件：

```text
eth0 up 1500
eth1 down 9000
```

---

最佳实践：

```bash
while read -r iface state mtu
do
    echo "$iface"
    echo "$state"
    echo "$mtu"
done < file
```

结果：

```text
iface=eth0
state=up
mtu=1500
```

---

Shell 自动按 IFS 分列：

```text
eth0 up 1500

 │    │   │
 ▼    ▼   ▼

iface state mtu
```

---

# 九、解析 CSV

简单CSV：

```text
name,age,city
tom,20,beijing
```

---

可以：

```bash
while IFS=',' read -r name age city
do
    echo "$name"
done < file.csv
```

---

注意：

复杂CSV：

```text
"abc,def",20
```

不要用Shell解析。

应使用：

```bash
python
csvkit
awk
perl
```

---

# 十、跳过空行

```bash
while IFS= read -r line
do
    [[ -z "$line" ]] && continue

    echo "$line"

done < file
```

---

# 十一、跳过注释

配置文件：

```text
# comment

eth0
eth1
```

---

写法：

```bash
while IFS= read -r line
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue

    echo "$line"

done < file
```

---

# 十二、推荐配置文件解析模板

例如：

```ini
name=morris
age=30

# comment
city=beijing
```

---

最佳实践：

```bash
while IFS= read -r line
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue

    key=${line%%=*}
    value=${line#*=}

    printf '%s => %s\n' "$key" "$value"

done < config.ini
```

---

心智模型：

```text
name=morris

      │
      ▼

  %%=*

      │
      ▼

    name


name=morris

      │
      ▼

   #*=

      │
      ▼

   morris
```

---

# 十三、不要使用管道驱动 while

很多人写：

```bash
cat file.txt | while IFS= read -r line
do
    count=$((count+1))
done

echo "$count"
```

结果：

```text
0
```

---

原因：

Pipeline 会创建子Shell：

```text
Parent Shell

    │
    ▼

while loop
(subshell)
```

变量修改发生在：

```text
Subshell
```

退出后丢失。

---

# 十四、正确方式

## 方法1

推荐：

```bash
while IFS= read -r line
do
    count=$((count+1))
done < file

echo "$count"
```

---

## 方法2

Process Substitution

```bash
while IFS= read -r line
do
    count=$((count+1))
done < <(cat file)

echo "$count"
```

---

心智模型：

```text
cat file

   │
   ▼

Pipe

   │
   ▼

stdin of while

while仍运行在当前Shell
```

---

# 十五、读取命令输出的最佳实践

不要：

```bash
for line in $(ip link)
do
    ...
done
```

---

推荐：

```bash
ip link | while IFS= read -r line
do
    echo "$line"
done
```

或者：

```bash
while IFS= read -r line
do
    echo "$line"
done < <(ip link)
```

---

# 十六、大文件处理最佳实践

例如：

```text
100GB日志
```

---

不要：

```bash
lines=$(cat huge.log)
```

原因：

```text
全部加载内存
```

---

推荐：

```bash
while IFS= read -r line
do
    process "$line"
done < huge.log
```

---

心智模型：

```text
Kernel Page Cache

       │
       ▼

read()

       │
       ▼

一行

       │
       ▼

处理

       │
       ▼

下一行
```

属于：

```text
Streaming Processing
```

---

# 十七、工程化最佳实践模板

```bash
while IFS= read -r line || [[ -n "$line" ]]
do
    # Skip blank line
    [[ -z "$line" ]] && continue

    # Skip comment
    [[ "$line" =~ ^[[:space:]]*# ]] && continue

    # Process
    echo "$line"

done < input.txt
```

---

# 最终推荐心智模型

## 文件读取

```text
File
 │
 ▼

stdin
 │
 ▼

read()
 │
 ▼

line variable
 │
 ▼

process
```

---

## 命令输出读取

```text
Command

   │
   ▼

Pipe

   │
   ▼

read()

   │
   ▼

line variable
```

---

## Shell逐行解析黄金模板

```bash
while IFS= read -r line || [[ -n "$line" ]]
do
    ...
done < file
```

记住这条规则：

```text
Shell 中：

for line in $(...)

几乎总是错误的。

while IFS= read -r

几乎总是正确的。
```