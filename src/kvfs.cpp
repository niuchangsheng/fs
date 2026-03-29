#include "kvfs.h"
#include "kv_device.h"
#include "metadata.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <sstream>
#include <vector>
#include <algorithm>

namespace kvfs {

/**
 * @brief 内部 FileHandle 实现类
 */
class FileHandleImpl : public FileHandle {
public:
    FileHandleImpl(std::string path, uint64_t inode_oid, OpenFlags flags)
        : path_(std::move(path)), inode_oid_(inode_oid), flags_(flags), offset_(0), closed_(false) {}

    const std::string& GetPath() const override { return path_; }
    uint64_t GetOffset() const override { return offset_; }
    uint64_t GetInodeOid() const { return inode_oid_; }
    OpenFlags GetFlags() const { return flags_; }
    bool IsClosed() const override { return closed_; }
    void SetOffset(uint64_t offset) { offset_ = offset; }
    void SetClosed(bool closed) { closed_ = closed; }

private:
    std::string path_;
    uint64_t inode_oid_;
    OpenFlags flags_;
    uint64_t offset_;
    bool closed_;
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
 * @brief 将路径成分连接为路径
 */
std::string joinPath(const std::vector<std::string>& components, size_t count) {
    std::string path;
    for (size_t i = 0; i < count && i < components.size(); ++i) {
        path += "/" + components[i];
    }
    return path.empty() ? "/" : path;
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

    KVEngineImpl(const std::string& persist_path) : device_(CreateMockKVDevice(persist_path)) {}

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
     * @brief 在目录中删除文件条目
     */
    bool removeDirEntry(uint64_t dir_oid, const std::string& name) {
        std::string dir_key = GetInodeKey(dir_oid);
        auto [found, dir_data] = device_->Get(dir_key).get();

        Inode dir_inode;
        DirData dir;
        if (!found) {
            return false;
        }

        dir_inode = Inode::Deserialize(dir_data);
        dir = extractDirData(dir_data);

        // 查找并删除条目
        bool removed = false;
        auto it = std::find_if(dir.entries.begin(), dir.entries.end(),
            [&name](const DirEntry& entry) { return entry.name == name; });

        if (it != dir.entries.end()) {
            dir.entries.erase(it);
            removed = true;
        }

        if (removed) {
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

        return removed;
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
            // 处理相对路径
            std::string full_path = path;
            if (!path.empty() && path[0] != '/') {
                // 相对路径：拼接到当前工作目录
                if (current_dir_ == "/") {
                    full_path = "/" + path;
                } else {
                    full_path = current_dir_ + "/" + path;
                }
            }

            // 解析路径
            uint64_t inode_oid = resolvePath(full_path);

            // 检查是否使用 Create 标志
            bool create = (static_cast<int>(flags) & static_cast<int>(OpenFlags::Create)) != 0;
            bool truncate = (static_cast<int>(flags) & static_cast<int>(OpenFlags::Truncate)) != 0;

            if (inode_oid == 0) {
                // 文件不存在
                if (!create) {
                    throw std::runtime_error("File not found: " + full_path);
                }

                // 创建新文件
                // 1. 确定父目录 OID
                auto components = splitPath(full_path);
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
                std::cout << "Opened existing file: " << full_path << " (inode " << inode_oid << ")" << std::endl;

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

            auto handle = std::make_shared<FileHandleImpl>(full_path, inode_oid, flags);

            // 处理 Append 标志：将偏移量设置到文件末尾
            bool append = (static_cast<int>(flags) & static_cast<int>(OpenFlags::Append)) != 0;
            if (append && inode_oid != 0) {
                std::string inode_key = GetInodeKey(inode_oid);
                auto [found, inode_data] = device_->Get(inode_key).get();
                if (found) {
                    Inode inode = Inode::Deserialize(inode_data);
                    handle->SetOffset(inode.size);
                }
            }

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

        // Check if handle is closed
        if (impl->IsClosed()) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot read from closed file handle")));
            return promise.get_future();
        }

        // Check if handle is write-only (ReadWrite = 0x03, WriteOnly = 0x02)
        OpenFlags flags = impl->GetFlags();
        bool write_only = (static_cast<int>(flags) & 0x03) == static_cast<int>(OpenFlags::WriteOnly);

        if (write_only) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot read from write-only file handle")));
            return promise.get_future();
        }

        try {
            uint64_t inode_oid = impl->GetInodeOid();
            std::string inode_key = GetInodeKey(inode_oid);
            auto [found, inode_data] = device_->Get(inode_key).get();

            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Inode not found: " + std::to_string(inode_oid))));
                return promise.get_future();
            }

            Inode inode = Inode::Deserialize(inode_data);

            // 计算实际可读取的数据量
            size_t available = inode.size;
            size_t offset = impl->GetOffset();

            std::cout << "Read: offset=" << offset << ", size=" << available
                      << ", inline_data_len=" << inode.inline_data_len
                      << ", is_inline=" << inode.is_inline << std::endl;

            if (offset >= available) {
                // 已经到达文件末尾
                std::cout << "Read: at end of file, returning 0" << std::endl;
                promise.set_value(0);
                return promise.get_future();
            }

            size_t bytes_to_read = std::min(count, available - offset);

            // 读取数据
            if (inode.is_inline) {
                // 内联数据存储在 inode 后面
                if (offset < inode.inline_data_len) {
                    size_t copy_len = std::min(bytes_to_read, inode.inline_data_len - offset);
                    const uint8_t* inline_data = inode_data.data() + sizeof(Inode);
                    std::memcpy(buf.data(), inline_data + offset, copy_len);

                    // 更新偏移量
                    impl->SetOffset(offset + copy_len);

                    // 更新访问时间
                    auto now = std::chrono::system_clock::now().time_since_epoch().count();
                    inode.atime = now;
                    std::vector<uint8_t> updated_inode = inode.Serialize();
                    updated_inode.insert(updated_inode.end(),
                                         inode_data.begin() + sizeof(Inode),
                                         inode_data.end());
                    device_->Put(inode_key, updated_inode).get();

                    promise.set_value(static_cast<ssize_t>(copy_len));
                } else {
                    promise.set_value(0);
                }
            } else {
                // TODO: 非内联数据（chunk 模式）
                promise.set_value(0);
            }
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

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

        // Check if handle is closed
        if (impl->IsClosed()) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot write to closed file handle")));
            return promise.get_future();
        }

        OpenFlags flags = impl->GetFlags();
        // Check if handle is read-only (ReadWrite = 0x03, ReadOnly = 0x01)
        bool read_only = (static_cast<int>(flags) & 0x03) == static_cast<int>(OpenFlags::ReadOnly);

        if (read_only) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot write to read-only file handle")));
            return promise.get_future();
        }

        try {
            uint64_t inode_oid = impl->GetInodeOid();
            std::string inode_key = GetInodeKey(inode_oid);
            auto [found, inode_data] = device_->Get(inode_key).get();

            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Inode not found: " + std::to_string(inode_oid))));
                return promise.get_future();
            }

            Inode inode = Inode::Deserialize(inode_data);

            // 计算写入位置
            size_t offset = impl->GetOffset();

            // 检查是否需要扩展数据
            size_t new_size = offset + data.size();

            // 简单实现：内联存储所有数据（适用于小文件）
            // 重新构建 inode + 数据
            std::vector<uint8_t> new_inode_data = inode.Serialize();

            // 获取现有数据（如果有）
            std::vector<uint8_t> existing_data;
            if (inode.is_inline && inode.inline_data_len > 0) {
                existing_data.assign(inode_data.begin() + sizeof(Inode),
                                     inode_data.begin() + sizeof(Inode) + inode.inline_data_len);
            }

            // 扩展或创建数据缓冲区
            if (new_size > existing_data.size()) {
                existing_data.resize(new_size);
            }

            // 复制新数据到指定位置
            std::memcpy(existing_data.data() + offset, data.data(), data.size());

            // 更新 inode 元数据
            inode.size = new_size;
            inode.inline_data_len = new_size;
            inode.is_inline = true;

            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            inode.mtime = now;
            inode.ctime = now;

            // 构建新的 inode 数据（inode 结构 + 内联数据）
            std::vector<uint8_t> updated_inode_data = inode.Serialize();
            updated_inode_data.insert(updated_inode_data.end(),
                                      existing_data.begin(),
                                      existing_data.end());

            // 写入设备
            device_->Put(inode_key, updated_inode_data).get();

            // 更新偏移量
            impl->SetOffset(offset + data.size());

            promise.set_value(static_cast<ssize_t>(data.size()));
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    off_t Lseek(std::shared_ptr<FileHandle> handle, off_t offset, Whence whence) override {
        auto impl = std::dynamic_pointer_cast<FileHandleImpl>(handle);
        if (!impl) {
            return -1;
        }

        // Check if handle is closed
        if (impl->IsClosed()) {
            return -1;
        }

        off_t new_offset;
        switch (whence) {
            case Whence::Set:
                new_offset = offset;
                break;
            case Whence::Cur:
                new_offset = static_cast<off_t>(impl->GetOffset()) + offset;
                break;
            case Whence::End:
                // 需要获取文件大小
                {
                    uint64_t inode_oid = impl->GetInodeOid();
                    std::string inode_key = GetInodeKey(inode_oid);
                    auto [found, inode_data] = device_->Get(inode_key).get();
                    if (!found) {
                        return -1;
                    }
                    Inode inode = Inode::Deserialize(inode_data);
                    new_offset = static_cast<off_t>(inode.size) + offset;
                }
                break;
            default:
                return -1;
        }

        if (new_offset < 0) {
            return -1;
        }

        impl->SetOffset(static_cast<uint64_t>(new_offset));
        return new_offset;
    }

    std::future<int> Close(std::shared_ptr<FileHandle> handle) override {
        std::promise<int> promise;

        auto impl = std::dynamic_pointer_cast<FileHandleImpl>(handle);
        if (!impl) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Invalid handle type")));
            return promise.get_future();
        }

        // Check if already closed
        if (impl->IsClosed()) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("File handle already closed")));
            return promise.get_future();
        }

        // Mark handle as closed
        impl->SetClosed(true);

        std::cout << "Closed file: " << impl->GetPath() << std::endl;
        promise.set_value(0);
        return promise.get_future();
    }

    std::future<int> Unlink(const std::string& path) override {
        std::promise<int> promise;

        try {
            // 根目录不能删除
            if (path == "/") {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Cannot unlink root directory")));
                return promise.get_future();
            }

            // 解析路径获取文件 inode
            uint64_t file_oid = resolvePath(path);
            if (file_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("File not found: " + path)));
                return promise.get_future();
            }

            // 获取父目录 OID
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

            // 从父目录中删除条目
            if (!removeDirEntry(parent_oid, components.back())) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Failed to remove directory entry")));
                return promise.get_future();
            }

            // 删除文件 inode
            std::string inode_key = GetInodeKey(file_oid);
            device_->Delete(inode_key).get();

            std::cout << "Unlinked file: " << path << " (inode " << file_oid << ")" << std::endl;
            promise.set_value(0);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<struct stat> Stat(const std::string& path) override {
        std::promise<struct stat> promise;

        try {
            // 解析路径获取 inode OID
            uint64_t inode_oid = resolvePath(path);
            if (inode_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("File not found: " + path)));
                return promise.get_future();
            }

            // 读取 inode
            std::string inode_key = GetInodeKey(inode_oid);
            auto [found, inode_data] = device_->Get(inode_key).get();
            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Inode not found: " + std::to_string(inode_oid))));
                return promise.get_future();
            }

            Inode inode = Inode::Deserialize(inode_data);

            // 填充 stat 结构
            struct stat st = {};
            st.st_size = static_cast<off_t>(inode.size);
            st.st_mode = inode.mode;
            st.st_uid = inode.uid;
            st.st_gid = inode.gid;
            st.st_atime = static_cast<time_t>(inode.atime / 1000000000ULL);
            st.st_mtime = static_cast<time_t>(inode.mtime / 1000000000ULL);
            st.st_ctime = static_cast<time_t>(inode.ctime / 1000000000ULL);

            // 设置文件类型
            if (inode.type == FileType::Directory) {
                st.st_mode |= S_IFDIR;
            } else {
                st.st_mode |= S_IFREG;
            }

            promise.set_value(st);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<int> Fsync(std::shared_ptr<FileHandle> handle) override {
        (void)handle;
        std::promise<int> promise;
        promise.set_value(0);
        return promise.get_future();
    }

    std::future<int> Mkdir(const std::string& path) override {
        std::promise<int> promise;

        try {
            // 解析路径，获取父目录和目录名
            std::vector<std::string> components = splitPath(path);
            if (components.empty()) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Invalid path: " + path)));
                return promise.get_future();
            }

            // 获取父目录 OID
            std::string parent_path = joinPath(components, components.size() - 1);
            uint64_t parent_oid = resolvePath(parent_path);
            if (parent_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Parent directory not found: " + parent_path)));
                return promise.get_future();
            }

            // 检查目录是否已存在
            uint64_t existing_oid = resolvePath(path);
            if (existing_oid != 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory already exists: " + path)));
                return promise.get_future();
            }

            // 创建新目录 inode
            std::string sb_key = GetSuperblockKey();
            auto [sb_found, sb_data] = device_->Get(sb_key).get();
            if (!sb_found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Superblock not found")));
                return promise.get_future();
            }
            Superblock sb = Superblock::Deserialize(sb_data);
            uint64_t dir_oid = sb.next_inode_oid;
            sb.next_inode_oid++;

            Inode dir_inode{};
            dir_inode.oid = dir_oid;
            dir_inode.type = FileType::Directory;
            dir_inode.mode = 0755;
            dir_inode.size = 0;
            dir_inode.link_count = 1;
            dir_inode.is_inline = true;
            dir_inode.inline_data_len = 0;

            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            dir_inode.atime = dir_inode.mtime = dir_inode.ctime = now;

            // 写入目录 inode
            std::string dir_key = GetInodeKey(dir_oid);
            std::vector<uint8_t> dir_data = dir_inode.Serialize();
            device_->Put(dir_key, dir_data).get();

            // 写入父目录条目
            createDirEntry(parent_oid, components.back(), dir_oid, FileType::Directory);

            // 更新 superblock
            device_->Put(sb_key, sb.Serialize()).get();

            std::cout << "Created new directory: " << path << " (inode " << dir_oid << ")" << std::endl;
            promise.set_value(0);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<std::vector<KVEngine::DirEntryInfo>> Readdir(const std::string& path) override {
        std::promise<std::vector<KVEngine::DirEntryInfo>> promise;

        try {
            // 解析路径获取目录 inode OID
            uint64_t dir_oid = resolvePath(path);
            if (dir_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory not found: " + path)));
                return promise.get_future();
            }

            // 读取目录 inode
            std::string dir_key = GetInodeKey(dir_oid);
            auto [found, dir_data] = device_->Get(dir_key).get();
            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory inode not found: " + std::to_string(dir_oid))));
                return promise.get_future();
            }

            Inode dir_inode = Inode::Deserialize(dir_data);

            // 验证是目录
            if (dir_inode.type != FileType::Directory) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Not a directory: " + path)));
                return promise.get_future();
            }

            // 提取目录条目
            DirData dir = extractDirData(dir_data);

            // 转换为 DirEntryInfo 列表
            std::vector<KVEngine::DirEntryInfo> entries;
            for (const auto& entry : dir.entries) {
                KVEngine::DirEntryInfo info;
                info.name = entry.name;
                info.type = entry.type;
                entries.push_back(info);
            }

            promise.set_value(entries);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<int> Rmdir(const std::string& path) override {
        std::promise<int> promise;

        try {
            // 根目录不能删除
            if (path == "/") {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Cannot remove root directory")));
                return promise.get_future();
            }

            // 解析路径获取目录 OID
            uint64_t dir_oid = resolvePath(path);
            if (dir_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory not found: " + path)));
                return promise.get_future();
            }

            // 读取目录 inode，验证是目录且为空
            std::string dir_key = GetInodeKey(dir_oid);
            auto [found, dir_data] = device_->Get(dir_key).get();
            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory inode not found: " + std::to_string(dir_oid))));
                return promise.get_future();
            }

            Inode dir_inode = Inode::Deserialize(dir_data);

            // 验证是目录
            if (dir_inode.type != FileType::Directory) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Not a directory: " + path)));
                return promise.get_future();
            }

            // 验证目录为空
            DirData dir = extractDirData(dir_data);
            if (!dir.entries.empty()) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory not empty: " + path)));
                return promise.get_future();
            }

            // 获取父目录 OID
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
            }

            // 从父目录中删除条目
            if (!removeDirEntry(parent_oid, components.back())) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Failed to remove directory entry")));
                return promise.get_future();
            }

            // 删除目录 inode
            device_->Delete(dir_key).get();

            std::cout << "Removed directory: " << path << " (inode " << dir_oid << ")" << std::endl;
            promise.set_value(0);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    std::future<int> Chdir(const std::string& path) override {
        std::promise<int> promise;

        try {
            // 解析路径获取目录 OID
            uint64_t dir_oid = resolvePath(path);
            if (dir_oid == 0) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory not found: " + path)));
                return promise.get_future();
            }

            // 读取目录 inode，验证是目录
            std::string dir_key = GetInodeKey(dir_oid);
            auto [found, dir_data] = device_->Get(dir_key).get();
            if (!found) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Directory inode not found: " + std::to_string(dir_oid))));
                return promise.get_future();
            }

            Inode dir_inode = Inode::Deserialize(dir_data);
            if (dir_inode.type != FileType::Directory) {
                promise.set_exception(std::make_exception_ptr(
                    std::runtime_error("Not a directory: " + path)));
                return promise.get_future();
            }

            // 设置当前工作目录
            current_dir_ = path;

            promise.set_value(0);
        } catch (const std::exception& e) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

private:
    std::unique_ptr<KVDevice> device_;
    std::string current_dir_ = "/";  // 默认当前目录为根目录
};

/**
 * @brief 工厂函数实现
 */
std::unique_ptr<KVEngine> CreateKVEngine() {
    return std::make_unique<KVEngineImpl>();
}

std::unique_ptr<KVEngine> CreateKVEngine(const std::string& persist_path) {
    return std::make_unique<KVEngineImpl>(persist_path);
}

} // namespace kvfs