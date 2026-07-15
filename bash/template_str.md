# Bash Template String（模板字符串）最佳实践与心智模式

> **核心思想：Template（模板） + Data（数据） + Render（渲染） = Output（输出）**
>
> 不要把 Shell 看成是在"拼字符串"，而应该看成是在**把数据渲染(Render)到模板(Template)中**。

---

# 一、为什么使用 Template？

很多 Shell 初学者都会这样写：

```bash
echo "hostname=$HOST"
echo "ip=$IP"
echo "gateway=$GW"
```

变量越来越多以后：

```bash
echo "hostname=$HOST"
echo "ip=$IP"
echo "gateway=$GW"
echo "dns=$DNS"
echo "mask=$MASK"
echo "mtu=$MTU"
...
```

最终：

- 配置格式和业务逻辑混在一起
- 不容易修改
- 不容易复用
- 容易漏字段

真正推荐的方法是：

```
        Template
           │
           │
           ▼
    +--------------+
    | Render Data  |
    +--------------+
           │
           ▼
       Output File
```

即：

> **Shell负责准备数据，而Template负责描述最终长什么样。**

---

# 二、最佳实践一：envsubst（★★★★★）

这是 Linux 下生成配置文件最经典的方法。

## 心智模型

```
          Environment Variables
             HOST
             IP
             GW
               │
               ▼
        +----------------+
        |   envsubst     |
        +----------------+
               │
               ▼
         config.tpl
               │
               ▼
          config.conf
```

envsubst 本质就是：

```
Template

↓

把所有 ${VAR}

↓

替换成环境变量

↓

输出结果
```

它并不会解析 Shell 语法。

只负责：

```
${NAME}
↓

变量值
```

---

# 示例一：生成配置文件

## Step1：模板

config.tpl

```text
hostname=${HOST}
ip=${IP}
gateway=${GW}
```

---

## Step2：Shell

```bash
HOST=server01
IP=192.168.10.10
GW=192.168.10.1

export HOST IP GW

envsubst < config.tpl > config.conf
```

---

输出：

```text
hostname=server01
ip=192.168.10.10
gateway=192.168.10.1
```

整个流程：

```
HOST=server01
IP=...

       │

       ▼

config.tpl

hostname=${HOST}

       │

 envsubst

       │

       ▼

hostname=server01
```

---

# 示例二：批量生成

假设：

servers.txt

```text
server01 10.0.0.1
server02 10.0.0.2
server03 10.0.0.3
```

模板：

```text
hostname=${HOST}
ip=${IP}
```

Shell：

```bash
while read HOST IP
do
    export HOST IP

    envsubst \
        < config.tpl \
        > output/${HOST}.conf

done < servers.txt
```

结果：

```
output/

server01.conf
server02.conf
server03.conf
```

数据流：

```
servers.txt

      │

      ▼

HOST/IP

      │

      ▼

config.tpl

      │

      ▼

envsubst

      │

      ▼

server01.conf
server02.conf
server03.conf
```

这就是 CI/CD、Docker、Kubernetes 中最常见的配置生成方式。

---

# 三、最佳实践二：Shell Here Document（★★★★★）

如果模板很短，就没有必要单独维护模板文件。

例如：

```bash
cat <<EOF
hostname=$HOST
ip=$IP
gateway=$GW
EOF
```

实际上：

```
Shell Script

↓

Template

↓

Shell变量展开

↓

stdout
```

---

## 示例一：单文件生成

```bash
HOST=server01
IP=192.168.1.1
GW=192.168.1.254

cat > config.conf <<EOF
hostname=$HOST
ip=$IP
gateway=$GW
EOF
```

输出：

```text
hostname=server01
ip=192.168.1.1
gateway=192.168.1.254
```

整个过程：

```
          Script

             │

             ▼

      Here Document

             │

       Variable Expand

             │

             ▼

      config.conf
```

---

# Here Document 的本质

很多人认为：

```
cat <<EOF
...
EOF
```

只是多行字符串。

实际上，它更像：

```
匿名模板（Anonymous Template）
```

可以理解成：

```
template.txt

↓

直接写进Shell里面
```

所以：

```
cat <<EOF
Hello $USER
EOF
```

等价于：

```
Template：

Hello $USER

↓

Render

↓

Hello Morris
```

---

## 示例二：批量生成（Here Document）

与 envsubst 的「示例二」相同数据源，只是把独立模板文件换成写在 Shell 里的匿名模板。

假设：

servers.txt

```text
server01 10.0.0.1
server02 10.0.0.2
server03 10.0.0.3
```

Shell：

```bash
mkdir -p output

while read HOST IP
do
    cat <<EOF > "output/${HOST}.conf"
hostname=$HOST
ip=$IP
EOF

done < servers.txt
```

结果：

```
output/

server01.conf
server02.conf
server03.conf
```

例如 `output/server01.conf`：

```text
hostname=server01
ip=10.0.0.1
```

数据流：

```
servers.txt

      │

      ▼

HOST/IP

      │

      ▼

Here Document（匿名模板）

      │

      ▼

Variable Expand

      │

      ▼

server01.conf
server02.conf
server03.conf
```

对比 envsubst 批量生成：

```
envsubst 路径：

  servers.txt → HOST/IP → config.tpl → envsubst → *.conf

Here Document 路径：

  servers.txt → HOST/IP → 脚本内 <<EOF ... EOF → *.conf
```

模板很短、不需要单独维护 `.tpl` 时，用 Here Document 批量生成更直接。

---

# Here Document 与 envsubst 的区别

## envsubst

```
Template

↓

独立文件

↓

Render

↓

Output
```

适合：

- 配置文件
- YAML
- JSON
- XML
- CLI
- SQL

模板可以长期维护。

---

## Here Document

```
Template

↓

写在Shell里

↓

Expand

↓

Output
```

适合：

- 十几行以内
- 一次性内容
- 临时CLI
- HTTP请求
- SQL

例如：

```bash
curl \
    -d @- <<EOF
{
    "host":"$HOST",
    "ip":"$IP"
}
EOF
```

这里：

JSON 本身就是模板。

---

# 四、批量生成文件的最佳实践

例如：

```
templates/

    vmc.tpl

data/

    vmcs.txt

output/
```

模板：

```text
hostname=${HOST}
ip=${IP}
gateway=${GW}
```

数据：

```
server01 10.0.0.1
server02 10.0.0.2
server03 10.0.0.3
```

脚本：

```bash
while read HOST IP
do
    export HOST IP

    envsubst \
        < templates/vmc.tpl \
        > output/${HOST}.conf

done < data/vmcs.txt
```

这是最推荐的组织方式。

数据、模板、逻辑完全分离。

---

# 五、Template Pipeline（模板流水线）

整个工程建议采用如下数据流：

```
                 Data Source
             (CSV/TXT/YAML/JSON)

                     │

                     ▼

            Parse Variables

                     │

                     ▼

              Environment

                     │

                     ▼

          Template File(.tpl)

                     │

              envsubst Render

                     │

                     ▼

          Generated Config

                     │

                     ▼

          Deploy / Execute
```

如果模板很短：

```
Shell Variables

        │

        ▼

Here Document

        │

 Variable Expansion

        │

        ▼

 Generated Content
```

---

# 六、什么时候使用哪一种？

| 场景 | 推荐方案 | 原因 |
|------|----------|------|
| 临时生成几行文本 | Here Document | 最简单，无需模板文件 |
| Shell 内嵌 JSON | Here Document | 代码集中，阅读方便 |
| Shell 内嵌 SQL | Here Document | 易于维护多行语句 |
| Shell 内嵌 CLI 配置 | Here Document | 临时配置最方便 |
| 大量配置文件 | envsubst | 模板独立，可复用 |
| CI/CD | envsubst | 配置与脚本解耦 |
| Docker EntryPoint | envsubst | 官方广泛采用 |
| Kubernetes Config 模板 | envsubst | 维护成本低 |
| 批量生成几十/几百个文件 | envsubst | 可循环渲染模板 |

---

# 七、最佳实践原则（Best Practices）

## 原则一：模板与数据分离

不要：

```bash
echo "hostname=$HOST"
echo "ip=$IP"
```

推荐：

```
Template

↓

Render

↓

Output
```

---

## 原则二：Shell负责流程，不负责格式

Shell：

```
读取数据

↓

循环

↓

调用Render

↓

部署
```

Template：

```
决定最终长什么样
```

职责单一，易于维护。

---

## 原则三：短模板用 Here Document，长模板用 envsubst

经验法则：

- **≤10～20 行**：优先使用 Here Document，阅读和维护成本最低。
- **>20 行或需要复用**：优先使用独立模板文件配合 `envsubst`。

---

## 原则四：批量生成采用"数据驱动"

不要：

```
if server1 ...

if server2 ...

if server3 ...
```

而是：

```
Data

↓

Loop

↓

Template

↓

Output
```

即：

```
One Template

+

Many Data

=

Many Outputs
```

---

# 八、最终心智模型（推荐牢记）

```
                    Template Rendering

          +-------------------------------+
          |           Template            |
          |  hostname=${HOST}            |
          |  ip=${IP}                    |
          +---------------+---------------+
                          ^
                          |
                   Variable Binding
                          |
         +----------------+----------------+
         |                                 |
         |                                 |
     Here Document                     envsubst
    (内嵌匿名模板)                   (独立模板文件)
         |                                 |
         +----------------+----------------+
                          |
                          ▼
                     Render Engine
                          |
                          ▼
                  Generated Content
                          |
                          ▼
               Config / JSON / SQL / CLI
                          |
                          ▼
                  Deploy / Execute
```

> **一句话总结：**
>
> **Template 描述结构，Data 提供内容，Render 完成绑定，Shell 负责流程。**  
> 当模板很短时，把模板直接嵌入脚本（Here Document）；当模板需要复用、维护或批量生成时，将模板独立出来并使用 `envsubst` 渲染。这种"模板 + 数据 + 渲染"的思维方式比直接拼接字符串更清晰、更易维护，也更符合现代自动化脚本的工程实践。