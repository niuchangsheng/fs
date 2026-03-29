#include "kv_device.h"

// RocksDB 头文件必须在全局命名空间中包含
#ifdef KVFS_USE_ROCKSDB
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <stdexcept>
#include <atomic>
#include <string>
#endif

#include <unordered_map>
#include <mutex>
#include <fstream>

namespace kvfs {

#ifdef KVFS_USE_ROCKSDB
/**
 * @brief RocksDB 后端实现
 * 使用临时目录模拟 KV 设备
 */
class MockKVDevice : public KVDevice {
public:
    MockKVDevice() : persist_path_(std::string()) {
        rocksdb::Options options;
        options.create_if_missing = true;

        // 使用唯一路径避免锁冲突
        static std::atomic<int> instance_id{0};
        db_path_ = "/tmp/kvfs_rocksdb_" + std::to_string(instance_id++);

        rocksdb::Status status = rocksdb::DB::Open(options, db_path_, &db_);
        if (!status.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }
    }

    MockKVDevice(const std::string& persist_path) : persist_path_(persist_path) {
        rocksdb::Options options;
        options.create_if_missing = true;

        db_path_ = persist_path;
        rocksdb::Status status = rocksdb::DB::Open(options, db_path_, &db_);
        if (!status.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }
    }

    ~MockKVDevice() override {
        delete db_;
        // 只有非持久化路径才清理临时数据
        if (persist_path_.empty()) {
            rocksdb::DestroyDB(db_path_, rocksdb::Options());
        }
    }

    std::future<bool> Put(const std::string& key, std::span<const uint8_t> value) override {
        rocksdb::Slice value_slice(
            reinterpret_cast<const char*>(value.data()),
            value.size()
        );
        rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), key, value_slice);
        std::promise<bool> promise;
        promise.set_value(status.ok());
        return promise.get_future();
    }

    std::future<std::pair<bool, std::vector<uint8_t>>> Get(const std::string& key) override {
        std::promise<std::pair<bool, std::vector<uint8_t>>> promise;
        std::string value;
        rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
        if (status.ok()) {
            promise.set_value({true, std::vector<uint8_t>(
                reinterpret_cast<const uint8_t*>(value.data()),
                reinterpret_cast<const uint8_t*>(value.data() + value.size()))});
        } else {
            promise.set_value({false, {}});
        }
        return promise.get_future();
    }

    std::future<bool> Delete(const std::string& key) override {
        rocksdb::Status status = db_->Delete(rocksdb::WriteOptions(), key);
        std::promise<bool> promise;
        promise.set_value(status.ok());
        return promise.get_future();
    }

    std::future<bool> Exists(const std::string& key) override {
        std::promise<bool> promise;
        std::string value;
        rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
        promise.set_value(status.ok());
        return promise.get_future();
    }

private:
    rocksdb::DB* db_ = nullptr;
    std::string db_path_;
    std::string persist_path_;  // 如果非空，则不清理数据
};
#else
/**
 * @brief 内存后端实现（默认）
 * 使用 unordered_map 模拟 KV 存储
 */
class MockKVDevice : public KVDevice {
public:
    MockKVDevice() : persist_path_(std::string()) {}

    MockKVDevice(const std::string& persist_path) : persist_path_(persist_path) {
        // 如果指定了持久化路径，尝试加载数据
        if (!persist_path_.empty()) {
            loadData();
        }
    }

    std::future<bool> Put(const std::string& key, std::span<const uint8_t> value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[key] = std::vector<uint8_t>(value.begin(), value.end());
        // 如果指定了持久化路径，保存数据
        if (!persist_path_.empty()) {
            saveData();
        }
        std::promise<bool> promise;
        promise.set_value(true);
        return promise.get_future();
    }

    std::future<std::pair<bool, std::vector<uint8_t>>> Get(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        std::promise<std::pair<bool, std::vector<uint8_t>>> promise;
        if (it != store_.end()) {
            promise.set_value({true, it->second});
        } else {
            promise.set_value({false, {}});
        }
        return promise.get_future();
    }

    std::future<bool> Delete(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        bool erased = store_.erase(key) > 0;
        if (!persist_path_.empty()) {
            saveData();
        }
        std::promise<bool> promise;
        promise.set_value(erased);
        return promise.get_future();
    }

    std::future<bool> Exists(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        bool exists = store_.find(key) != store_.end();
        std::promise<bool> promise;
        promise.set_value(exists);
        return promise.get_future();
    }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> store_;
    std::mutex mutex_;
    std::string persist_path_;

    void saveData() {
        if (persist_path_.empty()) return;
        std::ofstream file(persist_path_, std::ios::binary);
        if (file) {
            for (const auto& [key, value] : store_) {
                uint32_t key_len = key.size();
                uint32_t val_len = value.size();
                file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
                file.write(key.data(), key_len);
                file.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
                file.write(reinterpret_cast<const char*>(value.data()), val_len);
            }
        }
    }

    void loadData() {
        std::ifstream file(persist_path_, std::ios::binary);
        if (file) {
            while (file.peek() != EOF) {
                uint32_t key_len, val_len;
                file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
                if (file.eof()) break;
                std::string key(key_len, '\0');
                file.read(key.data(), key_len);
                file.read(reinterpret_cast<char*>(&val_len), sizeof(val_len));
                std::vector<uint8_t> value(val_len);
                file.read(reinterpret_cast<char*>(value.data()), val_len);
                store_[key] = std::move(value);
            }
        }
    }
};
#endif

std::unique_ptr<KVDevice> CreateMockKVDevice() {
    return std::make_unique<MockKVDevice>();
}

std::unique_ptr<KVDevice> CreateMockKVDevice(const std::string& persist_path) {
    return std::make_unique<MockKVDevice>(persist_path);
}

} // namespace kvfs
