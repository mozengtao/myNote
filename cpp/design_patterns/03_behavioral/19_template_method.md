# Pattern 19: Template Method

## 1. Problem the Pattern Solves

### Design Pressure
- Algorithm structure is fixed, but some steps vary
- Avoid code duplication across similar algorithms
- Control extension points for subclasses

### What Goes Wrong Without It
```cpp
// Copy-paste with minor variations
void DataProcessor1::process() {
    open();
    readData();    // Same
    validate();    // Same
    transform1();  // Different
    save();        // Same
}
void DataProcessor2::process() {
    open();
    readData();    // Same
    validate();    // Same
    transform2();  // Different
    save();        // Same
}
```

---

## 2. Core Idea (C++-Specific)

**Template Method defines the skeleton of an algorithm in a base class, letting subclasses override specific steps without changing the structure.**

```
+-------------------+
| AbstractClass     |
| templateMethod()  |  ← Calls primitiveOps in order
| primitiveOp1()    |  ← Hook or abstract
| primitiveOp2()    |  ← Hook or abstract
+-------------------+
         ^
         |
+-------------------+
| ConcreteClass     |
| primitiveOp1()    |  ← Override
| primitiveOp2()    |  ← Override
+-------------------+
```

Often combined with **Non-Virtual Interface (NVI)** idiom in C++.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `virtual` | Customization points | Override in subclass |
| `= 0` (pure virtual) | Required override | Force implementation |
| `protected` | Limit access | Subclass-only override |
| `final` | Prevent override | Lock step implementation |
| `private virtual` | NVI pattern | Strict control |

---

## 4. Canonical C++ Implementation

### Classic Template Method

```cpp
#include <iostream>
#include <string>

class DataMiner {
public:
    // Template method - defines algorithm structure
    void mine(const std::string& path) {
        openFile(path);
        extractData();
        parseData();
        analyzeData();
        generateReport();
        closeFile();
    }
    
    virtual ~DataMiner() = default;
    
protected:
    // Hooks with default implementation
    virtual void openFile(const std::string& path) {
        std::cout << "Opening " << path << "\n";
    }
    
    virtual void closeFile() {
        std::cout << "Closing file\n";
    }
    
    // Abstract methods - must override
    virtual void extractData() = 0;
    virtual void parseData() = 0;
    
    // Optional hook
    virtual void analyzeData() {
        std::cout << "Default analysis\n";
    }
    
    void generateReport() {  // Fixed step - not virtual
        std::cout << "Generating report\n";
    }
};

class CSVDataMiner : public DataMiner {
protected:
    void extractData() override {
        std::cout << "Extracting CSV data\n";
    }
    
    void parseData() override {
        std::cout << "Parsing CSV format\n";
    }
};

class PDFDataMiner : public DataMiner {
protected:
    void extractData() override {
        std::cout << "Extracting PDF text\n";
    }
    
    void parseData() override {
        std::cout << "Parsing PDF structure\n";
    }
    
    void analyzeData() override {
        std::cout << "PDF-specific analysis\n";
    }
};

int main() {
    CSVDataMiner csv;
    csv.mine("data.csv");
    
    std::cout << "\n";
    
    PDFDataMiner pdf;
    pdf.mine("report.pdf");
    
    return 0;
}
```

### NVI (Non-Virtual Interface) Style

```cpp
class Sorter {
public:
    // Public non-virtual interface
    void sort(std::vector<int>& data) {
        preProcess(data);
        doSort(data);
        postProcess(data);
    }
    
    virtual ~Sorter() = default;
    
private:
    // Private virtual customization points
    virtual void preProcess(std::vector<int>&) {}
    virtual void doSort(std::vector<int>& data) = 0;
    virtual void postProcess(std::vector<int>&) {}
};

class QuickSorter : public Sorter {
private:
    void doSort(std::vector<int>& data) override {
        std::sort(data.begin(), data.end());
    }
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Frameworks | Application lifecycle |
| Testing | Setup/teardown in test frameworks |
| I/O | File processing pipelines |
| Games | Game loop (input, update, render) |

---

## 6. Common Mistakes

### ❌ Template Method Calls Public Virtual Methods

```cpp
// BAD: Subclass can break algorithm by calling steps out of order
class Base {
public:
    virtual void step1();  // Public = callable directly
    void algorithm() { step1(); step2(); }
};
// Client calls: obj.step1();  // Bypasses algorithm!
// FIX: Make virtual methods protected or private (NVI)
```

---

## 7. Template Method vs Strategy

| Aspect | Template Method | Strategy |
|--------|-----------------|----------|
| Mechanism | Inheritance | Composition |
| Granularity | Override steps | Replace algorithm |
| Compile vs Runtime | Compile-time binding | Runtime binding |

---

## 8. Mental Model Summary

**When Template Method "Clicks":**

Use Template Method when you have an algorithm with **fixed structure but variable steps**. Put the structure in the base class, let subclasses customize specific steps. Use NVI for better encapsulation.

---

## 中文说明

### 模板方法模式要点

1. **核心结构**：
   - 基类定义算法骨架
   - 子类覆盖可变步骤

2. **三种钩子**：
   - 抽象方法：必须覆盖
   - 带默认实现的虚方法：可选覆盖
   - 非虚方法：不可覆盖

3. **NVI 惯用法**：
   - 公共方法非虚
   - 私有/保护虚方法作为定制点

4. **与策略模式区别**：
   - 模板方法：继承，覆盖步骤
   - 策略：组合，替换整个算法

