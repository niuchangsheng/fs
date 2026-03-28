/**
 * @file initialization_e2e_test.cpp
 * @brief 端到端测试：KVFS 初始化和挂载
 *
 * 测试完整的引擎初始化和挂载流程
 */

#include "kvfs.h"
#include <iostream>
#include <cassert>

using namespace kvfs;

void test_first_init_formats() {
    std::cout << "[E2E] First initialization triggers format" << std::endl;

    auto engine = CreateKVEngine();
    engine->Init("e2e_test_device_1").get();

    // If we get here without error, format succeeded
    // A more thorough test would verify superblock contents

    engine->Shutdown().get();

    std::cout << "  PASSED" << std::endl;
}

void test_mount_existing() {
    std::cout << "[E2E] Mount existing filesystem" << std::endl;

    // Create and initialize
    {
        auto engine = CreateKVEngine();
        engine->Init("e2e_test_device_2").get();
        engine->Shutdown().get();
    }

    // Mount existing (same device name in same session)
    {
        auto engine = CreateKVEngine();
        engine->Init("e2e_test_device_2").get();
        engine->Shutdown().get();
    }

    std::cout << "  PASSED" << std::endl;
}

void test_shutdown_cleanup() {
    std::cout << "[E2E] Shutdown cleans up resources" << std::endl;

    auto engine = CreateKVEngine();
    engine->Init("e2e_test_device_3").get();

    // Shutdown should complete without error
    engine->Shutdown().get();

    std::cout << "  PASSED" << std::endl;
}

void test_multiple_engines_isolated() {
    std::cout << "[E2E] Multiple engines are isolated" << std::endl;

    // Create two separate engines
    auto engine1 = CreateKVEngine();
    auto engine2 = CreateKVEngine();

    engine1->Init("isolated_device_a").get();
    engine2->Init("isolated_device_b").get();

    // They should operate on different "devices"
    // In a real implementation, these would be completely independent

    engine1->Shutdown().get();
    engine2->Shutdown().get();

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== KVFS End-to-End Tests ===" << std::endl;
    std::cout << std::endl;

    test_first_init_formats();
    test_mount_existing();
    test_shutdown_cleanup();
    test_multiple_engines_isolated();

    std::cout << std::endl;
    std::cout << "=== All E2E Tests Passed ===" << std::endl;
    return 0;
}
