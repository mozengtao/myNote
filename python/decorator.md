[Python 装饰器与 *args / **kwargs 深度解析](python_decorator.md)  
[]()  
[]()  
[]()  


## property decorator
```python
# @property decorator
class Circle:
    def __init__(self, radius):
        print("__init__")
        self.radius = radius

    @property
    def radius(self):
        """The radius property"""
        print("Get radius")
        return self._radius
    
    @radius.setter
    def radius(self, value):
        print("Set radius")
        self._radius = value
    
    @radius.deleter
    def radius(self):
        print("Delete radius")
        del self._radius
    
    @property
    def area(self):
        print("Get area")
        return 3.14 * self.radius ** 2

circle = Circle(10)
circle.radius = 20
print(circle.radius)
del circle.radius
#print(circle.area) # error

# providing read-only attributes
class Point:
    def __init__(self, x, y):
        self._x = x
        self._y = y

    @property
    def x(self):
        return self._x

    @x.setter
    def x(self, value):
        raise WriteOnlyError("x is read-only")

    @property
    def y(self):
        return self._y

    @y.setter
    def y(self, value):
        raise WriteOnlyError("y is read-only")

point = Point(10, 20)
print(point.x)
point.x = 30 # error

# creating read-write attributes
class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    def radius(self):
        return self._radius

    @radius.setter
    def radius(self, value):
        self._radius = float(value)

    @property
    def diameter(self):
        return self.radius * 2

    @diameter.setter
    def diameter(self, value):
        self.radius = value / 2

circle = Circle(42)
print(circle.radius)
print(circle.diameter)
circle.diameter = 100
print(circle.diameter)
print(circle.radius)

# providing write-only attributes
import hashlib
import os

class User:
    def __init__(self, name, password):
        self.name = name
        self.password = password

    @property
    def password(self):
        raise AttributeError("Password is write-only")

    @password.setter
    def password(self, plaintext):
        salt = os.urandom(32)
        self._hashed_password = hashlib.pbkdf2_hmac(
            "sha256", plaintext.encode("utf-8"), salt, 100_000
        )

john = User("John", "secret")
print(john._hashed_password)
john.password = "new_password"
print(john._hashed_password)

# example 1
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    @property
    def x(self):
        return self._x

    @x.setter
    def x(self, value):
        try:
            self._x = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError('"x" must be a number') from None

    @property
    def y(self):
        return self._y

    @y.setter
    def y(self, value):
        try:
            self._y = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError('"y" must be a number') from None

point = Point(12, 5)
print(point.x)
print(point.y)
point.x = "twelve" # error
point.y = "five" # error

# example 1 inheritance
class Coordinate:
    def __set_name__(self, owner, name):
        self._name = name

    def __get__(self, instance, owner):
        return instance.__dict__[self._name]

    def __set__(self, instance, value):
        try:
            instance.__dict__[self._name] = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError(f'"{self._name}" must be a number') from None

class Point:
    x = Coordinate()
    y = Coordinate()

    def __init__(self, x, y):
        self.x = x
        self.y = y

# example 2
import math

class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    @property
    def distance(self):
        return math.dist((0, 0), (self.x, self.y))

    @property
    def angle(self):
        return math.degrees(math.atan2(self.y, self.x))

    def as_cartesian(self):
        return self.x, self.y

    def as_polar(self):
        return self.distance, self.angle

point = Point(12, 5)
print(point.x)
print(point.y)
print(point.distance)
print(point.angle)
print(point.as_cartesian())
print(point.as_polar())

# caching computed attributes
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    def radius(self):
        return self._radius

    @radius.setter
    def radius(self, value):
        self._diameter = None
        self._radius = value

    @property
    def diameter(self):
        if self._diameter is None:
            sleep(0.5)  # Simulate a costly computation
            self._diameter = self._radius * 2
        return self._diameter

circle = Circle(42.0)

print(circle.radius)
print(circle.diameter)  # With delay
print(circle.diameter)  # Without delay
circle.radius = 100.0
print(circle.diameter)  # With delay
print(circle.diameter)  # Without delay

# enhancement
from functools import cached_property
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @cached_property
    def diameter(self):
        sleep(0.5)  # Simulate a costly computation
        return self.radius * 2

# enhancement
from functools import cache
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    @cache
    def diameter(self):
        sleep(0.5) # Simulate a costly computation
        return self.radius * 2

# Logging Attribute Access and Mutation
import logging

logging.basicConfig(
    format="%(asctime)s: %(message)s",
    level=logging.INFO,
    datefmt="%H:%M:%S"
)

class Circle:
    def __init__(self, radius):
        self._msg = '"radius" was %s. Current value: %s'
        self.radius = radius

    @property
    def radius(self):
        logging.info(self._msg % ("accessed", str(self._radius)))
        return self._radius

    @radius.setter
    def radius(self, value):
        try:
            self._radius = float(value)
            logging.info(self._msg % ("mutated", str(self._radius)))
        except ValueError:
            logging.info('validation error while mutating "radius"')

circle = Circle(42.0)
print(circle.radius)
circle.radius = 100
print(circle.radius)
circle.radius = "value" # error

# Managing Attribute Deletion
class TreeNode:
    def __init__(self, data):
        self._data = data
        self._children = []

    @property
    def children(self):
        return self._children

    @children.setter
    def children(self, value):
        if isinstance(value, list):
            self._children = value
        else:
            del self.children
            self._children.append(value)

    @children.deleter
    def children(self):
        self._children.clear()

    def __repr__(self):
        return f'{self.__class__.__name__}("{self._data}")'

root = TreeNode("root")
child1 = TreeNode("child 1")
child2 = TreeNode("child 2")
root.children = [child1, child2]
print(root.children)
del root.children
print(root.children)

#Creating Backward-Compatible Class APIs
```