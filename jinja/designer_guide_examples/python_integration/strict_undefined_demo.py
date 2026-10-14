#!/usr/bin/env python3
"""
第 9 节 案例 4：使用 StrictUndefined 捕获拼写错误或缺失字段。

Template 和 Data 都保存为独立文件：
  strict_undefined_demo_templates/default_typo.j2   <- hostname=[{{ hostnmae }}]（故意拼错 hostname）
  strict_undefined_demo_templates/strict_typo.j2    <- hostname={{ hostnmae }}
  strict_undefined_demo.json                         <- 渲染用的数据（只提供了正确的 hostname）
"""
import json
import os
from jinja2 import Environment, FileSystemLoader, StrictUndefined, UndefinedError

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "strict_undefined_demo_templates")

with open(os.path.join(BASE_DIR, "strict_undefined_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

# 默认 Undefined：模板里把 hostname 拼成了 hostnmae，
# Python 端只传了正确的 hostname，但默认 Undefined 会静默把 {{ hostnmae }} 渲染成空字符串
env_default = Environment(loader=FileSystemLoader(TEMPLATES_DIR))
tmpl_default = env_default.get_template("default_typo.j2")
output_default = tmpl_default.render(**data)
print("### default_undefined_typo_renders_empty")
print(output_default)
print("### END default_undefined_typo_renders_empty")
print()

# StrictUndefined：同样的拼写错误会在渲染时抛出 UndefinedError
env_strict = Environment(loader=FileSystemLoader(TEMPLATES_DIR), undefined=StrictUndefined)
tmpl_strict = env_strict.get_template("strict_typo.j2")
print("### strict_undefined_typo_raises")
try:
    tmpl_strict.render(**data)
except UndefinedError as exc:
    print(f"UndefinedError: {exc}")
print("### END strict_undefined_typo_raises")
