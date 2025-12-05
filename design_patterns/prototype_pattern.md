# Prototype Pattern (原型模式)

## ASCII Diagram

```
+-------------------+
|     Client        |
+-------------------+
| + operation()     |
+--------+----------+
         |
         | uses
         v
+-------------------+
|    Prototype      |<<interface>>
+-------------------+
| + clone()         |
+-------------------+
         ^
         |
   +-----+-----+
   |           |
+--+--------+  +----------+
|ConcreteP1 |  |ConcreteP2|
+-----------+  +----------+
| - field1  |  | - field1 |
| - field2  |  | - field2 |
+-----------+  +----------+
| + clone() |  | + clone()|
+-----------+  +----------+

Clone Process:
+------------+     clone()      +------------+
| Original   | --------------> | Copy       |
+------------+                  +------------+
| field1: A  |                  | field1: A  |
| field2: B  |                  | field2: B  |
+------------+                  +------------+
   Object 1                        Object 2
   (independent instances)
```

**中文说明：**
- **Prototype（原型接口）**：声明克隆自身的接口
- **ConcretePrototype（具体原型）**：实现克隆操作，返回自身的副本
- **Client（客户端）**：通过调用原型的 clone() 方法创建新对象
- **关键点**：通过复制现有对象来创建新对象，而非通过构造函数

---

## 核心思想

用原型实例指定创建对象的种类，并且通过**拷贝这些原型**创建新的对象。原型模式允许对象创建自身的副本，而无需依赖于它们的具体类。

**两种拷贝方式：**
- **浅拷贝（Shallow Copy）**：只复制对象本身，引用类型的属性仍指向原对象
- **深拷贝（Deep Copy）**：完全复制对象及其所有嵌套对象

---

## 应用场景

1. **创建成本高**：当对象的创建需要大量资源（数据库查询、网络请求、复杂计算）
2. **保护原对象**：需要修改对象但不想影响原对象
3. **动态配置对象**：需要在运行时指定要实例化的类
4. **实际应用**：
   - 配置模板系统
   - 游戏中的角色/怪物生成
   - 文档编辑器的"复制"功能
   - 数据库记录的复制
   - 图形编辑器中的图形复制

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 性能优化 | 避免重复的初始化代码，直接复制现有对象 |
| 简化创建 | 无需知道对象的具体类型 |
| 动态配置 | 可以在运行时添加或删除产品 |
| 保护原型 | 可以基于原型修改而不影响原对象 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 深拷贝复杂 | 对于有循环引用的复杂对象，实现深拷贝困难 |
| 克隆方法实现 | 每个类都需要实现 clone() 方法 |
| 隐藏构造过程 | 可能隐藏了对象的构造过程 |

---

## Python 代码示例

### 应用前：每次都重新创建对象

```python
import time


class GameCharacter:
    """游戏角色 - 初始化需要复杂计算"""
    
    def __init__(self, name, char_class, level):
        self.name = name
        self.char_class = char_class
        self.level = level
        
        # 模拟从数据库加载基础属性
        print(f"Loading base stats for {char_class}...")
        time.sleep(0.5)  # 模拟耗时操作
        
        # 模拟加载技能树
        print(f"Loading skill tree...")
        time.sleep(0.3)
        self.skills = self._load_skills(char_class)
        
        # 模拟加载装备模板
        print(f"Loading equipment...")
        time.sleep(0.2)
        self.equipment = self._load_equipment(char_class)
        
        self.stats = self._calculate_stats()
    
    def _load_skills(self, char_class):
        # 模拟复杂的技能加载
        skill_map = {
            "warrior": ["slash", "block", "charge"],
            "mage": ["fireball", "ice_shield", "teleport"],
            "archer": ["arrow", "trap", "stealth"],
        }
        return skill_map.get(char_class, [])
    
    def _load_equipment(self, char_class):
        equip_map = {
            "warrior": {"weapon": "sword", "armor": "plate"},
            "mage": {"weapon": "staff", "armor": "robe"},
            "archer": {"weapon": "bow", "armor": "leather"},
        }
        return equip_map.get(char_class, {})
    
    def _calculate_stats(self):
        base_stats = {"hp": 100, "mp": 50, "attack": 10, "defense": 5}
        # 根据等级调整
        for stat in base_stats:
            base_stats[stat] += self.level * 5
        return base_stats


# 问题：每次创建角色都要重新加载
print("Creating first warrior...")
start = time.time()
warrior1 = GameCharacter("Player1", "warrior", 10)
print(f"Time: {time.time() - start:.2f}s\n")

print("Creating second warrior (same class)...")
start = time.time()
warrior2 = GameCharacter("Player2", "warrior", 10)  # 重复加载相同数据！
print(f"Time: {time.time() - start:.2f}s")

# 问题：
# 1. 相同职业的角色重复加载相同数据
# 2. 创建大量角色时性能差
# 3. 无法基于现有角色快速创建变体
```

### 应用后：使用原型模式

```python
import copy
import time
from abc import ABC, abstractmethod


class Prototype(ABC):
    """原型接口"""
    
    @abstractmethod
    def clone(self):
        """返回自身的副本"""
        pass


class GameCharacter(Prototype):
    """游戏角色 - 支持克隆"""
    
    def __init__(self, name, char_class, level, preloaded=False):
        self.name = name
        self.char_class = char_class
        self.level = level
        
        if not preloaded:
            # 只有原型需要加载数据
            print(f"Loading base stats for {char_class}...")
            time.sleep(0.5)
            
            print(f"Loading skill tree...")
            time.sleep(0.3)
            self.skills = self._load_skills(char_class)
            
            print(f"Loading equipment...")
            time.sleep(0.2)
            self.equipment = self._load_equipment(char_class)
            
            self.stats = self._calculate_stats()
    
    def _load_skills(self, char_class):
        skill_map = {
            "warrior": ["slash", "block", "charge"],
            "mage": ["fireball", "ice_shield", "teleport"],
            "archer": ["arrow", "trap", "stealth"],
        }
        return skill_map.get(char_class, [])
    
    def _load_equipment(self, char_class):
        equip_map = {
            "warrior": {"weapon": "sword", "armor": "plate"},
            "mage": {"weapon": "staff", "armor": "robe"},
            "archer": {"weapon": "bow", "armor": "leather"},
        }
        return equip_map.get(char_class, {})
    
    def _calculate_stats(self):
        base_stats = {"hp": 100, "mp": 50, "attack": 10, "defense": 5}
        for stat in base_stats:
            base_stats[stat] += self.level * 5
        return base_stats
    
    def clone(self):
        """深拷贝自身"""
        # 使用 copy.deepcopy 实现深拷贝
        cloned = copy.deepcopy(self)
        return cloned
    
    def __str__(self):
        return (f"Character(name={self.name}, class={self.char_class}, "
                f"level={self.level}, skills={self.skills})")


class CharacterPrototypeRegistry:
    """原型注册表 - 管理预加载的原型"""
    
    def __init__(self):
        self._prototypes: dict[str, GameCharacter] = {}
    
    def register(self, key: str, prototype: GameCharacter):
        """注册原型"""
        self._prototypes[key] = prototype
    
    def unregister(self, key: str):
        """注销原型"""
        del self._prototypes[key]
    
    def clone(self, key: str, **attrs) -> GameCharacter:
        """克隆原型并可选地修改属性"""
        prototype = self._prototypes.get(key)
        if prototype is None:
            raise ValueError(f"Prototype '{key}' not found")
        
        cloned = prototype.clone()
        
        # 应用自定义属性
        for attr, value in attrs.items():
            setattr(cloned, attr, value)
        
        return cloned


# ========== 使用示例 ==========

print("="*50)
print("Initializing prototype registry (one-time cost)")
print("="*50)

# 1. 预加载原型（只做一次）
registry = CharacterPrototypeRegistry()

print("\nLoading Warrior prototype...")
start = time.time()
warrior_proto = GameCharacter("WarriorTemplate", "warrior", 1)
registry.register("warrior", warrior_proto)
print(f"Time: {time.time() - start:.2f}s")

print("\nLoading Mage prototype...")
start = time.time()
mage_proto = GameCharacter("MageTemplate", "mage", 1)
registry.register("mage", mage_proto)
print(f"Time: {time.time() - start:.2f}s")

print("\n" + "="*50)
print("Creating characters from prototypes (fast!)")
print("="*50)

# 2. 从原型快速创建角色
print("\nCloning warriors...")
start = time.time()
warriors = []
for i in range(5):
    warrior = registry.clone("warrior", name=f"Warrior_{i}", level=10+i)
    warriors.append(warrior)
print(f"Created 5 warriors in {time.time() - start:.4f}s")

print("\nCloning mages...")
start = time.time()
mages = []
for i in range(5):
    mage = registry.clone("mage", name=f"Mage_{i}", level=10+i)
    mages.append(mage)
print(f"Created 5 mages in {time.time() - start:.4f}s")

# 3. 验证独立性
print("\n" + "="*50)
print("Verifying independence of cloned objects")
print("="*50)

warrior1 = warriors[0]
warrior2 = warriors[1]

print(f"\nBefore modification:")
print(f"Warrior1: {warrior1}")
print(f"Warrior2: {warrior2}")

# 修改 warrior1 不影响 warrior2
warrior1.skills.append("berserk")
warrior1.equipment["weapon"] = "legendary_sword"

print(f"\nAfter modifying Warrior1:")
print(f"Warrior1: {warrior1}")
print(f"Warrior2: {warrior2}")  # 不受影响

print(f"\nWarrior1 skills: {warrior1.skills}")
print(f"Warrior2 skills: {warrior2.skills}")  # 保持原样


# ========== 浅拷贝 vs 深拷贝示例 ==========

print("\n" + "="*50)
print("Shallow Copy vs Deep Copy Demo")
print("="*50)


class ShallowCloneDemo:
    def __init__(self):
        self.name = "Original"
        self.nested = {"key": "value", "list": [1, 2, 3]}
    
    def shallow_clone(self):
        """浅拷贝 - 嵌套对象仍是引用"""
        return copy.copy(self)
    
    def deep_clone(self):
        """深拷贝 - 完全独立"""
        return copy.deepcopy(self)


original = ShallowCloneDemo()

# 浅拷贝
shallow = original.shallow_clone()
shallow.name = "Shallow"
shallow.nested["key"] = "modified"  # 会影响原对象！

print(f"Original name: {original.name}")
print(f"Original nested: {original.nested}")  # nested["key"] 被修改了！
print(f"Shallow name: {shallow.name}")
print(f"Shallow nested: {shallow.nested}")

# 重置
original = ShallowCloneDemo()

# 深拷贝
deep = original.deep_clone()
deep.name = "Deep"
deep.nested["key"] = "modified"  # 不会影响原对象

print(f"\nOriginal name: {original.name}")
print(f"Original nested: {original.nested}")  # 保持不变
print(f"Deep name: {deep.name}")
print(f"Deep nested: {deep.nested}")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **创建性能** | 每次都重新加载/计算 | 复制已有对象，极快 |
| **资源消耗** | 重复访问数据库/网络 | 只需一次加载，之后复制 |
| **灵活性** | 必须知道具体类和构造参数 | 只需调用 clone()，无需知道具体类 |
| **定制化** | 从零开始配置 | 基于模板修改，保留大部分配置 |
| **独立性** | N/A | 克隆对象与原型完全独立 |

---

## Python 内置支持

```python
import copy

# Python 提供了内置的拷贝支持

class MyClass:
    def __init__(self, value, nested):
        self.value = value
        self.nested = nested
    
    def __copy__(self):
        """自定义浅拷贝行为"""
        print("Custom shallow copy")
        new_obj = type(self).__new__(type(self))
        new_obj.__dict__.update(self.__dict__)
        return new_obj
    
    def __deepcopy__(self, memo):
        """自定义深拷贝行为"""
        print("Custom deep copy")
        new_obj = type(self).__new__(type(self))
        memo[id(self)] = new_obj
        for k, v in self.__dict__.items():
            setattr(new_obj, k, copy.deepcopy(v, memo))
        return new_obj


obj = MyClass(42, {"list": [1, 2, 3]})

shallow = copy.copy(obj)      # 调用 __copy__
deep = copy.deepcopy(obj)     # 调用 __deepcopy__
```

---

## 实际应用：配置模板系统

```python
import copy
from dataclasses import dataclass, field
from typing import Dict, List


@dataclass
class ServerConfig:
    """服务器配置"""
    host: str = "localhost"
    port: int = 8080
    workers: int = 4
    ssl: bool = False
    ssl_cert: str = ""
    ssl_key: str = ""
    headers: Dict[str, str] = field(default_factory=dict)
    allowed_origins: List[str] = field(default_factory=list)
    
    def clone(self):
        return copy.deepcopy(self)


class ConfigTemplates:
    """配置模板管理器"""
    
    def __init__(self):
        # 预定义模板
        self._templates = {
            "development": ServerConfig(
                host="localhost",
                port=3000,
                workers=1,
                ssl=False,
                headers={"X-Debug": "true"},
                allowed_origins=["*"]
            ),
            "staging": ServerConfig(
                host="0.0.0.0",
                port=8080,
                workers=2,
                ssl=True,
                ssl_cert="/etc/ssl/staging.crt",
                ssl_key="/etc/ssl/staging.key",
                headers={"X-Environment": "staging"},
                allowed_origins=["https://staging.example.com"]
            ),
            "production": ServerConfig(
                host="0.0.0.0",
                port=443,
                workers=8,
                ssl=True,
                ssl_cert="/etc/ssl/production.crt",
                ssl_key="/etc/ssl/production.key",
                headers={
                    "X-Environment": "production",
                    "Strict-Transport-Security": "max-age=31536000"
                },
                allowed_origins=["https://example.com", "https://www.example.com"]
            ),
        }
    
    def get_config(self, template_name: str, **overrides) -> ServerConfig:
        """获取配置模板的副本，可选覆盖某些值"""
        if template_name not in self._templates:
            raise ValueError(f"Unknown template: {template_name}")
        
        config = self._templates[template_name].clone()
        
        for key, value in overrides.items():
            if hasattr(config, key):
                setattr(config, key, value)
        
        return config


# 使用
templates = ConfigTemplates()

# 基于生产模板，自定义端口
custom_config = templates.get_config("production", port=8443, workers=16)
print(custom_config)

# 基于开发模板
dev_config = templates.get_config("development", port=5000)
print(dev_config)
```

