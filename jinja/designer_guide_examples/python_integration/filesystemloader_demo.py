#!/usr/bin/env python3
"""
第 9 节 案例 2：使用 FileSystemLoader 从目录加载模板。
目录结构：
  python_integration/
    filesystemloader_demo.py
    filesystemloader_demo.json   <- 渲染用的数据
    fsloader_templates/
      device.j2                 <- Jinja 模板
"""
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(os.path.join(BASE_DIR, "fsloader_templates")),
    trim_blocks=True,
    lstrip_blocks=True,
)

with open(os.path.join(BASE_DIR, "filesystemloader_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("device.j2")
output = template.render(**data)

print("### filesystemloader_output")
print(output, end="")
print("### END filesystemloader_output")
