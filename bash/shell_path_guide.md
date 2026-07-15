# Shell 中关于路径（Path）的黄金法则及心智模式

> **一句话总结：**
>
> **Shell 脚本应该始终围绕 Path（路径）而不是 Filename（文件名）进行设计。**
>
> **Path 是一等公民（First-class Citizen），Filename 只是 Path 的一个属性。**

---

# 一、先建立正确的心智模型

很多初学者的思维：

```
文件
 │
 ▼
文件名
 │
 ▼
命令
```

实际上 Linux 并不是这样工作的。

Linux 真正工作的对象是：

```
Filesystem
      │
      ▼
Directory
      │
      ▼
Path
      │
      ▼
inode
      │
      ▼
File
```

例如：

```
vmc/recover/astatine0_recover.conf
```

真正有意义的是整个 Path：

```
vmc/recover/astatine0_recover.conf
```

而不是

```
astatine0_recover.conf
```

因为：

```
foo.txt
```

在不同目录可能有很多个：

```
project1/foo.txt
project2/foo.txt
backup/foo.txt
```

Filename 不具有唯一性。

Path 才具有上下文(Context)。

---

# 二、黄金法则一：数据流始终传递 Path，而不是 Filename（★★★★★）

错误：

```
Directory
    │
    ▼
ls
    │
    ▼
Filename
    │
    ▼
重新拼路径
    │
    ▼
Command
```

例如：

```bash
ls vmc/recover |
while read -r file
do
    ./evcconfig.sh "vmc/recover/$file"
done
```

这里：

```
Directory Context
        ↓
丢失
        ↓
再补回来
```

属于信息丢失。

---

正确：

```
Directory
      │
      ▼
Glob
      │
      ▼
Path
      │
      ▼
Command
```

例如：

```bash
for file in vmc/recover/*
do
    ./evcconfig.sh "$file"
done
```

整个流水线：

```
vmc/recover/a.conf
vmc/recover/b.conf
vmc/recover/c.conf
```

从未丢失上下文。

---

# 三、黄金法则二：不要解析 ls（★★★★★）

经典原则：

```
Don't parse ls.
```

为什么？

ls 是给人看的。

不是给脚本看的。

例如：

```
ls
```

可能输出：

```
a file.txt
hello
world.txt
```

到底：

```
"a file.txt"
```

还是：

```
"a"
"file.txt"
```

无法可靠解析。

---

错误：

```bash
ls *.txt |
while read file
do
    ...
done
```

正确：

```bash
for file in *.txt
do
    ...
done
```

或者：

```bash
find . -type f
```

---

# 四、黄金法则三：Shell 自己负责 Path Expansion（★★★★★）

Shell 最大优势就是：

```
pathname expansion
```

例如：

```
*.conf
```

Shell 自动展开：

```
a.conf
b.conf
c.conf
```

例如：

```bash
for file in vmc/recover/*.conf
```

Shell 自动得到：

```
vmc/recover/a.conf
vmc/recover/b.conf
vmc/recover/c.conf
```

无需自己拼接。

---

# 五、黄金法则四：需要 basename 时再取 basename

很多脚本反过来了：

```
basename
    ↓
拼路径
```

其实应该：

```
Path
    ↓
Command
```

只有：

打印

日志

UI

显示

才需要：

```bash
basename "$file"
```

例如：

```bash
for file in vmc/recover/*
do
    echo "Processing $(basename "$file")"
done
```

Path 用于处理。

Filename 用于展示。

---

# 六、黄金法则五：目录变化时，Path 最稳定

错误：

```bash
cd vmc/recover

for f in *
do
    ...
done
```

以后：

```
cd
```

稍微变化：

脚本全部失效。

推荐：

```bash
RECOVER_DIR=vmc/recover

for file in "$RECOVER_DIR"/*
do
    ...
done
```

整个脚本：

Directory 变化：

只改：

```
RECOVER_DIR
```

即可。

---

# 七、黄金法则六：find 输出的是 Path，不是 Filename

例如：

```bash
find vmc/recover -type f
```

输出：

```
vmc/recover/a.conf
vmc/recover/b.conf
```

因此：

```bash
find vmc/recover -type f |
while read -r file
do
    cat "$file"
done
```

非常自然。

---

# 八、黄金法则七：始终引用 Path（Quote Every Path）

错误：

```bash
cat $file
```

如果：

```
hello world.txt
```

变成：

```
cat hello world.txt
```

两个参数。

正确：

```bash
cat "$file"
```

Shell Best Practice：

> Always quote variables that contain pathnames.

---

# 九、黄金法则八：文件列表优先使用数组

例如：

```bash
files=(vmc/recover/*.conf)

for file in "${files[@]}"
do
    echo "$file"
done
```

整个数组：

```
[
 vmc/recover/a.conf
 vmc/recover/b.conf
]
```

而不是：

```
[
 a.conf
 b.conf
]
```

这样：

所有后续函数：

```
process()
backup()
upload()
delete()
```

都直接接受 Path。

---

# 十、黄金法则九：函数接口统一使用 Path

推荐：

```
process_file(path)
```

不要：

```
process_file(dir, filename)
```

例如：

错误：

```bash
upload vmc/recover a.conf
```

函数内部：

```
dir + "/" + filename
```

推荐：

```bash
upload vmc/recover/a.conf
```

调用者负责提供完整 Path。

函数无需猜测。

---

# 十一、黄金法则十：Directory、Filename、Extension 是 Path 的派生信息

Path：

```
vmc/recover/a.conf
```

可以得到：

```
dirname
↓

vmc/recover
```

```
basename
↓

a.conf
```

```
extension
↓

conf
```

因此：

应该：

```
Path
 │
 ├── dirname
 ├── basename
 └── extension
```

而不是：

```
dirname
+
basename
=
Path
```

---

# 十二、典型示例

## 示例一：遍历配置文件

推荐：

```bash
for conf in configs/*.yaml
do
    validate "$conf"
done
```

而不是：

```bash
ls configs |
while read f
do
    validate configs/"$f"
done
```

---

## 示例二：批量上传

推荐：

```bash
find images -name '*.png' |
while read -r image
do
    scp "$image" server:/tmp/
done
```

---

## 示例三：批量删除

推荐：

```bash
find logs -mtime +30 -type f -delete
```

而不是：

```
ls
→ grep
→ awk
→ rm
```

---

## 示例四：批量处理

```bash
files=(data/*.csv)

for file in "${files[@]}"
do
    python process.py "$file"
done
```

Python：

```
process.py

argv[1]

↓

完整 Path
```

无需再拼接。

---

## 示例五：函数设计

推荐：

```bash
compress()
{
    local file="$1"

    gzip "$file"
}

compress vmc/recover/a.conf
```

而不是：

```bash
compress()
{
    dir="$1"
    file="$2"

    gzip "$dir/$file"
}
```

---

# 十三、Shell Path Flow（推荐心智模型）

```
Filesystem
      │
      ▼
Directory
      │
      ▼
Path Generation
(glob/find)
      │
      ▼
Path Stream
      │
      ▼
Processing
(cat/cp/scp/rm/python)
      │
      ▼
Output
```

整个 Pipeline 始终传递：

```
Path
```

而不是：

```
Filename
```

---

# 十四、最终心智模型（★★★★★）

```
                  Linux Filesystem

                        │
                        ▼
                 Directory (Context)
                        │
         ┌──────────────┴──────────────┐
         │                             │
         ▼                             ▼
   Glob Expansion                 find Command
 (*.conf, */*.txt)              (find -type f ...)
         │                             │
         └──────────────┬──────────────┘
                        ▼
              Stream of Full Pathnames
                        │
        ┌───────────────┼────────────────┐
        ▼               ▼                ▼
      cat             python           scp/rm/cp
                        │
                        ▼
                  Business Logic
                        │
                        ▼
         basename / dirname（仅用于展示或派生信息）
```

---

# 十五、核心原则（Checklist）

✅ 始终围绕 **Path** 编程，而不是 **Filename**。  
✅ 数据流保持 **完整路径（Full Path）**，不要中途丢失目录上下文。  
✅ 不要解析 `ls`，优先使用 **glob (`*`)** 或 **find**。  
✅ 让 Shell 的 **pathname expansion** 自动完成路径展开。  
✅ 所有路径变量都使用双引号：`"$path"`。  
✅ 函数接口优先接收 **Path**，而不是目录 + 文件名。  
✅ `basename`、`dirname` 仅用于显示、日志或派生信息，而不是作为主数据流。  
✅ **Path 是一等公民（First-class Citizen）**；Filename、Directory、Extension 都是 Path 的派生属性。