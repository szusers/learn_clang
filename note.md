# 优秀实践：面向对象设计模式中的访问者模式
**核心目的：将 数据结构（例如汽车及其零件）与 作用于结构上的操作分离开来。**
本质上，访问者模式允许向类族（访问者）添加新的虚函数 ，而无需修改类本身。它通过**创建一个访问者类**来实现**所有相应的虚函数特化**。访问者模式**以实例引用作为输入**，并通过**双重分派**来实现目标。

代码主要分为两个部分：被访问的元素（Elements，主要为器（零）件或者不同类别的组件） 和 访问者（Visitors，**主要为实现不同功能（虚函数）的集合，是组建虚函数实现的集合体**）。

A. 被访问的元素（汽车零件）
- CarElement: 这是一个抽象基类（接口），规定所有零件必须有一个 accept 方法。
- 具体零件 (Body, Engine, Wheel): 它们只存储自己的数据（比如 Wheel 的名字），并实现 accept 方法。它们的 accept 方法非常简单：直接告诉访问者“请访问我这个类型的零件”。
- Car: 这是一个组合体，它包含了所有的零件。当它 accept 访问者时，它会先让所有子零件去接待访问者，最后自己再接待。
B. 访问者（对零件执行的操作）
- CarElementVisitor: 这是一个抽象基类，定义了访问每一种零件时应该调用的接口（visit_body, visit_wheel 等）。
- CarElementPrintVisitor: 具体操作一。它的逻辑是“打印”零件的信息。
- CarElementDoVisitor: 具体操作二。它的逻辑是“模拟动作”（如踢轮子、启动引擎）。

## 目的为：由被访问元素自己确定应该调用访问者中的哪些对应操作。被访问者提供一个统一的接口，然后然后不同的访问者提供这个接口的不同实现。
如果你想给汽车增加一个新的功能，比如 “清洗汽车零件” (CarElementWashVisitor)：
- 传统做法：你需要修改 Body 类增加 wash()，修改 Engine 类增加 wash()... 这违反了“开闭原则”。
- 访问者模式：你不需要修改任何零件类。你只需要新建一个 CarElementWashVisitor 类，在里面写上洗轮子、洗引擎的逻辑即可（**在被访问者提供的统一的接口visit_car, visit_wheel, visit_engine中实现这个功能类自己的逻辑即可**）

```python
"""
Visitor pattern example.
"""

from abc import ABCMeta, abstractmethod
from typing import NoReturn

NOT_IMPLEMENTED: str = "You should implement this."

class CarElement(metaclass = ABCMeta):
    @abstractmethod
    def accept(self, visitor: CarElementVisitor) -> NoRe:
        visitor.visit_engine(self)

class Wheel(CarElement):
    def __init__(self, name: str) -> None:
        self.name = name

    def accept(self, visitor: CarElementVisitor) -> None:
        visitor.visit_wheel(self)

class Car(CarElement):
    def __init__(self) -> None:
        self.elements: list[CarElement] = [
            Wheel("front left"), 
            Wheel("front right"),
            Wheel("back left"), 
            Wheel("back right"),
            Body(), 
            Engine()
        ]

    def accept(self, visitor):
        for element in self.elements:
            element.accept(visitor)
        visitor.visit_car(self)

class CarElementVisitor(metaclass = ABCMeta):
    @abstractmethod
    def visit_body(self, element: CarElement) -> NoReturn:
        raise NotImplementedError(NOT_IMPLEMENTED)

    @abstractmethod
    def visit_engine(self, element: CarElement) -> NoReturn:
        raise NotImplementedError(NOT_IMPLEMENTED)

    @abstractmethod
    def visit_wheel(self, element: CarElement) -> NoReturn:
        raise NotImplementedError(NOT_IMPLEMENTED)

    @abstractmethod
    def visit_car(self, element: CarElement) -> NoReturn:
        raise NotImplementedError(NOT_IMPLEMENTED)

class CarElementDoVisitor(CarElementVisitor):
    def visit_body(self, body: Body) -> None:
        print("Moving my body.")

    def visit_car(self, car: Car) -> None:
        print("Starting my car.")

    def visit_wheel(self, wheel: Wheel) -> None:
        print(f"Kicking my {wheel.name} wheel.")

    def visit_engine(self, engine: Engine) -> None:
        print("Starting my engine.")

class CarElementPrintVisitor(CarElementVisitor):
    def visit_body(self, body: Body) -> None:
        print("Visiting body.")

    def visit_car(self, car: Car) -> None:
        print("Visiting car.")

    def visit_wheel(self, wheel: Wheel) -> None:
        print(f"Visiting {wheel.name} wheel.")

    def visit_engine(self, engine: Engine) -> None:
        print("Visiting engine.")

if __name__ == "__main__":
    car: Car = Car()
    car.accept(CarElementPrintVisitor())
    car.accept(CarElementDoVisitor())
```


## 进一步思考：和c++虚函数之间的分发差异：（虚函数是单分派，只由被调用者的类型确定需要调用的函数是哪个。 访问者模式是双分派，最终的调用结果由被访问者类型以及传入的访问者类型同时确定）

在 C++ 中，简单的虚函数（单分派）和访问者模式（双分派）的区别可以总结为：**你是想方便地“增加新的类”，还是想方便地“增加新的操作”？**

为了让你彻底明白，我们对比一下两种设计方案：

### 方案 A：纯虚函数方案（简单多态）
假设我们在 `CarElement` 基类里定义所有动作：

```cpp
class CarElement {
public:
    virtual void print() = 0;
    virtual void doAction() = 0;
    // virtual void wash() = 0; // 以后想加个“洗车”功能？
};

class Wheel : public CarElement {
    void print() override { cout << "Visiting wheel"; }
    void doAction() override { cout << "Kicking wheel"; }
    // void wash() override { cout << "Washing car"; } // 如果要加上洗车功能的话所有的子类都需要同时加上一个具体的实现，很麻烦
};
```

*   **优点（增加类很容易）**：如果你想增加一种新的零件 `Window`，你只需要写一个 `Window` 类并实现 `print` 和 `doAction`。不需要改动现有代码。
*   **缺点（增加操作很痛苦）**：如果逻辑
class CarElement {
public:
    virtual void accept(CarElementVisitor& v) = 0;
};

class Wheel : public CarElement {
public:
    void accept(CarElementVisitor& v) override { v.visitWheel(*this); }
};
```

*   **优点（增加操作很容易）**：如果你想增加“洗车（`wash`）”功能，你**不需要修改零件类**。你只需要写一个新类 `CarWashVisitor` 继承自 `CarElementVisitor` 即可（在visitWheel这个虚函数中实现具体的洗车逻辑）。
*   **缺点（增加类很痛苦）**：如果你增加一个零件 `Window`，你需要修改 `CarElementVisitor` 接口，导致所有的 Visitor 实现类都要跟着改。

---

### 为什么说虚函数不“完美”对应？

C++ 的虚函数属于 **单分派（Single Dispatch）**。
当你执行 `ptr->func()` 时，程序只根据 `ptr` **指向的对象类型**来决定调用哪个函数。

而访问者模式实现了 **双分派（Double Dispatch）**。
当你执行 `element->accept(visitor)` 时，最终执行哪个逻辑取决于**两个**因素：
1.  **Element 的类型**（是 Wheel 还是 Engine？）
2.  **Visitor 的类型**（是 Print 还是 DoAction？）

### 总结对比

| 维度 | 简单虚函数 (Polymorphism) | 访问者模式 (Visitor Pattern) |
| :--- | :--- | :--- |
| **核心机制** | 单分派 (根据对象类型分发) | 双分派 (根据对象类型+访问者类型分发) |
| **擅长场景** | 经常需要增加**新的零件类型** (子类) | 经常需要增加**新的功能操作** (方法) |
| **对零件类的污染** | 逻辑全写在零件类里，类会变得很臃肿 | 零件类很干净，逻辑被抽离到了 Visitor 中 |
| **开闭原则** | 对扩展类开放，对扩展方法关闭 | 对扩展方法开放，对扩展类关闭 |

**所以，在 C++ 开发中：**
*   如果你的对象结构（零件）非常稳定，但你经常要给这些对象写各种各样的算法、分析、转换逻辑，那么 **Visitor 模式** 是完美的选择（比如 **LLVM 编译器** 中，AST 语法树节点是稳定的，但针对语法树的优化、检查、代码生成操作非常多，所以 LLVM 大量使用了 Visitor 模式）。
*   如果你只是简单的抽象，主要为了以后能派生出更多子类，那么普通的 **虚函数** 就够了。


# 此外，一些计算机设计会对频繁访问的寄存器进行缓存 。因此，程序可以通过尽可能地将同一个寄存器分配给 move 指令的源寄存器和目标寄存器来进一步优化。如果编译器使用静态单赋值形式 （SSA）等中间表示形式 ，这一点尤为重要。特别是，当 SSA 没有被完全优化时，它可能会人为地生成额外的 move 指令。

理解：实际上就是对于同一块寄存器进行反复擦写。使用move之后的结果覆盖move之前的结果

## 出现原因：编译器在将程序转换为SSA（静态单赋值形式，是一种中间表示（IR），其中每个变量仅被赋值一次）格式时，原始中间表示 (IR) 中的现有变量会被拆分成多个版本，新变量通常以原名称加下标的形式表示，这样每个定义都有自己的版本。在两个控制流路径的连接点，可能还需要添加一些语句来为变量的新版本赋值。从 SSA 格式转换为机器代码也同样高效。但这样会存在一个问题是，在碰到分支或者跳转时会导致丢失需要拿来运算的变量。因此需要一个 Φ 函数通过控制流来确认需要参与运算的变量是哪个。
具体详细内容参见：![SSA From WIKI](https://en.wikipedia.org/wiki/Static_single-assignment_form)


## 接下来，还需要搞清楚IR以及SSA之间的关系：
在编译器设计中，**IR（中间表示）是“载体”，而 SSA（静态单赋值）是“表现形式”**。

简单来说：**IR 是一门中间语言，而 SSA 是这门语言必须遵循的一套排版规则。** 现代编译器（如 LLVM）的 IR 几乎全都是基于 SSA 形式的。

以下是它们之间深层关系的详细拆解：

---

### 1. 容器与规范的关系 (Container vs. Form)
*   **IR (Intermediate Representation)**：是编译器用来表示源代码的内部数据结构。它比源代码更接近机器码，但又与具体硬件无关。
*   **SSA (Static Single Assignment)**：是 IR 的一种属性。如果一个 IR 满足“每个变量只被赋值一次”和“变量在使用前必须先定义”，那么它就是 **SSA-based IR**。

**类比：** 如果 IR 是一个**文档**，那么 SSA 就是一种**“修订追踪”模式**。在普通文档里你可以擦掉旧文字写新文字（变量重写），但在 SSA 模式下，你不能擦除，只能在后面新起一行写（新版本变量）（也就是llm教程文档里说的那样，SSA值无法更改，并且只有当指令重新执行是他才会获得新值）。

---

### 2. 二者的结合阶段

| 阶段 | 形式 | 说明 |
| :--- | :--- | :--- |
| **前端生成 (Initial IR)** | **非 SSA / 内存 SSA** | 简单地把变量存入内存（`alloca`），容易实现。 |
| **中端优化 (Optimizing IR)** | **纯 SSA** | 运行 `mem2reg`。这是 IR 最强大的时刻，各种优化算法在此运行。 |
| **后端生成 (Machine IR)** | **逐渐脱离 SSA** | 当分配具体的物理寄存器（如 `rax`, `rbx`）时，SSA 形式消失，因为寄存器必须被重复利用。 |

**一句话总结：**
**IR 是编译器的语言，而 SSA 是让这门语言变得“好做数学分析”的一种严格语法。** 没有 SSA，现代 IR 的优化效率将大打折扣；没有 IR，SSA 也只是一个数学理论，无法在计算机里运行。