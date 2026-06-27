# Bash 命令查找顺序（Command Resolution Order）心智模型

---

# 一、为什么要理解命令查找顺序

当 Bash 遇到一行命令：

```bash
foo arg1 arg2
```

Bash 并不知道：

```text
foo
```

到底是：

```text
Alias
Function
Builtin
External Command
```

因此 Bash 必须进行查找（Resolution）。

很多问题都与此有关：

```bash
ls
```

为什么执行的是函数？

---

```bash
grep
```

为什么执行的是 /usr/bin/grep？

---

```bash
cd
```

为什么没有对应的可执行文件？

---

```bash
command ls
```

为什么绕过了函数？

---

这些问题的根源都是：

```text
Command Resolution Order
(命令查找顺序)
```

---

# 二、整体心智模型

对于：

```bash
something arg1 arg2
```

Bash 的查找过程：

```text
                something
                     │
                     ▼
             ┌─────────────┐
             │ Alias ?     │
             └─────────────┘
                     │
          Yes ───────┘
                     │ No
                     ▼
             ┌─────────────┐
             │ Function ?  │
             └─────────────┘
                     │
          Yes ───────┘
                     │ No
                     ▼
             ┌─────────────┐
             │ Builtin ?   │
             └─────────────┘
                     │
          Yes ───────┘
                     │ No
                     ▼
             ┌─────────────┐
             │ PATH Search │
             └─────────────┘
                     │
                     ▼
                 execve()
```

记忆口诀：

```text
Alias
  ↓
Function
  ↓
Builtin
  ↓
PATH
```

简称：

```text
A → F → B → P
```

---

# 三、第一层：Alias

---

## 示例

```bash
alias ll='ls -l'
```

查看：

```bash
alias
```

输出：

```text
alias ll='ls -l'
```

执行：

```bash
ll
```

Bash 首先检查：

```text
Alias Table
```

发现：

```text
ll -> ls -l
```

于是替换：

```bash
ll
```

变成：

```bash
ls -l
```

然后继续解析。

---

执行流程：

```text
ll
 │
 ▼
Alias Lookup
 │
 ▼
ls -l
 │
 ▼
继续解析
```

---

ASCII 图

```text
User Input

   ll
    │
    ▼
+----------+
| Alias ?  |
+----------+
    │
   YES
    │
    ▼
 ls -l
```

---

# 四、第二层：Function

---

## 示例

```bash
ls() {
    echo "my ls"
}
```

执行：

```bash
ls
```

Bash 查找：

```text
Alias ?
```

无。

继续：

```text
Function ?
```

发现：

```bash
ls()
```

于是执行函数。

输出：

```text
my ls
```

---

注意：

```text
不会继续搜索 PATH
```

---

流程：

```text
ls
 │
 ▼
Alias ?
 │
 ▼
No
 │
 ▼
Function ?
 │
 ▼
Yes
 │
 ▼
执行函数
```

---

ASCII 图

```text
ls
 │
 ▼
Function Table
 │
 ▼
Found
 │
 ▼
ls()
```

---

# 五、第三层：Builtin

---

Builtin：

```text
Bash内置命令
```

例如：

```bash
cd
echo
read
export
jobs
wait
alias
type
```

---

查看：

```bash
type cd
```

输出：

```text
cd is a shell builtin
```

---

为什么 cd 必须是 builtin？

例如：

```bash
cd /tmp
```

需要修改：

```text
当前Shell工作目录
```

如果：

```text
fork
exec
```

运行：

```text
子进程目录改变
```

父进程目录不会变。

因此：

```text
cd
必须运行在当前Shell
```

所以实现为 Builtin。

---

流程：

```text
cd
 │
 ▼
Alias ?
 │
 ▼
No
 │
 ▼
Function ?
 │
 ▼
No
 │
 ▼
Builtin ?
 │
 ▼
Yes
 │
 ▼
执行builtin
```

---

# 六、第四层：PATH搜索

如果前三层都没找到：

```text
Alias
Function
Builtin
```

Bash 开始搜索 PATH。

---

## 示例

```bash
grep root /etc/passwd
```

执行：

```bash
echo $PATH
```

例如：

```text
/usr/local/bin
/usr/bin
/bin
```

---

搜索过程：

```text
/usr/local/bin/grep
      │
      ▼
不存在

/usr/bin/grep
      │
      ▼
找到
```

于是：

```c
execve("/usr/bin/grep")
```

---

ASCII 图

```text
grep
 │
 ▼
PATH
 │
 ├── /usr/local/bin/grep
 │
 ├── /usr/bin/grep
 │
 └── /bin/grep
```

找到即停止。

---

# 七、完整案例分析

---

## 案例1：只有外部命令

```bash
grep root /etc/passwd
```

执行路径：

```text
grep
 │
 ▼
Alias ?     No
Function ?  No
Builtin ?   No
PATH ?      Yes
 │
 ▼
/usr/bin/grep
 │
 ▼
fork + exec
```

---

## 案例2：函数覆盖外部命令

```bash
grep() {
    echo fake grep
}
```

执行：

```bash
grep
```

流程：

```text
grep
 │
 ▼
Alias ?     No
Function ?  Yes
 │
 ▼
执行函数
```

不会执行：

```text
/usr/bin/grep
```

---

## 案例3：Alias覆盖函数

```bash
alias grep='echo alias grep'

grep() {
    echo function grep
}
```

执行：

```bash
grep
```

结果：

```text
alias grep
```

因为：

```text
Alias优先于Function
```

---

流程：

```text
grep
 │
 ▼
Alias ?
 │
 ▼
Found
 │
 ▼
echo alias grep
```

---

# 八、如何查看命令最终解析结果

---

## 最推荐：type

```bash
type grep
```

可能输出：

```text
grep is /usr/bin/grep
```

---

函数：

```bash
type foo
```

输出：

```text
foo is a function
```

---

builtin：

```bash
type cd
```

输出：

```text
cd is a shell builtin
```

---

alias：

```bash
type ll
```

输出：

```text
ll is aliased to `ls -l'
```

---

# 九、command 的作用

---

假设：

```bash
ls() {
    echo fake ls
}
```

执行：

```bash
ls
```

输出：

```text
fake ls
```

---

执行：

```bash
command ls
```

输出：

```text
真正的ls结果
```

---

因为：

```bash
command
```

告诉 Bash：

```text
跳过 Alias
跳过 Function
```

直接查找：

```text
Builtin
PATH
```

---

流程：

```text
command ls
      │
      ▼
Alias     Skip
Function  Skip
Builtin ?
PATH ?
```

---

# 十、Bash Hash Cache

每次搜索 PATH：

```text
/usr/local/bin
/usr/bin
/bin
...
```

成本较高。

因此 Bash 会缓存结果。

---

第一次：

```bash
grep
```

搜索：

```text
/usr/bin/grep
```

缓存：

```text
grep → /usr/bin/grep
```

---

查看：

```bash
hash
```

例如：

```text
hits command
 10  /usr/bin/grep
  3  /usr/bin/awk
```

---

之后：

```bash
grep
```

直接：

```text
Hash Lookup
```

无需再次遍历 PATH。

---

# 十一、实际工程脚本中的命令解析

例如：

```bash
ncs_cli_exec() {
    nomad alloc exec \
        -task "$TASK" \
        -job "$JOB" \
        ncs_cli \
        "$@"
}
```

调用：

```bash
ncs_cli_exec \
    "show vmc status"
```

解析过程：

```text
ncs_cli_exec
      │
      ▼
Function Lookup
      │
      ▼
Function Found
      │
      ▼
nomad
      │
      ▼
Alias ?      No
Function ?   No
Builtin ?    No
PATH ?       Yes
      │
      ▼
/usr/bin/nomad
      │
      ▼
fork
      │
      ▼
execve()
```

---

# 十二、从Linux视角理解

可以把 Bash 看成一个命令调度器：

```text
                Bash
                  │
                  ▼
         解析命令名字
                  │
      ┌───────────┼───────────┐
      │           │           │
      ▼           ▼           ▼
   Alias      Function     Builtin
                              │
                              ▼
                          External
                           Command
                              │
                              ▼
                           execve()
```

---

# 十三、最终心智模型

记住下面这张图即可：

```text
用户输入

    something
         │
         ▼

 ┌─────────────┐
 │ Alias ?     │
 └─────────────┘
         │
         ▼

 ┌─────────────┐
 │ Function ?  │
 └─────────────┘
         │
         ▼

 ┌─────────────┐
 │ Builtin ?   │
 └─────────────┘
         │
         ▼

 ┌─────────────┐
 │ PATH Search │
 └─────────────┘
         │
         ▼

      execve()
```

核心原则：

```text
Alias
优先于
Function

Function
优先于
Builtin

Builtin
优先于
外部命令(PATH)

找到第一个匹配项后立即停止搜索。
```

因此：

```text
Bash 并不是先知道某个名字是什么。

而是每遇到一个命令名，
按照固定顺序：

Alias → Function → Builtin → PATH

逐层查找，

找到第一个匹配项后立即执行。
```