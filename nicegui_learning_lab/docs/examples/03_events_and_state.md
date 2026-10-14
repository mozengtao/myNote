# 示例 3：事件、输入与状态 —— 命令参数生成器

## 1. Why —— 为什么工程中需要它

真实的工程 GUI 小工具里，"点击一个按钮"几乎从来不是孤立的一步：它通常是
"读取若干输入 -> 校验 -> 执行某个动作 -> 把结果或错误原因展示给用户"这条
链路的最后一环。如果校验逻辑和 UI 代码混在一起，不仅难以单独测试，日后
想把这段逻辑挪到后端 Service、或者换一套 UI 框架时，都要把校验规则重新
抄一遍、重新调试一遍。

本示例用一个"命令参数生成器"演示这整条链路，并且刻意把校验函数写成一个
**完全不依赖 nicegui 的纯函数**——这是贯穿本项目的一条设计原则：UI 框架
会变、会升级、甚至会被换掉，但"主机名必须符合什么格式"这条业务规则不应该
因此被迫重写。

## 2. Mental Model —— 心智模型

```
+-------------------+      +----------------------+      +-------------------+
|   UI 输入组件        | ---> |   纯函数（无 nicegui）    | ---> |   UI 展示/反馈        |
|  input / select /  |      |  validate_hostname()  |      |  label / notify     |
|  textarea           |      |  build_preview_       |      |                     |
|  （持有 .value）      |      |  command()            |      |                     |
+-------------------+      +----------------------+      +-------------------+
```

一次"点击生成预览"的完整流程：

```
用户点击"生成预览"
      |
      v
handle_generate() 被调用
      |
      v
读取 host_input.value / env_select.value / extra_textarea.value
      |
      v
调用纯函数 validate_hostname(host)
      |
      +-- 返回非 None（有错误原因） --> ui.notify(type='negative') --> 结束，不生成预览
      |
      +-- 返回 None（校验通过）
              |
              v
      调用纯函数 build_preview_command(host, env, extra_args)
              |
              v
      preview_label.text = 预览文本
              |
              v
      ui.notify(type='positive')，提示"预览已生成"
```

**页面级状态 vs 应用级状态**：`host_input`、`env_select`、`extra_textarea`
这几个对象各自持有的 `.value`，只属于"当前这一个浏览器标签页、当前这一次
页面加载"——这叫**页面级状态**：用户刷新页面，`index()` 重新执行一遍，
这些组件被重新创建，所有 `.value` 都回到初始值，彼此互不影响，也不会
"记住"上一次打开页面时填过什么。

如果将来需要让"当前选中的目标环境"这类信息在**多个页面之间共享**（比如
示例 9 要做一个"全局环境切换器"，顶部导航栏和好几个子页面都要读写同一个
"当前环境"），就不能再依赖某一个 `ui.select` 组件自己的 `.value`——那只是
某一个页面上某一个组件的局部状态。这时候需要一个更上层的**应用状态**
容器（NiceGUI 提供了 `app.storage.user` / `app.storage.general` 这类按
会话或全局持久化的存储机制）。这个话题在本示例里先点到为止，示例 9 会
专门展开"页面级状态"与"应用级状态"的边界和取舍。

**为什么校验函数要写成不依赖 nicegui 的纯函数**：`validate_hostname()`
的输入输出都是普通的 `str` / `None`，不接触任何 `ui.xxx` 对象。这意味着：

- 它可以被单独 `import` 后，用 `assert` 或 pytest 直接测试，不需要启动
  NiceGUI 应用、不需要浏览器、甚至不需要安装 nicegui（本示例的交付验证
  就是这么做的）。
- 未来如果要把它挪到 `services/` 层（比如给命令执行器做输入白名单校验），
  函数签名完全不用变。
- 它更容易被复用：比如一个命令行工具如果想复用同一套"主机名格式"规则，
  直接 `import` 这个函数即可，完全不需要牵扯 NiceGUI。

## 3. Architecture —— 架构说明

本示例仍然只有一个文件，但和示例 1、示例 2 不同的是，这里**已经出现了
值得独立出来的纯函数**，所以把它们放在文件顶部、`index()` 函数之外：

```
examples/03_events_and_state.py
    |
    +-- ENVIRONMENTS / _HOSTNAME_PATTERN   # 模块级常量
    |
    +-- validate_hostname(hostname)        # 纯函数，不依赖 nicegui
    +-- build_preview_command(host, env,   # 纯函数，不依赖 nicegui
    |                          extra_args)
    |
    +-- index()                            # @ui.page('/')，构建页面
    |       |
    |       +-- handle_generate()           # "生成预览"按钮的回调
    |       +-- handle_copy()               # "复制"按钮的回调
    |
    +-- if __name__ in {...}: ui.run(...)
```

依赖方向：`validate_hostname` / `build_preview_command` 不 `import nicegui`，
`index()` 及其内部的回调函数才会用到 `ui.*`。诚实地说，这两个纯函数目前
就写在 `examples/` 里，而不是 `services/` 里——因为它们还只被这一个示例
用到，没有"多个示例共用同一份校验规则"的真实需求。如果以后真的出现第二个
示例需要同样的主机名校验规则，那就是把它搬进 `services/` 的信号（而不是
提前过度设计）。

## 4. Complete Code —— 完整代码

完整源码见 [`examples/03_events_and_state.py`](../../examples/03_events_and_state.py)。
关键片段：

```python
def validate_hostname(hostname: str) -> str | None:
    if not hostname:
        return '主机名不能为空'
    if not _HOSTNAME_PATTERN.fullmatch(hostname):
        return '主机名只能包含字母、数字、点号（.）和连字符（-）'
    return None


def build_preview_command(host: str, env: str, extra_args: str) -> str:
    extra = extra_args.strip()
    preview = f'ssh {host} -- deploy --env {env}'
    if extra:
        preview = f'{preview} {extra}'
    return preview


def handle_generate() -> None:
    host = host_input.value.strip()
    env = env_select.value
    extra_args = extra_textarea.value or ''

    error = validate_hostname(host)
    if error is not None:
        preview_label.text = ''
        ui.notify(error, type='negative')
        return

    preview_text = build_preview_command(host, env, extra_args)
    preview_label.text = preview_text
    ui.notify('预览已生成', type='positive')
```

安全上的关键点：`build_preview_command()` 只做字符串拼接，整个示例里没有
任何一行代码会把拼出来的字符串交给 `subprocess` 或 `os.system` 去执行。
这不是疏漏，而是刻意的设计——一个格式合法的主机名（比如
`$(rm -rf /)` 其实就不满足 `_HOSTNAME_PATTERN`，但读者不应该因此误以为
"只要正则校验通过了，这段字符串就可以安全地丢给 shell 执行"，两者是完全
不同的安全问题，真正需要执行命令的示例会引入专门的白名单机制。

## 5. Execution —— 运行方式

```bash
cd /home/morrism/myNote/nicegui_learning_lab
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 examples/03_events_and_state.py
```

打开 `http://localhost:8080` 后预期现象：

1. 顶部标题"命令参数生成器"，下方一行说明文字（强调只做展示、不执行命令）。
2. 一个主机名输入框、一个目标环境下拉框（默认 `dev`）、一个额外参数的
   多行文本框，以及一个空白的预览展示区域。
3. 主机名留空直接点击"生成预览"：弹出红色通知"主机名不能为空"，预览区域
   保持空白。
4. 主机名填 `bad host!`（包含空格和感叹号）后点击"生成预览"：弹出红色
   通知"主机名只能包含字母、数字、点号（.）和连字符（-）"。
5. 主机名填 `cmts-01.lab.local`，环境选 `staging`，额外参数填
   `--dry-run`，点击"生成预览"：弹出绿色通知"预览已生成"，预览区域显示
   `ssh cmts-01.lab.local -- deploy --env staging --dry-run`。
6. 点击"复制"：弹出绿色通知"已复制到剪贴板"，此时可以在任意文本框里粘贴
   验证剪贴板内容确实是上一步的预览文本。
7. 刷新页面后直接点击"复制"（还没点过"生成预览"）：弹出黄色通知
   "请先生成预览，再复制"。

## 6. Lifecycle —— 一次真实操作的生命周期

以"主机名 `cmts-01.lab.local`、环境 `staging`、额外参数 `--dry-run`，
点击‘生成预览’"为例：

| 阶段 | 发生了什么 |
| --- | --- |
| User Action | 用户依次填写三个输入组件，点击"生成预览"按钮。 |
| Event Handler | NiceGUI 调用 `handle_generate()`。 |
| State/Service | 从 `host_input.value` / `env_select.value` / `extra_textarea.value` 读取当前页面级状态；本示例没有 Service 层，调用的是模块内的两个纯函数。 |
| Business Operation | `validate_hostname(host)` 做格式校验；通过后 `build_preview_command(host, env, extra_args)` 拼出预览文本。 |
| Result/Exception | 校验失败 -> 走"提前返回"分支，不生成预览；校验通过 -> 走"成功"分支。 |
| UI Feedback | 失败：`ui.notify(error, type='negative')`；成功：`preview_label.text = preview_text` 更新展示区域 + `ui.notify('预览已生成', type='positive')`。 |

补充一条"点击复制"的生命周期（它依赖上一次操作留下的状态）：

| 阶段 | 发生了什么 |
| --- | --- |
| User Action | 用户点击"复制"按钮。 |
| Event Handler | NiceGUI 调用 `handle_copy()`。 |
| State/Service | 读取 `preview_label.text`（这是上一次"生成预览"操作留下的页面级状态，不是重新计算的）。 |
| Business Operation | 判断这段文本是否为空。 |
| Result/Exception | 为空 -> 提示"请先生成预览"；非空 -> 调用 `ui.clipboard.write(text)`。 |
| UI Feedback | `ui.notify(type='warning')` 或 `ui.notify(type='positive')`。 |

## 7. API Reference —— API 速查表

| API/Concept | Purpose | Input | Output/Effect | Common Scenario |
| --- | --- | --- | --- | --- |
| `ui.textarea(label=...)` | 多行文本输入框 | `label: str` | 返回 `Textarea` 对象，`.value` 持有当前多行文本 | 填写额外参数、备注、多行配置片段 |
| `ui.select(options, value=...)` | 下拉选择框 | `options: list`，默认 `value` | `.value` 持有当前选中项 | 在有限集合里选择目标环境 |
| `ui.clipboard.write(text)` | 把字符串写入系统剪贴板 | `text: str` | 同步调用，无需 `await`；浏览器剪贴板内容被替换 | "一键复制"类功能，避免用户手动选中文本 |
| `ui.notify(msg, type='negative')` | 展示一条红色的失败通知 | `msg: str` | 浏览器弹出红色通知条 | 输入校验失败时的即时反馈 |
| 纯函数 + `.value` 的组合模式 | 把校验/业务规则与 UI 组件解耦 | 从组件 `.value` 读出普通类型，传给纯函数 | 纯函数可被单独测试、未来可下沉到 `services/` | 任何"先校验、再执行"的事件回调 |
| `preview_label.text = ...` | 更新一个已创建的 `ui.label` 的展示内容 | `str` | 浏览器上对应文字被替换 | 用业务操作的结果刷新只读展示区域 |

## 8. Failure Modes —— 常见坑

1. **在事件回调里用 `_HOSTNAME_PATTERN.match()` 而不是 `.fullmatch()`。**
   - 症状：像 `cmts-01; rm -rf /` 这样的危险字符串，因为开头的
     `cmts-01` 部分匹配上了正则，`match()` 会误判为"校验通过"，而
     `fullmatch()` 要求整个字符串都符合规则才算通过，才能正确拒绝它。
   - 修复：涉及"整个字符串是否合法"这类校验，优先使用 `fullmatch()`，
     而不是 `match()` / `search()`。

2. **点击"生成预览"前没有先点清空，以为 `preview_label.text` 会在校验
   失败时自动保留上一次成功的预览内容，结果却被清空了，误以为是 bug。**
   - 症状：先成功生成一次预览，然后把主机名清空再点"生成预览"，发现
     预览区域也被清空了，怀疑代码有问题。
   - 修复：这其实是本示例刻意的行为——`handle_generate` 在校验失败分支
     里显式执行了 `preview_label.text = ''`，为的是避免用户看到一段
     "看起来通过了校验，其实对应的是上一次合法输入"的预览文本，造成误导。
     如果业务上需要"保留上一次成功结果"，需要显式把这一行去掉并在文档里
     说明清楚，而不是让行为变得模糊。

3. **直接把 `extra_textarea.value` 当成非 `None` 来用（省略了
   `or ''` 这个兜底）。**
   - 症状：某些 NiceGUI 版本或某些浏览器交互路径下，一个从未被用户输入过
     的 `ui.textarea` 的 `.value` 可能是 `None` 而不是空字符串，直接对
     `None` 调用 `.strip()` 会抛出 `AttributeError: 'NoneType' object
     has no attribute 'strip'`。
   - 修复：读取任何用户输入组件的值准备做字符串操作之前，养成
     `xxx.value or ''` 这种防御性写法的习惯，不要假设它一定是 `str`。

## 9. Exercises —— 练习

1. **基础修改**：把合法主机名的字符集合从"字母、数字、点号、连字符"扩展
   为再额外允许下划线 `_`，并相应更新 `ui.notify` 里的错误提示文案。
   验收标准：输入 `cmts_01.lab.local` 能通过校验并生成预览；
   `validate_hostname` 对应的错误提示文案也要同步更新成包含"下划线"的说明。

2. **增加实际功能**：给预览结果增加一个"生成时间"展示（例如
   "生成于 14:32:07"），每次成功生成预览时更新这个时间戳。
   提示：使用标准库 `datetime.now()` 格式化即可，不需要引入新的第三方包。
   验收标准：连续两次点击"生成预览"（间隔几秒），两次显示的时间戳不同。

3. **改善错误处理/模块设计**：把 `validate_hostname` 的返回类型从
   "`None` 或一句错误字符串"改成返回一个小的结果对象（例如
   `dataclass`，包含 `ok: bool` 和 `reason: str | None` 两个字段），并
   相应更新 `handle_generate` 里的调用方式；额外写一段
   `if __name__ == '__main__':` 下的自测代码（几组 `assert`），证明新的
   返回类型在各种输入下行为仍然正确。
   验收标准：`python3 -m py_compile` 通过；直接运行
   `python3 examples/03_events_and_state.py` 并手动在浏览器里验证校验
   行为和此前完全一致；新增的自测 `assert` 能够正常跑完不报错。

## 10. Summary —— 小结

本示例的心智模型是："事件回调是‘读取当前输入 -> 调用不依赖 UI 的纯函数
做校验/业务计算 -> 用结果更新展示组件或弹出通知’这条链路的粘合层；页面级
状态存在于各个输入组件自己的 `.value` 里，生命周期绑定到这一次页面加载；
当多个页面需要共享某个状态时，才需要升级到应用级状态"。

```
input/select/textarea (.value)
          |
          v
   纯函数：validate_hostname / build_preview_command
          |
   +------+------+
   |             |
 失败           成功
   |             |
 notify        label.text = ... ; notify
(negative)       (positive)
```

这个模型直接对应后面几乎所有"表单类"示例的骨架。下一步（示例 4）会引入
第一个真正的 `services/` 层模块——配置文件校验器，到那时你会看到：当
"业务规则"不再是一两个简单的纯函数，而是需要读写文件、处理多种异常类型
时，继续把它们堆在 `examples/*.py` 里就会让单个文件迅速膨胀，这才是
"为什么需要专门的 Service 层"这个问题的真正答案，而不是"因为教程说要
分层"。
