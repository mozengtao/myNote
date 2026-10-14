#!/usr/bin/env python3
"""
Case 3：生成 CLI 命令。
- 根据输入数据生成多条 CLI 命令
- 对可选参数进行条件输出
- 避免多余空格和空行
- 对最终命令进行可读性验证（逐行打印 + 行数统计）

Template 和 Data 都保存为独立文件：
  case3_cli_commands.j2    <- Jinja 模板
  case3_cli_commands.json  <- 渲染用的数据
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

with open(os.path.join(BASE_DIR, "case3_cli_commands.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("case3_cli_commands.j2")
output = template.render(**data)

print("### case3_output")
print(output, end="")
print("### END case3_output")
print()

lines = output.splitlines()
print("### case3_readability_check")
print(f"total_lines={len(lines)}")
for i, line in enumerate(lines, start=1):
    print(f"{i}: {line!r}")
print("### END case3_readability_check")
