#include "kv_device.h"

// RocksDB 头文件必须在全局命名空间中包含
#ifdef KVFS_USE_ROCKSDB
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <stdexcept>
#endif

#include <unordered_map>
#include <mutex>

namespace kvfs {

#ifdef KVFS_USE_ROCKSDB
/**
 * @brief RocksDB 后端实现
 */
class MockKVDevice : public KVDevice {
public:
    MockKVDevice() {
        rocksdb::Options options;
        options.create_if_missing = true;
        rocksdb::Status status = rocksdb::DB::Open(options, ":memory:", &db_);
        if (!status.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }
    }

    ~MockKVDevice() override {
        delete db_;
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
};
#else
/**
 * @brief 内存后端实现（默认）
 * 使用 unordered_map 模拟 KV 存储
 */
class MockKVDevice : public KVDevice {
public:
    std::future<bool> Put(const std::string& key, std::span<const uint8_t> value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[key] = std::vector<uint8_t>(value.begin(), value.end());
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
};
#endif

std::unique_ptr<KVDevice> CreateMockKVDevice() {
    return std::make_unique<MockKVDevice>();
}

} // namespace kvfs
