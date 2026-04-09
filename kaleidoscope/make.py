#! /usr/bin/env python3
import os
import sys
import subprocess


debug_flag = ""
clang_cc_flag = ""
clang_cxx_flag = ""
linker_flag = ""
cxx_compiler = ""

os.chdir("src")


if len(sys.argv) > 1:
    if sys.argv[1] == "-h" or sys.argv[1] == "--help":
        options = [
            ("-h, --help", "Print this message"),
            ("-d, --debug", "Build with debug symbols"),
            ("-r, --release", "Build with release symbools"),
            ("-c, --clean", "Clean build directory"),
            ("-l, --llvm", "Build CXX excuable with llvm stdc++"),
        ]

        max_opt_width = max(len(opt) for opt, _ in options) + 2  # 加一些额外空格

        print("Usage: make.py [options]")
        print("Options:")
        for opt, desc in options:
            print(f"  {opt:<{max_opt_width}}{desc}")

        exit(0)

    args = sys.argv[1:]
    if "-d" in args or "--debug" in args:
        debug_flag = "-DCMAKE_BUILD_TYPE=Debug"
    else:
        debug_flag = "-DCMAKE_BUILD_TYPE=Release"

    if "-c" in args or "--clean" in args:
        subprocess.run("rm -rf build", shell=True, check=True)

    if "-l" in args or "--llvm" in args:
        clang_cc_flag = "CC=clang-20"
        clang_cxx_flag = "CXX=clang++-20"
        linker_flag = "-DCMAKE_EXE_LINKER_FLAGS='-fuse-ld=lld'"
        cxx_compiler = "-DUSE_LLVM=ON"
    os.makedirs("build", exist_ok=True)
    os.chdir("build")
    print(f"cwd = {os.getcwd()}")
    # subprocess.run(
    #     f"{clang_cc_flag} {clang_cxx_flag} cmake {debug_flag} {linker_flag} ..",
    #     shell=True,
    #     check=True,
    # )
    subprocess.run(f"cmake {debug_flag} {cxx_compiler} ..", shell=True, check=True)
    subprocess.run("make -j", shell=True, check=True)
