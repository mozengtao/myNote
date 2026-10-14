# 跨示例多用户 / 安全审查报告

> 本文档是一次**只读审查**：不修改 `examples/01-08`、它们对应的文档、
> `models/`、`services/`、`infrastructure/`、`requirements.txt`、
> `.gitignore`、`config/`、`ui/`、`app.py`，也不修改本文档之外的任何
> 已有文件。所有结论都基于真实执行的命令与真实读到的源码，命令与输出
> 原样贴在下面，没有删改。
>
> 审查时间：本次任务执行时（见交付报告的时间戳）。
> 审查范围：整个 `nicegui_learning_lab` 仓库（`examples/01~08`、
> `services/`、`infrastructure/`、`models/`）。
> 审查方式：静态阅读源码 + 全仓库 `grep` + 实际运行 `pytest`。

## 1. 命令执行安全：是否存在 `shell=True` 或字符串拼接命令

### 1.1 真实命令与真实输出

```bash
cd /home/morrism/myNote/nicegui_learning_lab
grep -rn "shell=True" . --include='*.py' --exclude-dir=.venv --exclude-dir=__pycache__ --exclude-dir=.pytest_cache
```

真实输出：

```
./infrastructure/process_runner.py:105:    在传入 `list[str]` 且不设置 `shell=True` 时，参数之间不会经过任何
```

唯一一处命中是**文档字符串里提到"没有设置 `shell=True`"这件事本身**，
不是真的调用了 `shell=True`。全仓库源码里不存在任何一处
`subprocess.run(..., shell=True)` 或等价写法。

```bash
grep -rn "subprocess" . --include='*.py' --exclude-dir=.venv --exclude-dir=__pycache__ --exclude-dir=.pytest_cache
```

真实输出（已在交付报告"验证命令及真实输出"一节完整贴出，这里摘录关键
几行）：

```
./infrastructure/process_runner.py:53:import subprocess
./infrastructure/process_runner.py:128:        completed = subprocess.run(
./infrastructure/process_runner.py:134:    except subprocess.TimeoutExpired as exc:
./infrastructure/process_runner.py:204:        proc = await asyncio.create_subprocess_exec(
```

### 1.2 结论

全仓库**只有两个**真正会调用操作系统启动子进程的地方，都在
`infrastructure/process_runner.py` 里：

- `run_subprocess()`：`subprocess.run(argv, cwd=..., capture_output=True,
  timeout=timeout)`，其中 `argv = [executable, *args]` 是一个 `list[str]`，
  **没有传 `shell=True`**，因此 `args` 中的任何字符（`;`、`|`、`` ` ``、
  `$()` 等）都不会被 shell 解析，只会被当成 argv 的字面值传给
  `execve()`。
- `stream_subprocess()`：`asyncio.create_subprocess_exec(executable, *args,
  ...)`——`create_subprocess_exec`（区别于 `create_subprocess_shell`）同样
  从设计上就不经过 shell。

**结论：全仓库不存在 shell 注入面。** 这个安全保证的另一半来自
`services/command_service.py` 的 `COMMAND_WHITELIST` + `_resolve_args()`：
`executable` 永远来自 `shutil.which()` 对一个固定命令名的解析结果，
`args` 永远先经过 `ArgSpec`（`choice`/`regex`）校验，不存在任何"把界面上
的任意字符串直接拼进命令行"的路径。

## 2. 路径安全：`config_service.py` / `file_service.py` 是否都在用 `path_guard.confine_path()`

### 2.1 真实命令与真实输出

```bash
grep -n "confine_path" services/config_service.py services/file_service.py
```

```
services/config_service.py:36:from services.path_guard import confine_path
services/config_service.py:59:    return confine_path(filename, base_dir, must_exist=True)
services/file_service.py:37:from services.path_guard import confine_path
services/file_service.py:95:    resolved_path = confine_path(filename, base_dir, must_exist=True)
```

```bash
grep -n " / filename\|base_dir /" services/config_service.py services/file_service.py
```

```
(无输出)
```

### 2.2 结论

两个文件里**唯一**把"外部提供的文件名"变成"磁盘路径"的地方，都是直接
调用 `confine_path()`（`config_service.resolve_config_path()` 第 59 行、
`file_service.read_text_file()` 第 95 行），没有任何一处自己写
`base_dir / filename` 这种拼接后不经过越界检查的代码。

`confine_path()`（`services/path_guard.py`）本身的校验顺序也经过检查：
先拒绝绝对路径（防止 `Path('/a') / '/etc/passwd'` 直接整体替换掉左边
路径这个 pathlib 陷阱），再用 `.resolve()` 解析 `..`/符号链接之后，用
`candidate.relative_to(base)` 确认结果仍落在 `base_dir` 内部——这个顺序
是正确的（如果先 `relative_to` 再处理绝对路径，攻击者可以用绝对路径
绕过检查）。

`services/file_service.py` 的 `list_viewable_files()` 里有一处
`base.iterdir()` 直接遍历 `base_dir`——这不经过 `confine_path()`，但这是
**遍历一个已经受信任的目录本身**，不涉及任何外部输入，不是路径穿越面。

**结论：两个文件都严格通过 `confine_path()` 做路径确认，没有发现绕过。**

## 3. 多用户状态泄露排查：examples/01~08 的模块级 / 全局可变状态

### 3.1 真实命令与真实输出

```bash
for f in examples/0*.py; do echo "=== $f ==="; grep -nE "^_[A-Za-z_]+\s*:\s*(list|dict|set)\[|^_[A-Za-z_]+\s*=\s*(\[|\{)" "$f"; done
```

```
=== examples/01_hello_nicegui.py ===
=== examples/02_layout_and_components.py ===
=== examples/03_events_and_state.py ===
=== examples/04_config_checker.py ===
=== examples/05_task_table.py ===
41:_DATA_SOURCE: list[TaskRecord] = generate_mock_tasks(_MOCK_TASK_COUNT, seed=42)
42:_TASKS_BY_ID: dict[str, TaskRecord] = {task.id: task for task in _DATA_SOURCE}
47:_COLUMNS = [
60:_STATUS_FILTER_OPTIONS = ['全部'] + [status.value for status in TaskStatus]
=== examples/06_command_console.py ===
=== examples/07_background_task.py ===
=== examples/08_test_console.py ===
79:_HISTORY: list[TaskRecord] = []
83:_HISTORY_BY_ID: dict[str, TaskRecord] = {}
86:_STATUS_LABELS: dict[TaskStatus, str] = {
95:_STATUS_COLOR_CLASSES: dict[TaskStatus, str] = {
106:_HISTORY_COLUMNS = [
```

另外在 `services/command_service.py` 里确认了两处模块级字典：

```bash
grep -n "^[A-Z_]*\s*:\s*dict\|^[A-Z_]*\s*=\s*_build" services/command_service.py
```

```
services/command_service.py:78:LIST_DIRECTORY_CHOICES: dict[str, Path] = {
services/command_service.py:202:COMMAND_WHITELIST: dict[str, CommandSpec] = _build_whitelist()
```

### 3.2 逐个判断

| 示例 | 模块级可变/共享状态 | 是否被用户操作改变 | 多用户并发下的后果 | 对应文档是否已标注 |
| --- | --- | --- | --- | --- |
| 01 | 无 | — | 无风险 | — |
| 02 | 无（`ENVIRONMENTS`/`IDLE_STATUS_TEXT` 是只读常量） | — | 无风险 | — |
| 03 | 无（`ENVIRONMENTS`/`_HOSTNAME_PATTERN` 是只读常量/编译后的正则） | — | 无风险 | — |
| 04 | 无（`CONFIG_DIR`/`REQUIRED_FIELDS` 是只读常量；每次请求都重新从磁盘读取，不缓存状态） | — | 无风险 | — |
| 05 | `_DATA_SOURCE`/`_TASKS_BY_ID`：模块级，但由 `generate_mock_tasks(200, seed=42)` **只生成一次、之后从不被任何事件回调修改**（排序 `_sorted_source()` 返回新列表，不就地修改源数据） | 否——所有用户看到的都是同一份固定模拟数据，不存在"甲的操作影响乙看到的数据"的路径 | **低风险**：所有用户看到完全相同的 200 条静态模拟任务，没有真实用户数据、没有"串台"概念（本来就该一样）。真正的会话状态（分页/排序/筛选）在 `_TableState`（函数内局部变量，每次页面加载重新创建）里，天然按会话隔离 | **部分标注**：文档 `docs/examples/05_task_table.md` 的 Mermaid 图里把 `_DATA_SOURCE` 明确标注为"模块级、只读、常驻内存"，但没有用一句话专门点出"所有用户看到的是同一份数据"这个事实。**建议补充**（不在本次任务修改范围内，留给集成阶段）。 |
| 06 | `COMMAND_WHITELIST`/`LIST_DIRECTORY_CHOICES`：模块级，但在模块加载时通过 `_build_whitelist()` 构建一次之后**永不在运行时被修改**（`run_command()`/`stream_command()` 只读取，不写入） | 否 | 无风险：这是只读配置，等价于常量 | 文档已明确描述为"固定白名单...不提供任何运行时注册新命令的接口" |
| 07 | 无（`_PageState` 在每次 `background_task_page()` 被调用时新建，是函数局部变量，天然按会话隔离） | — | 无风险 | — |
| 08 | `_HISTORY: list[TaskRecord]` / `_HISTORY_BY_ID: dict[str, TaskRecord]`：模块级、**可变**、**所有浏览器标签页共享** | **是**——任何用户点击"开始运行"产生的 `TaskRecord` 都会被 `_HISTORY.append(record)` 追加进同一份全局列表，所有访问这个页面的人都能在历史表格里看到并点开查看详情（包括完整日志） | **已知、已承认的高风险模式**（本例全是模拟数据，不含真实敏感信息，但这正是"数据串扰"问题的典型模式，等比例放大到真实业务数据就是一个真正的信息泄露漏洞） | **已充分标注**：① 模块文档"历史记录的简化处理"一节明确写出"所有打开这个页面的浏览器标签页共享同一份历史...也没有按用户做任何隔离"；② `docs/examples/08_test_console.md` 的 Failure Modes 第 3 条、Exercise 4 都专门讨论了这个问题，并明确指向"这是示例 9 要展开的话题" |

### 3.3 结论

- 示例 01/02/03/04/06/07：没有发现任何未声明的跨用户状态串扰风险。
- 示例 05：模块级共享数据是只读静态模拟数据，无真实串扰风险，文档可以
  更明确一点，但不是一个需要紧急修复的问题。
- 示例 08：`_HISTORY`/`_HISTORY_BY_ID` 是本次审查里**唯一**一处真正会把
  "甲用户的操作结果"展示给"乙用户"看的模块级可变状态——但这个局限性
  已经在代码注释和文档里被显式承认、显式讨论，并且正是 `examples/09_
  multi_user_state.py`（本次任务新增）要正式解决的问题。**这是一个"已
  知且已文档化的局限"，不是一个"被遗漏的 bug"。**

## 4. 任务生命周期一致性：`TaskRecord.status` 是否只通过 `transition_to()` 修改

### 4.1 真实命令与真实输出

```bash
grep -rn "\.status\s*=" . --include='*.py' --exclude-dir=.venv --exclude-dir=__pycache__ --exclude-dir=.pytest_cache
```

```
./services/task_query_service.py:172:        task for task in tasks if task.status == status_filter
./tests/test_task_query_service.py:36:        assert first.status == second.status
./tests/test_task_query_service.py:104:    all_matching = [task for task in tasks if task.status == TaskStatus.FAILED]
./tests/test_task_query_service.py:110:    assert all(task.status == TaskStatus.FAILED for task in page_items)
./models/task.py:17:赋值 `.status = ...`，这样非法跳转（例如还没运行过就从 PENDING 直接变成
./models/task.py:109:        self.status = new_status
```

### 4.2 结论

全仓库里唯一一处真正的赋值语句 `self.status = new_status` 在
`models/task.py` 第 109 行，就在 `TaskRecord.transition_to()` 方法内部
——这正是这个方法**唯一允许存在的**赋值点，且赋值之前已经执行过
`can_transition(self.status, new_status)` 的合法性校验（见
`models/task.py` 第 106~108 行）。

其余所有命中的 `.status ==` / `.status =` 都是**比较**（`==`），不是
赋值。具体检查了以下几处调用点：

- `services/test_service.py`：`run_test_suite()` 内部所有状态变化
  （`PENDING -> RUNNING`、`-> FAILED`/`SUCCEEDED`/`CANCELLED`）全部通过
  `record.transition_to(...)` 完成（第 226、234、250、260 行附近）。
- `services/task_query_service.py`：`generate_mock_tasks()` 的
  `_advance_to()` 辅助函数同样全部通过 `record.transition_to(...)`
  完成，函数文档里也明确写了"必须全程通过
  `TaskRecord.transition_to()` 完成，不能直接赋值 `.status`"。
- `examples/05_task_table.py` / `examples/08_test_console.py`：这两个
  UI 文件都只**读取** `record.status`（展示文案、按状态筛选），没有任何
  一处修改它。

**结论：`TaskRecord.status` 的所有变化都严格通过 `transition_to()` 完成，
没有发现任何直接赋值绕过状态机的情况。**

## 5. 分层例外记录与评价：`infrastructure/process_runner.py` 为什么被允许 import `services/errors.py`

### 5.1 该模块自己是怎么解释这个例外的

`infrastructure/process_runner.py` 模块文档字符串原文（"层次说明（一个
刻意的、狭窄的例外，请知悉）"一节）：

> 项目的总原则是"infrastructure/ 绝不 import services/\*"。但本次任务
> 同时提出了两条更具体、互相之间会和这条总原则冲突的硬性要求：
> 1. "所有你抛出的异常必须是 `services/errors.py` 里已有类型的实例"；
> 2. 测试规格明确要求**直接**调用 `process_runner.run_subprocess()`
>    （不经过 `command_service`）来触发超时场景，并断言它直接抛出
>    `services.errors.CommandTimeoutError`。
>
> 要同时满足这两条，`process_runner.py` 就必须能够直接抛出
> `services.errors` 里定义的异常类型，这意味着它必须 import
> `services.errors`。本模块因此只做了这一个狭窄的例外：只 import
> `services.errors`（一个不依赖 nicegui、不包含任何业务逻辑、本身也不
> import 本项目其他任何模块的纯异常定义模块），绝不 import `services/`
> 下的其他任何模块……也绝不 import nicegui。

### 5.2 一个值得指出的内部文档不一致

`infrastructure/__init__.py`（包级文档字符串）原文：

> 这里的模块负责封装 subprocess 执行……它们不允许导入 services/、ui/、
> examples/ 或 nicegui——只依赖标准库，这样"与操作系统的边界"才可以被
> 单独、独立地测试。

包级文档说的是一条**没有例外**的绝对规则，而 `process_runner.py` 自己
的模块文档里给出的却是一个**有例外**的版本。两者并不矛盾（后者是对前者
的具体细化），但如果只看包级 `__init__.py` 而不往下看
`process_runner.py` 的模块文档，会得到"绝对不行"的错误印象。这是一个
纯文档层面的小瑕疵，建议在后续维护 `infrastructure/__init__.py` 时同步
提一句"除 `services/errors.py` 这个无依赖的异常定义模块外"，但**本次
审查不修改 `infrastructure/` 下的任何文件**，这里只记录、不处理。

### 5.3 独立评估

**这个例外本身是合理的，理由：**

1. **方向性受限、范围极小**：只允许 import 一个具体文件
   (`services/errors.py`)，而不是"services/ 这个包"，而且这个文件本身
   经过检查确认：不依赖 nicegui，不依赖 `services/` 下任何其他模块，是
   一个真正的"无依赖叶子模块"（纯 `Exception` 子类定义）。它和
   `services/path_guard.py` 对待 `services.errors` 的方式完全一致——
   项目里已经有一个先例，把 `services/errors.py` 当作"全项目共享、谁都
   能依赖"的特例对待。
2. **目的明确、经过权衡**：是为了同时满足"异常类型统一"与"测试规格要求
   直接测试 `process_runner`"这两条互相冲突的硬性要求，不是因为偷懒或
   疏忽而抄近道。
3. **有清晰的书面边界**：文档明确写出"绝不 import `services/` 下的其他
   任何模块……也绝不 import nicegui"，不是一个无限扩张的口子。

**但也存在真实的长期风险，值得记录：**

1. **缺乏自动化强制手段**：审查时专门检查过——

   ```bash
   grep -rln "import-linter\|importlinter\|layer\b" --include='*.py' --include='*.toml' --include='*.ini' --include='*.cfg' . --exclude-dir=.venv
   find . -maxdepth 1 -iname "*.toml" -o -iname "*.cfg" -o -iname "*.ini" | grep -v .venv
   ```

   两个命令都没有输出——仓库里**没有**任何 `import-linter`/类似工具的
   配置文件，这条"只能 import `services/errors.py`"的边界完全依赖人工
   代码评审来维持，没有 CI 检查会在有人不小心多 import 了一个
   `services/` 下的其他模块时自动报错。先例一旦开了口子，长期来看全靠
   自律。
2. **依赖方向出现了一个局部反转**：`infrastructure` 本应是最底层、被
   所有人依赖、自己不依赖任何人的层；现在它反过来依赖了 `services/`
   包内的一个文件，哪怕这个文件本身很"干净"，从架构图上看，依赖箭头
   出现了一次不符合"严格分层"直觉的指向。

**更好的替代方案（供后续迭代参考，本次不改动代码）：**

把 `services/errors.py` 这几个异常类（或者至少
`CommandTimeoutError`/`CommandExecutionError` 这两个 `process_runner.py`
真正用得到的类型）搬到一个两者都能依赖、本身不依赖任何人的位置——例如
`models/` 包（本项目里 `models/task.py` 已经被明确定位为"纯标准库、不
依赖 nicegui、不依赖本项目任何其他模块"的最底层模块，是这类"全项目共享
词汇"的天然归宿）。这样 `infrastructure/` 和 `services/` 都变成"依赖
`models/`"，不再出现 `infrastructure -> services` 这一条反向箭头，`
import-linter` 之类的工具也可以直接按"models 不依赖任何人，
infrastructure 只依赖 models，services 依赖 models + infrastructure"
这条更干净的规则去强制检查，不需要为 `services/errors.py` 单独开一条
例外规则。这是一个纯粹的"属于哪个包"重新归类问题，不涉及任何行为变化，
风险很低，但**超出本次任务允许修改的文件范围**，这里只作为审查建议
记录。

## 6. 总结表格：必须跟进修复 vs. 已文档化的已知局限

| 问题 | 严重程度 | 分类 | 建议处理方式 |
| --- | --- | --- | --- |
| 示例 8 `_HISTORY`/`_HISTORY_BY_ID` 模块级共享，不按用户隔离 | 中（当前全是模拟数据，无真实敏感信息；设计模式本身若原样用于生产数据会是真实的信息泄露） | **已文档化的已知局限** | 不需要在本次任务里修复——`examples/09_multi_user_state.py` 已经给出"如何改造成按用户隔离"的正式演示代码；若未来要让示例 8 本身也遵循这个模式，应作为一次独立、显式的改造任务去做，而不是顺手改掉 |
| 示例 5 `_DATA_SOURCE`/`_TASKS_BY_ID` 共享同一份模拟数据，文档未明确点出"所有用户看到相同数据" | 低（只读静态演示数据，没有真实串扰风险） | **建议在文档阶段补充说明**，非功能性缺陷 | 留给后续维护 `docs/examples/05_task_table.md` 时顺手加一句话，不属于本次任务范围 |
| `infrastructure/__init__.py`（包级文档）与 `process_runner.py`（模块级文档）关于"是否允许 import services/"的措辞不完全一致 | 低（纯文档措辞问题，不影响实际行为，`process_runner.py` 自身的例外说明已经足够清楚） | **文档措辞不一致，建议后续澄清** | 留给后续维护 `infrastructure/__init__.py` 时补一句引用，不属于本次任务范围（本次审查不修改 `infrastructure/` 下任何文件） |
| `infrastructure -> services.errors` 这个分层例外缺乏自动化强制手段（没有 import-linter 之类的配置） | 低~中（目前范围很窄且有清晰书面边界，但长期维护全靠人工评审自律） | **架构设计取舍，已被模块文档显式记录和论证**，评估为"合理但有改进空间" | 可选的后续改进：把相关异常类型搬到 `models/` 包，消除反向依赖箭头；或引入 `import-linter` 做自动化检查。两者都超出本次任务范围，仅作记录 |
| 命令执行（`shell=True`/字符串拼接命令） | — | **未发现问题** | 无需跟进 |
| 路径安全（`config_service`/`file_service` 绕过 `confine_path()`） | — | **未发现问题** | 无需跟进 |
| `TaskRecord.status` 直接赋值绕过 `transition_to()` | — | **未发现问题** | 无需跟进 |
| 示例 1/2/3/4/6/7 模块级状态串扰 | — | **未发现问题**（无风险的只读常量或天然按会话隔离的局部变量） | 无需跟进 |
