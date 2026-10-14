#!/usr/bin/env python3
"""
common_errors_demo.py
验证 jinja_template_designer_guide.md 第 11 节 (Common Errors & Debugging) 的可复现示例。
"""
from jinja2 import Environment, StrictUndefined, UndefinedError, TemplateSyntaxError
import yaml

env = Environment(trim_blocks=True, lstrip_blocks=True, keep_trailing_newline=True)


def show(name, template_src, data):
    t = env.from_string(template_src)
    out = t.render(**data)
    print(f"### {name}")
    print(out)
    print(f"### END {name}")
    print()


# 1. 变量名拼写错误：hostname 拼成 hostnmae，默认 Undefined 下不报错、静默输出空
show(
    "err1_typo_variable_name",
    "hostname=[{{ hostnmae }}]",
    {"hostname": "router-01"},
)

# 2. 缺失字段和 Undefined：访问嵌套缺失字段
show(
    "err2_missing_nested_field",
    "mtu=[{{ device.mtu }}]",
    {"device": {"name": "eth0"}},
)

# 3. 把 Python 表达式语法误用到 Jinja 里（例如用 Python 的 len() 而不是 Jinja 的 length 过滤器）
print("### err3_python_syntax_confusion")
try:
    bad_tmpl = env.from_string("{{ len(items) }}")
    print(bad_tmpl.render(items=[1, 2, 3]))
except Exception as exc:
    print(f"{type(exc).__name__}: {exc}")
print("### END err3_python_syntax_confusion")
print()
print("### err3_correct_jinja_filter")
good_tmpl = env.from_string("{{ items | length }}")
print(good_tmpl.render(items=[1, 2, 3]))
print("### END err3_correct_jinja_filter")
print()

# 4. Filter 与 Test 混淆：`is length` 不存在；length 是 filter 不是 test
print("### err4_filter_test_confusion")
try:
    bad_tmpl = env.from_string("{{ items is length(3) }}")
    print(bad_tmpl.render(items=[1, 2, 3]))
except Exception as exc:
    print(f"{type(exc).__name__}: {exc}")
print("### END err4_filter_test_confusion")
print()
print("### err4_correct_filter_usage")
good_tmpl = env.from_string("{{ (items | length) == 3 }}")
print(good_tmpl.render(items=[1, 2, 3]))
print("### END err4_correct_filter_usage")
print()

# 5. default 对空字符串和 0 的处理不符合预期
show("err5_default_on_zero_wrong_expectation", "retries=[{{ retries | default(3) }}]", {"retries": 0})
show("err5_default_on_zero_with_boolean_true", "retries=[{{ retries | default(3, true) }}]", {"retries": 0})

# 6. 循环中的变量作用域问题（已在 set 示例中演示，这里再给一个最小复现）
show(
    "err6_loop_scope_minimal",
    "{% set found = false %}"
    "{% for x in items %}{% if x == 2 %}{% set found = true %}{% endif %}{% endfor %}"
    "found=[{{ found }}] (WRONG: still false)",
    {"items": [1, 2, 3]},
)

# 7. include 与 import 的使用场景混淆：import 默认不共享调用方 context
IMPORT_TEMPLATE = """\
{% set hostname = 'router-01' %}
{% import "err7_macro.j2" as m %}
{{ m.show_hostname() }}
"""
from jinja2 import DictLoader

env_dict = Environment(
    loader=DictLoader(
        {
            "err7_macro.j2": "{% macro show_hostname() %}hostname={{ hostname | default('(not visible without context)') }}{% endmacro %}",
            "err7_main.j2": IMPORT_TEMPLATE,
        }
    ),
    trim_blocks=True,
    lstrip_blocks=True,
)
print("### err7_import_without_context")
print(env_dict.get_template("err7_main.j2").render())
print("### END err7_import_without_context")
print()

# 8. 输出中的多余换行或空格（典型原因：block 标签自带的换行没有被处理）
ERR8_TEMPLATE = "line1\n{% if true %}\nline2\n{% endif %}\nline3\n"

env_plain = Environment()  # 没有开启 trim_blocks / lstrip_blocks
print("### err8_extra_blank_lines_problem")
print(repr(env_plain.from_string(ERR8_TEMPLATE).render()))
print("### END err8_extra_blank_lines_problem")
print()

env_fixed = Environment(trim_blocks=True, lstrip_blocks=True)  # 推荐的 Environment 级修复
print("### err8_fixed_with_trim_lstrip_blocks")
print(repr(env_fixed.from_string(ERR8_TEMPLATE).render()))
print("### END err8_fixed_with_trim_lstrip_blocks")
print()

# 9. YAML 缩进和列表格式错误：忘记写 "- " 列表项前缀，
#    每次循环都输出一个同名的顶层 key，YAML 不会报错，而是静默地用后一个值覆盖前一个值，
#    最终丢失了除最后一个接口以外的所有数据——这比直接报错更危险。
BAD_YAML_TEMPLATE = """\
hostname: {{ hostname }}
interfaces:
{% for interface in interfaces %}
name: {{ interface }}
{% endfor %}
"""
GOOD_YAML_TEMPLATE = """\
hostname: {{ hostname }}
interfaces:
{% for interface in interfaces %}
  - name: {{ interface }}
{% endfor %}
"""
data = {"hostname": "router-01", "interfaces": ["eth0", "eth1"]}
bad_output = env.from_string(BAD_YAML_TEMPLATE).render(**data)
good_output = env.from_string(GOOD_YAML_TEMPLATE).render(**data)
print("### err9_yaml_missing_dash_silently_loses_data")
print(bad_output, end="")
print("parsed =", yaml.safe_load(bad_output))
print("### END err9_yaml_missing_dash_silently_loses_data")
print()
print("### err9_yaml_properly_nested")
print(good_output, end="")
print("parsed =", yaml.safe_load(good_output))
print("### END err9_yaml_properly_nested")
print()

# 10. HTML 转义配置不正确
env_no_escape = Environment(autoescape=False)
tmpl = env_no_escape.from_string("<p>{{ comment }}</p>")
print("### err10_html_no_autoescape_xss_risk")
print(tmpl.render(comment="<script>alert(1)</script>"))
print("### END err10_html_no_autoescape_xss_risk")
print()
env_escape = Environment(autoescape=True)
tmpl = env_escape.from_string("<p>{{ comment }}</p>")
print("### err10_html_with_autoescape_safe")
print(tmpl.render(comment="<script>alert(1)</script>"))
print("### END err10_html_with_autoescape_safe")
print()

# 11. 将复杂业务逻辑放入模板（反例 vs 正例，只展示渲染结果一致但模板复杂度不同）
BUSINESS_LOGIC_IN_TEMPLATE = (
    "{% set score = (a * 0.4 + b * 0.3 + c * 0.3) %}"
    "{% if score >= 90 %}A{% elif score >= 80 %}B{% elif score >= 60 %}C{% else %}D{% endif %}"
)
show(
    "err11_business_logic_in_template",
    BUSINESS_LOGIC_IN_TEMPLATE,
    {"a": 95, "b": 88, "c": 92},
)


def compute_grade(a, b, c):
    score = a * 0.4 + b * 0.3 + c * 0.3
    if score >= 90:
        return "A"
    if score >= 80:
        return "B"
    if score >= 60:
        return "C"
    return "D"


show(
    "err11_business_logic_in_python",
    "{{ grade }}",
    {"grade": compute_grade(95, 88, 92)},
)

# 12. 在模板中访问不存在的属性
print("### err12_strict_undefined_catches_bad_attribute")
env_strict = Environment(undefined=StrictUndefined)
tmpl = env_strict.from_string("{{ device.ip_address }}")
try:
    print(tmpl.render(device={"name": "eth0"}))
except UndefinedError as exc:
    print(f"UndefinedError: {exc}")
print("### END err12_strict_undefined_catches_bad_attribute")
