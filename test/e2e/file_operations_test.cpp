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

TEST_F(FileOperationsE2ETest, UnlinkFile) {
    // TODO: Implement Open/Unlink APIs
    GTEST_SKIP() << "Unlink API not yet implemented";
}

TEST_F(FileOperationsE2ETest, MultipleFilesCoexist) {
    // TODO: Implement for multiple files
    GTEST_SKIP() << "File operations not yet implemented";
}
