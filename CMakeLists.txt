cmake_minimum_required(VERSION 3.16)
project(kvfs LANGUAGES CXX)

# 设置 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
add_compile_options(-Wall -Wextra -Wpedantic -O3)

# 包含头文件目录
include_directories(include)

# -----------------------------------------------------------------------------
# 子目录
# -----------------------------------------------------------------------------

# 核心库
add_subdirectory(src)

# 示例程序
add_subdirectory(examples)

# 测试
add_subdirectory(test)
