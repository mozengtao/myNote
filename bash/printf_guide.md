# Bash `printf` 多行模板：引号使用、格式保持与最佳实践

---

# 一句话心智模型

> **把 printf 看成一个"文本模板生成器"，Shell 只负责变量替换，printf 负责按顺序输出。**

因此：

> **静态文本 → 单引号**
>
> **需要变量展开 → 双引号**

这是 Bash 写大型文本（YANG、JSON、XML、Config、Markdown、Dockerfile、Kubernetes YAML 等）的黄金法则。

---

# 先看原来的例子

```bash
build_date=$(date +%Y-%m-%d)
major=1
minor=300

printf '%s\n' \
    'module vmc-device-version {' \
    '' \
    "        namespace \"urn:vecima:yang:vmc-device-version:${major}.${minor}\";" \
    '        prefix vmc-device-version;' \
    '        organization "Vecima Networks Inc";' \
    '        contact "https://www.vecima.com";' \
    '        description "NED Versioning Yang";' \
    '' \
    "        revision ${build_date} {" \
    '                description "NED Versioning Yang";' \
    '        }' \
    '}' \
> vmc-device-version.yang
```

输出：

```yang
module vmc-device-version {

        namespace "urn:vecima:yang:vmc-device-version:1.300";
        prefix vmc-device-version;
        organization "Vecima Networks Inc";
        contact "https://www.vecima.com";
        description "NED Versioning Yang";

        revision 2026-07-28 {
                description "NED Versioning Yang";
        }
}
```

可以看到：

Shell 不负责缩进。

printf 只是把字符串原样打印出来。

所以：

> **最终文件长什么样，就让 printf 参数长什么样。**

---

# Shell 到底做了什么？

例如这一行：

```bash
'        prefix vmc-device-version;'
```

Shell解析：

```
这是一个字符串

内容：
________prefix vmc-device-version;
```

（前面的空格全部保留）

printf收到：

```
"        prefix vmc-device-version;"
```

然后打印。

没有任何神秘操作。

---

# 为什么静态文本推荐单引号？

例如：

```bash
'organization "Vecima Networks Inc";'
```

这里包含：

```
"
空格
;
```

全部都会原样输出。

Shell不会解析：

```
$
`
\
!
```

所以：

> **单引号 = 原样复制。**

这是最安全的。

---

# 双引号什么时候使用？

只有一种情况：

> **需要变量展开。**

例如：

```bash
"revision ${build_date} {"
```

Shell先展开：

```
revision 2026-07-28 {
```

再交给printf。

再例如：

```bash
"namespace \"urn:vecima:${major}.${minor}\";"
```

最终：

```
namespace "urn:vecima:1.300";
```

---

# 为什么不用单引号？

例如：

```bash
'revision ${build_date} {'
```

输出：

```
revision ${build_date} {
```

变量不会展开。

因为：

> **单引号禁止所有变量展开。**

---

# 为什么双引号里面需要 \"？

例如：

```bash
"namespace "abc";"
```

Shell会认为：

```
"namespace "
abc
";"
```

字符串已经结束。

因此必须：

```bash
"namespace \"abc\";"
```

Shell得到：

```
namespace "abc";
```

---

# 如果字符串里没有双引号？

就不用转义。

例如：

```bash
"revision ${date} {"
```

没有：

```
"
```

因此：

```
无需 \
```

---

# 为什么静态文本不用双引号？

例如：

```bash
"prefix vmc-device-version;"
```

虽然可以。

但是：

Shell仍然会检查：

```
$
`
\
!
```

例如：

```bash
"price $100"
```

Shell会尝试展开：

```
$100
```

容易产生意外。

所以：

**没有变量 → 单引号。**

这是Shell社区长期形成的最佳实践。

---

# 黄金法则（最重要）

## 第一条

静态文本：

```bash
'abc'
```

不要：

```bash
"abc"
```

---

## 第二条

需要变量：

```bash
"abc ${var}"
```

不要：

```bash
'abc ${var}'
```

---

## 第三条

尽量一行一个字符串。

例如：

```bash
printf '%s\n' \
    'line1' \
    'line2' \
    "line ${var}" \
    'line4'
```

不要：

```bash
printf '%s\n' \
'line1''line2'"line${var}"
```

可读性非常差。

---

# 为什么每一行都是一个参数？

因为：

```
printf '%s\n'
```

表示：

```
每一个参数

↓

%s

↓

自动输出一个\n
```

例如：

```bash
printf '%s\n' \
    A \
    B \
    C
```

输出：

```
A
B
C
```

所以：

> **一个参数 = 输出一行。**

这是理解整个写法的关键。

---

# 空行怎么办？

直接：

```bash
''
```

例如：

```bash
printf '%s\n' \
    'line1' \
    '' \
    'line2'
```

输出：

```
line1

line2
```

非常清晰。

---

# 为什么不要自己写 \n？

例如：

不要：

```bash
printf 'line1\nline2\n'
```

因为：

大型模板：

```
难维护

难缩进

难插入变量

容易漏\n
```

推荐：

```bash
printf '%s\n' \
    'line1' \
    'line2'
```

每行一个参数。

---

# 推荐格式（推荐指数★★★★★）

```
printf '%s\n' \
    '第一行' \
    '第二行' \
    "第三行 ${var}" \
    '' \
    '第五行'
```

优点：

✓ 对齐漂亮

✓ Git Diff友好

✓ 插入新行容易

✓ 删除容易

✓ 阅读舒服

✓ 不容易漏换行

---

# 如果变量很多怎么办？

例如：

```bash
module=$1
version=$2
date=$(date +%F)
```

推荐：

```bash
printf '%s\n' \
    "module ${module} {" \
    "    version ${version};" \
    "    revision ${date};" \
    '}'
```

而不是：

```bash
printf '%s\n' \
    "module $module {" \
    "version $version;" \
    "revision $date;"
```

推荐统一：

```
${var}
```

原因：

边界清晰。

例如：

```
${major}.${minor}
```

比：

```
$major.$minor
```

更容易阅读。

---

# 如果双引号太多怎么办？

例如：

```yang
namespace "xxxx";
organization "Vecima";
```

如果全部写：

```bash
"namespace \"xxxx\";"
```

会出现大量：

```
\"\"\"\"\"\"\"\
```

可读性下降。

推荐：

静态行全部：

```bash
'namespace "xxxx";'
```

只有变量：

```bash
"namespace \"urn:${major}.${minor}\";"
```

这样：

> **99%的行都是单引号。**

真正需要变量时：

> **只有少数双引号。**

代码非常整洁。

---

# 与 Here Document 的比较

printf：

```bash
printf '%s\n' \
    'line1' \
    "line ${var}"
```

优点：

- 每行独立
- Git Diff 最友好
- 插入删除简单
- 不会出现EOF对齐问题

适合：

- 中小模板
- 自动生成配置
- YANG
- JSON
- XML

---

Here Document：

```bash
cat <<EOF
module {
    revision ${date}
}
EOF
```

优点：

- 最接近最终文件
- 大文件最舒服

适合：

- 数百行模板
- License
- Markdown
- HTML
- Shell脚本模板

---

经验法则：

```
几十行以内
    ↓
printf '%s\n'

几百行以上
    ↓
cat <<EOF
```

---

# 最佳实践（Checklist）

✅ 静态文本全部使用单引号

✅ 需要变量才使用双引号

✅ 双引号内出现 `"` 时使用 `\"`

✅ 一个参数对应输出一行

✅ 空行使用 `''`

✅ 使用 `printf '%s\n'` 自动换行，不手写 `\n`

✅ 缩进直接写在字符串中，保持与最终文件一致

✅ 变量统一写成 `${var}`，避免边界歧义

✅ 最终输出文件应尽量做到“所见即所得（WYSIWYG）”：Shell 源码中的每一行，应尽可能与生成文件中的对应行保持一致，仅变量位置发生替换

---

# 最终心智模型

```
Shell Script

        printf '%s\n'
             │
             │
    ┌────────┴─────────┐
    │                  │
静态文本           动态文本
(单引号)          (双引号)
    │                  │
    │          Shell展开变量
    │                  │
    └────────┬─────────┘
             │
      printf 按顺序输出
             │
             ▼
      最终配置文件 / YANG / JSON / XML

核心原则：

    最终文件长什么样
            =
    printf 参数就长什么样

只有变量位置发生替换，其余内容（缩进、空格、引号、对齐）全部保持与目标文件一致。
```