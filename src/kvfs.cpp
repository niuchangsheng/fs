#include "kvfs.h"
#include "kv_device.h"
#include "metadata.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <sstream>
#include <vector>

namespace kvfs {

// OpenFlags 位运算支持
inline OpenFlags operator|(OpenFlags a, OpenFlags b) {
    return static_cast<OpenFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline OpenFlags operator&(OpenFlags a, OpenFlags b) {
    return static_cast<OpenFlags>(static_cast<int>(a) & static_cast<int>(b));
}

/**
 * @brief 内部 FileHandle 实现类
 */
class FileHandleImpl : public FileHandle {
public:
    FileHandleImpl(std::string path, uint64_t inode_oid, OpenFlags flags)
        : path_(std::move(path)), inode_oid_(inode_oid), flags_(flags), offset_(0) {}

    const std::string& GetPath() const override { return path_; }
    uint64_t GetOffset() const override { return offset_; }
    uint64_t GetInodeOid() const { return inode_oid_; }
    OpenFlags GetFlags() const { return flags_; }
    void SetOffset(uint64_t offset) { offset_ = offset; }

private:
    std::string path_;
    uint64_t inode_oid_;
    OpenFlags flags_;
    uint64_t offset_;
};

/**
 * @brief 解析路径成分
 */
std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> components;
    if (path.empty() || path[0] != '/') {
        throw std::runtime_error("Path must be absolute: " + path);
    }
    if (path == "/") {
        return components; // 根目录返回空
    }

    std::istringstream ss(path.substr(1)); // 跳过开头的 '/'
    std::string component;
    while (std::getline(ss, component, '/')) {
        if (!component.empty() && component != ".") {
            if (component == "..") {
                if (!components.empty()) {
                    components.pop_back();
                }
            } else {
                components.push_back(component);
            }
        }
    }
    return components;
}

/**
 * @brief 目录条目结构
 */
struct DirEntry {
    std::string name;
    uint64_t inode_oid;
    FileType type;

    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> data(sizeof(uint32_t) + name.size() + sizeof(uint64_t) + sizeof(uint8_t));
        size_t offset = 0;
        uint32_t name_len = static_cast<uint32_t>(name.size());
        std::memcpy(data.data() + offset, &name_len, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(data.data() + offset, name.data(), name.size());
        offset += name.size();
        std::memcpy(data.data() + offset, &inode_oid, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        uint8_t type_val = static_cast<uint8_t>(type);
        std::memcpy(data.data() + offset, &type_val, sizeof(uint8_t));
        return data;
    }

    static DirEntry Deserialize(const std::vector<uint8_t>& data) {
        DirEntry entry;
        size_t offset = 0;
        uint32_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        entry.name.resize(name_len);
        std::memcpy(entry.name.data(), data.data() + offset, name_len);
        offset += name_len;
        std::memcpy(&entry.inode_oid, data.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        uint8_t type_val;
        std::memcpy(&type_val, data.data() + offset, sizeof(uint8_t));
        entry.type = static_cast<FileType>(type_val);
        return entry;
    }
};

/**
 * @brief 目录数据：包含多个 DirEntry
 */
struct DirData {
    std::vector<DirEntry> entries;

    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> data;
        for (const auto& entry : entries) {
            auto serialized = entry.Serialize();
            data.insert(data.end(), serialized.begin(), serialized.end());
        }
        return data;
    }

    static DirData Deserialize(const std::vector<uint8_t>& data) {
        DirData dir;
        size_t offset = 0;
        while (offset < data.size()) {
            if (offset + sizeof(uint32_t) > data.size()) break;
            uint32_t name_len;
            std::memcpy(&name_len, data.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            size_t entry_size = sizeof(uint32_t) + name_len + sizeof(uint64_t) + sizeof(uint8_t);
            if (offset + name_len + sizeof(uint64_t) + sizeof(uint8_t) > data.size()) break;

            std::vector<uint8_t> entry_data(entry_size);
            std::memcpy(entry_data.data(), data.data() + offset - sizeof(uint32_t), entry_size);
            dir.entries.push_back(DirEntry::Deserialize(entry_data));
            offset += name_len + sizeof(uint64_t) + sizeof(uint8_t);
        }
        return dir;
    }
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

    /**
     * @brief 从 inode 数据中提取目录数据
     */
    DirData extractDirData(const std::vector<uint8_t>& inode_data) {
        Inode inode = Inode::Deserialize(inode_data);
        if (inode_data.size() > sizeof(Inode)) {
            // 有 payload 数据
            std::vector<uint8_t> payload(inode_data.begin() + sizeof(Inode), inode_data.end());
            return DirData::Deserialize(payload);
        }
        return DirData{}; // 空目录
    }

    /**
     * @brief 解析路径到 inode OID
     * @return 如果找到返回 inode OID，否则返回 0
     */
    uint64_t resolvePath(const std::string& path) {
        if (path == "/") {
            return 1; // 根目录 inode OID 总是 1
        }

        auto components = splitPath(path);
        if (components.empty()) {
            return 1;
        }

        uint64_t current_oid = 1; // 从根目录开始

        for (size_t i = 0; i < components.size(); ++i) {
            // 读取当前目录 inode
            std::string dir_key = GetInodeKey(current_oid);
            auto [found, dir_data] = device_->Get(dir_key).get();
            if (!found) {
                return 0;
            }

            Inode dir_inode = Inode::Deserialize(dir_data);
            if (dir_inode.type != FileType::Directory) {
                throw std::runtime_error("Path component is not a directory: " + components[i]);
            }

            // 解析目录数据
            DirData dir = extractDirData(dir_data);

            // 查找目标条目
            bool found_entry = false;
            for (const auto& entry : dir.entries) {
                if (entry.name == components[i]) {
                    current_oid = entry.inode_oid;
                    found_entry = true;
                    break;
                }
            }

            if (!found_entry) {
                return 0;
            }
        }

        return current_oid;
    }

    /**
     * @brief 在目录中创建新文件条目
     */
    void createDirEntry(uint64_t dir_oid, const std::string& name, uint64_t file_oid, FileType type) {
        std::string dir_key = GetInodeKey(dir_oid);
        auto [found, dir_data] = device_->Get(dir_key).get();

        Inode dir_inode;
        DirData dir;
        if (found) {
            dir_inode = Inode::Deserialize(dir_data);
            dir = extractDirData(dir_data);
        } else {
            throw std::runtime_error("Directory inode not found: " + std::to_string(dir_oid));
        }

        // 添加新条目
        DirEntry entry{name, file_oid, type};
        dir.entries.push_back(entry);

        // 更新目录大小
        auto serialized_dir = dir.Serialize();
        dir_inode.size = serialized_dir.size();

        // 更新时间
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        dir_inode.mtime = now;
        dir_inode.ctime = now;

        // 写回目录
        std::vector<uint8_t> new_dir_data = dir_inode.Serialize();
        new_dir_data.insert(new_dir_data.end(), serialized_dir.begin(), serialized_dir.end());
        device_->Put(dir_key, new_dir_data).get();
    }

    std::future<std::shared_ptr<FileHandle>> Open(const std::string& path, OpenFlags flags) override {
        std::promise<std::shared_ptr<FileHandle>> promise;

        try {
            // 解析路径
            uint64_t inode_oid = resolvePath(path);

            // 检查是否使用 Create 标志
            bool create = (static_cast<int>(flags) & static_cast<int>(OpenFlags::Create)) != 0;
            bool truncate = (static_cast<int>(flags) & static_cast<int>(OpenFlags::Truncate)) != 0;

            if (inode_oid == 0) {
                // 文件不存在
                if (!create) {
                    throw std::runtime_error("File not found: " + path);
                }

                // 创建新文件
                // 1. 确定父目录 OID
                auto components = splitPath(path);
                uint64_t parent_oid = 1; // 默认根目录

                if (components.size() > 1) {
                    // 解析父目录
                    std::string parent_path = "/";
                    for (size_t i = 0; i < components.size() - 1; ++i) {
                        parent_path += components[i] + "/";
                    }
                    // 移除尾部的 '/'
                    if (parent_path.size() > 1) {
                        parent_path.pop_back();
                    }
                    parent_oid = resolvePath(parent_path);
                    if (parent_oid == 0) {
                        throw std::runtime_error("Parent directory not found: " + parent_path);
                    }
                }

                // 2. 分配新 inode OID
                // 读取 superblock 获取下一个 OID
                std::string sb_key = GetSuperblockKey();
                auto [sb_found, sb_data] = device_->Get(sb_key).get();
                if (!sb_found) {
                    throw std::runtime_error("Superblock not found");
                }
                Superblock sb = Superblock::Deserialize(sb_data);

                inode_oid = sb.next_inode_oid;
                sb.next_inode_oid++;

                // 3. 创建新 inode
                Inode new_inode{};
                new_inode.oid = inode_oid;
                new_inode.type = FileType::RegularFile;
                new_inode.mode = 0644;
                new_inode.size = 0;
                new_inode.link_count = 1;
                new_inode.is_inline = true;
                new_inode.inline_data_len = 0;

                auto now = std::chrono::system_clock::now().time_since_epoch().count();
                new_inode.atime = new_inode.mtime = new_inode.ctime = now;

                // 4. 写入 inode 和目录条目
                std::string inode_key = GetInodeKey(inode_oid);
                std::vector<uint8_t> inode_data = new_inode.Serialize();
                device_->Put(inode_key, inode_data).get();

                // 写入父目录条目
                createDirEntry(parent_oid, components.back(), inode_oid, FileType::RegularFile);

                // 5. 更新 superblock
                device_->Put(sb_key, sb.Serialize()).get();

                std::cout << "Created new file: " << path << " (inode " << inode_oid << ")" << std::endl;
            } else {
                // 文件已存在
                std::cout << "Opened existing file: " << path << " (inode " << inode_oid << ")" << std::endl;

                // 处理 Truncate 标志
                if (truncate) {
                    std::string inode_key = GetInodeKey(inode_oid);
                    auto [found, inode_data] = device_->Get(inode_key).get();
                    if (found) {
                        Inode inode = Inode::Deserialize(inode_data);
                        inode.size = 0;
                        inode.inline_data_len = 0;
                        inode.is_inline = true;
                        auto now = std::chrono::system_clock::now().time_since_epoch().count();
                        inode.mtime = now;
                        device_->Put(inode_key, inode.Serialize()).get();
                    }
                }
            }

            auto handle = std::make_shared<FileHandleImpl>(path, inode_oid, flags);
            promise.set_value(handle);
        } catch (const std::exception& e) {
            std::cerr << "Open failed: " << e.what() << std::endl;
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<ssize_t> Read(std::shared_ptr<FileHandle> handle, std::span<uint8_t> buf, size_t count) override {
        std::promise<ssize_t> promise;

        // Check handle type
        auto impl = std::dynamic_pointer_cast<FileHandleImpl>(handle);
        if (!impl) {
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Invalid handle type")));
            return promise.get_future();
        }

        // Check if handle is write-only
        OpenFlags flags = impl->GetFlags();
        bool write_only = (static_cast<int>(flags) & static_cast<int>(OpenFlags::WriteOnly)) != 0;

        if (write_only) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot read from write-only file handle")));
            return promise.get_future();
        }

        // TODO: Implement actual read logic
        (void)buf; (void)count;
        promise.set_value(0);
        return promise.get_future();
    }

    std::future<ssize_t> Write(std::shared_ptr<FileHandle> handle, std::span<const uint8_t> data) override {
        std::promise<ssize_t> promise;

        // Check if handle is read-only
        auto impl = std::dynamic_pointer_cast<FileHandleImpl>(handle);
        if (!impl) {
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Invalid handle type")));
            return promise.get_future();
        }

        OpenFlags flags = impl->GetFlags();
        bool read_only = (static_cast<int>(flags) & static_cast<int>(OpenFlags::ReadOnly)) != 0;

        if (read_only) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot write to read-only file handle")));
            return promise.get_future();
        }

        // TODO: Implement actual write logic
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