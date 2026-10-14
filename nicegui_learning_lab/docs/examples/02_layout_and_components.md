# 示例 2：布局与常用组件 —— 开发者工具箱首页

## 1. Why —— 为什么工程中需要它

示例 1 的页面只有两行文字和一个按钮，随便摆放都不会乱。但只要页面上出现
三五个输入框、几个按钮、还要分区展示状态，"随便摆放"立刻就会变成一团
看不懂的代码。工程 GUI 小工具（命令面板、配置校验器、日志查看器）几乎
都需要这种"多区块"页面：一块放操作入口，一块放当前状态，可能还有一块放
历史记录。

本示例要解决的问题是：**在 NiceGUI 里，布局不是靠额外的"模板语言"或
"样式表"来描述的，而是靠 Python 代码本身的嵌套结构**——这对有
C/C++/Go/Rust 背景、习惯了"逻辑和界面分离"的读者来说，是一个需要专门
建立的心智模型。

## 2. Mental Model —— 心智模型

先分清两类组件：

```
+-------------------------+       +------------------------+
|     布局容器组件          |       |     展示/输入组件        |
|  row() / column() /     |       |  label() / input() /   |
|  card()                 |       |  select() / button()   |
|                         |       |                        |
|  本身也是 Python 的      |       |  不能再用 with 包裹      |
|  context manager，      |       |  其他组件；它们是         |
|  用 with 语法接纳子组件   |       |  "叶子节点"              |
+------------------------+       +------------------------+
```

再看本示例页面的实际结构（用缩进表示 `with` 嵌套层级）：

```
column()                         # 整个页面的最外层，纵向排列
  label("开发者工具箱")              # 标题
  label("...说明文字...")           # 说明
  row()                           # 左右并排的两个区块
    card("最近状态")
      label("最近状态")             # 卡片标题
      label(status_label)         # 唯一的"展示状态"组件
    card("常用操作")
      label("常用操作")             # 卡片标题
      column()                   # 卡片内部再纵向排列
        input("目标主机名")         # 输入型组件，持有 .value
        select("目标环境")          # 输入型组件，持有 .value
        row()                     # 两个按钮并排
          button("记录目标")
          button("清空状态")
```

这里最重要的一条规则是：**组件出现在页面上的位置，完全由它在代码里是
"哪一行、在哪一个 `with` 块里"创建的来决定**。NiceGUI 没有"先声明结构、
再填内容"的两阶段过程——`with ui.card():` 这一行代码执行的那一刻，一个
卡片就已经被创建并放进了当前所在的容器；接下来这个 `with` 块里的每一行，
都会把新组件放进刚创建的这张卡片里。这和写 HTML/CSS 时"先搭骨架、再填充"
的直觉不同，更接近"一边执行代码、一边往上搭积木"。

另外要分清"展示型组件"和"输入型组件"：`ui.label` 只用来**显示**信息，
它没有用户可以编辑的 `.value`；而 `ui.input` / `ui.select` 这类组件既能
显示（当前选中的值），又持有一个可以被用户修改、也可以被你的代码读取/
写入的 `.value` 属性。这个区分会贯穿本项目几乎所有的示例。

## 3. Architecture —— 架构说明

和示例 1 一样，本示例仍然只有一个文件、没有 Service 层——这个页面唯一的
"业务逻辑"是"如果主机名为空就不更新状态"，简单到不值得抽成独立模块。

```
examples/02_layout_and_components.py
    |
    +-- ENVIRONMENTS / IDLE_STATUS_TEXT   # 模块级常量
    |
    +-- index()                          # @ui.page('/')，构建整个页面
    |       |
    |       +-- handle_record()           # "记录目标"按钮的回调
    |       +-- handle_clear()            # "清空状态"按钮的回调
    |
    +-- if __name__ in {...}: ui.run(...)
```

依赖方向：只 `import nicegui`，不依赖 `models/`、`services/`。如果未来
要给"记录目标主机"加上真正的连通性检查（比如 ping 一下），那段校验/探测
逻辑应该被抽到 `services/` 里，`handle_record` 只负责"读取输入 -> 调用
Service -> 把结果写回 `status_label`"。

## 4. Complete Code —— 完整代码

完整源码见 [`examples/02_layout_and_components.py`](../../examples/02_layout_and_components.py)。
关键片段：

```python
with ui.row().classes('w-full items-start gap-4'):
    with ui.card().mark('card-recent-status'):
        ui.label('最近状态').classes('text-lg font-semibold')
        status_label = ui.label(IDLE_STATUS_TEXT).mark('status-label')

    with ui.card().mark('card-quick-actions'):
        ui.label('常用操作').classes('text-lg font-semibold')
        with ui.column().classes('gap-2'):
            host_input = ui.input(label='目标主机名').mark('host-input')
            env_select = ui.select(
                ENVIRONMENTS, value=ENVIRONMENTS[0], label='目标环境'
            ).mark('env-select')

            def handle_record() -> None:
                host = host_input.value.strip()
                env = env_select.value
                if not host:
                    status_label.text = '记录失败：主机名不能为空'
                    ui.notify('请先填写目标主机名', type='warning')
                    return
                status_label.text = f'已记录目标：{host}（环境：{env}）'
                ui.notify(f'已记录 {host} / {env}', type='positive')
```

注意 `status_label` 是在"最近状态"卡片里创建的，但 `handle_record` 是在
"常用操作"卡片内部定义的嵌套函数——Python 的闭包规则允许内层函数引用外层
函数（这里是 `index()`）作用域里的任何变量，不要求这个变量必须在闭包函数
"定义之前"的那一行就已经创建，只要求在闭包函数**真正被调用**的那一刻，
这个变量已经存在。由于用户不可能在 `index()` 整个执行完、页面渲染完成
之前就点到按钮，这里不会有问题。

## 5. Execution —— 运行方式

```bash
cd /home/morrism/myNote/nicegui_learning_lab
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 examples/02_layout_and_components.py
```

打开 `http://localhost:8080` 后预期看到：

1. 顶部是标题"开发者工具箱"和一行说明文字。
2. 下方左右并排两张卡片："最近状态"（左）和"常用操作"（右）。
3. "最近状态"卡片里只有一行文字，初始内容是"尚未执行任何操作"。
4. "常用操作"卡片里有一个主机名输入框、一个环境下拉框（默认选中 `dev`）、
   以及"记录目标"和"清空状态"两个按钮。
5. 不填主机名直接点击"记录目标"：弹出黄色警告通知，左侧状态文字变成
   "记录失败：主机名不能为空"。
6. 填入任意主机名、切换环境后点击"记录目标"：弹出绿色通知，左侧状态文字
   更新为"已记录目标：xxx（环境：yyy）"。
7. 点击"清空状态"：左侧状态文字恢复为"尚未执行任何操作"。

## 6. Lifecycle —— 一次真实操作的生命周期

以"填写主机名 `cmts-01`、选择环境 `staging` 后点击‘记录目标’"为例：

| 阶段 | 发生了什么 |
| --- | --- |
| User Action | 用户在输入框里敲入 `cmts-01`，在下拉框里选中 `staging`，然后点击"记录目标"按钮。 |
| Event Handler | 浏览器把点击事件发给服务器，NiceGUI 调用 `handle_record()`。 |
| State/Service | `handle_record` 通过 `host_input.value` / `env_select.value` 读取**此刻**输入框和下拉框的当前值（页面级状态），本示例没有 Service 层。 |
| Business Operation | 判断 `host` 是否为空字符串——这是本示例里唯一的"业务规则"。 |
| Result/Exception | 为空：走"失败"分支；非空：走"成功"分支，拼出状态文案。 |
| UI Feedback | 失败分支：`status_label.text` 更新为失败提示 + `ui.notify(type='warning')`；成功分支：`status_label.text` 更新为成功文案 + `ui.notify(type='positive')`。 |

## 7. API Reference —— API 速查表

| API/Concept | Purpose | Input | Output/Effect | Common Scenario |
| --- | --- | --- | --- | --- |
| `ui.row()` | 水平排列子组件的布局容器 | 无必填参数 | `with` 块内创建的组件从左到右排列 | 让多张卡片或多个控件并排展示 |
| `ui.column()` | 垂直排列子组件的布局容器 | 无必填参数 | `with` 块内创建的组件从上到下排列 | 在卡片内部把输入框、下拉框纵向堆叠 |
| `ui.card()` | 带边框/阴影的分组容器 | 无必填参数 | 视觉上形成一个独立区块 | 把页面拆成"常用操作""最近状态"这类逻辑分区 |
| `ui.input(label=...)` | 单行文本输入框 | `label: str` | 返回 `Input` 对象，`.value` 持有当前文本 | 让用户填写主机名、关键字等短文本 |
| `ui.select(options, value=...)` | 下拉选择框 | `options: list`，`value` 为默认选中项 | 返回 `Select` 对象，`.value` 持有当前选中项 | 让用户从有限集合里选一个（如目标环境） |
| `ui.label(text)` | 纯展示文字 | `text: str` | 之后可通过 `.text = 新内容` 更新显示 | 展示状态、标题、说明文字 |
| `ui.button(text, on_click=...)` | 可点击按钮 | `text: str`，`on_click: Callable` | 点击时调用回调 | 触发"记录""清空"这类一次性操作 |

## 8. Failure Modes —— 常见坑

1. **忘记在 `with ui.row():` 下面缩进，导致组件实际上创建在了外层容器里。**
   - 症状：代码能跑，页面也能打开，但卡片没有像预期那样"并排"显示，而是
     跟外层的标题、说明文字挤在了同一纵列里，布局完全不对。
   - 修复：检查缩进——NiceGUI 用 Python 的缩进/`with` 块来表达嵌套关系，
     缩进错一级，组件所属的容器就会跟着错一级，这一点编辑器的自动缩进
     偶尔会"帮倒忙"，务必肉眼确认。

2. **在事件回调里直接写 `host = host_input`（忘记 `.value`），或者把
   `.value` 在回调定义时就取出来存成一个普通变量。**
   - 症状一：`host` 拿到的是整个 `Input` 组件对象，而不是字符串，后续
     `host.strip()` 会抛 `AttributeError`。
   - 症状二：如果在 `index()` 顶层写
     `host = host_input.value`（而不是在 `handle_record` 内部读取），
     无论用户后来怎么修改输入框，`host` 永远是页面刚打开那一刻的空字符串，
     点击按钮永远走"失败"分支——这是"没有在事件发生的那一刻去读取当前值"
     的典型坑。
   - 修复：永远在事件回调函数**内部**、需要用到这个值的那一行，去读取
     `xxx_input.value`，不要提前缓存。

3. **两张卡片之间没有设置间距，挤在一起分不清边界，于是尝试手写大量
   行内样式字符串，代码越写越乱。**
   - 症状：`ui.row()` 默认的子元素间距可能不够明显，初学者容易一上来就
     堆砌零散的 CSS 字符串，导致后续维护困难。
   - 修复：优先使用 `.classes('gap-4')` 这种 Tailwind 间距工具类统一控制
     间距，而不是给每个组件单独写 margin；如果布局依然复杂，考虑拆分成
     更小的辅助函数而不是堆更多行内样式。

## 9. Exercises —— 练习

1. **基础修改**：给"最近状态"卡片再加一行 `ui.label`，固定显示当前选中的
   目标环境（即使用户还没点击"记录目标"，也要随下拉框的选择实时变化）。
   提示：需要给 `ui.select` 传入 `on_change=...` 参数。
   验收标准：切换下拉框选项后，这行新加的文字立刻更新，无需点击任何按钮。

2. **增加实际功能**：新增第三张卡片"历史记录"，每次成功点击"记录目标"后，
   把这次的"主机名 + 环境"追加到一个 `ui.column()` 里，以一条新的
   `ui.label` 形式显示出来（不需要持久化，刷新页面丢失即可）。
   验收标准：连续点击三次"记录目标"（使用不同的主机名），"历史记录"卡片
   里应该能看到三条对应的记录，按时间顺序从上到下排列。

3. **改善错误处理/模块设计**：把"主机名不能为空"这条校验规则，改写成一个
   独立的、不依赖 nicegui 的纯函数（参考示例 3 的做法），`handle_record`
   只负责调用它并根据返回结果更新 UI。
   验收标准：新增的校验函数可以被单独 `import` 并用 `assert` 测试，
   `handle_record` 内部不再直接写 `if not host:` 这样的裸判断。

## 10. Summary —— 小结

本示例的心智模型是："`row()` / `column()` / `card()` 是可以嵌套的布局
容器，组件在代码里被创建的位置，就是它在页面上出现的位置；`label` 是
纯展示组件，`input` / `select` 是既能展示又能被用户修改、且持有 `.value`
的输入组件"。

```
row (水平)
 |-- card (最近状态) -> label (展示型)
 |-- card (常用操作) -> column (垂直)
                          |-- input  (输入型)
                          |-- select (输入型)
                          |-- row -> button, button
```

这个"布局即代码结构"的模型会在后面几乎每个带界面的示例里复用。示例 3 会
在这个基础上加入真正的输入校验逻辑，并且开始讨论"页面级状态"与"应用级
状态"的边界；当某个页面的区块多到让单个文件难以维护时（比如示例 6 的
文件查看面板），我们才会考虑把某个区块拆成独立的函数甚至独立的模块。
