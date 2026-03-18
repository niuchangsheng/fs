#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <span>
#include <cstdint>
#include <sys/stat.h>

namespace kvfs {

/**
 * @brief 文件打开标志，模拟 POSIX O_RDONLY, O_WRONLY, O_RDWR, O_CREAT 等
 */
enum class OpenFlags : int {
    ReadOnly = 0x01,
    WriteOnly = 0x02,
    ReadWrite = 0x03,
    Create = 0x10,
    Truncate = 0x20,
    Append = 0x40
};

/**
 * @brief Lseek 控制字
 */
enum class Whence : int {
    Set = 0,    // SEEK_SET
    Cur = 1,    // SEEK_CUR
    End = 2     // SEEK_END
};

/**
 * @brief 文件句柄 (File Handle)
 * 封装了文件的打开状态、读写指针及引用计数
 */
class FileHandle {
public:
    virtual ~FileHandle() = default;
    virtual const std::string& GetPath() const = 0;
    virtual uint64_t GetOffset() const = 0;
};

/**
 * @brief KVFS 核心引擎接口
 */
class KVEngine {
public:
    virtual ~KVEngine() = default;

    /**
     * @brief 初始化引擎 (初始化 SPDK 环境、绑定设备、挂载文件系统)
     */
    virtual std::future<void> Init(const std::string& device_path) = 0;

    /**
     * @brief 关闭引擎并释放资源
     */
    virtual std::future<void> Shutdown() = 0;

    /**
     * @brief 打开文件
     * @param path 文件路径
     * @param flags 打开模式
     * @return 成功返回文件句柄，失败抛出异常或返回空
     */
    virtual std::future<std::shared_ptr<FileHandle>> Open(const std::string& path, OpenFlags flags) = 0;

    /**
     * @brief 异步读取数据
     * @param handle 文件句柄
     * @param buf 目标缓冲区 (由调用者提供并保证在操作完成前有效)
     * @param count 字节数
     * @return 实际读取的字节数
     */
    virtual std::future<ssize_t> Read(std::shared_ptr<FileHandle> handle, std::span<uint8_t> buf, size_t count) = 0;

    /**
     * @brief 异步写入数据
     * @param handle 文件句柄
     * @param data 要写入的数据
     * @return 实际写入的字节数
     */
    virtual std::future<ssize_t> Write(std::shared_ptr<FileHandle> handle, std::span<const uint8_t> data) = 0;

    /**
     * @brief 修改文件读写偏移量
     */
    virtual off_t Lseek(std::shared_ptr<FileHandle> handle, off_t offset, Whence whence) = 0;

    /**
     * @brief 关闭文件句柄
     */
    virtual std::future<int> Close(std::shared_ptr<FileHandle> handle) = 0;

    /**
     * @brief 删除文件
     */
    virtual std::future<int> Unlink(const std::string& path) = 0;

    /**
     * @brief 获取文件状态属性
     */
    virtual std::future<struct stat> Stat(const std::string& path) = 0;

    /**
     * @brief 强制将脏数据和元数据刷入 KV 硬件
     */
    virtual std::future<int> Fsync(std::shared_ptr<FileHandle> handle) = 0;
};

/**
 * @brief 创建 KVEngine 的工厂函数
 */
std::unique_ptr<KVEngine> CreateKVEngine();

} // namespace kvfs
