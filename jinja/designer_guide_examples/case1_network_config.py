#!/usr/bin/env python3
"""
Case 1：生成网络设备配置。
- 生成设备名称
- 遍历接口列表
- 根据 enabled 决定是否输出接口
- 使用默认值处理可选字段 (mtu)
- 生成格式正确的文本配置

Template 和 Data 都保存为独立文件：
  case1_network_config.j2    <- Jinja 模板
  case1_network_config.json  <- 渲染用的数据
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

with open(os.path.join(BASE_DIR, "case1_network_config.json"), encoding="utf-8") as f:
    config = json.load(f)

template = env.get_template("case1_network_config.j2")
output = template.render(**config)

print("### case1_output")
print(output, end="")
print("### END case1_output")
