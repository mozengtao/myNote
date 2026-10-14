#!/usr/bin/env python3
"""
variables_expressions_demo.py
验证 jinja_template_designer_guide.md 第 3 节 (Variables & Expressions) 的所有示例。
"""
from jinja2 import Environment

env = Environment(trim_blocks=True, lstrip_blocks=True)


def show(name, template_src, data):
    t = env.from_string(template_src)
    out = t.render(**data)
    print(f"### {name}")
    print(out)
    print(f"### END {name}")
    print()


# 基本变量
show("var_simple", "Hello, {{ name }}!", {"name": "Morris"})

# 属性访问 user.name
show(
    "attr_access_dict_as_attr",
    "{{ user.name }}",
    {"user": {"name": "Alice", "age": 18}},
)

# 索引访问 user["name"]
show(
    "index_access_dict_key",
    '{{ user["name"] }}',
    {"user": {"name": "Alice", "age": 18}},
)

# 列表索引访问
show(
    "index_access_list",
    "{{ items[0] }}",
    {"items": ["eth0", "eth1", "eth2"]},
)

# 嵌套字典/列表访问
show(
    "nested_access",
    "{{ config.network.interface }}",
    {"config": {"network": {"interface": "eth0"}}},
)
show(
    "nested_list_in_dict",
    "{{ config.network.interfaces[1] }}",
    {"config": {"network": {"interfaces": ["eth0", "eth1"]}}},
)

# 访问不存在的变量/属性：默认 Undefined 渲染为空字符串
show(
    "missing_top_level_var",
    "[{{ missing_var }}]",
    {},
)
show(
    "missing_attribute",
    "[{{ user.nickname }}]",
    {"user": {"name": "Alice"}},
)

# foo.bar 与 foo["bar"] 的查找顺序差异：
# .attr 先尝试 getattr()，失败后再退化为 getitem()
# ["key"] 先尝试 getitem()，失败后再退化为 getattr()
# 对于普通 dict，二者结果通常一致；但当对象同时有同名"属性"和"键"时会有歧义。
class ObjWithBothAttrAndItem(dict):
    """演示属性和字典键同名但值不同的歧义场景。"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # 把 self.name 设置为一个"属性"，与字典键 'name' 的值不同
        self.name = "ATTRIBUTE-VALUE"


obj = ObjWithBothAttrAndItem(name="DICT-KEY-VALUE")
show("attr_vs_item_dot_syntax", "{{ obj.name }}", {"obj": obj})
show("attr_vs_item_bracket_syntax", '{{ obj["name"] }}', {"obj": obj})

# ---------- 表达式 ----------

# 字符串拼接 ~
show(
    "concat_tilde",
    "{{ 'vlan-' ~ vlan_id }}",
    {"vlan_id": 100},
)

# 算术运算
show("arith_add", "{{ 10 + 5 }}", {})
show("arith_sub", "{{ 10 - 5 }}", {})
show("arith_mul", "{{ 10 * 5 }}", {})
show("arith_div", "{{ 10 / 4 }}", {})
show("arith_floordiv", "{{ 10 // 4 }}", {})
show("arith_mod", "{{ 10 % 4 }}", {})

# 比较
show("cmp_eq", "{{ mtu == 1500 }}", {"mtu": 1500})
show("cmp_ne", "{{ mtu != 1500 }}", {"mtu": 9000})
show("cmp_gt", "{{ mtu > 1500 }}", {"mtu": 9000})
show("cmp_lt", "{{ mtu < 1500 }}", {"mtu": 1000})

# 逻辑
show("logic_and", "{{ enabled and mtu > 0 }}", {"enabled": True, "mtu": 1500})
show("logic_or", "{{ enabled or is_backup }}", {"enabled": False, "is_backup": True})
show("logic_not", "{{ not enabled }}", {"enabled": False})

# 成员判断 in
show(
    "membership_in_list",
    "{{ 'eth0' in interfaces }}",
    {"interfaces": ["eth0", "eth1"]},
)
show(
    "membership_not_in",
    "{{ 'eth9' not in interfaces }}",
    {"interfaces": ["eth0", "eth1"]},
)

# 三元表达式
show(
    "ternary_expr",
    "{{ 'UP' if interface.enabled else 'DOWN' }}",
    {"interface": {"enabled": True}},
)
show(
    "ternary_expr_false",
    "{{ 'UP' if interface.enabled else 'DOWN' }}",
    {"interface": {"enabled": False}},
)

# 函数调用
show(
    "func_call_builtin",
    "{{ range(3) | list }}",
    {},
)


def format_mtu(value):
    return f"MTU={value}"


show(
    "func_call_custom_global",
    "{{ format_mtu(1500) }}",
    {"format_mtu": format_mtu},
)

# 属性访问 + 索引访问组合
show(
    "attr_and_index_combo",
    "{{ devices[0].interfaces[1].name }}",
    {
        "devices": [
            {
                "interfaces": [
                    {"name": "eth0"},
                    {"name": "eth1"},
                ]
            }
        ]
    },
)

# ---------- {% set %} ----------

# 普通变量赋值
show(
    "set_basic",
    "{% set greeting = 'Hello' %}{{ greeting }}, {{ name }}!",
    {"name": "Morris"},
)

# 表达式赋值
show(
    "set_expression",
    "{% set total_ports = len_a + len_b %}{{ total_ports }}",
    {"len_a": 3, "len_b": 5},
)

# 多变量解包
show(
    "set_multi_unpack",
    "{% set a, b = (1, 2) %}a={{ a }}, b={{ b }}",
    {},
)

# 预先计算值以简化模板
show(
    "set_precompute",
    "{% set enabled_count = interfaces | selectattr('enabled') | list | length %}"
    "Enabled interfaces: {{ enabled_count }}",
    {
        "interfaces": [
            {"name": "eth0", "enabled": True},
            {"name": "eth1", "enabled": False},
            {"name": "eth2", "enabled": True},
        ]
    },
)

# 循环中 set 的作用域陷阱：{% set total = total + item.value %} 不会"跨迭代累加"
show(
    "set_loop_scope_pitfall",
    "{% set total = 0 %}"
    "{% for item in items %}"
    "{% set total = total + item.value %}"
    "{% endfor %}"
    "total (WRONG, still 0) = {{ total }}",
    {"items": [{"value": 10}, {"value": 20}, {"value": 30}]},
)

# 正确做法 1：用 namespace() 保存可跨循环变化的状态
show(
    "set_namespace_correct",
    "{% set ns = namespace(total=0) %}"
    "{% for item in items %}"
    "{% set ns.total = ns.total + item.value %}"
    "{% endfor %}"
    "total (namespace) = {{ ns.total }}",
    {"items": [{"value": 10}, {"value": 20}, {"value": 30}]},
)

# 正确做法 2（推荐）：优先在 Python 里把复杂计算做完，模板只做展示
precomputed_total = sum(item["value"] for item in [{"value": 10}, {"value": 20}, {"value": 30}])
show(
    "set_precomputed_in_python",
    "total (computed in Python) = {{ total }}",
    {"total": precomputed_total},
)
