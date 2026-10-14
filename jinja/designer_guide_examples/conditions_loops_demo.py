#!/usr/bin/env python3
"""
conditions_loops_demo.py
验证 jinja_template_designer_guide.md 第 4 节 (Conditions & Loops) 的所有示例。
"""
from jinja2 import Environment

env = Environment(trim_blocks=True, lstrip_blocks=True, keep_trailing_newline=True)


def show(name, template_src, data):
    t = env.from_string(template_src)
    out = t.render(**data)
    print(f"### {name}")
    print(out, end="")
    if not out.endswith("\n"):
        print()
    print(f"### END {name}")
    print()


# ---------- if / elif / else ----------
IF_ELIF_ELSE_TEMPLATE = """\
{% if level == 'error' %}
[ERROR] {{ message }}
{% elif level == 'warning' %}
[WARN] {{ message }}
{% else %}
[INFO] {{ message }}
{% endif %}
"""
show("if_elif_else_error", IF_ELIF_ELSE_TEMPLATE, {"level": "error", "message": "disk full"})
show("if_elif_else_warning", IF_ELIF_ELSE_TEMPLATE, {"level": "warning", "message": "disk 80% full"})
show("if_elif_else_info", IF_ELIF_ELSE_TEMPLATE, {"level": "info", "message": "backup done"})

# 多条件组合
show(
    "if_multi_condition",
    "{% if enabled and mtu >= 1500 %}OK{% else %}SKIP{% endif %}",
    {"enabled": True, "mtu": 9000},
)
show(
    "if_multi_condition_skip",
    "{% if enabled and mtu >= 1500 %}OK{% else %}SKIP{% endif %}",
    {"enabled": True, "mtu": 1000},
)

# is defined / is none
show(
    "if_is_defined",
    "{% if port is defined %}port={{ port }}{% else %}no port{% endif %}",
    {},
)
show(
    "if_is_none",
    "{% if port is none %}port is None{% else %}port={{ port }}{% endif %}",
    {"port": None},
)

# 判断列表是否为空
show(
    "if_list_empty",
    "{% if interfaces %}has interfaces{% else %}no interfaces{% endif %}",
    {"interfaces": []},
)
show(
    "if_list_nonempty",
    "{% if interfaces %}has interfaces{% else %}no interfaces{% endif %}",
    {"interfaces": ["eth0"]},
)

# 根据配置生成不同的 YAML 字段
YAML_FIELD_TEMPLATE = """\
name: {{ device.name }}
{% if device.description is defined %}
description: {{ device.description }}
{% endif %}
{% if device.vlan is defined %}
vlan: {{ device.vlan }}
{% endif %}
"""
show(
    "yaml_conditional_fields_full",
    YAML_FIELD_TEMPLATE,
    {"device": {"name": "eth0", "description": "uplink", "vlan": 10}},
)
show(
    "yaml_conditional_fields_minimal",
    YAML_FIELD_TEMPLATE,
    {"device": {"name": "eth1"}},
)

# ---------- for 循环 ----------

# 遍历列表
show(
    "for_basic_list",
    "{% for item in items %}{{ item }} {% endfor %}",
    {"items": ["eth0", "eth1", "eth2"]},
)

# 遍历字典
show(
    "for_dict_items",
    "{% for key, value in config.items() %}{{ key }}={{ value }} {% endfor %}",
    {"config": {"mtu": 1500, "enabled": True}},
)

# 遍历对象（字典）列表
show(
    "for_object_list",
    "{% for device in devices %}{{ device.name }}({{ device.status }}) {% endfor %}",
    {
        "devices": [
            {"name": "sw1", "status": "up"},
            {"name": "sw2", "status": "down"},
        ]
    },
)

# 嵌套循环
show(
    "for_nested_loop",
    "{% for device in devices %}"
    "{% for interface in device.interfaces %}"
    "{{ device.name }}.{{ interface }} "
    "{% endfor %}"
    "{% endfor %}",
    {
        "devices": [
            {"name": "sw1", "interfaces": ["eth0", "eth1"]},
            {"name": "sw2", "interfaces": ["eth0"]},
        ]
    },
)

# 循环内的 if
show(
    "for_with_inner_if",
    "{% for device in devices %}"
    "{% if device.enabled %}{{ device.name }} {% endif %}"
    "{% endfor %}",
    {
        "devices": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
        ]
    },
)

# 带过滤条件的循环： {% for x in xs if x.enabled %}
show(
    "for_with_filter_condition",
    "{% for device in devices if device.enabled %}{{ device.name }} {% endfor %}",
    {
        "devices": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
            {"name": "eth2", "enabled": True},
        ]
    },
)

# for ... else：没有任何元素被迭代时执行
show(
    "for_else_empty",
    "{% for device in devices %}{{ device.name }} {% else %}no devices{% endfor %}",
    {"devices": []},
)
show(
    "for_else_filtered_empty",
    "{% for device in devices if device.enabled %}{{ device.name }} {% else %}no enabled devices{% endfor %}",
    {"devices": [{"name": "eth0", "enabled": False}]},
)
show(
    "for_else_nonempty",
    "{% for device in devices %}{{ device.name }} {% else %}no devices{% endfor %}",
    {"devices": [{"name": "eth0"}]},
)

# loop.index / loop.index0：同一份数据对比
show(
    "loop_index_vs_index0",
    "{% for item in items %}{{ loop.index }}:{{ loop.index0 }}:{{ item }} {% endfor %}",
    {"items": ["eth0", "eth1", "eth2"]},
)

# loop.first / loop.last
show(
    "loop_first_last",
    "{% for item in items %}"
    "{% if loop.first %}[FIRST]{% endif %}"
    "{{ item }}"
    "{% if loop.last %}[LAST]{% endif %} "
    "{% endfor %}",
    {"items": ["eth0", "eth1", "eth2"]},
)

# loop.length
show(
    "loop_length",
    "{% for item in items %}{{ loop.index }}/{{ loop.length }} {% endfor %}",
    {"items": ["eth0", "eth1", "eth2"]},
)

# loop.cycle()：典型用途（如交替样式/交替标记）
show(
    "loop_cycle",
    "{% for item in items %}{{ loop.cycle('odd', 'even') }}:{{ item }} {% endfor %}",
    {"items": ["eth0", "eth1", "eth2", "eth3"]},
)
