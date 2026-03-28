# src/CMakeLists.txt
# KVFS 核心库

# 可选：RocksDB 后端
option(USE_ROCKSDB "Use RocksDB as mock backend" OFF)

if(USE_ROCKSDB)
    find_package(RocksDB REQUIRED)
    add_definitions(-DKVFS_USE_ROCKSDB=1)
endif()

add_library(kvfs_core SHARED
    kvfs.cpp
    mock_kv_device.cpp
)

# 设置头文件可见性
target_include_directories(kvfs_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 链接 RocksDB (如果启用)
if(USE_ROCKSDB)
    target_link_libraries(kvfs_core PRIVATE RocksDB::rocksdb)
endif()
