# examples/CMakeLists.txt
# 示例程序

add_executable(kvfs_example main.cpp)
target_link_libraries(kvfs_example PRIVATE kvfs_core)
