#!/usr/bin/env python3
"""
Case 6：调试缺失字段和 Undefined。
- 演示默认 Undefined 的表现
- 演示 StrictUndefined 的报错
- 演示 is defined 和 default 的处理
- 展示如何定位模板字段名拼写错误

Template 和 Data 都保存为独立文件：
  case6_templates/description_field.j2        <- {{ device.description }}（字段缺失）
  case6_templates/hostname_typo.j2             <- {{ device.hostnmae }}（故意拼错 hostname）
  case6_templates/is_defined_guard.j2          <- is defined 显式判断
  case6_templates/default_filter_fallback.j2   <- default 过滤器兜底
  case6_data.json                              <- DEVICE 数据（注意：没有 "description" 字段）
"""
import json
import os
from jinja2 import Environment, FileSystemLoader, StrictUndefined, UndefinedError

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "case6_templates")

with open(os.path.join(BASE_DIR, "case6_data.json"), encoding="utf-8") as f:
    DEVICE = json.load(f)

# 1) 默认 Undefined：缺失字段静默渲染为空字符串，容易被忽略
env_default = Environment(loader=FileSystemLoader(TEMPLATES_DIR))
tmpl = env_default.get_template("description_field.j2")
print("### case6_default_undefined_silent")
print(f"[{tmpl.render(device=DEVICE)}]")
print("### END case6_default_undefined_silent")
print()

# 2) 同样的拼写错误：把 hostname 拼成 hostnmae，默认 Undefined 依旧不报错
tmpl_typo = env_default.get_template("hostname_typo.j2")
print("### case6_default_undefined_typo_silent")
print(f"[{tmpl_typo.render(device=DEVICE)}]")
print("### END case6_default_undefined_typo_silent")
print()

# 3) StrictUndefined：访问缺失字段会立刻抛出 UndefinedError，方便在 CI 中快速定位问题
env_strict = Environment(loader=FileSystemLoader(TEMPLATES_DIR), undefined=StrictUndefined)
tmpl_strict = env_strict.get_template("description_field.j2")
print("### case6_strict_undefined_raises")
try:
    tmpl_strict.render(device=DEVICE)
except UndefinedError as exc:
    print(f"UndefinedError: {exc}")
print("### END case6_strict_undefined_raises")
print()

# 4) is defined：在模板内部显式判断，避免依赖 Undefined 的隐式行为
tmpl_is_defined = env_default.get_template("is_defined_guard.j2")
print("### case6_is_defined_guard")
print(tmpl_is_defined.render(device=DEVICE))
print("### END case6_is_defined_guard")
print()

# 5) default 过滤器：提供兜底值，而不是报错或留空
tmpl_default_filter = env_default.get_template("default_filter_fallback.j2")
print("### case6_default_filter_fallback")
print(tmpl_default_filter.render(device=DEVICE))
print("### END case6_default_filter_fallback")
print()

# 6) 适用于 CI/自动化配置生成的建议：用 StrictUndefined 跑一次"预检"渲染，
#    任何拼写错误或缺失字段都会在生成阶段直接报错，而不是生成一份带空洞的配置文件。
def render_strict_or_report(template_name, context):
    env = Environment(loader=FileSystemLoader(TEMPLATES_DIR), undefined=StrictUndefined)
    tmpl = env.get_template(template_name)
    try:
        return True, tmpl.render(**context)
    except UndefinedError as exc:
        return False, str(exc)


ok, result = render_strict_or_report("hostname_typo.j2", {"device": DEVICE})
print("### case6_ci_precheck_pattern")
print(f"ok={ok}, result={result}")
print("### END case6_ci_precheck_pattern")
