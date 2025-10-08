[yaml](https://yaml.org/)  
[YAML Tutorial : A Complete Language Guide with Examples](https://spacelift.io/blog/yaml)  
[onlineyamltools](https://onlineyamltools.com/)  
[YAML Formatter](https://jsonformatter.org/yaml-formatter#Sample)  
[Learn X in Y minutes](https://learnxinyminutes.com/yaml/)  
[How to Work with YAML in Python](https://earthly.dev/blog/yaml-in-python/)  
[YAML: The Missing Battery in Python](https://realpython.com/python-yaml/)  
[]()  
[]()  

```yaml
# YAML三种基本数据结构
	标量：单一的值，如字符串、数字、布尔值等
	列表：一系列有序的值
	字典：键值对的集合

# 缩进：YAML 使用空格缩进来表示层次结构，缩进必须一致，通常使用 2 个或 4 个空格，不能使用制表符（Tab）

# 注释：注释以 # 开头，直到行尾

# YAML 基本语法
# 键值对（字典）, YAML 使用冒号 : 分隔键和值，键和值之间需要留空格
name: John Doe
age: 30
married: true

# 列表, 列表使用破折号 - 表示每一项
fruits:
  - apple
  - banana
  - orange

 # 嵌套结构, 通过缩进实现字典和列表的嵌套结构
 person:
  name: John Doe
  age: 30
  children:
    - name: Jane
      age: 10
    - name: Joe
      age: 8
# person 是一个字典，里面包含 name 和 age，还有一个嵌套的 children 列表。
# children 列表中每个项也是一个字典

# 多行字符串, 如果需要表示多行字符串，可以使用 | 或 >
# | 保留换行符
description: |
  This is a long description.
  It can span multiple lines.
# > 将换行符转为空格
description: >
  This is a long description
  that will be rendered as a single line.

# 布尔值和 Null, 布尔值用 true 或 false 表示，null 值可以用 null、~ 或空字符串表示
is_active: true
is_admin: false
value: null
empty_value: ~

# 引用和锚点, YAML 支持锚点（&）和引用（*），用于重用数据
default_settings: &defaults
  timeout: 30
  retries: 3

server1:
  <<: *defaults
  timeout: 60
# &defaults 定义了一个锚点，包含 timeout 和 retries 的值
# <<: *defaults 引用了该锚点并继承了默认设置，同时可以覆盖部分值（如 timeout 设置为 60）


# YAML 示例
version: 1.0
application:
  name: ExampleApp
  environment: production

servers:
  web:
    ip: 192.168.1.1
    roles:
      - frontend
      - load_balancer
  database:
    ip: 192.168.1.2
    roles:
      - backend
      - db

users:
  - name: admin
    role: superuser
    active: true
  - name: guest
    role: viewer
    active: false

settings:
  retries: 5
  debug: false
  logs:
    path: /var/log/app.log
    level: info
```

```python
import yaml

names_yaml = """
- 'eric'
- 'justin'
- 'mary-kate'
"""

names = yaml.safe_load(names_yaml)

with open('names.txt', 'w') as file:
  yaml.dump(names, file)

print(open('names.txt').read())

with open('names.txt', 'r') as file:
    loaded_names = yaml.safe_load(file)

print(loaded_names)
```

[YAML: The Missing Battery in Python](https://realpython.com/python-yaml/)  
[Python YAML: How to Load, Read, and Write YAML](https://python.land/data-processing/python-yaml)  
[PyYAML Documentation](https://pyyaml.org/wiki/PyYAMLDocumentation)  
[YAML: The Missing Battery in Python](https://realpython.com/python-yaml/)  
[Working with YAML Files in Python](https://betterstack.com/community/guides/scaling-python/yaml-files-in-python/)  
[Reading YAML & JSON: My Wholesome Guide to Learning Python](https://nepalbrothers.medium.com/reading-yaml-my-wholesome-guide-to-learning-python-part-4-c7317a4a81f1)  
[Class & dataclass: My Wholesome Guide to Learning Python (Part 3)](https://nepalbrothers.medium.com/my-wholesome-guide-to-learning-python-part-3-464b455f3d44)  
[]()  
[]()  
[]()  