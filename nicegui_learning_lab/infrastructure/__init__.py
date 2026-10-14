"""Infrastructure 层：唯一被允许直接与操作系统打交道的包。

这里的模块负责封装 subprocess 执行（在真实部署中，也包括 SSH 等外部
系统）。它们不允许导入 services/、ui/、examples/ 或 nicegui——只依赖
标准库，这样"与操作系统的边界"才可以被单独、独立地测试。
"""
