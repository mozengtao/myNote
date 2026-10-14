#!/usr/bin/env python3
"""
whitespace_demo.py
验证 jinja_template_designer_guide.md 第 7 节 (Whitespace Control) 的所有对比示例。
同一份模板/数据，在不同 Environment 配置下渲染，并用 repr() 精确展示换行与空格。
"""
from jinja2 import Environment

DATA = {
    "interfaces": [
        {"name": "eth0"},
        {"name": "eth1"},
        {"name": "eth2"},
    ]
}

PLAIN_TEMPLATE = """interfaces:
{% for interface in interfaces %}
  - name: {{ interface.name }}
{% endfor %}
"""

WS_CONTROL_TEMPLATE = """interfaces:
{%- for interface in interfaces %}
  - name: {{ interface.name }}
{%- endfor %}
"""


def dump(name, text):
    print(f"### {name} (raw render)")
    print(text, end="")
    print(f"### END {name} (raw render)")
    print(f"### {name} (repr)")
    print(repr(text))
    print(f"### END {name} (repr)")
    print()


# 1) 默认 Environment（不开启 trim_blocks / lstrip_blocks），不带空白控制符
env_default = Environment()
tmpl = env_default.from_string(PLAIN_TEMPLATE)
dump("plain_loop_default_env", tmpl.render(**DATA))

# 2) 默认 Environment，使用 {%- -%} 显式空白控制
tmpl = env_default.from_string(WS_CONTROL_TEMPLATE)
dump("ws_control_loop_default_env", tmpl.render(**DATA))

# 3) trim_blocks=True, lstrip_blocks=True 的 Environment，渲染“未加控制符”的普通模板
env_trim_lstrip = Environment(trim_blocks=True, lstrip_blocks=True)
tmpl = env_trim_lstrip.from_string(PLAIN_TEMPLATE)
dump("plain_loop_trim_lstrip_env", tmpl.render(**DATA))

# 4) keep_trailing_newline 对比：默认 Environment 会丢弃模板源文件末尾的最后一个换行符
SIMPLE_TEMPLATE = "Hello, {{ name }}!\n"

env_default2 = Environment()  # keep_trailing_newline=False (默认)
tmpl = env_default2.from_string(SIMPLE_TEMPLATE)
dump("keep_trailing_newline_false", tmpl.render(name="Morris"))

env_keep_newline = Environment(keep_trailing_newline=True)
tmpl = env_keep_newline.from_string(SIMPLE_TEMPLATE)
dump("keep_trailing_newline_true", tmpl.render(name="Morris"))

# 5) {{- -}} 控制表达式周围空白
EXPR_TEMPLATE = "A\n{{- ' B' -}}\nC\n"
tmpl = env_default.from_string(EXPR_TEMPLATE)
dump("expr_whitespace_control", tmpl.render())
