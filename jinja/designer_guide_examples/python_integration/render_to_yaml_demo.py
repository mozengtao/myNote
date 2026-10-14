#!/usr/bin/env python3
"""
第 9 节 案例 3：将渲染结果写入 YAML 文件。

Template 和 Data 都保存为独立文件：
  render_to_yaml_demo.j2    <- Jinja 模板
  render_to_yaml_demo.json  <- 渲染用的数据
"""
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(BASE_DIR),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

with open(os.path.join(BASE_DIR, "render_to_yaml_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("render_to_yaml_demo.j2")
output = template.render(**data)

OUT_PATH = os.path.join(BASE_DIR, "generated_device.yaml")
with open(OUT_PATH, "w", encoding="utf-8") as f:
    f.write(output)

print("### render_to_yaml_written_content")
with open(OUT_PATH, "r", encoding="utf-8") as f:
    print(f.read(), end="")
print("### END render_to_yaml_written_content")
