# Pattern 23: Interpreter

## 1. Problem the Pattern Solves

### Design Pressure
- Need to evaluate a language or grammar
- Expressions are represented as trees
- Adding new operations on expressions

### What Goes Wrong Without It
```cpp
// Without interpreter: switch-based evaluation
double eval(Expr* e) {
    switch (e->type) {
        case ADD: return eval(e->left) + eval(e->right);
        case MUL: return eval(e->left) * eval(e->right);
        // Every new type = modify this
    }
}
```

---

## 2. Core Idea (C++-Specific)

**Interpreter defines a representation for a language's grammar along with an interpreter that uses the representation to interpret sentences.**

```
         +------------+
         | Expression |
         | interpret()|
         +------------+
              ^
     +--------+--------+
     |                 |
+----------+     +-------------+
| Terminal |     | NonTerminal |
| (Number) |     | (Add, Mul)  |
+----------+     +-------------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Virtual method | `interpret()` | Polymorphic evaluation |
| `std::variant` | Modern AST | Type-safe nodes |
| `std::visit` | Evaluation | Pattern matching |

---

## 4. Canonical C++ Implementation

### Classic OOP

```cpp
#include <memory>
#include <iostream>
#include <map>

class Context {
public:
    std::map<std::string, int> variables;
};

class Expression {
public:
    virtual ~Expression() = default;
    virtual int interpret(Context& ctx) = 0;
};

class Number : public Expression {
public:
    explicit Number(int value) : value_(value) {}
    int interpret(Context&) override { return value_; }
private:
    int value_;
};

class Variable : public Expression {
public:
    explicit Variable(std::string name) : name_(std::move(name)) {}
    int interpret(Context& ctx) override { return ctx.variables[name_]; }
private:
    std::string name_;
};

class Add : public Expression {
public:
    Add(std::unique_ptr<Expression> l, std::unique_ptr<Expression> r)
        : left_(std::move(l)), right_(std::move(r)) {}
    int interpret(Context& ctx) override {
        return left_->interpret(ctx) + right_->interpret(ctx);
    }
private:
    std::unique_ptr<Expression> left_, right_;
};

class Multiply : public Expression {
public:
    Multiply(std::unique_ptr<Expression> l, std::unique_ptr<Expression> r)
        : left_(std::move(l)), right_(std::move(r)) {}
    int interpret(Context& ctx) override {
        return left_->interpret(ctx) * right_->interpret(ctx);
    }
private:
    std::unique_ptr<Expression> left_, right_;
};

int main() {
    // (x + 3) * 5
    auto expr = std::make_unique<Multiply>(
        std::make_unique<Add>(
            std::make_unique<Variable>("x"),
            std::make_unique<Number>(3)
        ),
        std::make_unique<Number>(5)
    );
    
    Context ctx;
    ctx.variables["x"] = 7;
    
    std::cout << expr->interpret(ctx) << "\n";  // (7+3)*5 = 50
    return 0;
}
```

### Modern: `std::variant`

```cpp
#include <variant>
#include <memory>

struct Number { int value; };
struct Variable { std::string name; };
struct Add;
struct Multiply;

using Expr = std::variant<
    Number, 
    Variable,
    std::unique_ptr<Add>,
    std::unique_ptr<Multiply>
>;

struct Add { Expr left, right; };
struct Multiply { Expr left, right; };

int eval(const Expr& e, std::map<std::string, int>& ctx) {
    return std::visit([&](const auto& node) -> int {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, Number>) {
            return node.value;
        } else if constexpr (std::is_same_v<T, Variable>) {
            return ctx[node.name];
        } else {
            return eval(node->left, ctx) + eval(node->right, ctx);  // etc
        }
    }, e);
}
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Compilers | Expression evaluation |
| Databases | SQL parsing |
| Config | DSL interpretation |
| Games | Scripting languages |

---

## 6. When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| Complex grammar | Parser generators (ANTLR) |
| Performance critical | Bytecode compilation |

---

## 7. Mental Model Summary

**When Interpreter "Clicks":**

Use Interpreter for **simple grammars** where the language is stable and **expression trees** naturally represent the structure. For complex languages, use parser generators.

---

## 中文说明

### 解释器模式要点

1. **核心结构**：
   - 终结符表达式（数字、变量）
   - 非终结符表达式（加法、乘法）

2. **两种实现**：
   - 经典 OOP：虚函数 interpret()
   - 现代 C++：std::variant + std::visit

3. **适用场景**：
   - 简单表达式语言
   - DSL 解释
   - SQL 解析

4. **不适用场景**：
   - 复杂语法（用解析器生成器）
   - 性能关键（编译成字节码）

