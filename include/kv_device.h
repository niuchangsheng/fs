#pragma once

#include <string>
#include <vector>
#include <span>
#include <future>
#include <cstdint>
#include <memory>

namespace kvfs {

/**
 * @brief 抽象的底层 KV 设备接口 (Hardware Abstraction Layer)
 * 用于隔离具体的存储后端（如 SPDK NVMe, 内存 Mock, 或其他数据库）
 */
class KVDevice {
public:
    virtual ~KVDevice() = default;

    /**
     * @brief 异步写入 KV 对
     */
    virtual std::future<bool> Put(const std::string& key, std::span<const uint8_t> value) = 0;

    /**
     * @brief 异步读取 KV 对
     * @return 返回 pair: <是否找到, 数据内容>
     */
    virtual std::future<std::pair<bool, std::vector<uint8_t>>> Get(const std::string& key) = 0;

    /**
     * @brief 异步删除 KV 对
     */
    virtual std::future<bool> Delete(const std::string& key) = 0;

    /**
     * @brief 异步检查 Key 是否存在
     */
    virtual std::future<bool> Exists(const std::string& key) = 0;
};

/**
 * @brief 创建一个基于内存的 Mock KV 设备，用于无硬件环境下的开发和测试
 */
std::unique_ptr<KVDevice> CreateMockKVDevice();

} // namespace kvfs
