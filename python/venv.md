# Python 3 venv 虚拟环境：常用命令、作用与心智模型

## 1. 一句话理解 venv

`venv` 的本质：

> **为一个 Python 项目创建一套隔离的 Python 运行环境。**

可以把它理解成：

```text
系统 Python
    │
    ├── /usr/bin/python3
    │
    └── 系统级 packages
           │
           │ python3 -m venv ./venv
           ▼
      ┌──────────────────────┐
      │        ./venv/       │
      │                      │
      │  Python Interpreter  │
      │  pip                 │
      │  site-packages/      │
      │                      │
      │  PyYAML              │
      │  Jinja2              │
      │  ...                 │
      └──────────────────────┘
```

核心目标：

```text
Project A
    └── venv_A
         ├── Python
         ├── PyYAML 6.x
         └── Jinja2 3.x

Project B
    └── venv_B
         ├── Python
         ├── PyYAML 5.x
         └── Jinja2 2.x
```

两个项目互不干扰。

## 2. 最重要的心智模型

使用 Python venv 时，建议始终把环境理解成：

```text
             Project
                │
                ▼
        ┌───────────────┐
        │     venv      │
        ├───────────────┤
        │ python        │
        │ pip           │
        │ site-packages │
        └───────────────┘
                │
                ▼
          execute.py
```

也就是说：

项目代码 + venv + requirements.txt = 一个相对独立的 Python 项目运行环境。

其中：

| 东西 | 作用 |
|------|------|
| python3 | 创建 venv / 使用系统 Python |
| venv | 创建隔离环境 |
| ./venv/bin/python3 | 这个项目自己的 Python |
| pip | 安装/删除/查看 Python package |
| ./venv/lib/.../site-packages | 第三方 Python 包实际安装位置 |
| requirements.txt | 项目依赖清单 |
| activate | 修改当前 Shell，让命令默认使用 venv |

## 3. 创建虚拟环境

### 3.1 最常见命令

```bash
python3 -m venv ./venv
```

含义：

```text
python3
  │
  └── -m venv
          │
          └── 创建 ./venv/
```

这里的 `-m` 非常重要。

它表示：

让 Python 运行一个 Python module。

所以：

```bash
python3 -m venv ./venv
```

可以理解成：

```text
使用当前 python3
        │
        ▼
运行 Python 自带的 venv 模块
        │
        ▼
创建 ./venv 虚拟环境
```

## 4. 为什么推荐 python3 -m venv

推荐：

```bash
python3 -m venv ./venv
```

而不是寻找一个独立的 `venv` 命令。

原因是：

```text
python3
   │
   └── -m venv
          │
          ▼
      使用这个 Python
      创建对应的环境
```

因此可以明确知道：

这个 venv 是由哪个 Python 创建的。

例如系统中存在：

```text
/usr/bin/python3
/usr/local/bin/python3
```

甚至：

```text
python3.10
python3.11
python3.12
```

可以明确指定：

```bash
python3.11 -m venv ./venv
```

于是：

```text
./venv
   │
   └── 基于 Python 3.11 创建
```

## 5. venv 创建后里面有什么

执行：

```bash
python3 -m venv ./venv
```

通常会得到：

```text
venv/
├── bin/
│   ├── activate
│   ├── python
│   ├── python3
│   ├── pip
│   └── ...
│
├── include/
│
├── lib/
│   └── python3.x/
│       └── site-packages/
│
└── pyvenv.cfg
```

最重要的几个东西：

```text
venv/
│
├── bin/python3
│      │
│      └── 项目使用的 Python
│
├── bin/pip
│      │
│      └── 给这个 Python 安装 package
│
├── bin/activate
│      │
│      └── 修改 Shell 环境
│
└── lib/python3.x/site-packages/
       │
       └── 第三方 package
```

## 6. 激活虚拟环境

Linux/macOS：

```bash
source ./venv/bin/activate
```

激活后通常会看到：

```text
(venv) user@host:~/project$
```

这不是创建了一个新的 Shell。

它主要做的是：

修改当前 Shell 的 PATH

例如原来：

```text
PATH=/usr/local/bin:/usr/bin:...
```

激活后：

```text
PATH=./venv/bin:/usr/local/bin:/usr/bin:...
```

于是：

```text
python3
pip
```

会优先找到：

```text
./venv/bin/python3
./venv/bin/pip
```

## 7. 激活的真正心智模型

不要把：

```bash
source ./venv/bin/activate
```

理解成：

“进入虚拟机”

它实际上更接近：

修改当前 Shell 的命令搜索路径。

可以理解为：

激活之前

```text
python3
  │
  ▼
/usr/bin/python3
```

激活之后

```text
python3
  │
  ▼
./venv/bin/python3
```

所以：

```bash
which python3
```

激活前可能：

```text
/usr/bin/python3
```

激活后：

```text
/path/to/project/venv/bin/python3
```

这是理解 venv 最重要的实验之一。

## 8. 如何确认当前使用的是哪个 Python

推荐：

```bash
which python3
```

以及：

```bash
python3 --version
```

更可靠：

```bash
python3 -c "import sys; print(sys.executable)"
```

例如：

```text
/path/to/project/venv/bin/python3
```

它直接告诉你：

当前 Python 解释器到底是谁。

## 9. 查看当前 Python 的 package 安装位置

可以：

```bash
python3 -c "import site; print(site.getsitepackages())"
```

或者：

```bash
python3 -m site
```

核心关注：

`site-packages`

例如：

```text
./venv/lib/python3.11/site-packages/
```

这就是：

venv 中第三方 Python package 的主要安装位置。

## 10. 安装第三方 package

例如：

```bash
pip install --no-cache-dir pyyaml
```

以及：

```bash
pip install --no-cache-dir jinja2
```

执行后：

```text
venv/
└── lib/
    └── python3.x/
        └── site-packages/
            ├── yaml/
            ├── jinja2/
            └── ...
```

## 11. pip install 的心智模型

核心关系：

```text
pip
 │
 │ install
 ▼
PyPI / package source
 │
 ▼
download package
 │
 ▼
install
 │
 ▼
venv/site-packages/
```

例如：

```bash
pip install pyyaml
```

本质是：

```text
PyYAML
   │
   ▼
下载
   │
   ▼
安装到当前 Python 环境
   │
   ▼
venv/lib/python3.x/site-packages/
```

## 12. 为什么推荐 python3 -m pip

虽然：

```bash
pip install pyyaml
```

很常见。

但工程实践中更推荐：

```bash
python3 -m pip install pyyaml
```

原因：

```text
python3
   │
   └── -m pip
          │
          ▼
       这个 Python
          │
          ▼
       对应的 pip
```

这样可以减少：

```text
python3 是 A
pip 却是 B
```

这种环境混乱。

例如：

```bash
python3 -m pip --version
```

可以确认 pip 属于哪个 Python。

## 13. --no-cache-dir 是什么

例如：

```bash
pip install --no-cache-dir pyyaml
```

意思是：

不使用 pip 的本地下载缓存。

普通情况下 pip 可能保留下载缓存：

```text
~/.cache/pip/
```

使用：

```text
--no-cache-dir
```

可以减少缓存占用。

常见于：

- Docker
- CI/CD
- 临时构建环境

例如：

```bash
python3 -m pip install --no-cache-dir -r requirements.txt
```

## 14. 查看已经安装的 package

```bash
pip list
```

例如：

```text
Package    Version
---------- -------
Jinja2     3.x
PyYAML     6.x
pip        24.x
setuptools ...
```

推荐：

```bash
python3 -m pip list
```

## 15. 查看某一个 package

```bash
pip show pyyaml
```

可以看到：

- Name
- Version
- Location
- Requires

特别重要的是：

`Location`

它可以告诉你：

PyYAML 到底安装在哪里

## 16. 测试 package 是否可以 import

例如：

```bash
python3 -c "import yaml; print(yaml.__version__)"
```

Jinja2：

```bash
python3 -c "import jinja2; print(jinja2.__version__)"
```

注意：

`pip package name`

和：

`Python import name`

不一定相同。

例如：

```text
pip install pyyaml
       │
       ▼
import yaml
```

而不是：

```python
import pyyaml
```

## 17. pip freeze

查看当前环境中安装的 package：

```bash
pip freeze
```

例如：

```text
Jinja2==3.1.6
PyYAML==6.0.2
```

## 18. 生成 requirements.txt

常用：

```bash
pip freeze > requirements.txt
```

它的含义：

```text
当前 venv
    │
    │ pip freeze
    ▼
已经安装的 package
    │
    │ >
    ▼
requirements.txt
```

得到：

```text
Jinja2==3.1.6
PyYAML==6.0.2
```

## 19. requirements.txt 的核心作用

不要把它理解成：

Python 的配置文件

更准确的是：

项目依赖环境的声明/快照。

例如：

```text
requirements.txt
        │
        ├── PyYAML==6.0.2
        └── Jinja2==3.1.6
```

其他开发者拿到项目：

```text
requirements.txt
        │
        ▼
pip install -r requirements.txt
        │
        ▼
创建类似的 Python 依赖环境
```

## 20. 从 requirements.txt 安装

```bash
pip install --no-cache-dir -r requirements.txt
```

`-r`：

```text
-r requirements.txt
```

意思：

从 requirements 文件读取 package 列表。

例如：

```text
requirements.txt

PyYAML==6.0.2
Jinja2==3.1.6
```

执行：

```bash
pip install -r requirements.txt
```

相当于：

```bash
pip install PyYAML==6.0.2
pip install Jinja2==3.1.6
```

## 21. 一个完整项目初始化流程

推荐：

```bash
mkdir my-project
cd my-project

python3 -m venv ./venv

source ./venv/bin/activate

python3 -m pip install --upgrade pip

python3 -m pip install --no-cache-dir pyyaml
python3 -m pip install --no-cache-dir jinja2

python3 -m pip freeze > requirements.txt
```

项目结构：

```text
my-project/
├── venv/
├── requirements.txt
└── execute.py
```

## 22. 运行 Python 程序

激活环境：

```bash
source ./venv/bin/activate
```

然后：

```bash
python3 execute.py --help
```

此时：

```text
python3
   │
   ▼
./venv/bin/python3
   │
   ▼
execute.py
```

## 23. 不激活也可以运行

这是非常重要的一点。

不需要：

```bash
source ./venv/bin/activate
```

可以直接：

```bash
./venv/bin/python3 execute.py --help
```

甚至：

```bash
./venv/bin/pip install pyyaml
```

这说明：

venv 并不依赖 activate 才能工作。

activate 只是方便你操作。

## 24. 激活 vs 直接指定路径

两种方式：

### 方式 A：激活

```bash
source ./venv/bin/activate

python3 execute.py
pip install pyyaml
```

心智模型：

```text
修改 PATH
   │
   ├── python3 → venv/bin/python3
   └── pip     → venv/bin/pip
```

### 方式 B：不激活

```bash
./venv/bin/python3 execute.py
./venv/bin/pip install pyyaml
```

心智模型：

```text
明确指定 executable
        │
        ▼
./venv/bin/python3
```

工程脚本、CI/CD 中经常更喜欢方式 B，因为更加明确。

## 25. 推荐的更稳健方式

例如：

```bash
./venv/bin/python3 -m pip install pyyaml
```

这里有两层绑定：

```text
./venv/bin/python3
        │
        └── -m pip
              │
              ▼
        这个 Python 对应的 pip
```

这是非常稳健的写法。

## 26. 退出虚拟环境

```bash
deactivate
```

执行：

```bash
deactivate
```

之后：

```text
python3
  │
  ▼
系统 Python
```

## 27. 删除虚拟环境

venv 本质上就是一个目录。

因此可以：

```bash
rm -rf ./venv
```

然后重新创建：

```bash
python3 -m venv ./venv
```

再：

```bash
python3 -m pip install -r requirements.txt
```

所以：

```text
venv
 │
 ├── 可以删除
 ├── 可以重新创建
 └── 不应该作为项目源码提交
```

## 28. 为什么通常不要把 venv 提交到 Git

不要：

```bash
git add venv/
```

因为：

```text
venv/
```

包含：

- Python
- pip
- site-packages
- scripts
- 大量环境文件

通常非常大，而且与机器、Python 版本、OS 等有关。

推荐：

`.gitignore`

```gitignore
venv/
.venv/
```

然后提交：

```text
requirements.txt
```

例如：

```text
project/
├── .gitignore
├── requirements.txt
├── execute.py
└── templates/
```

而不是：

```text
project/
├── venv/              # 不提交
├── requirements.txt
└── execute.py
```

## 29. 推荐 .gitignore

```gitignore
venv/
.venv/
__pycache__/
*.py[cod]
```

## 30. .venv 和 venv

两种都常见：

```text
venv/
```

或者：

```text
.venv/
```

很多现代 Python 项目更喜欢：

```text
.venv/
```

因为：

```text
.venv/
```

默认是隐藏目录。

创建：

```bash
python3 -m venv .venv
```

激活：

```bash
source .venv/bin/activate
```

运行：

```bash
.venv/bin/python3 execute.py
```

两者本质没有区别。

## 31. 查看 Python 版本

系统 Python：

```bash
python3 --version
```

venv Python：

```bash
./venv/bin/python3 --version
```

激活后：

```bash
python3 --version
```

## 32. 查看 pip 版本及归属

```bash
python3 -m pip --version
```

例如：

```text
pip 25.x from
.../venv/lib/python3.x/site-packages/pip
(Python 3.x)
```

这个命令非常适合排查：

pip 到底属于哪个 Python？

## 33. 检查环境是否正确

进入项目后：

```bash
source ./venv/bin/activate

which python3
which pip

python3 --version
python3 -m pip --version
```

理想结果：

```text
/path/to/project/venv/bin/python3
/path/to/project/venv/bin/pip
Python 3.x.x
pip ... /path/to/project/venv/...
```

## 34. 常用命令速查表

| 命令 | 作用 |
|------|------|
| python3 -m venv ./venv | 创建虚拟环境 |
| python3 -m venv .venv | 创建隐藏虚拟环境 |
| source ./venv/bin/activate | 激活环境 |
| deactivate | 退出环境 |
| which python3 | 查看当前 Python |
| which pip | 查看当前 pip |
| python3 --version | 查看 Python 版本 |
| python3 -m pip --version | 查看 pip 及其归属 |
| python3 -m pip list | 查看已安装 package |
| python3 -m pip freeze | 输出已安装 package + 版本 |
| python3 -m pip show pyyaml | 查看 package 详细信息 |
| python3 -m pip install pyyaml | 安装 package |
| python3 -m pip install --no-cache-dir pyyaml | 不使用 pip cache 安装 |
| python3 -m pip uninstall pyyaml | 删除 package |
| python3 -m pip install -r requirements.txt | 根据依赖文件安装 |
| python3 -m pip freeze > requirements.txt | 生成依赖快照 |
| ./venv/bin/python3 execute.py | 直接使用 venv Python |
| rm -rf ./venv | 删除虚拟环境 |

## 35. 最推荐的工程工作流

### 第一次创建项目

```bash
mkdir my-project
cd my-project

python3 -m venv .venv

source .venv/bin/activate

python3 -m pip install --upgrade pip

python3 -m pip install pyyaml
python3 -m pip install jinja2

python3 -m pip freeze > requirements.txt
```

### 日常开发

```bash
cd my-project

source .venv/bin/activate

python3 execute.py --help
python3 execute.py config.yaml
```

### 新机器部署

```bash
git clone <project>
cd <project>

python3 -m venv .venv

source .venv/bin/activate

python3 -m pip install -r requirements.txt

python3 execute.py --help
```

## 36. CI/CD 中的推荐方式

CI 中可以不依赖 activate：

```bash
python3 -m venv .venv

.venv/bin/python3 -m pip install --no-cache-dir -r requirements.txt

.venv/bin/python3 execute.py --help
```

甚至：

```bash
.venv/bin/python3 -m pip install --no-cache-dir -r requirements.txt
.venv/bin/python3 execute.py
```

这种方式最大的优点：

每一步都明确指定使用哪个 Python。

## 37. 一个非常重要的坑：sudo + pip

尽量避免：

```bash
sudo pip install pyyaml
```

因为这通常意味着：

```text
系统 Python
      │
      ▼
系统 site-packages
```

可能污染系统环境。

更推荐：

```bash
python3 -m venv .venv
source .venv/bin/activate

python3 -m pip install pyyaml
```

形成：

```text
Project
   │
   ▼
.venv
   │
   ▼
PyYAML
```

## 38. 一个非常重要的坑：pip 和 python 不匹配

例如：

```bash
which python3
# /project/.venv/bin/python3

which pip
# /usr/bin/pip
```

这就是典型问题：

```text
python3 → venv
pip     → system
```

此时：

```bash
pip install xxx
```

可能安装到了系统环境。

所以推荐：

```bash
python3 -m pip install xxx
```

而不是单纯：

```bash
pip install xxx
```

## 39. 第二个重要坑：忘记激活

例如：

```bash
python3 -m venv .venv

python3 -m pip install pyyaml
```

如果没有：

```bash
source .venv/bin/activate
```

那么这里的：

`python3`

很可能还是系统 Python。

正确：

```bash
source .venv/bin/activate

python3 -m pip install pyyaml
```

或者直接：

```bash
.venv/bin/python3 -m pip install pyyaml
```

## 40. 第三个重要坑：requirements.txt 不是 venv

这是非常重要的概念区分。

```text
requirements.txt
        │
        │ 描述
        ▼
项目依赖

.venv/
        │
        │ 保存
        ▼
实际 Python 环境
```

所以：

`requirements.txt ≠ venv`

更准确：

```text
requirements.txt
        │
        │ 创建/恢复
        ▼
       venv
        │
        ▼
实际运行环境
```

## 41. 第四个重要坑：requirements.txt 与 pip freeze

两者不是完全等价的概念。

```bash
pip freeze > requirements.txt
```

得到的是：

当前环境实际安装了什么。

而理想的项目依赖声明通常更关注：

项目运行需要什么。

例如项目源码：

```python
import yaml
import jinja2
```

项目真正需要：

- PyYAML
- Jinja2

但 `pip freeze` 可能包含更多间接依赖或工具包。

所以：

`pip freeze`

更像：

环境快照

而：

`requirements.txt`

可以作为：

项目依赖声明

具体是否直接用 `pip freeze` 生成，取决于项目复杂度和依赖管理方式。

## 42. venv 的整体架构

建议形成下面这个架构图：

```text
                    Python Project
                          │
          ┌───────────────┴───────────────┐
          │                               │
       Source Code                   requirements.txt
          │                               │
          │                               │
          ▼                               │
       execute.py                         │
          │                               │
          │                               │
          └───────────────┐               │
                          ▼               │
                        .venv ◄───────────┘
                          │
             ┌────────────┼────────────┐
             │            │            │
             ▼            ▼            ▼
          python         pip       site-packages
             │                         │
             │                    ┌────┴─────┐
             │                    │          │
             ▼                    ▼          ▼
       execute.py              PyYAML     Jinja2
```

## 43. 最核心的四个对象

学习 venv 不需要记几十条命令。

先掌握这四个对象：

```text
             Python Project
                    │
        ┌───────────┼───────────┐
        │           │           │
        ▼           ▼           ▼
      Python       pip       requirements
        │           │           │
        │           │           │
        │           ▼           │
        │    install package    │
        │           │           │
        ▼           ▼           ▼
                  .venv
```

对应：

```text
Python
  → 谁来运行代码？

pip
  → 谁来安装依赖？

venv
  → 依赖安装到哪里？

requirements.txt
  → 项目需要哪些依赖？
```

## 44. 最终心智模型

把整个 Python venv 工作流压缩成：

```text
                 项目
                  │
                  ▼
          python3 -m venv .venv
                  │
                  ▼
              创建环境
                  │
                  ▼
          source .venv/bin/activate
                  │
                  ▼
             切换 PATH
                  │
                  ▼
       python3 / python3 -m pip
                  │
             ┌────┴────┐
             │         │
             ▼         ▼
          运行代码    安装依赖
                       │
                       ▼
                 site-packages
                       │
                       ▼
               pip freeze
                       │
                       ▼
              requirements.txt
                       │
                       │ 新机器
                       ▼
             pip install -r
                       │
                       ▼
                恢复环境
```

因此可以记住一句话：

venv 管环境隔离，Python 管代码执行，pip 管依赖安装，requirements.txt 管依赖描述。

## 45. 我最推荐记住的 10 条命令

```bash
# 1. 创建环境
python3 -m venv .venv

# 2. 激活
source .venv/bin/activate

# 3. 确认 Python
which python3

# 4. 确认 Python 版本
python3 --version

# 5. 确认 pip 属于哪个 Python
python3 -m pip --version

# 6. 安装依赖
python3 -m pip install pyyaml jinja2

# 7. 查看依赖
python3 -m pip list

# 8. 导出依赖
python3 -m pip freeze > requirements.txt

# 9. 从依赖文件恢复
python3 -m pip install -r requirements.txt

# 10. 退出环境
deactivate
```

如果是脚本/CI，我尤其推荐：

```bash
.venv/bin/python3 -m pip install -r requirements.txt
.venv/bin/python3 execute.py --help
```

因为它不依赖 Shell 是否已经 activate，解释器路径是显式的，工程行为更加确定。
