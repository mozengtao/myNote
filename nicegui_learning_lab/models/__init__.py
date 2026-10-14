"""Model 层：services/ 与 ui/ 共用的纯数据结构。

本包下任何模块都不允许 import nicegui，也不允许 import services/、ui/、
examples/ 或 infrastructure/ 中的任何内容。Model 只描述"事物是什么"
（例如一个任务及其状态），从不描述"如何做某件事"。
"""
