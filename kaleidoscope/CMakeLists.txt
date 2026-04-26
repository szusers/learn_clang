cmake_minimum_required(VERSION 3.22)

message("USE_LLVM='${USE_LLVM}'")
# 显示定义使用LLVM标准库，否则使用默认gnu选项。并且这里的设置一定要放在project前面。否则还是会找回默认的gnu
if(USE_LLVM)
    # 选择 clang 工具链
    find_program(CMAKE_C_COMPILER NAMES clang-20 clang)
    find_program(CMAKE_CXX_COMPILER NAMES clang++-20 clang++)

    if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
        message(FATAL_ERROR "USE_LLVM=ON but no Clang found!")
    endif()

    # 启用 LLVM 的 C++ 标准库
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -stdlib=libc++")
    # 设置 LLVM 的头文件与库路径（Ubuntu LLVM 20 默认）
    set(LLVM_PREFIX "/usr/lib/llvm-20")
    # 这是一个“系统级（system）头文件路径”，不是用户代码路径。这样能避免某些标准库非项目本身的警告
    # include_directories("${LLVM_PREFIX}/include/c++/v1")
    add_compile_options("-I${LLVM_PREFIX}/include/c++/v1")
    add_link_options("-fuse-ld=lld-20")
    link_directories("${LLVM_PREFIX}/lib")

    # 链接 libc++ 运行时库（不使用 libstdc++）
    # 显式声明可以完全锁定 LLVM 的 runtime 体系，保证：1、只用 libc++ / libc++abi / libunwind 2、不混入 GNU 的 runtime 库（避免 ABI 不兼容）
    link_libraries(c++ c++abi unwind)

    # 设置运行时库搜索路径
    set(CMAKE_INSTALL_RPATH "${LLVM_PREFIX}/lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

project(kaleidoscope_llvm20)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# cmake当前正在编译的目录 CMAKE_CURRENT_BINARY_DIR
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


# ==============================================================================
# 3. LLVM 核心配置 (解决 undefined reference llvm::DisableABIBreakingChecks)
# ==============================================================================
# 即使手动指定了 clang，也需要 find_package 来获取 LLVM 的库组件路径，不然后面没法使用这些变量
# find_package(LLVM ...) 是衔接你的代码和庞大 LLVM 库的桥梁。它保证了无论 LLVM 安装在 /usr/lib/ 还是 /opt/local/，你的 CMakeLists.txt 都能正常工作
find_package(LLVM 20.1.8 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# 获取 LLVM 提供的宏定义和包含路径
add_definitions(${LLVM_DEFINITIONS})


# CMAKE_CXX_FLAGS
# 在编译每一个 C++ 源文件时，把这些选项追加到编译器命令行。
# CMAKE_EXE_LINKER_FLAGS
# 在生成可执行文件（不是库）时，把这些选项追加到链接器命令行。

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
    add_compile_options(-DNDEBUG)
    add_compile_options(-m64)
    message("BUILD TYOE: Release")
else()
    add_compile_options(-m64)
    add_compile_options(-g)
    add_compile_options(-Werror)
    # -g保留调试符号方便报错给出行号。并且这里是往CMAKE_CXX_FLAGS这个变量上做追加操作
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    message("BUILD TYPE: Debug")
endif()

# ==============================================================================
# 5. 目标定义与链接 (修正版)
# ==============================================================================
# 确保在链接之前已经获取了组件库名
# 我们增加了一个 'demangle' 组件，它是 support 的依赖
llvm_map_components_to_libnames(llvm_libs 
    core 
    support 
    native 
    analysis 
    executionengine 
    instcombine 
    scalaropts
    demangle
)

add_executable(Lexer Lexer.cpp)

# 包含路径
target_include_directories(Lexer PRIVATE .)
# 使用 SYSTEM 标记 LLVM 包含路径为系统库，自动屏蔽其内部警告
target_include_directories(Lexer SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

# ----------------- 核心修复：链接顺序与系统库 -----------------
target_link_libraries(Lexer PRIVATE 
    ${llvm_libs}
    pthread
    dl
    z
    tinfo
)
