#!/usr/bin/env python3
"""
inheritance_demo/render.py
验证 jinja_template_designer_guide.md 第 8.2 节 (Template Inheritance) 的完整案例。
目录结构：
  inheritance_demo/
    render.py
    templates/
      base.j2                   <- 基础模板，定义 interfaces / services 两个 block
      child_access_switch.j2    <- 覆盖 interfaces block
      child_core_router.j2      <- 覆盖 interfaces block，并用 super() 扩展 services block
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

switch_data = {
    "hostname": "access-sw-01",
    "interfaces": ["eth0", "eth1"],
}
router_data = {
    "hostname": "core-rt-01",
    "interfaces": ["eth0", "eth1", "eth2"],
}

print("### child_access_switch_output")
print(env.get_template("child_access_switch.j2").render(**switch_data), end="")
print("### END child_access_switch_output")
print()

print("### child_core_router_output")
print(env.get_template("child_core_router.j2").render(**router_data), end="")
print("### END child_core_router_output")
