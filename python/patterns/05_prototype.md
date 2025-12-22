# Prototype Pattern in Python

---

## 1. Pattern Name: Prototype

**Purpose / Problem Solved:**
Create new objects by copying (cloning) existing objects, rather than creating from scratch. Useful when object creation is expensive or complex, or when you need variations of a base configuration.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                          PROTOTYPE                                |
|------------------------------------------------------------------|
| + clone() -> Prototype                                            |
| + deep_clone() -> Prototype                                       |
+------------------------------------------------------------------+
         ^                    ^                    ^
         |                    |                    |
+----------------+    +----------------+    +----------------+
| ConcreteProto1 |    | ConcreteProto2 |    | ConcreteProto3 |
|----------------|    |----------------|    |----------------|
| - field1       |    | - field1       |    | - field1       |
| - field2       |    | - field2       |    | - field2       |
| + clone()      |    | + clone()      |    | + clone()      |
+----------------+    +----------------+    +----------------+

CLONING PROCESS:
+-------------------+         +-------------------+
|    PROTOTYPE      |  clone  |      CLONE        |
|-------------------|  -----> |-------------------|
| state: {...}      |         | state: {...}      |
| id: 0x1234        |         | id: 0x5678        |
+-------------------+         +-------------------+
       |                             |
       |  Original object            |  New object with
       |  remains unchanged          |  same initial state
       v                             v

SHALLOW vs DEEP COPY:
+-------------------+              +-------------------+
|  Original Object  |              |   Shallow Clone   |
|-------------------|              |-------------------|
| value: 42         |   shallow    | value: 42         |
| list: [1,2,3] ----+----copy----->| list: ----+       |
+-------------------+    |         +-----------+-------+
                         |                     |
                         +---------------------+
                         (same list reference!)

+-------------------+              +-------------------+
|  Original Object  |              |    Deep Clone     |
|-------------------|              |-------------------|
| value: 42         |    deep      | value: 42         |
| list: [1,2,3]     |----copy----->| list: [1,2,3]     |
+-------------------+              +-------------------+
                                   (independent copy)
```

**中文说明：**
原型模式通过克隆现有对象来创建新对象。当对象创建代价高昂（需要数据库查询、网络请求或复杂计算）时，克隆比从头创建更高效。Python的`copy`模块提供了`copy()`（浅拷贝）和`deepcopy()`（深拷贝）。浅拷贝只复制对象本身，内部引用的可变对象仍然共享；深拷贝递归复制所有嵌套对象。选择哪种取决于是否需要完全独立的副本。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Prototype |
|----------------|---------------------------|
| **`copy.copy()`** | Built-in shallow copy |
| **`copy.deepcopy()`** | Built-in deep copy (recursive) |
| **`__copy__()`** | Customize shallow copy behavior |
| **`__deepcopy__(memo)`** | Customize deep copy behavior |
| **`dataclasses.replace()`** | Create copy with modified fields |
| **`dict.copy()`** | Shallow copy for dictionaries |
| **`list.copy()` or `list[:]`** | Shallow copy for lists |
| **`@dataclass(frozen=True)`** | Immutable objects (copy returns new) |
| **Pickle** | Serialize and deserialize for deep copy |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Python copy module** | Standard library for copying objects |
| **NumPy arrays** | `array.copy()` for independent copies |
| **Pandas DataFrame** | `df.copy()` for data manipulation |
| **Django QuerySet** | Querysets are cloned on modification |
| **SQLAlchemy** | `query.filter()` returns new query (prototype-like) |
| **attrs** | `attrs.evolve(obj, field=value)` returns modified copy |

```python
# Pandas example - DataFrame.copy() is prototype pattern
import pandas as pd

original = pd.DataFrame({"a": [1, 2], "b": [3, 4]})
clone = original.copy()  # Deep copy

clone["a"] = [10, 20]  # Modify clone
print(original["a"])   # [1, 2] - original unchanged
```

---

## 5. Python Module Examples

### Example 1: Basic Prototype with copy Module

```python
#!/usr/bin/env python3
"""
Prototype Pattern - Basic Implementation with copy Module

Demonstrates shallow vs deep copy and customization.
"""

import copy
from dataclasses import dataclass, field
from typing import List


@dataclass
class Address:
    """Nested object to demonstrate deep copy."""
    street: str
    city: str
    country: str


@dataclass
class Person:
    """
    Prototype class demonstrating copy behavior.
    
    Uses __copy__ and __deepcopy__ for custom clone logic.
    """
    name: str
    age: int
    address: Address
    tags: List[str] = field(default_factory=list)
    
    def __copy__(self):
        """
        Customize shallow copy.
        
        This is called by copy.copy().
        We can control what gets copied and how.
        """
        print(f"  __copy__ called for {self.name}")
        # Create new Person with same primitive values
        # but SHARE the mutable objects (address, tags)
        return Person(
            name=self.name,
            age=self.age,
            address=self.address,  # Same reference!
            tags=self.tags,        # Same reference!
        )
    
    def __deepcopy__(self, memo):
        """
        Customize deep copy.
        
        This is called by copy.deepcopy().
        memo is a dictionary to handle circular references.
        """
        print(f"  __deepcopy__ called for {self.name}")
        # Create completely independent copy
        return Person(
            name=copy.deepcopy(self.name, memo),
            age=self.age,
            address=copy.deepcopy(self.address, memo),
            tags=copy.deepcopy(self.tags, memo),
        )
    
    def clone(self, deep: bool = True):
        """Convenience method for cloning."""
        if deep:
            return copy.deepcopy(self)
        return copy.copy(self)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Prototype Pattern - Shallow vs Deep Copy")
    print("=" * 60)
    
    # Create original object
    original = Person(
        name="Alice",
        age=30,
        address=Address("123 Main St", "New York", "USA"),
        tags=["developer", "python"]
    )
    print(f"\nOriginal: {original}")
    
    # Shallow copy
    print("\n--- Shallow Copy ---")
    shallow = copy.copy(original)
    print(f"Shallow copy created")
    print(f"  original is shallow: {original is shallow}")
    print(f"  original.address is shallow.address: {original.address is shallow.address}")
    print(f"  original.tags is shallow.tags: {original.tags is shallow.tags}")
    
    # Modify shallow copy's shared reference
    shallow.name = "Bob"
    shallow.address.city = "Los Angeles"  # Modifies BOTH!
    shallow.tags.append("designer")       # Modifies BOTH!
    
    print(f"\nAfter modifying shallow copy:")
    print(f"  original.address.city: {original.address.city}")  # Changed!
    print(f"  original.tags: {original.tags}")  # Changed!
    
    # Reset original for deep copy demo
    original = Person(
        name="Alice",
        age=30,
        address=Address("123 Main St", "New York", "USA"),
        tags=["developer", "python"]
    )
    
    # Deep copy
    print("\n--- Deep Copy ---")
    deep = copy.deepcopy(original)
    print(f"Deep copy created")
    print(f"  original is deep: {original is deep}")
    print(f"  original.address is deep.address: {original.address is deep.address}")
    print(f"  original.tags is deep.tags: {original.tags is deep.tags}")
    
    # Modify deep copy - original is unaffected
    deep.name = "Carol"
    deep.address.city = "Chicago"
    deep.tags.append("manager")
    
    print(f"\nAfter modifying deep copy:")
    print(f"  original.address.city: {original.address.city}")  # Unchanged!
    print(f"  original.tags: {original.tags}")  # Unchanged!
    print(f"  deep.address.city: {deep.address.city}")
    print(f"  deep.tags: {deep.tags}")
    
    # Using the clone method
    print("\n--- Using clone() method ---")
    clone = original.clone(deep=True)
    print(f"Clone: {clone}")
```

---

### Example 2: Prototype Registry

```python
#!/usr/bin/env python3
"""
Prototype Pattern - Registry of Prototypes

Maintains a registry of prototype objects that can be cloned.
Useful for creating variations of base configurations.
"""

import copy
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Dict, Any, Optional, ClassVar


class Prototype(ABC):
    """Abstract base class for prototypes."""
    
    @abstractmethod
    def clone(self) -> "Prototype":
        """Return a clone of this object."""
        pass


@dataclass
class GameCharacter(Prototype):
    """Game character prototype."""
    name: str
    health: int
    attack: int
    defense: int
    skills: list = field(default_factory=list)
    equipment: dict = field(default_factory=dict)
    
    def clone(self) -> "GameCharacter":
        """Deep clone the character."""
        return copy.deepcopy(self)
    
    def __str__(self):
        return (
            f"{self.name}: HP={self.health}, "
            f"ATK={self.attack}, DEF={self.defense}, "
            f"Skills={self.skills}"
        )


class CharacterRegistry:
    """
    Registry of prototype characters.
    
    Stores base prototypes that can be cloned and customized.
    """
    
    _prototypes: ClassVar[Dict[str, GameCharacter]] = {}
    
    @classmethod
    def register(cls, name: str, prototype: GameCharacter) -> None:
        """Register a prototype."""
        cls._prototypes[name] = prototype
        print(f"Registered prototype: {name}")
    
    @classmethod
    def unregister(cls, name: str) -> None:
        """Remove a prototype."""
        del cls._prototypes[name]
    
    @classmethod
    def clone(cls, name: str, **modifications) -> GameCharacter:
        """
        Clone a prototype and optionally modify it.
        
        Args:
            name: Name of registered prototype
            **modifications: Fields to modify after cloning
        """
        if name not in cls._prototypes:
            raise KeyError(f"Prototype '{name}' not found")
        
        # Clone the prototype
        character = cls._prototypes[name].clone()
        
        # Apply modifications
        for key, value in modifications.items():
            if hasattr(character, key):
                setattr(character, key, value)
        
        return character
    
    @classmethod
    def list_prototypes(cls) -> list:
        """List all registered prototypes."""
        return list(cls._prototypes.keys())


# ============== SETUP PROTOTYPES ==============

def setup_character_prototypes():
    """Initialize the prototype registry with base characters."""
    
    # Warrior prototype
    warrior = GameCharacter(
        name="Warrior",
        health=150,
        attack=25,
        defense=20,
        skills=["Slash", "Block", "Charge"],
        equipment={"weapon": "sword", "armor": "plate"}
    )
    CharacterRegistry.register("warrior", warrior)
    
    # Mage prototype
    mage = GameCharacter(
        name="Mage",
        health=80,
        attack=35,
        defense=10,
        skills=["Fireball", "Ice Shard", "Teleport"],
        equipment={"weapon": "staff", "armor": "robe"}
    )
    CharacterRegistry.register("mage", mage)
    
    # Rogue prototype
    rogue = GameCharacter(
        name="Rogue",
        health=100,
        attack=30,
        defense=15,
        skills=["Backstab", "Stealth", "Poison"],
        equipment={"weapon": "daggers", "armor": "leather"}
    )
    CharacterRegistry.register("rogue", rogue)
    
    # Tank prototype (derived from warrior)
    tank = CharacterRegistry.clone(
        "warrior",
        name="Tank",
        health=200,
        attack=15,
        defense=35
    )
    tank.skills.append("Taunt")
    CharacterRegistry.register("tank", tank)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Prototype Pattern - Character Registry")
    print("=" * 60)
    
    # Setup prototypes
    print("\n--- Registering Prototypes ---")
    setup_character_prototypes()
    
    print(f"\nAvailable prototypes: {CharacterRegistry.list_prototypes()}")
    
    # Clone and customize characters
    print("\n--- Creating Characters ---")
    
    # Create warrior named "Conan"
    conan = CharacterRegistry.clone("warrior", name="Conan the Barbarian")
    print(f"Player 1: {conan}")
    
    # Create mage named "Gandalf"
    gandalf = CharacterRegistry.clone("mage", name="Gandalf", health=100)
    print(f"Player 2: {gandalf}")
    
    # Create rogue with extra skill
    shadow = CharacterRegistry.clone("rogue", name="Shadow")
    shadow.skills.append("Smoke Bomb")
    print(f"Player 3: {shadow}")
    
    # Verify prototypes unchanged
    print("\n--- Verify Prototypes Unchanged ---")
    original_rogue = CharacterRegistry.clone("rogue")
    print(f"Original Rogue skills: {original_rogue.skills}")
    print(f"Shadow skills: {shadow.skills}")
    
    # Create an army of identical soldiers
    print("\n--- Creating Army (Multiple Clones) ---")
    soldier_template = GameCharacter(
        name="Soldier",
        health=100,
        attack=20,
        defense=15,
        skills=["Attack"],
        equipment={"weapon": "spear", "armor": "chainmail"}
    )
    
    army = [soldier_template.clone() for _ in range(5)]
    for i, soldier in enumerate(army):
        soldier.name = f"Soldier #{i+1}"
    
    for soldier in army:
        print(f"  {soldier.name}: HP={soldier.health}")
    
    # Modify one soldier, others unaffected
    army[0].health = 50
    print(f"\nAfter damaging Soldier #1:")
    for soldier in army:
        print(f"  {soldier.name}: HP={soldier.health}")
```

---

### Example 3: Document Templates (Practical Example)

```python
#!/usr/bin/env python3
"""
Prototype Pattern - Document Templates

Uses prototypes for document templates that can be 
cloned and customized.
"""

import copy
from dataclasses import dataclass, field
from datetime import datetime
from typing import List, Optional
from abc import ABC


@dataclass
class DocumentSection:
    """A section in a document."""
    title: str
    content: str
    subsections: List["DocumentSection"] = field(default_factory=list)


@dataclass
class Document:
    """
    Document prototype.
    
    Complex object that benefits from cloning rather than
    building from scratch each time.
    """
    title: str
    author: str
    created_at: datetime
    sections: List[DocumentSection] = field(default_factory=list)
    metadata: dict = field(default_factory=dict)
    
    def clone(self) -> "Document":
        """Create a deep clone with updated timestamp."""
        clone = copy.deepcopy(self)
        clone.created_at = datetime.now()
        return clone
    
    def add_section(self, title: str, content: str) -> "Document":
        """Add a section (fluent API)."""
        self.sections.append(DocumentSection(title, content))
        return self
    
    def set_metadata(self, key: str, value) -> "Document":
        """Set metadata (fluent API)."""
        self.metadata[key] = value
        return self
    
    def __str__(self):
        lines = [
            f"Title: {self.title}",
            f"Author: {self.author}",
            f"Created: {self.created_at}",
            f"Sections: {len(self.sections)}",
            f"Metadata: {self.metadata}",
        ]
        for section in self.sections:
            lines.append(f"  - {section.title}")
        return "\n".join(lines)


class DocumentTemplates:
    """Registry of document templates."""
    
    _templates: dict = {}
    
    @classmethod
    def register(cls, name: str, template: Document):
        """Register a document template."""
        cls._templates[name] = template
    
    @classmethod
    def create(cls, template_name: str, **overrides) -> Document:
        """Create a new document from a template."""
        if template_name not in cls._templates:
            raise KeyError(f"Template '{template_name}' not found")
        
        doc = cls._templates[template_name].clone()
        
        for key, value in overrides.items():
            if hasattr(doc, key):
                setattr(doc, key, value)
        
        return doc


# ============== SETUP TEMPLATES ==============

def setup_document_templates():
    """Create standard document templates."""
    
    # Project Proposal Template
    proposal = Document(
        title="Project Proposal Template",
        author="Template System",
        created_at=datetime.now(),
    )
    proposal.add_section("Executive Summary", "Brief overview of the project...")
    proposal.add_section("Objectives", "What the project aims to achieve...")
    proposal.add_section("Timeline", "Project milestones and deadlines...")
    proposal.add_section("Budget", "Estimated costs and resources...")
    proposal.add_section("Risk Analysis", "Potential risks and mitigation...")
    proposal.set_metadata("category", "business")
    proposal.set_metadata("version", "1.0")
    
    DocumentTemplates.register("proposal", proposal)
    
    # Technical Report Template
    report = Document(
        title="Technical Report Template",
        author="Template System",
        created_at=datetime.now(),
    )
    report.add_section("Abstract", "Summary of findings...")
    report.add_section("Introduction", "Background and context...")
    report.add_section("Methodology", "Approach and methods used...")
    report.add_section("Results", "Data and findings...")
    report.add_section("Discussion", "Analysis and interpretation...")
    report.add_section("Conclusion", "Summary and recommendations...")
    report.add_section("References", "Sources cited...")
    report.set_metadata("category", "technical")
    report.set_metadata("peer_reviewed", False)
    
    DocumentTemplates.register("technical_report", report)
    
    # Meeting Minutes Template
    minutes = Document(
        title="Meeting Minutes Template",
        author="Template System",
        created_at=datetime.now(),
    )
    minutes.add_section("Attendees", "List of participants...")
    minutes.add_section("Agenda Items", "Topics discussed...")
    minutes.add_section("Decisions Made", "Key decisions...")
    minutes.add_section("Action Items", "Tasks assigned...")
    minutes.add_section("Next Meeting", "Date and agenda for next meeting...")
    minutes.set_metadata("category", "administrative")
    
    DocumentTemplates.register("meeting_minutes", minutes)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Prototype Pattern - Document Templates")
    print("=" * 60)
    
    # Setup templates
    setup_document_templates()
    
    # Create documents from templates
    print("\n--- Creating Project Proposal ---")
    proposal = DocumentTemplates.create(
        "proposal",
        title="New Product Launch Proposal",
        author="Alice Johnson"
    )
    print(proposal)
    
    print("\n--- Creating Technical Report ---")
    report = DocumentTemplates.create(
        "technical_report",
        title="Performance Analysis Q4 2024",
        author="Bob Smith"
    )
    report.set_metadata("peer_reviewed", True)
    print(report)
    
    print("\n--- Creating Meeting Minutes ---")
    minutes = DocumentTemplates.create(
        "meeting_minutes",
        title="Weekly Standup - Dec 20, 2024",
        author="Carol Davis"
    )
    print(minutes)
    
    # Customize a cloned document
    print("\n--- Customizing Cloned Document ---")
    custom = DocumentTemplates.create("proposal", author="David Lee")
    custom.title = "Custom Proposal: AI Integration"
    custom.sections[0].content = "This proposal outlines AI integration plans..."
    custom.add_section("Technical Requirements", "Hardware and software needs...")
    print(custom)
    
    # Verify template unchanged
    print("\n--- Verify Template Unchanged ---")
    original = DocumentTemplates.create("proposal")
    print(f"Original section count: {len(original.sections)}")
    print(f"Custom section count: {len(custom.sections)}")
```

---

### Example 4: Configuration Variants

```python
#!/usr/bin/env python3
"""
Prototype Pattern - Configuration Variants

Creates configuration variants by cloning and modifying
a base configuration.
"""

import copy
from dataclasses import dataclass, field, replace
from typing import List, Optional


@dataclass
class ServerConfig:
    """Immutable server configuration."""
    host: str
    port: int
    workers: int
    debug: bool
    ssl_enabled: bool
    database_url: str
    cache_size: int
    features: frozenset = field(default_factory=frozenset)
    
    def with_changes(self, **changes) -> "ServerConfig":
        """
        Create a new config with specified changes.
        
        Uses dataclasses.replace for immutable update.
        """
        return replace(self, **changes)
    
    def enable_feature(self, feature: str) -> "ServerConfig":
        """Enable a feature, returning new config."""
        new_features = self.features | {feature}
        return replace(self, features=new_features)
    
    def disable_feature(self, feature: str) -> "ServerConfig":
        """Disable a feature, returning new config."""
        new_features = self.features - {feature}
        return replace(self, features=new_features)
    
    def scale(self, factor: float) -> "ServerConfig":
        """Scale workers and cache, returning new config."""
        return replace(
            self,
            workers=max(1, int(self.workers * factor)),
            cache_size=int(self.cache_size * factor)
        )


class ConfigurationManager:
    """Manages configuration prototypes and variants."""
    
    def __init__(self):
        self._base_configs: dict = {}
    
    def register_base(self, name: str, config: ServerConfig):
        """Register a base configuration."""
        self._base_configs[name] = config
    
    def create_variant(self, base_name: str, **changes) -> ServerConfig:
        """Create a variant from a base configuration."""
        if base_name not in self._base_configs:
            raise KeyError(f"Base config '{base_name}' not found")
        
        base = self._base_configs[base_name]
        return base.with_changes(**changes)
    
    def get_base(self, name: str) -> ServerConfig:
        """Get a copy of a base configuration."""
        return copy.deepcopy(self._base_configs[name])


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Prototype Pattern - Configuration Variants")
    print("=" * 60)
    
    manager = ConfigurationManager()
    
    # Base development config
    dev_config = ServerConfig(
        host="localhost",
        port=8000,
        workers=1,
        debug=True,
        ssl_enabled=False,
        database_url="sqlite:///dev.db",
        cache_size=100,
        features=frozenset(["hot_reload", "debug_toolbar"])
    )
    manager.register_base("development", dev_config)
    
    # Base production config
    prod_config = ServerConfig(
        host="0.0.0.0",
        port=443,
        workers=8,
        debug=False,
        ssl_enabled=True,
        database_url="postgres://prod:5432/app",
        cache_size=10000,
        features=frozenset(["monitoring", "rate_limiting"])
    )
    manager.register_base("production", prod_config)
    
    print("\n--- Development Config ---")
    print(f"Dev: {dev_config}")
    
    print("\n--- Production Config ---")
    print(f"Prod: {prod_config}")
    
    # Create variants
    print("\n--- Creating Variants ---")
    
    # Staging: like production but different port and DB
    staging = manager.create_variant(
        "production",
        port=8443,
        database_url="postgres://staging:5432/app",
        workers=4
    )
    print(f"Staging: host={staging.host}, port={staging.port}, workers={staging.workers}")
    
    # High-load production: scaled up
    high_load = prod_config.scale(2.0)
    print(f"High-load: workers={high_load.workers}, cache={high_load.cache_size}")
    
    # Feature toggle
    print("\n--- Feature Toggles ---")
    with_analytics = prod_config.enable_feature("analytics")
    print(f"With analytics: {with_analytics.features}")
    
    without_monitoring = with_analytics.disable_feature("monitoring")
    print(f"Without monitoring: {without_monitoring.features}")
    
    # Chaining changes
    print("\n--- Chaining Changes ---")
    custom = (
        dev_config
        .with_changes(port=9000)
        .enable_feature("profiler")
        .scale(2.0)
    )
    print(f"Custom dev: port={custom.port}, workers={custom.workers}")
    print(f"Features: {custom.features}")
    
    # Verify immutability
    print("\n--- Verify Immutability ---")
    print(f"Original dev port: {dev_config.port}")
    print(f"Original dev workers: {dev_config.workers}")
    print(f"Original dev features: {dev_config.features}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- Object creation is **expensive** (DB queries, network calls, complex computation)
- You need **variations** of a base object
- Classes are determined at **runtime**
- You want to avoid a **hierarchy of factories**
- Objects are mostly **similar** with small differences

### Avoid When:
- Objects are simple and cheap to create
- Each object needs unique complex initialization
- Deep object graphs with circular references (tricky to clone)

### Python Idiom:
Use `dataclasses.replace()` for immutable prototypes:

```python
from dataclasses import dataclass, replace

@dataclass(frozen=True)
class Point:
    x: int
    y: int

p1 = Point(1, 2)
p2 = replace(p1, x=10)  # Point(x=10, y=2)
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Factory Method** | Alternative - create new vs clone existing |
| **Abstract Factory** | Can use prototypes instead of factory methods |
| **Memento** | Similar mechanism for saving/restoring state |
| **Composite** | Often cloned as tree structures |

