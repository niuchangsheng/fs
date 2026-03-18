#include "kvfs.h"
#include "kv_device.h"
#include "metadata.h"
#include <stdexcept>
#include <iostream>
#include <chrono>

namespace kvfs {

/**
 * @brief 内部 FileHandle 实现类
 */
class FileHandleImpl : public FileHandle {
public:
    FileHandleImpl(std::string path, OpenFlags flags)
        : path_(std::move(path)), flags_(flags), offset_(0) {}

    const std::string& GetPath() const override { return path_; }
    uint64_t GetOffset() const override { return offset_; }

private:
    std::string path_;
    OpenFlags flags_;
    uint64_t offset_;
};

/**
 * @brief KVEngine 默认实现
 */
class KVEngineImpl : public KVEngine {
public:
    KVEngineImpl() : device_(CreateMockKVDevice()) {}

    std::future<void> Init(const std::string& device_path) override {
        std::cout << "Initializing KVFS Engine with device: " << device_path << std::endl;
        
        // 尝试读取 Superblock
        std::string sb_key = GetSuperblockKey();
        auto fut = device_->Get(sb_key);
        auto [found, data] = fut.get(); // 阻塞等待 (Init 可以是阻塞的，或者链式调用)

        if (!found) {
            std::cout << "Superblock not found. Formatting new KVFS..." << std::endl;
            
            // 1. 初始化 Superblock
            Superblock sb{};
            sb.magic = 0x4B564653; // "KVFS"
            sb.version = 1;
            sb.block_size = 4096;
            sb.total_capacity = 1024ULL * 1024 * 1024 * 100; // 100GB dummy
            sb.root_inode_oid = 1;
            sb.next_inode_oid = 2; // 0 is invalid, 1 is root
            sb.next_chunk_oid = 1;

            std::vector<uint8_t> sb_data = sb.Serialize();
            device_->Put(sb_key, sb_data).get();

            // 2. 初始化 Root Inode
            Inode root_inode{};
            root_inode.oid = sb.root_inode_oid;
            root_inode.type = FileType::Directory;
            root_inode.mode = 0755;
            root_inode.size = 0;
            root_inode.link_count = 2; // '.' and '..' (logical)
            root_inode.is_inline = true;
            root_inode.inline_data_len = 0; // Empty directory initially
            
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            root_inode.atime = root_inode.mtime = root_inode.ctime = now;

            std::string root_key = GetInodeKey(root_inode.oid);
            device_->Put(root_key, root_inode.Serialize()).get();

            std::cout << "Format complete. Root Inode created." << std::endl;
        } else {
            Superblock sb = Superblock::Deserialize(data);
            if (sb.magic != 0x4B564653) {
                throw std::runtime_error("Invalid KVFS Magic Number");
            }
            std::cout << "KVFS successfully mounted. Version: " << sb.version << std::endl;
        }

        std::promise<void> promise;
        promise.set_value();
        return promise.get_future();
    }

    std::future<void> Shutdown() override {
        std::cout << "Shutting down KVFS Engine..." << std::endl;
        std::promise<void> promise;
        promise.set_value();
        return promise.get_future();
    }

    std::future<std::shared_ptr<FileHandle>> Open(const std::string& path, OpenFlags flags) override {
        std::promise<std::shared_ptr<FileHandle>> promise;
        auto handle = std::make_shared<FileHandleImpl>(path, flags);
        promise.set_value(handle);
        return promise.get_future();
    }

    std::future<ssize_t> Read(std::shared_ptr<FileHandle> handle, std::span<uint8_t> buf, size_t count) override {
        (void)handle; (void)buf; (void)count;
        std::promise<ssize_t> promise;
        promise.set_value(0); 
        return promise.get_future();
    }

    std::future<ssize_t> Write(std::shared_ptr<FileHandle> handle, std::span<const uint8_t> data) override {
        (void)handle;
        std::promise<ssize_t> promise;
        promise.set_value(data.size());
        return promise.get_future();
    }

    off_t Lseek(std::shared_ptr<FileHandle> handle, off_t offset, Whence whence) override {
        (void)handle; (void)whence;
        return offset;
    }

    std::future<int> Close(std::shared_ptr<FileHandle> handle) override {
        (void)handle;
        std::promise<int> promise;
        promise.set_value(0);
        return promise.get_future();
    }

    std::future<int> Unlink(const std::string& path) override {
        (void)path;
        std::promise<int> promise;
        promise.set_value(0);
        return promise.get_future();
    }

    std::future<struct stat> Stat(const std::string& path) override {
        (void)path;
        std::promise<struct stat> promise;
        struct stat st = {};
        promise.set_value(st);
        return promise.get_future();
    }

    std::future<int> Fsync(std::shared_ptr<FileHandle> handle) override {
        (void)handle;
        std::promise<int> promise;
        promise.set_value(0);
        return promise.get_future();
    }

private:
    std::unique_ptr<KVDevice> device_;
};

/**
 * @brief 工厂函数实现
 */
std::unique_ptr<KVEngine> CreateKVEngine() {
    return std::make_unique<KVEngineImpl>();
}

} // namespace kvfs