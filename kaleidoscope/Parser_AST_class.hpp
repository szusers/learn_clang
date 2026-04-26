/*
Parser_AST_class.hpp
*/

/*
在第一步有了Lexer.cpp提供的token之后，下一步的任务是写一个parser（解析器）。输出目标：Parser
解析完代码后，产出的结果就是抽象语法树（AST）。在开始写解析逻辑之前，必须先定义好AST长什么样。
因此，在本文件的开始，我们将先声明AST上各个节点上的对象的类型（即它都有哪些类）！！！
*/

/*
什么是 AST？
AST（抽象语法树）是源代码在计算机内存中的树状表现形式。它的最大特点是"抽象"：它只保留代码的执行逻辑和核心数据，而丢弃掉所有用来辅助阅读的语法细节（比如空格、括号、分号等）。
为了在 C++ 中表示这种树，文档采用"面向对象编程（OOP）"的设计模式，为Kaleidoscope
语言中的每一种语法结构都定义了一个 C++ 类（Class）。
文档将这些节点分为两大类：表达式节点和函数节点。
*/

/*
A. 表达式节点（Expressions）
*/

/*
ExprAST
（基类）：这是一个空的基类（只包含一个虚构析构函数）。所有的表达式都会继承它，它的作用是提供一个统一的类型，方便我们在树中嵌套各种不同的表达式。
*/

/*
NumberExprAST（数字节点）：用来表示一个具体的数字（如 1.0）。它内部有一个 double
Val 变量来保存这个数字的值。
*/

/*
VariableExprAST（变量节点）：用来表示对变量的引用（如 x）。它内部有一个
std::string Name 来保存变量的名字。
*/

/*
BinaryExprAST（二元运算节点）：用来表示数学运算（如 x + y）。它保存了操作符 char
Op（如 +），以及运算符左边（LHS）和右边（RHS）的两个子节点。
*/

/*
CallExprAST（函数调用节点）：用来表示调用某个函数（如sin(x)）。它保存了被调用的函数名
Callee，以及一个包含所有传入参数子节点的列表Args。
*/

/*
B. 函数节点（Functions）：除了表达式，语言还需要能定义函数。
*/

/*
PrototypeAST（函数原型节点）：它代表了函数的"接口"或"声明"。它只保存函数的名字和参数的名字列表。（注意：由于Kaleidoscope
只有 double 类型，所以这里不需要保存参数类型，这也是为了简化教程。）
*/

/*
FunctionAST（完整的函数节点）：它代表了一个完整的函数定义。它由两部分组成：一个函数原型（Proto，定义了名字和参数）和一个函数体（Body，函数内部执行的表达式）。
*/
#pragma once
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <memory>
#include <string>
#include <vector>

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
  // virtual llvm::Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name_;

public:
  VariableExprAST(const std::string &Name) : Name_(Name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
/// 二元运算符表达式的节点，里面保存的是左边以及右边对象的节点（那么这里就有一个问题，在实际运行时
/// Parser 解析到某个位置之前，我不知道左边和右边具体对应哪个ExprAST子类，
/// 所以在编写代码的时候就可以用基类指针来存储。那么我估计这里有一个共同基类的原因就是因为便于我们嵌套类型不同的表达式。例如x+y左边就是Var，右边就是Num）
class BinaryExprAST : public ExprAST {
  char Op_;
  std::unique_ptr<ExprAST> LHS_, RHS_;

public:
  // 这里需要注意的是，std::unique_ptr只能移动不能拷贝
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op_(Op), LHS_(std::move(LHS)), RHS_(std::move(RHS)) {}
};

/// CallExprAST - Expression class for function calls.
/// 保存函数名和函数参数的类（在函数调用处解析函数调用）
class CallExprAST : public ExprAST {
  std::string Callee_;
  std::vector<std::unique_ptr<ExprAST>> Args_;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee_(Callee), Args_(std::move(Args)) {}
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
/// 只保存函数的名字和参数的名字列表的类（解析函数声明）
class PrototypeAST {
  std::string Name_;
  std::vector<std::string> Args_;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name_(Name), Args_(std::move(Args)) {}

  const std::string &getName() const { return Name_; }
};

/// FunctionAST - This class represents a function definition itself.
/// 内含保存函数名+参数列表的PrototypeAST对象+函数主体的表达式对象
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto_;
  std::unique_ptr<ExprAST> Body_;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto_(std::move(Proto)), Body_(std::move(Body)) {}
};