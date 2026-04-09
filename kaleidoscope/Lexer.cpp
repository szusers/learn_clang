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
int getNextToken() { return gettok(); }

/////////////////////////////////////
/* Basic Expression Parsing */
////////////////////////////////////

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
  getNextToken(); // eat ). 调用这个函数之前LastChar是 ) ,
                  // 也就是此时这个函数返回值是 ),
                  // 调用之后就直接快进到下一个字符了，也就是 ）后面的表达式了。
  return V;
}
} // namespace
