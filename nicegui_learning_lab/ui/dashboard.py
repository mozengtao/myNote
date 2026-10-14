"""仪表盘外壳：把四个面板组装成 `app.py` 的主页面 `/`。

本模块只负责"页面骨架"（标题、说明、标签页导航、页脚链接），不包含任何
业务逻辑——每个标签页的内容都委托给对应的 `ui/*_panel.py` 模块构建。
"""

from __future__ import annotations

from nicegui import ui

from ui.command_panel import build_command_panel
from ui.config_panel import build_config_panel
from ui.log_panel import build_log_panel
from ui.task_panel import build_task_panel

#: 页脚"查看教学示例"链接：指向本项目按编号组织的 9 个独立教学示例，
#: 每一个都可以单独用 `python3 examples/0N_xxx.py` 启动。
_EXAMPLE_LINKS = [
    ('01', '最小应用', '01_hello_nicegui'),
    ('02', '布局与组件', '02_layout_and_components'),
    ('03', '事件与状态', '03_events_and_state'),
    ('04', 'JSON 配置检查器', '04_config_checker'),
    ('05', '任务表格', '05_task_table'),
    ('06', '命令控制台', '06_command_console'),
    ('07', '后台任务', '07_background_task'),
    ('08', '综合测试控制台', '08_test_console'),
    ('09', '多用户状态隔离', '09_multi_user_state'),
]


def build_dashboard_page() -> None:
    """构建主页面：标题 + 四个标签页面板 + 指向教学示例的页脚链接。"""
    ui.label('NiceGUI 学习实验室 —— 综合工具台').classes('text-2xl font-bold').mark('dashboard-title')
    ui.label(
        '本页面把"配置检查 / 命令控制台 / 任务与测试 / 运行日志"这四个面板组装到'
        '同一个应用里，全部复用与 examples/01-09 完全相同的 models/ 与 services/ 代码'
        '——这是从教学示例走向一个可以长期跑着用的小工具的例子。如果你是第一次'
        '阅读本项目，建议先按顺序看完下面链接到的 9 个教学示例，再回来看这个'
        '综合页面。'
    ).classes('text-sm text-gray-600 max-w-3xl').mark('dashboard-description')

    with ui.tabs().classes('w-full mt-4') as tabs:
        config_tab = ui.tab('配置检查').mark('dashboard-tab-config')
        command_tab = ui.tab('命令控制台').mark('dashboard-tab-command')
        task_tab = ui.tab('任务与测试').mark('dashboard-tab-task')
        log_tab = ui.tab('运行日志').mark('dashboard-tab-log')

    with ui.tab_panels(tabs, value=config_tab).classes('w-full'):
        with ui.tab_panel(config_tab):
            build_config_panel()
        with ui.tab_panel(command_tab):
            build_command_panel()
        with ui.tab_panel(task_tab):
            build_task_panel()
        with ui.tab_panel(log_tab):
            build_log_panel()

    ui.separator().classes('mt-6')
    ui.label('教学示例（每个都可以独立运行，建议按编号顺序阅读）').classes(
        'text-sm font-bold mt-4'
    ).mark('dashboard-examples-title')
    with ui.row().classes('gap-x-4 gap-y-1 flex-wrap mt-1'):
        for number, title, module_name in _EXAMPLE_LINKS:
            ui.label(f'{number}. {title}（examples/{module_name}.py）').classes(
                'text-xs text-gray-500'
            ).mark(f'dashboard-example-link-{number}')
