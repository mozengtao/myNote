"""Service 层：配置、命令执行与测试运行相关的业务规则。

本包强制遵守的硬性约定（代码评审保证）：services/ 下任何模块都不允许
`import nicegui`，也不允许导入 `ui/`、`examples/` 中的任何内容。Service
只能导入 `models/`、`infrastructure/` 以及标准库，这样即使在没有安装
nicegui 的机器上，每一个 Service 也都能用普通的 pytest 测试。
"""
