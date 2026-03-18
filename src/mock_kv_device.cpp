#include "kv_device.h"
#include <unordered_map>
#include <mutex>

namespace kvfs {

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

std::unique_ptr<KVDevice> CreateMockKVDevice() {
    return std::make_unique<MockKVDevice>();
}

} // namespace kvfs
