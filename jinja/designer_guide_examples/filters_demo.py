#!/usr/bin/env python3
"""
filters_demo.py
验证 jinja_template_designer_guide.md 第 5 节 (Filters) 中用到的所有示例。
每个示例都用 "### name" 作为分隔标记打印，方便从标准输出中逐段摘录到文档里。
"""
from jinja2 import Environment, Undefined

env = Environment(trim_blocks=True, lstrip_blocks=True)


def show(name, template_src, data):
    tmpl = env.from_string(template_src)
    out = tmpl.render(**data)
    print(f"### {name}")
    print(out)
    print(f"### END {name}")
    print()


# ---------- default ----------
show(
    "default_basic_undefined",
    "{{ value | default('N/A') }}",
    {},
)
show(
    "default_basic_empty_string",
    "{{ value | default('N/A') }}",
    {"value": ""},
)
show(
    "default_basic_zero",
    "{{ value | default('N/A') }}",
    {"value": 0},
)
show(
    "default_basic_false",
    "{{ value | default('N/A') }}",
    {"value": False},
)
show(
    "default_boolean_true_undefined",
    "{{ value | default('N/A', true) }}",
    {},
)
show(
    "default_boolean_true_empty_string",
    "{{ value | default('N/A', true) }}",
    {"value": ""},
)
show(
    "default_boolean_true_zero",
    "{{ value | default('N/A', true) }}",
    {"value": 0},
)
show(
    "default_boolean_true_false",
    "{{ value | default('N/A', true) }}",
    {"value": False},
)

# ---------- trim / upper / lower / capitalize / title ----------
show(
    "trim",
    "[{{ hostname | trim }}]",
    {"hostname": "   router-01   "},
)
show(
    "upper",
    "{{ name | upper }}",
    {"name": "router-01"},
)
show(
    "lower",
    "{{ name | lower }}",
    {"name": "ROUTER-01"},
)
show(
    "capitalize",
    "{{ text | capitalize }}",
    {"text": "hello world"},
)
show(
    "title",
    "{{ text | title }}",
    {"text": "hello world"},
)

# ---------- replace ----------
show(
    "replace",
    "{{ path | replace('/', '_') }}",
    {"path": "/etc/network/interfaces"},
)

# ---------- join ----------
show(
    "join_basic",
    "{{ interfaces | join(', ') }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)
show(
    "join_attribute",
    "{{ devices | join(', ', attribute='name') }}",
    {"devices": [{"name": "sw1"}, {"name": "sw2"}, {"name": "sw3"}]},
)

# ---------- length ----------
show(
    "length_list",
    "{{ interfaces | length }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)
show(
    "length_string",
    "{{ 'router-01' | length }}",
    {},
)

# ---------- first / last ----------
show(
    "first",
    "{{ interfaces | first }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)
show(
    "last",
    "{{ interfaces | last }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)

# ---------- sort ----------
show(
    "sort_default",
    "{{ ports | sort | join(', ') }}",
    {"ports": [443, 22, 8080, 80]},
)
show(
    "sort_reverse",
    "{{ ports | sort(reverse=true) | join(', ') }}",
    {"ports": [443, 22, 8080, 80]},
)
show(
    "sort_attribute",
    "{{ devices | sort(attribute='name') | map(attribute='name') | join(', ') }}",
    {"devices": [{"name": "sw3"}, {"name": "sw1"}, {"name": "sw2"}]},
)

# ---------- unique ----------
show(
    "unique",
    "{{ vlans | unique | list }}",
    {"vlans": [10, 20, 10, 30, 20]},
)

# ---------- reverse ----------
show(
    "reverse_list",
    "{{ interfaces | reverse | list }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)
show(
    "reverse_string",
    "{{ 'router' | reverse }}",
    {},
)

# ---------- int / float / string ----------
show(
    "int_from_str",
    "{{ mtu | int }}",
    {"mtu": "1500"},
)
show(
    "int_invalid_default",
    "{{ mtu | int(0) }}",
    {"mtu": "not-a-number"},
)
show(
    "float_filter",
    "{{ cpu_usage | float }}",
    {"cpu_usage": "12.5"},
)
show(
    "string_filter",
    "{{ (vlan_id | string) ~ '-tag' }}",
    {"vlan_id": 100},
)

# ---------- round / abs ----------
show(
    "round_default",
    "{{ 12.3456 | round(2) }}",
    {},
)
show(
    "round_ceil",
    "{{ 12.1 | round(0, 'ceil') }}",
    {},
)
show(
    "abs_filter",
    "{{ delta | abs }}",
    {"delta": -8},
)

# ---------- list ----------
show(
    "list_from_string",
    "{{ 'abc' | list }}",
    {},
)
show(
    "list_materialize_map",
    "{{ (numbers | map('int') | list) }}",
    {"numbers": ["1", "2", "3"]},
)

# ---------- dictsort (the real "dict" related filter) / items ----------
show(
    "dictsort_filter",
    "{% for key, value in config | dictsort %}{{ key }}={{ value }} {% endfor %}",
    {"config": {"mtu": 1500, "enabled": True, "name": "eth0"}},
)
show(
    "items_filter",
    "{% for key, value in config | items %}{{ key }}={{ value }} {% endfor %}",
    {"config": {"name": "eth0", "mtu": 1500}},
)

# ---------- map ----------
show(
    "map_attribute",
    "{{ devices | map(attribute='name') | list }}",
    {"devices": [{"name": "sw1"}, {"name": "sw2"}]},
)
show(
    "map_filter_name",
    "{{ names | map('upper') | list }}",
    {"names": ["eth0", "eth1"]},
)

# ---------- select / reject ----------
show(
    "select_test",
    "{{ numbers | select('odd') | list }}",
    {"numbers": [1, 2, 3, 4, 5, 6]},
)
show(
    "reject_test",
    "{{ numbers | reject('odd') | list }}",
    {"numbers": [1, 2, 3, 4, 5, 6]},
)

# ---------- selectattr / rejectattr ----------
show(
    "selectattr_basic",
    "{% for d in devices | selectattr('enabled') | list %}{{ d.name }} {% endfor %}",
    {
        "devices": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
            {"name": "eth2", "enabled": True},
        ]
    },
)
show(
    "rejectattr_basic",
    "{% for d in devices | rejectattr('enabled') | list %}{{ d.name }} {% endfor %}",
    {
        "devices": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
            {"name": "eth2", "enabled": True},
        ]
    },
)
show(
    "selectattr_with_test",
    "{{ devices | selectattr('mtu', 'equalto', 1500) | map(attribute='name') | list }}",
    {
        "devices": [
            {"name": "eth0", "mtu": 1500},
            {"name": "eth1", "mtu": 9000},
        ]
    },
)

# ---------- tojson ----------
show(
    "tojson_filter",
    "{{ config | tojson }}",
    {"config": {"name": "eth0", "mtu": 1500, "enabled": True}},
)

# ---------- escape / e / safe ----------
env_autoescape_off = Environment(autoescape=False)
tmpl = env_autoescape_off.from_string("{{ raw | escape }}")
print("### escape_filter")
print(tmpl.render(raw="<b>eth0</b>"))
print("### END escape_filter")
print()

tmpl = env_autoescape_off.from_string("{{ raw | e }}")
print("### e_alias_filter")
print(tmpl.render(raw="<b>eth0</b>"))
print("### END e_alias_filter")
print()

env_autoescape_on = Environment(autoescape=True)
tmpl = env_autoescape_on.from_string("{{ raw }}")
print("### autoescape_on_default")
print(tmpl.render(raw="<b>eth0</b>"))
print("### END autoescape_on_default")
print()

tmpl = env_autoescape_on.from_string("{{ raw | safe }}")
print("### safe_filter_bypasses_autoescape")
print(tmpl.render(raw="<b>eth0</b>"))
print("### END safe_filter_bypasses_autoescape")
print()

# ---------- composed cases A-D ----------
show(
    "case_A_string_cleanup",
    "{{ hostname | trim | lower }}",
    {"hostname": "  Router-01  "},
)
show(
    "case_B_list_formatting",
    "{{ interfaces | join(', ') }}",
    {"interfaces": ["eth0", "eth1", "eth2"]},
)
show(
    "case_C_object_filter",
    "{% for device in devices | selectattr('enabled') | list %}"
    "{{ device.name }}\n"
    "{% endfor %}",
    {
        "devices": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
            {"name": "eth2", "enabled": True},
        ]
    },
)
show(
    "case_D_missing_field_default",
    "{{ device.description | default('No description') }}",
    {"device": {"name": "eth0"}},
)
