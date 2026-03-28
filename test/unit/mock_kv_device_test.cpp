/**
 * @file mock_kv_device_test.cpp
 * @brief 单元测试：MockKVDevice
 */

#include "kv_device.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace kvfs;

class MockKVDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = CreateMockKVDevice();
    }

    std::unique_ptr<KVDevice> device;
};

TEST_F(MockKVDeviceTest, PutAndGet) {
    std::string key = "test_key";
    std::string value = "Hello, KVFS!";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    bool put_result = device->Put(key, data).get();
    ASSERT_TRUE(put_result);

    auto [found, retrieved] = device->Get(key).get();
    ASSERT_TRUE(found);
    ASSERT_EQ(retrieved.size(), value.size());
    ASSERT_EQ(memcmp(retrieved.data(), value.data(), value.size()), 0);
}

TEST_F(MockKVDeviceTest, GetNonExistent) {
    auto [found, retrieved] = device->Get("nonexistent").get();
    ASSERT_FALSE(found);
}

TEST_F(MockKVDeviceTest, Delete) {
    std::string key = "to_delete";
    std::string value = "data";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    device->Put(key, data).get();

    bool del_result = device->Delete(key).get();
    ASSERT_TRUE(del_result);

    auto [found, retrieved] = device->Get(key).get();
    ASSERT_FALSE(found);
}

TEST_F(MockKVDeviceTest, Exists) {
    std::string key = "exists_test";
    std::string value = "data";
    std::span<const uint8_t> data(
        reinterpret_cast<const uint8_t*>(value.data()),
        value.size()
    );

    ASSERT_FALSE(device->Exists(key).get());

    device->Put(key, data).get();
    ASSERT_TRUE(device->Exists(key).get());

    device->Delete(key).get();
    ASSERT_FALSE(device->Exists(key).get());
}

TEST_F(MockKVDeviceTest, PersistenceWithinSession) {
    for (int i = 0; i < 5; i++) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        std::span<const uint8_t> data(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.size()
        );
        device->Put(key, data).get();
    }

    for (int i = 0; i < 5; i++) {
        std::string key = "key_" + std::to_string(i);
        ASSERT_TRUE(device->Exists(key).get());
    }
}
