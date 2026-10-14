"""示例 2：布局与常用组件 —— 一个"开发者工具箱"首页。

为什么需要这个示例
------------------
示例 1 只有一个孤零零的按钮，但真实的工程 GUI 小工具几乎总是需要把多个
输入项、按钮和状态展示分区摆放，否则页面很快会变成一团乱麻。本示例用
`ui.row()` / `ui.column()` / `ui.card()` 搭出一个"开发者工具箱"首页，
演示如何把页面拆成几个逻辑区块，以及"布局容器"与"展示/输入组件"之间
的分工。

心智模型
--------
NiceGUI 里没有独立的"布局文件"或"模板语言"：`ui.row()`、`ui.column()`、
`ui.card()` 本身既是容器，又是可以用 `with` 语法进入的上下文管理器。
一个组件最终出现在页面的什么位置，只取决于它是在哪一个 `with` 块里、
用哪一行代码创建的——这和声明式的 HTML/CSS 布局思路不同，更接近"按代码
执行顺序逐步搭积木"。

    row()                      —— 水平排列子组件（左右并排）
      card("最近状态")          —— 一个带阴影边框的分组容器
        label(status)          —— 纯展示型组件，不接受用户输入
      card("常用操作")
        column()               —— 垂直排列子组件（上下堆叠）
          input(host)          —— 输入型组件，持有 .value
          select(env)          —— 输入型组件，持有 .value
          row()
            button(record)     —— 触发事件回调
            button(clear)

本文件不依赖 models/、services/ 中的任何模块。
"""

from __future__ import annotations

from nicegui import ui

#: 目标环境下拉框的可选项；这是一个教学用的简化列表，真实项目里这类
#: "有限选项集合"通常会来自配置文件或某个 Service，而不是硬编码在 UI 里。
ENVIRONMENTS = ['dev', 'staging', 'prod']

#: 状态标签在未执行任何操作时的占位文案，记录在这里避免魔法字符串重复出现。
IDLE_STATUS_TEXT = '尚未执行任何操作'


@ui.page('/')
def index() -> None:
    """构建"开发者工具箱"首页上的全部 UI 元素。"""
    with ui.column().classes('w-full gap-4 p-4'):
        ui.label('开发者工具箱').classes('text-2xl font-bold').mark('toolbox-title')
        ui.label(
            '这是一个用 row / column / card 组织出来的多区块页面示例，'
            '左侧展示状态，右侧放置常用操作。'
        ).mark('toolbox-description')

        with ui.row().classes('w-full items-start gap-4'):
            # 区块一："最近状态"——只做展示，不包含任何输入型组件。
            with ui.card().mark('card-recent-status'):
                ui.label('最近状态').classes('text-lg font-semibold')
                status_label = ui.label(IDLE_STATUS_TEXT).mark('status-label')

            # 区块二："常用操作"——包含输入型组件（input/select）和按钮。
            with ui.card().mark('card-quick-actions'):
                ui.label('常用操作').classes('text-lg font-semibold')
                with ui.column().classes('gap-2'):
                    host_input = ui.input(label='目标主机名').mark('host-input')
                    env_select = ui.select(
                        ENVIRONMENTS, value=ENVIRONMENTS[0], label='目标环境'
                    ).mark('env-select')

                    def handle_record() -> None:
                        """读取当前输入框/下拉框的值，更新左侧的状态标签。

                        注意：这里必须通过 `host_input.value` /
                        `env_select.value` 读取"当前"的值，而不能在事件
                        回调定义时就把值提前存成普通变量——因为回调函数
                        只会在用户点击的那一刻才真正执行，这时候用户可能
                        已经改过输入框的内容了。
                        """
                        host = host_input.value.strip()
                        env = env_select.value
                        if not host:
                            status_label.text = '记录失败：主机名不能为空'
                            ui.notify('请先填写目标主机名', type='warning')
                            return
                        status_label.text = f'已记录目标：{host}（环境：{env}）'
                        ui.notify(f'已记录 {host} / {env}', type='positive')

                    def handle_clear() -> None:
                        """把状态标签恢复成初始占位文案。"""
                        status_label.text = IDLE_STATUS_TEXT

                    with ui.row().classes('gap-2'):
                        ui.button('记录目标', on_click=handle_record).mark(
                            'record-button'
                        )
                        ui.button('清空状态', on_click=handle_clear).mark(
                            'clear-button'
                        )


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 2：布局与常用组件')
