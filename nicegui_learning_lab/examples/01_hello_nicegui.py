"""示例 1：最小可运行的 NiceGUI 应用 —— Hello NiceGUI。

为什么需要这个示例
------------------
在学习任何一个新的 UI 框架时，第一件事应该是搞清楚"把这个框架跑起来最少
需要写多少代码"，而不是一上来就被一堆组件和布局细节淹没。本示例只做三件
事：显示一段文字、放一个按钮、点击按钮后给出反馈——用来建立"页面定义"
"组件创建"“事件回调"这三个核心概念的最小心智模型，后面的示例都是在此基础
上不断叠加复杂度。

心智模型
--------
`index()` 这个函数本身就是"页面"：它在 `@ui.page('/')` 装饰下，每当有
浏览器访问根路径 `/` 时就会被调用一次，函数体内创建的每一个 `ui.xxx(...)`
组件都会被加入到这次页面渲染的结果里。`ui.run()` 并不是"运行一次程序"，
而是"启动一个会一直运行的 Web 服务器"：之后每一次浏览器请求都会触发
`index()` 重新执行一次，服务器端的 Python 代码和浏览器里看到的网页之间，
靠的就是 NiceGUI 在背后维护的这条 WebSocket 连接来同步状态和事件。

本文件不依赖 models/、services/ 中的任何模块——它是一个可以独立运行的
教学脚本。
"""

from __future__ import annotations

from nicegui import ui


@ui.page('/')
def index() -> None:
    """定义应用的首页：浏览器每访问一次 `/`，这个函数就会被调用一次。"""
    ui.label('Hello, NiceGUI!').classes('text-2xl font-bold').mark('hello-title')
    ui.label(
        '这是本项目的第一个示例：一个只有一个按钮的最小 NiceGUI 应用，'
        '用来说明"页面定义"与"事件回调"是如何协同工作的。'
    ).mark('hello-description')

    def handle_click() -> None:
        """按钮的事件回调：这段代码运行在服务器端，而不是浏览器里。"""
        ui.notify('你好，这是来自服务器端的问候！', type='positive')

    ui.button('打个招呼', on_click=handle_click).mark('hello-button')


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 1：Hello NiceGUI')
