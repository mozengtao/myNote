#!/usr/bin/env python3
"""
tests_demo.py
验证 jinja_template_designer_guide.md 第 6 节 (Tests) 中用到的所有示例。
用同一份 "端口配置" 数据集，在 缺失 / None / 有效值 三种输入下跑同一个模板。
"""
from jinja2 import Environment

env = Environment(trim_blocks=True, lstrip_blocks=True)

PORT_TEMPLATE = """\
{% if port is defined and port is not none %}
port: {{ port }}
{% else %}
# port is not configured
{% endif %}
"""

tmpl = env.from_string(PORT_TEMPLATE)

print("### port_missing (未传入 port)")
print(tmpl.render())
print("### END port_missing")
print()

print("### port_none (port=None)")
print(tmpl.render(port=None))
print("### END port_none")
print()

print("### port_valid (port=8080)")
print(tmpl.render(port=8080))
print("### END port_valid")
print()


def show(name, template_src, data):
    t = env.from_string(template_src)
    out = t.render(**data)
    print(f"### {name}")
    print(out)
    print(f"### END {name}")
    print()


# is defined / is undefined
show(
    "is_defined_true",
    "{{ port is defined }}",
    {"port": 22},
)
show(
    "is_defined_false",
    "{{ port is defined }}",
    {},
)
show(
    "is_undefined_true",
    "{{ port is undefined }}",
    {},
)

# is none
show(
    "is_none_true",
    "{{ port is none }}",
    {"port": None},
)
show(
    "is_none_false",
    "{{ port is none }}",
    {"port": 0},
)

# type tests
show(
    "is_string",
    "{{ name is string }}",
    {"name": "eth0"},
)
show(
    "is_number_int",
    "{{ mtu is number }}",
    {"mtu": 1500},
)
show(
    "is_integer",
    "{{ mtu is integer }}",
    {"mtu": 1500},
)
show(
    "is_float",
    "{{ cpu is float }}",
    {"cpu": 12.5},
)
show(
    "is_boolean",
    "{{ enabled is boolean }}",
    {"enabled": True},
)
show(
    "is_mapping",
    "{{ config is mapping }}",
    {"config": {"mtu": 1500}},
)
show(
    "is_sequence",
    "{{ interfaces is sequence }}",
    {"interfaces": ["eth0", "eth1"]},
)
show(
    "is_iterable",
    "{{ interfaces is iterable }}",
    {"interfaces": ["eth0", "eth1"]},
)

# odd / even / divisibleby
show(
    "is_odd",
    "{{ 3 is odd }}",
    {},
)
show(
    "is_even",
    "{{ 4 is even }}",
    {},
)
show(
    "is_divisibleby",
    "{{ 9 is divisibleby(3) }}",
    {},
)
show(
    "is_divisibleby_false",
    "{{ 10 is divisibleby(3) }}",
    {},
)

# sameas
show(
    "is_sameas_true_identity",
    "{{ enabled is sameas true }}",
    {"enabled": True},
)
show(
    "is_sameas_one_is_not_true",
    "{{ enabled is sameas true }}",
    {"enabled": 1},
)

# filter vs test contrast
show(
    "filter_vs_test_filter",
    "{{ name | upper }}",
    {"name": "eth0"},
)
show(
    "filter_vs_test_test",
    "{{ name is string }}",
    {"name": "eth0"},
)

# default filter vs defined test (explicit contrast)
show(
    "default_filter_on_missing",
    "{{ mtu | default(1500) }}",
    {},
)
show(
    "defined_test_on_missing",
    "{{ mtu is defined }}",
    {},
)
