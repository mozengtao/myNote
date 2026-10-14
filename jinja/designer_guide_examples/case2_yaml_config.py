#!/usr/bin/env python3
"""
Case 2：生成 YAML 配置。
- 遍历服务列表
- 处理可选配置
- 控制空列表与缺失字段
- 正确控制缩进与换行

Template 和 Data 都保存为独立文件：
  case2_yaml_config.j2    <- Jinja 模板
  case2_yaml_config.json  <- 渲染用的数据（包含 with_services / no_services 两组）
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

with open(os.path.join(BASE_DIR, "case2_yaml_config.json"), encoding="utf-8") as f:
    all_data = json.load(f)

data_with_services = all_data["with_services"]
data_no_services = all_data["no_services"]

template = env.get_template("case2_yaml_config.j2")

print("### case2_with_services_output")
print(template.render(**data_with_services), end="")
print("### END case2_with_services_output")
print()

print("### case2_empty_services_output")
print(template.render(**data_no_services), end="")
print("### END case2_empty_services_output")
print()

# 验证输出是真正合法的 YAML
import yaml

parsed = yaml.safe_load(template.render(**data_with_services))
print("### case2_yaml_parse_check")
print(parsed)
print("### END case2_yaml_parse_check")
