# src/CMakeLists.txt
# KVFS 核心库

add_library(kvfs_core SHARED
    kvfs.cpp
    mock_kv_device.cpp
)

# 设置头文件可见性
target_include_directories(kvfs_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
