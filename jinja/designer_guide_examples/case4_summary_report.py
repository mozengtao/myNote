#!/usr/bin/env python3
"""
Case 4：从数据列表生成摘要报告。
- 遍历数据
- 使用 selectattr 筛选对象
- 使用 map 和 join 格式化结果
- 使用 length 计算数量
- 生成清晰的汇总文本

Template 和 Data 都保存为独立文件：
  case4_summary_report.j2    <- Jinja 模板
  case4_summary_report.json  <- 渲染用的数据
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

with open(os.path.join(BASE_DIR, "case4_summary_report.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("case4_summary_report.j2")
output = template.render(**data)

print("### case4_output")
print(output, end="")
print("### END case4_output")
