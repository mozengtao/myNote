#!/usr/bin/env python3
"""
Case 5：使用模板继承生成多种配置。
- base_device.j2    基础模板（hostname + interfaces/routing 两个 block + 公共的 !/end）
- access_switch.j2  子模板：只覆盖 interfaces block
- core_router.j2    子模板：覆盖 interfaces block 和 routing block

Template 和 Data 都保存为独立文件：
  case5_templates/base_device.j2 / access_switch.j2 / core_router.j2  <- Jinja 模板
  case5_data.json                                                     <- 渲染用的数据（switch / router 两组）
"""
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(os.path.join(BASE_DIR, "case5_templates")),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

with open(os.path.join(BASE_DIR, "case5_data.json"), encoding="utf-8") as f:
    all_data = json.load(f)

switch_data = all_data["switch"]
router_data = all_data["router"]

print("### case5_access_switch_output")
print(env.get_template("access_switch.j2").render(**switch_data), end="")
print("### END case5_access_switch_output")
print()

print("### case5_core_router_output")
print(env.get_template("core_router.j2").render(**router_data), end="")
print("### END case5_core_router_output")
