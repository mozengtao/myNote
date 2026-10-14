#!/usr/bin/env python3
"""
第 9 节 案例 5：对 HTML 输出正确配置自动转义。

Template 和 Data 都保存为独立文件：
  autoescape_html_demo_templates/comment.j2        <- <p>Comment: {{ comment }}</p>
  autoescape_html_demo_templates/hostname_text.j2  <- hostname={{ hostname }}
  autoescape_html_demo.json                         <- 渲染用的数据（comment 模拟用户输入, hostname 用于纯文本场景）
"""
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "autoescape_html_demo_templates")

with open(os.path.join(BASE_DIR, "autoescape_html_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

# 正确做法：渲染 HTML 片段时开启 autoescape=True（或用 select_autoescape 按文件扩展名自动判断），
# 防止用户输入被当成 HTML/JS 注入执行
env_html = Environment(loader=FileSystemLoader(TEMPLATES_DIR), autoescape=True)
html_template = env_html.get_template("comment.j2")

print("### autoescape_true_html_output")
print(html_template.render(comment=data["comment"]))
print("### END autoescape_true_html_output")
print()

# 对比：autoescape=False 时，用户输入会被原样注入，存在 XSS 风险
env_html_unsafe = Environment(loader=FileSystemLoader(TEMPLATES_DIR), autoescape=False)
html_template_unsafe = env_html_unsafe.get_template("comment.j2")

print("### autoescape_false_html_output_unsafe")
print(html_template_unsafe.render(comment=data["comment"]))
print("### END autoescape_false_html_output_unsafe")
print()

# 纯文本/配置文件场景：autoescape=False 是合理默认值（不需要转义 HTML 特殊字符）
env_text = Environment(loader=FileSystemLoader(TEMPLATES_DIR), autoescape=False)
text_template = env_text.get_template("hostname_text.j2")
print("### autoescape_false_plain_text_output")
print(text_template.render(hostname=data["hostname"]))
print("### END autoescape_false_plain_text_output")
