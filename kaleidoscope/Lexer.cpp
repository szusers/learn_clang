/*
本教程运行一种名为"万花筒"的玩具语言。为了保持简单，所以 Kaleidoscope
唯一的数值类型是一种64位浮点类型（在C语言中称为"双精"）。因此，
所有值都隐式双精度，语言本身没有类型声明。这使得语言非常接近
原始的语法。例如，以下简单例子计算斐波那契数列：

# Compute the x'th fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)

# This expression will compute the 40th number.
fib(40)

我们还允许 Kaleidoscope 调用C标准库函数——LLVM JIT
让这变得非常简单。这意味着你可以在使用函数之前先用"外部"关键词定义函数（这对递归函数也很有用）。例如：

extern sin(arg);
extern cos(arg);
extern atan2(arg1 arg2);

atan2(sin(.4), cos(42))
*/

#include "Parser_AST_class.hpp"
#include <cctype>
#include <map>
#include <string>

/// LogError* - These are little helper functions for error handling.
/// 普通的打印错误字符串的函数
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

namespace {

///////////////////////////////////////////
/* Lexer 分词器 */
//////////////////////////////////////////

// 词法分析器返回的token类型
enum class Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4, // 判断这个token是否为一个标识符（变量名/参数名/关键字）
  tok_number = -5,
};

// 标识符被填充到 IdentifierStr
std::string IdentifierStr;
// 数值被填充到 NumVal
double NumVal;

constexpr auto TOK_EOF = static_cast<int>(Token::tok_eof);
constexpr auto TOK_DEF = static_cast<int>(Token::tok_def);
constexpr auto TOK_EXTERN = static_cast<int>(Token::tok_extern);
constexpr auto TOK_IDENTIFIER = static_cast<int>(Token::tok_identifier);
constexpr auto TOK_NUMBER = static_cast<int>(Token::tok_number);

/// 核心词法分析函数（逐个字符读取文本）。返回当前遍历到的token，并且其中的LastChar总是指向当前token结束位置的下一个字符
int gettok() {
  // LastChar
  // 是静态变量，用于记住上一次读取但尚未处理的字符。注意：函数外部的变量以及函数由于在匿名namspace中因此无需使用static。但是这里一定需要，因为这个表示的是静态变量只初始化一次
  static char LastChar = ' ';

  // 跳过空白字符（空格、制表符、换行符等），并获取下一个非空格字符
  /*
  在C语言中，isspace 认定的空白字符包括：

  字符    ASCII 值    含义    转义序列
  空格    32          普通空格    ' '
  换页    12          换页符      '\f'
  换行    10          新行符      '\n'
  回车    13          回车符      '\r'
  水平制表符 9         Tab 键      '\t'
  垂直制表符 11        垂直制表    '\v'

  注意：空字符 '\0' (ASCII 0) 不是空白字符。

  当前字符为空白字符时，该函数返回true
  */
  while (isspace(static_cast<unsigned char>(LastChar)))
    LastChar = getchar();

  // 识别标识符（变量名/函数名）和关键字（def/extern）
  // 并且标识符的第一位必须是字母而不能是数字！！！
  if (isalpha(static_cast<unsigned char>(LastChar))) {
    IdentifierStr = LastChar;
    while (isalnum(static_cast<unsigned char>(LastChar = getchar()))) {
      // 若为数字或者字母则持续拼接标识符
      IdentifierStr += LastChar;
    }
    if (IdentifierStr == "def") {
      return TOK_DEF;
    }
    if (IdentifierStr == "extern") {
      return TOK_EXTERN;
    }
    return TOK_IDENTIFIER;
  }

  if (isdigit(static_cast<unsigned char>(LastChar)) ||
      LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    // 因为这里进来的时候已经确定当前输入的字符是一个数字字符了，因此这里NumStr要先加上第一次getchar得到的数字
    int dot_count = 0;
    do {
      if (LastChar == '.')
        ++dot_count;
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(static_cast<unsigned char>(LastChar)) || LastChar == '.');
    // 检查是否有多余的小数点
    if (dot_count > 1) {
      // 报错机制（比如打印错误信息，退出程序等）
      fprintf(stderr, "Error: invalid number format: %s\n", NumStr.c_str());
      return TOK_EOF; // 或者返回一个表示错误的 token
    }
    if (dot_count == 1 && !isdigit(static_cast<unsigned char>(LastChar))) {
      fprintf(stderr, "Error: invalid number format with single dot: %s\n",
              NumStr.c_str());
      return TOK_EOF; // 或者返回一个表示错误的 token
    }
    // 第二个参数传入0表示忽略这个函数产生的报错
    NumVal = strtod(NumStr.c_str(), nullptr);
    return TOK_NUMBER;
  }

  if (LastChar == '#') {
    // 这种情况表示遇到注释，需要忽略这一整行
    do
      LastChar = getchar();
    while (LastChar != '\n' && LastChar != EOF && LastChar != '\r');
    if (LastChar != EOF) {
      // 表示没有到输入流的末尾，可以继续分析注释的下一行
      return gettok(); // 下一行的递归调用会无缝续上这里获取的最后一个换行符或者制表符
    }
  }

  if (LastChar == EOF) {
    // 处理文件结束符
    return TOK_EOF;
  }

  // 6. 处理未知字符（通常是操作符，如 '+', '-', '*', '(', ')' 等）
  int ThisChar = LastChar;
  LastChar = getchar(); // 读取下一个字符供下次调用使用
  return ThisChar;      // 直接返回该字符的 ASCII 码
}

int CurTok;
// fix:
// 调用gettok的时候更新当前指向的token并保存（原本并没有给CurTok赋值这一步）
int getNextToken() { return CurTok = gettok(); }

/////////////////////////////////////
/* Basic Expression Parsing */
////////////////////////////////////

std::unique_ptr<ExprAST> ParseExpression(); // 后面定义这个函数

/// numberexpr ::= number  这个程序非常简单：当当前令牌是 tok_number 令牌时，
/// 它期望被调用。它取当前的数字值全局变量NumVal，创建一个 NumberExprAST
/// 节点，将词法表推进到下一个令牌，最后返回。
std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();
  // 返回这个子类对应的父类是为了方便挂载在AST上。因为其实父类和子类都在一个地址。
  // 挂载的时候挂载父类，实际使用的时候可以轻松转换回子类（这其实是类型擦除的另外一种方式）
  return std::move(Result);
}

/// parenexpr ::= '(' expression ')'  解释：括号表达式被定义为：一个左圆括号
/// '('跟着一个 expression（表达式）跟着一个右圆括号 ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.  跳过左括号，直接处理 ( 之后的表达式。那么此时就是 (
                  // 后面的IdentifierStr这个token了
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");

  getNextToken(); // eat ). 调用这个函数之前Curtok是 ) ,
                  // 也就是此时这个函数返回值是 )
                  // (这种情况同时也是返回的LastChar),
                  // 调用之后就直接快进到下一个token了，也就是）后面的表达式了。
  // 直接返回括号里面的表达式，例如 (4+5)中的 4+5
  return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
/// 处理变量引用以及函数调用。如果一个标识符后面紧跟着的不是
/// ( , 那么就表示它是单个的标识符，也就是变量而不是一个函数。
// 这个函数和上面不同的是它不是要处理括号里面的表达式，它是一个分支判断到底是变量还是一个函数调用。是函数调用的话就要同时处理括号外的函数名和括号内的函数参数
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier.
                  // 保存标识符之后让此时的token指向标识符后面的下一项

  if (CurTok != '(') // Simple variable ref. (一个简单的变量引用（别名）)
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
                  // 。gettok这个函数里面本身就没有处理括号的逻辑。因此碰到括号直接跳过它去处理括号里面的表达式
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression()) // 分别解析每个函数参数以及表达式主体
        // 如果这个函数有多个参数需要解析的话，需要在这里循环多次调用ParseExpression，具体顺序为：Identifier->Expr->Primary->Expr->Primary...
        // 直到遇到 ）。这就是一次循环解析一个函数参数的调用开销
        // 存储参数。在这里push_back和emplace_back在效率上没有区别
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')') // 有闭环 ) 则正常退出
        break;

      if (CurTok != ',') // 缺少 , 分隔参数说明参数列表的写法有误
        return LogError("Expected ')' or ',' in argument list");

      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();
  // 这条分支返回的是解析出来的函数对象。
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
/// 现在我们已经实现了所有简单的表达式解析逻辑，可以定义一个辅助函数，将它们封装成一个入口点。我们将这类表达式称为“主要”表达式，原因将在后续教程中详细说明。为了解析任意主要表达式，我们需要确定它的表达式类型：
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr  初始入口的解析定义：1、标识符表达式
///   2、数字表达式（常数）3、括号表达式（函数）
decltype(auto) ParsePrimary() {
  // 使用decltype(auto)和auto表示返回值类型的区别在于：auto按值进行类型推导，会丢弃引用以及const将函数作为副本返回。但是decltype(auto)完全按照表达式原样进行推导
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case TOK_IDENTIFIER:
    // 变量名和函数的解析走这条分支
    return ParseIdentifierExpr();
  case TOK_NUMBER:
    return ParseNumberExpr();
  case '(': // 括号后面跟着的就是表达式的主体内容，所以只要当前的token是 (
            // 那就进入括号表达式解析的入口。括号表达式走这条分支
    return ParseParenExpr();
  }
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.  保存运算符优先级的map
std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.

int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                       std::unique_ptr<ExprAST> LHS);

/// expression ::= primary binoprhs
/// 首先，表达式是一个主表达式，后面可能跟着一系列 [binop,primaryexpr] 对：
/// 解析器把表达式当成“primary + 运算符”的流，通过比较优先级逐步组合成
/// AST，而括号已经在 primary 阶段被处理掉了.
/// 解析运算符表达式的原理是：把复杂的表达式看成一串primmary（基本表达式）+运算符
std::unique_ptr<ExprAST> ParseExpression() {
  // 解析表达式的时候从解析入口开始解析（因为我们需要判断当前的token类型是什么，然后我们才能对症下药去解析对应的表达式）
  auto LHS = ParsePrimary();
  if (!LHS) {
    return nullptr;
  }
  return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                       std::unique_ptr<ExprAST> LHS) {
  /*
    这个函数在获取token优先级时总是能确保当前的Curtok落在二元运算的原因在于：算法本质是流式消费token，CurTok始终指向下一个要处理的token
    并且在ParsePrimary函数所支持的所有处理分支中，各个函数在解析完当前的token之后都有使用getNextToken将CurTok指向下一个待处理的token
    因此我们可以认为，每当处理完一个表达式/数字/标识符之后CurTok总是指向下一个要处理的token，而这个token如果是一个二元运算符的话就会进入ParseBinOpRHS函数进行优先级比较和合并，否则就直接返回LHS（例如处理的是函数参数这里就直接返回LHS）。
    */
  while (true) {
    // 1️⃣ 获取当前 token 的优先级
    int TokPrec = GetTokPrecedence();

    // 2️⃣ 如果当前不是合法的二元运算符，或优先级不够 →
    // 结束（这种情况直接返回LHS）。解析函数参数的时候也是在这里直接返回LHS（函数参数标识符/变量）
    // 这个if决定这一层LHS是否还要继续合并右边的RHS，是否还要继续干活吃token
    if (TokPrec < ExprPrec)
      return LHS;

    // 3️⃣ 记录当前运算符
    int BinOp = CurTok;
    getNextToken(); // 吃掉运算符

    // 4️⃣解析右侧primary（先走入口，判断当前的Curtok应该是按照标识符还是数字还是括号表达式解析）
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    // 5️⃣ 看看 RHS 后面还有没有更高优先级的运算符
    int NextPrec = GetTokPrecedence();

    if (TokPrec < NextPrec) {
      // 这个if决定RHS是否还要继续扩展，扩展RHS是下一层要做的事情，并不在这一层解决。也就是说这里决定是否还有下一层
      // 递归：把 RHS 扩展成更完整的表达式（也就是看右边是否还有右边）
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // 6️⃣ 合并 LHS 和 RHS
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/*
至此，表达式的处理就完成了。现在，我们可以让解析器指向任意的词法单元流，并从中构建表达式，直到遇到第一个不属于该表达式的词法单元为止。接下来，我们需要处理函数定义等等。
*/

////////////////////////////////////////////////////////////////
// Parsing the Rest 解析剩余部分
///////////////////////////////////////////////////////////////

/// prototype
///   ::= id '(' id* ')'  定义这种形式为函数声明
std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != TOK_IDENTIFIER) {
    // 解析函数没有标识符（函数名）抛出错误
    return LogErrorP("Expected function name in prototype");
  }

  std::string FnName = IdentifierStr;
  getNextToken(); // eat function name.
                  // 解析完函数名之后让CurTok指向函数名后面的token
  if (CurTok != '(') {
    return LogErrorP("Expected '(' in prototype");
  }
  // Read the List of Argument Names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == TOK_IDENTIFIER) {
    ArgNames.push_back(IdentifierStr);
    // 如果函数的声明写法上不是用空格分隔，而是使用,进行分隔的话，需要解注释下面这个getNextToken()，让CurTok跳过,
    // getNextToken(); // eat ,
  }
  if (CurTok != ')') {
    // 如果退出不是因为碰到
    // ）而是因为碰到其他token，那么说明函数声明的参数列表写法有误
    return LogErrorP("Expected ')' in prototype");
  }

  // success. 解析成功，返回一个函数声明对象
  getNextToken();

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// 鉴于此，函数定义非常简单，只需一个函数原型加上一个用于实现函数体的表达式即可：
/// definition ::= 'def' prototype expression   例如：def foo(x, y) x + y
std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat def.
  // 先解析函数名以及函数参数
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  // 后解析函数主体的表达式
  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

// 此外，我们支持使用“extern”声明诸如“sin”和“cos”之类的函数，也支持用户函数的前置声明。这些“extern”只是函数原型，没有函数体：
/// external ::= 'extern' prototype
/// 支持解析带有extern关键字的函数声明，例如：extern sin(arg);
std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat extern.
  return ParsePrototype();
}

// 最后，我们还将允许用户输入任意顶级表达式并即时求值。我们将通过定义匿名零元（无参数）函数来实现这一点：
/// toplevelexpr ::= expression
/// 这个函数的作用是让表达式变成可执行单位（函数），从而让表达式可以立刻执行，而不是生成AST上的一个ExprAST(表达式结点)
/*
顶级表达式做的事情是：

把用户输入的：

1 + 2 * 3

变成内部等价的：

def __anon_expr() 1 + 2 * 3

也就是：

一个匿名函数
没有参数
函数体就是这个表达式
*/
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}
} // namespace
