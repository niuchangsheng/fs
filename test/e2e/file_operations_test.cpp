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

TEST_F(FileOperationsE2ETest, OpenWriteOnlyFile) {
    // file-003: Open file with WriteOnly flag
    // Step 1: Create a file with some content
    auto read_handle = engine->Open("/writeonly_test.txt", OpenFlags::Create).get();
    ASSERT_NE(read_handle, nullptr);

    std::string content = "Hello, WriteOnly Test!";
    std::vector<uint8_t> data(content.begin(), content.end());
    engine->Write(read_handle, data).get();
    engine->Close(read_handle).get();

    // Step 2: Open file with OpenFlags::WriteOnly
    auto wo_handle = engine->Open("/writeonly_test.txt", OpenFlags::WriteOnly).get();
    ASSERT_NE(wo_handle, nullptr);
    ASSERT_EQ(wo_handle->GetPath(), "/writeonly_test.txt");

    // Step 3: Verify read operations fail on WriteOnly handle
    std::vector<uint8_t> read_buf(100);
    auto read_future = engine->Read(wo_handle, read_buf, read_buf.size());

    // Read should throw exception or return error
    bool read_failed = false;
    try {
        read_future.get();
    } catch (const std::exception& e) {
        read_failed = true;
        std::cout << "Expected read failure: " << e.what() << std::endl;
    }

    ASSERT_TRUE(read_failed) << "Read on WriteOnly handle should fail";
}

TEST_F(FileOperationsE2ETest, UnlinkFile) {
    // TODO: Implement Open/Unlink APIs
    GTEST_SKIP() << "Unlink API not yet implemented";
}

TEST_F(FileOperationsE2ETest, OpenReadWriteFile) {
    // file-004: Open file with ReadWrite flag
    // Step 1: Create a file
    auto create_handle = engine->Open("/readwrite_test.txt", OpenFlags::Create).get();
    ASSERT_NE(create_handle, nullptr);

    // Step 2: Write initial content
    std::string initial_content = "Initial content for ReadWrite test!";
    std::vector<uint8_t> initial_data(initial_content.begin(), initial_content.end());
    engine->Write(create_handle, initial_data).get();
    engine->Close(create_handle).get();

    // Step 3: Open file with OpenFlags::ReadWrite
    auto rw_handle = engine->Open("/readwrite_test.txt", OpenFlags::ReadWrite).get();
    ASSERT_NE(rw_handle, nullptr);
    ASSERT_EQ(rw_handle->GetPath(), "/readwrite_test.txt");

    // Step 4: Seek to end and append new content
    engine->Lseek(rw_handle, 0, Whence::End);

    std::string new_content = " - Modified content!";
    std::vector<uint8_t> write_data(new_content.begin(), new_content.end());
    auto write_result = engine->Write(rw_handle, write_data).get();
    ASSERT_EQ(write_result, static_cast<ssize_t>(write_data.size()))
        << "Write on ReadWrite handle should succeed";

    // Step 5: Seek back to beginning and read all content
    engine->Lseek(rw_handle, 0, Whence::Set);

    std::vector<uint8_t> read_buf(200);
    auto read_result = engine->Read(rw_handle, read_buf, read_buf.size()).get();
    ASSERT_GT(read_result, 0) << "Read on ReadWrite handle should succeed";

    // Verify the content matches
    std::string expected = initial_content + new_content;
    std::string actual(read_buf.begin(), read_buf.begin() + read_result);
    ASSERT_EQ(actual.substr(0, expected.size()), expected)
        << "Read content should match written data";

    engine->Close(rw_handle).get();
}

TEST_F(FileOperationsE2ETest, MultipleFilesCoexist) {
    // TODO: Implement for multiple files
    GTEST_SKIP() << "File operations not yet implemented";
}
