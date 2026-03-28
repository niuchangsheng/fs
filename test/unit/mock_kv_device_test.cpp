/**
 * @file mock_kv_device_test.cpp
 * @brief 单元测试：MockKVDevice
 *
 * 测试内存模拟 KV 设备的基本操作
 */

#include "kv_device.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace kvfs;

void test_put_get() {
    std::cout << "[TEST] Put/Get basic operation" << std::endl;

    auto device = CreateMockKVDevice();

    std::string key = "test_key";
    std::string value = "Hello, KVFS!";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    bool put_result = device->Put(key, data).get();
    assert(put_result == true);

    auto [found, retrieved] = device->Get(key).get();
    assert(found == true);
    assert(retrieved.size() == value.size());
    assert(memcmp(retrieved.data(), value.data(), value.size()) == 0);

    std::cout << "  PASSED" << std::endl;
}

void test_get_nonexistent() {
    std::cout << "[TEST] Get non-existent key" << std::endl;

    auto device = CreateMockKVDevice();

    auto [found, retrieved] = device->Get("nonexistent").get();
    assert(found == false);

    std::cout << "  PASSED" << std::endl;
}

void test_delete() {
    std::cout << "[TEST] Delete operation" << std::endl;

    auto device = CreateMockKVDevice();

    std::string key = "to_delete";
    std::string value = "data";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    // Put then delete
    device->Put(key, data).get();
    bool del_result = device->Delete(key).get();
    assert(del_result == true);

    // Verify deleted
    auto [found, retrieved] = device->Get(key).get();
    assert(found == false);

    std::cout << "  PASSED" << std::endl;
}

void test_exists() {
    std::cout << "[TEST] Exists operation" << std::endl;

    auto device = CreateMockKVDevice();

    std::string key = "exists_test";
    std::string value = "data";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    // Not exists initially
    assert(device->Exists(key).get() == false);

    // Exists after put
    device->Put(key, data).get();
    assert(device->Exists(key).get() == true);

    // Not exists after delete
    device->Delete(key).get();
    assert(device->Exists(key).get() == false);

    std::cout << "  PASSED" << std::endl;
}

void test_persistence_within_session() {
    std::cout << "[TEST] Data persistence within session" << std::endl;

    auto device = CreateMockKVDevice();

    // Put multiple keys
    for (int i = 0; i < 5; i++) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        std::span<const uint8_t> data(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.size()
        );
        device->Put(key, data).get();
    }

    // Verify all keys still exist
    for (int i = 0; i < 5; i++) {
        std::string key = "key_" + std::to_string(i);
        assert(device->Exists(key).get() == true);
    }

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== MockKVDevice Unit Tests ===" << std::endl;
    std::cout << std::endl;

    test_put_get();
    test_get_nonexistent();
    test_delete();
    test_exists();
    test_persistence_within_session();

    std::cout << std::endl;
    std::cout << "=== All Unit Tests Passed ===" << std::endl;
    return 0;
}
