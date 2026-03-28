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
# SPDK 依赖项处理 (占位符)
# -----------------------------------------------------------------------------

# 定义核心库
add_library(kvfs_core SHARED
    src/kvfs.cpp
    src/mock_kv_device.cpp
)

# 设置头文件可见性
target_include_directories(kvfs_core PUBLIC include)

# -----------------------------------------------------------------------------
# GTest 依赖
# -----------------------------------------------------------------------------
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# -----------------------------------------------------------------------------
# 可执行文件示例 / 单元测试 / E2E 测试
# -----------------------------------------------------------------------------

# 基础示例
add_executable(kvfs_example examples/main.cpp)
target_link_libraries(kvfs_example PRIVATE kvfs_core)

# 单元测试
add_executable(mock_kv_device_test test/unit/mock_kv_device_test.cpp)
target_link_libraries(mock_kv_device_test PRIVATE kvfs_core GTest::gtest_main)

add_executable(metadata_test test/unit/metadata_test.cpp)
target_link_libraries(metadata_test PRIVATE kvfs_core GTest::gtest_main)

# E2E 测试
add_executable(initialization_e2e_test test/e2e/initialization_test.cpp)
target_link_libraries(initialization_e2e_test PRIVATE kvfs_core GTest::gtest_main)

add_executable(file_operations_e2e_test test/e2e/file_operations_test.cpp)
target_link_libraries(file_operations_e2e_test PRIVATE kvfs_core GTest::gtest_main)

# -----------------------------------------------------------------------------
# 注册测试
# -----------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(mock_kv_device_test)
gtest_discover_tests(metadata_test)
gtest_discover_tests(initialization_e2e_test)
gtest_discover_tests(file_operations_e2e_test)
