/**
 * @file file_operations_test.cpp
 * @brief 端到端测试：文件操作
 */

#include "kvfs.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

using namespace kvfs;

class FileOperationsE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = CreateKVEngine();
        engine->Init("file_test_device").get();
    }

    void TearDown() override {
        engine->Shutdown().get();
    }

    std::unique_ptr<KVEngine> engine;
};

TEST_F(FileOperationsE2ETest, OpenCreateFile) {
    // file-001: Open file with Create flag creates new file
    auto handle = engine->Open("/test.txt", OpenFlags::Create).get();

    // Verify FileHandle is returned (not null)
    ASSERT_NE(handle, nullptr);

    // Verify handle->GetPath() returns '/test.txt'
    ASSERT_EQ(handle->GetPath(), "/test.txt");
}

TEST_F(FileOperationsE2ETest, WriteThenReadRoundtrip) {
    // TODO: Implement Open/Write/Read APIs
    GTEST_SKIP() << "Write/Read APIs not yet implemented";
}

TEST_F(FileOperationsE2ETest, OpenReadOnlyFile) {
    // file-002: Open file with ReadOnly flag
    // Step 1: Create a file with some content
    auto write_handle = engine->Open("/readonly_test.txt", OpenFlags::Create).get();
    ASSERT_NE(write_handle, nullptr);

    std::string content = "Hello, ReadOnly Test!";
    std::vector<uint8_t> data(content.begin(), content.end());
    engine->Write(write_handle, data).get();
    engine->Close(write_handle).get();

    // Step 2: Open file with OpenFlags::ReadOnly
    auto ro_handle = engine->Open("/readonly_test.txt", OpenFlags::ReadOnly).get();
    ASSERT_NE(ro_handle, nullptr);
    ASSERT_EQ(ro_handle->GetPath(), "/readonly_test.txt");

    // Step 3: Verify write operations fail on ReadOnly handle
    std::vector<uint8_t> write_data = {1, 2, 3};
    auto write_future = engine->Write(ro_handle, write_data);

    // Write should throw exception or return error
    bool write_failed = false;
    try {
        write_future.get();
    } catch (const std::exception& e) {
        write_failed = true;
        std::cout << "Expected write failure: " << e.what() << std::endl;
    }

    ASSERT_TRUE(write_failed) << "Write on ReadOnly handle should fail";
}

TEST_F(FileOperationsE2ETest, UnlinkFile) {
    // TODO: Implement Open/Unlink APIs
    GTEST_SKIP() << "Unlink API not yet implemented";
}

TEST_F(FileOperationsE2ETest, MultipleFilesCoexist) {
    // TODO: Implement for multiple files
    GTEST_SKIP() << "File operations not yet implemented";
}
