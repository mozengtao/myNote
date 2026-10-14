"""`nicegui_learning_lab` 的主入口：把 `ui/*.py` 四个面板组装成一个多页面应用。

与 `examples/01-09` 的关系
--------------------------
`examples/*.py` 是教学用的独立示例，各自单独运行、专注讲清一个心智模型；
本文件则是"整合阶段"的产出（见 `docs/multitask_plan.md`）：把其中会被复用
的 Service 调用组装成一个真实可用的小工具，复用与各示例完全相同的
`models/`、`services/`、`infrastructure/` 代码，不重新实现任何业务逻辑或
安全校验。详见 `ui/__init__.py` 与 `docs/architecture.md`。

运行方式
--------
    python3 -m pip install -r requirements.txt
    python3 app.py
然后浏览器访问 http://localhost:8080

注意：与 `examples/*.py` 不同，本文件位于项目根目录，Python 会自动把
"脚本自身所在目录"（也就是项目根目录本身）加入 `sys.path`，所以
`from ui...`/`from services...` 这类导入不需要像 `examples/*.py` 那样
额外插入 `sys.path`（它们位于 `examples/` 子目录下，脚本自身所在目录是
`examples/`，不包含项目根目录，这正是 `docs/multitask_plan.md`"集成阶段
修正记录"一节里记录的那个问题的根因）。
"""

from __future__ import annotations

import logging

from nicegui import ui

from ui.dashboard import build_dashboard_page
from ui.log_panel import install_log_buffer_handler

# 日志约定（见冻结契约）：标准库 logging，统一格式，在 app.py 里配置一次。
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(name)s: %(message)s',
)
logger = logging.getLogger(__name__)

# 把"运行日志"面板的缓冲区 handler 挂载到根 logger 上——只在这里调用一次
# （见 ui/log_panel.py 的 install_log_buffer_handler() 文档说明：重复调用
# 会导致日志重复，app.py 作为唯一入口天然保证了这一点）。
install_log_buffer_handler()


@ui.page('/')
def index_page() -> None:
    """应用主页面：直接委托给 `ui.dashboard.build_dashboard_page()`。"""
    build_dashboard_page()


if __name__ in {'__main__', '__mp_main__'}:
    logger.info('app.py 启动中…')
    ui.run(title='NiceGUI 学习实验室')
