#!/usr/bin/env python3
"""
macro_demo/render.py
验证 jinja_template_designer_guide.md 第 8.3 节 (Macro) 的完整案例。
目录结构：
  macro_demo/
    render.py
    templates/
      macros.j2                     <- 定义 banner()/interface_line()/interface_line_with_default()/hostname_line()
      use_import.j2                  <- {% import ... as m %}，不共享调用方 context
      use_from_import.j2             <- {% from ... import name1, name2 %}
      use_import_with_context.j2     <- {% import ... as m with context %}，显式共享 context
"""
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(os.path.join(BASE_DIR, "templates")),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

data = {"hostname": "router-01"}

print("### use_import_output")
print(env.get_template("use_import.j2").render(**data), end="")
print("### END use_import_output")
print()

print("### use_from_import_output")
print(env.get_template("use_from_import.j2").render(**data), end="")
print("### END use_from_import_output")
print()

print("### use_import_with_context_output")
print(env.get_template("use_import_with_context.j2").render(**data), end="")
print("### END use_import_with_context_output")
