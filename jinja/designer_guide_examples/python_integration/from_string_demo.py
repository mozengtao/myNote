#!/usr/bin/env python3
"""
第 9 节 案例 1：使用 from_string() 渲染字符串模板。

Template 和 Data 都保存为独立文件：
  from_string_demo.j2    <- 模板文本（读成字符串后传给 from_string()）
  from_string_demo.json  <- 渲染用的数据

注意：from_string() 本身接受的是任意来源的字符串（可以来自文件、数据库、网络），
这里把模板文本放进 .j2 文件只是为了方便维护，并不等同于用 FileSystemLoader 按名字加载模板
（FileSystemLoader 用法见 filesystemloader_demo.py）。
"""
import json
import os
from jinja2 import Environment, StrictUndefined

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

env = Environment(
    undefined=StrictUndefined,
    autoescape=False,
)

with open(os.path.join(BASE_DIR, "from_string_demo.j2"), encoding="utf-8") as f:
    template_source = f.read()

with open(os.path.join(BASE_DIR, "from_string_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.from_string(template_source)
output = template.render(**data)

print("### from_string_output")
print(output)
print("### END from_string_output")
