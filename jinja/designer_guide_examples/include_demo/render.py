#!/usr/bin/env python3
"""
include_demo/render.py
验证 jinja_template_designer_guide.md 第 8.1 节 (Include) 的完整案例。
目录结构：
  include_demo/
    render.py
    templates/
      main.j2       <- 主模板，内部 {% include "banner.j2" %}
      banner.j2     <- 被复用的公共片段（会自动继承 main.j2 的 context，例如 hostname）
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

data = {
    "hostname": "router-01",
    "interfaces": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
    ],
}

template = env.get_template("main.j2")
output = template.render(**data)

print("### include_demo_output")
print(output, end="")
print("### END include_demo_output")
