/**
 * @file initialization_test.cpp
 * @brief 端到端测试：KVFS 初始化和挂载
 */

#include "kvfs.h"
#include <gtest/gtest.h>

using namespace kvfs;

class InitializationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = CreateKVEngine();
    }

    void TearDown() override {
        if (engine) {
            engine->Shutdown().get();
        }
    }

    std::unique_ptr<KVEngine> engine;
};

TEST_F(InitializationE2ETest, FirstInitFormats) {
    engine->Init("e2e_test_device_1").get();
    // If we get here without error, format succeeded
}

TEST_F(InitializationE2ETest, MountExisting) {
    // Create and initialize
    engine->Init("e2e_test_device_2").get();
    engine->Shutdown().get();
    engine.reset();

    // Mount existing
    engine = CreateKVEngine();
    engine->Init("e2e_test_device_2").get();
}

TEST_F(InitializationE2ETest, ShutdownCleanup) {
    engine->Init("e2e_test_device_3").get();
    engine->Shutdown().get();
}

TEST_F(InitializationE2ETest, MultipleEnginesIsolated) {
    auto engine1 = CreateKVEngine();
    auto engine2 = CreateKVEngine();

    engine1->Init("isolated_device_a").get();
    engine2->Init("isolated_device_b").get();

    engine1->Shutdown().get();
    engine2->Shutdown().get();
}
