# 示例 1：Hello NiceGUI —— 最小可运行应用

## 1. Why —— 为什么工程中需要它

在动手做任何真正有用的 GUI 小工具（命令面板、配置校验器、日志查看器）之前，
必须先搞清楚一件更基础的事：**一个 NiceGUI 应用最少要包含哪几个部分才能跑
起来**。如果跳过这一步，直接去抄一个复杂示例，遇到问题时根本分不清是
"布局写错了"还是"事件没绑定对"还是"服务器根本没启动"。

本示例把复杂度压到最低：一段文字、一个按钮、一次点击反馈。它要建立的是后面
所有示例都会用到的三个概念：**页面定义**、**组件创建**、**事件回调**，以及
一个对 C/C++/Go/Rust 背景的读者尤其重要的问题——**哪些代码跑在服务器端，
哪些东西只存在于浏览器里**。

## 2. Mental Model —— 心智模型

先看三个角色之间的关系：

```
+------------------+        HTTP 请求 / WebSocket         +------------------+
|   浏览器 (客户端)  | <---------------------------------> |  NiceGUI 服务器   |
|                  |                                     |  (运行你的 .py)   |
|  显示 HTML/CSS/JS |                                     |  index() 函数     |
|  触发用户事件      |  -- 点击事件通过 WebSocket 发回 -->   |  handle_click()   |
+------------------+                                      +------------------+
```

再看一次"点击按钮"背后，代码是如何被组织和调用的：

```
ui.run(title=...)                 # 启动 Web 服务器，开始监听浏览器请求
   |
   v
浏览器访问 http://localhost:8080/
   |
   v
NiceGUI 发现 '/' 对应 index() 函数 --> 调用 index()
   |
   v
index() 函数体从上到下执行一次：
   ui.label(...)   --> 生成一段文字对应的 HTML
   ui.label(...)   --> 生成第二段文字
   ui.button(...)  --> 生成一个按钮，并把 handle_click 注册为它的点击回调
   |
   v
浏览器收到这次渲染的结果，显示出页面
   |
   v
用户点击按钮
   |
   v
浏览器通过 WebSocket 通知服务器"按钮被点击了"
   |
   v
服务器执行 handle_click()  --> 调用 ui.notify(...)
   |
   v
服务器把"请显示一条通知"的指令发回浏览器，浏览器弹出通知
```

核心抽象只有三个：

- **页面（Page）**：一个被 `@ui.page(path)` 装饰的自由函数，`path` 对应浏览器
  访问的 URL；函数体内创建的所有组件，共同构成这次请求返回的页面内容。
- **组件（Component / Element）**：`ui.label(...)`、`ui.button(...)` 这类
  调用，每一次调用都会创建一个 UI 元素，并返回这个元素的 Python 对象，供你
  之后继续操作（比如调用 `.mark(...)` 打标记）。
- **事件回调（Event Handler）**：一个普通的 Python 函数（或 lambda），通过
  `on_click=...` 这样的参数注册给某个组件；它只在对应事件真正发生时才会被
  服务器调用，不是在页面渲染时就执行。

## 3. Architecture —— 架构说明

本示例只有一个文件 `examples/01_hello_nicegui.py`，**没有任何 Service 层**，
这是诚实的架构描述，而不是偷懒：这个示例压根没有值得抽取的业务逻辑（它唯一
的"业务动作"就是弹一条通知），把它硬拆成 `ui/` + `services/` 两层反而会让
读者分不清"为什么要分层"这个问题的真正答案。

```
examples/01_hello_nicegui.py
    |
    +-- index()           # @ui.page('/')，定义页面结构
    |       |
    |       +-- handle_click()   # 嵌套函数，作为按钮的事件回调
    |
    +-- if __name__ in {...}: ui.run(...)   # 启动入口
```

依赖方向：本文件只 `import nicegui`，不依赖 `models/`、`services/` 中的任何
模块。从示例 4 开始，当示例里出现"真正的业务规则"（例如配置文件格式校验）
时，才会引入 `services/` 层，并遵循"UI 只管读取输入、调用 Service、更新
组件"这条分工。

## 4. Complete Code —— 完整代码

完整源码见 [`examples/01_hello_nicegui.py`](../../examples/01_hello_nicegui.py)。
这里贴出最核心的片段辅助讲解：

```python
@ui.page('/')
def index() -> None:
    ui.label('Hello, NiceGUI!').classes('text-2xl font-bold').mark('hello-title')
    ui.label('这是本项目的第一个示例……').mark('hello-description')

    def handle_click() -> None:
        ui.notify('你好，这是来自服务器端的问候！', type='positive')

    ui.button('打个招呼', on_click=handle_click).mark('hello-button')


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 1：Hello NiceGUI')
```

几个细节值得注意：

- `.classes('text-2xl font-bold')` 是 NiceGUI 组件通用的链式调用，用来附加
  Tailwind CSS 类名；它不是本示例的重点，不需要深究，知道"可以链式调用来
  继续配置这个组件"就够了。
- `.mark('hello-button')` 给组件打上一个标记，方便协调者在集成阶段做
  "无浏览器冒烟测试"（检查页面渲染出的 HTML 里是否包含这个标记）；本示例
  不要求你自己写这类测试。
- `if __name__ in {'__main__', '__mp_main__'}:` 这个写法比单纯判断
  `== '__main__'` 更宽松，是因为 NiceGUI 在某些场景下会用 `multiprocessing`
  重新加载这个脚本，子进程里 `__name__` 会是 `'__mp_main__'`；两种情况都要
  调用 `ui.run()`，否则重载后子进程什么都不会显示。

## 5. Execution —— 运行方式

```bash
cd /home/morrism/myNote/nicegui_learning_lab
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 examples/01_hello_nicegui.py
```

启动后终端会打印出类似 `NiceGUI ready to go on http://localhost:8080` 的
日志。用浏览器打开 `http://localhost:8080`，预期现象：

1. 页面顶部显示加粗的标题 "Hello, NiceGUI!"。
2. 标题下方是一段说明文字。
3. 点击"打个招呼"按钮后，页面右上角（或下方，取决于 NiceGUI 版本的默认位置）
   弹出一条绿色背景的通知，内容是"你好，这是来自服务器端的问候！"。
4. 反复点击按钮，每次都会弹出一条新的通知——证明 `handle_click()` 确实是
   每次点击都重新执行一次，而不是只在页面首次加载时执行一次。

## 6. Lifecycle —— 一次真实操作的生命周期

以"用户点击‘打个招呼’按钮"为例：

| 阶段 | 发生了什么 |
| --- | --- |
| User Action | 用户在浏览器里点击"打个招呼"按钮。 |
| Event Handler | 浏览器通过已建立的 WebSocket 连接，把这次点击事件发送给服务器；NiceGUI 在服务器端找到这个按钮注册的回调 `handle_click`，调用它。 |
| State/Service | 本示例没有需要读取的输入状态，也没有 Service 层——`handle_click` 直接进入下一步。 |
| Business Operation | 唯一的"业务动作"就是构造一句问候文案（这里是写死的字符串）。 |
| Result/Exception | 没有需要处理的异常路径；本示例总是"成功"。 |
| UI Feedback | 调用 `ui.notify(msg, type='positive')`，NiceGUI 把"显示一条通知"的指令通过 WebSocket 发回浏览器，浏览器渲染出通知组件。 |

## 7. API Reference —— API 速查表

| API/Concept | Purpose | Input | Output/Effect | Common Scenario |
| --- | --- | --- | --- | --- |
| `@ui.page(path)` | 把一个自由函数注册为某个 URL 路径对应的页面 | `path: str`（如 `'/'`） | 被装饰的函数在每次该路径被访问时调用一次 | 定义应用的首页或任意子页面 |
| `ui.label(text)` | 创建一段纯展示文字 | `text: str` | 返回一个 `Label` 对象，可继续 `.classes()` / `.mark()` | 标题、说明文字、只读状态展示 |
| `ui.button(text, on_click=...)` | 创建一个可点击按钮 | `text: str`，`on_click: Callable` | 返回 `Button` 对象；点击时调用 `on_click` | 触发任意一次性操作 |
| `ui.notify(msg, type=...)` | 弹出一条短暂的通知 | `msg: str`，`type: 'positive'｜'negative'｜'warning'` 等 | 浏览器上弹出一条带颜色的通知条 | 给用户即时反馈（成功/失败/警告） |
| `.mark(id)` | 给元素打一个测试标记 | `id: str` | 渲染出的 HTML 会带上这个标记，便于断言 | 无浏览器冒烟测试、集成测试 |
| `ui.run(title=...)` | 启动 NiceGUI 内置的 Web 服务器 | `title: str` 等可选参数 | 阻塞运行，开始监听浏览器请求 | 每个可独立运行的 NiceGUI 脚本的入口 |

## 8. Failure Modes —— 常见坑

1. **忘记写 `if __name__ in {'__main__', '__mp_main__'}:` 这层判断，直接
   在模块顶层调用 `ui.run()`。**
   - 症状：这个文件一旦被别的模块 `import`（比如未来写测试时），就会立刻
     尝试启动一个 Web 服务器，导致测试进程卡住或报端口占用错误。
   - 修复：始终把 `ui.run()` 放在 `if __name__ in {'__main__', '__mp_main__'}:`
     块里，让"定义页面"和"启动服务器"这两件事分开。

2. **把 `ui.notify(...)` 写在了 `index()` 函数体里、按钮创建之前，期望它在
   页面加载时就弹一次通知，调试按钮点击时却发现通知"消失"得很快，误以为
   是按钮没绑定成功。**
   - 症状：页面一打开通知就弹出来并很快消失，点击按钮时却感觉"什么都没
     发生"（其实是又弹了一条，只是读者没注意到两条通知内容相同）。
   - 修复：把需要在"点击时"触发的逻辑放进 `handle_click` 这个嵌套函数里，
     而不是直接写在 `index()` 的顶层——`index()` 顶层的代码只在页面渲染时
     执行一次，和"点击"这个事件没有关系。

3. **忘记先执行 `pip install -r requirements.txt` 就直接运行脚本。**
   - 症状：报错 `ModuleNotFoundError: No module named 'nicegui'`。
   - 修复：先创建/激活虚拟环境，再 `pip install -r requirements.txt`
     （见第 5 节）。这个错误本身是无害的，但初学者经常会误以为是自己的
     代码写错了。

## 9. Exercises —— 练习

1. **基础修改**：把按钮文案改成"再见"，点击后用 `ui.notify(..., type='warning')`
   弹出一条黄色的告别语。
   验收标准：运行脚本后，页面上的按钮文字确实变成"再见"，点击后弹出的通知
   背景色是黄色而不是绿色。

2. **增加实际功能**：在说明文字和按钮之间加一个 `ui.label`，初始内容是
   "已点击 0 次"；每次点击按钮时，这段文字里的数字加一。
   提示：你需要在 `index()` 函数里用一个闭包变量（例如一个单元素列表或
   `nonlocal` 计数器）记录点击次数。
   验收标准：第一次点击后文字变为"已点击 1 次"，第二次点击变为
   "已点击 2 次"，依此类推；刷新浏览器页面后计数器归零（因为 `index()`
   会重新执行）。

3. **改善错误处理/模块设计**：把问候语的文案抽成一个模块级常量
   `GREETING_MESSAGE`，并新增一个"安静模式"开关：加一个
   `ui.checkbox`（可以先查官方文档确认其用法，别凭记忆猜测参数），勾选后
   点击按钮不再弹出 `ui.notify`，而是只更新页面上的一个状态文字。
   验收标准：勾选"安静模式"后点击按钮，页面不再弹出通知，但状态文字确实
   发生了变化；取消勾选后恢复原有的通知行为。

## 10. Summary —— 小结

本示例建立的心智模型是："一个 NiceGUI 页面就是一个被 `@ui.page` 装饰的函数，
函数体按顺序创建组件，组件可以通过参数（如 `on_click`）挂上事件回调，回调
只在事件真正发生时才由服务器执行"。

```
@ui.page('/') 函数
   |-- 创建展示型组件（label）
   |-- 创建交互型组件（button），挂上事件回调
          |-- 回调函数：业务逻辑 + UI 反馈（notify）
```

这个模型几乎是后面所有示例的地基：示例 2 会在这个地基上加入更复杂的布局，
示例 3 会加入真正的输入校验，而当某个示例的"业务逻辑"开始变得值得独立测试
时（例如示例 4 的配置文件校验），我们才会引入 `services/` 层——那时你会
看到事件回调从"自己做所有事"变成"读取输入 -> 调用 Service -> 更新 UI"
这种更清晰的三段式结构。
