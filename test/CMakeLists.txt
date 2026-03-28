# test/CMakeLists.txt
# 单元测试和 E2E 测试

# 查找 GTest 依赖
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 单元测试
add_executable(mock_kv_device_test unit/mock_kv_device_test.cpp)
target_link_libraries(mock_kv_device_test PRIVATE kvfs_core GTest::gtest_main)

add_executable(metadata_test unit/metadata_test.cpp)
target_link_libraries(metadata_test PRIVATE kvfs_core GTest::gtest_main)

# E2E 测试
add_executable(initialization_e2e_test e2e/initialization_test.cpp)
target_link_libraries(initialization_e2e_test PRIVATE kvfs_core GTest::gtest_main)

add_executable(file_operations_e2e_test e2e/file_operations_test.cpp)
target_link_libraries(file_operations_e2e_test PRIVATE kvfs_core GTest::gtest_main)

# 注册测试
include(GoogleTest)
gtest_discover_tests(mock_kv_device_test)
gtest_discover_tests(metadata_test)
gtest_discover_tests(initialization_e2e_test)
gtest_discover_tests(file_operations_e2e_test)
