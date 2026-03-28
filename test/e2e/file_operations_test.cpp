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
    // file-007: Unlink deletes a file
    // Step 1: Create a file
    auto handle = engine->Open("/unlink_test.txt", OpenFlags::Create).get();
    ASSERT_NE(handle, nullptr) << "Open should return a valid handle";

    // Write some content
    std::string content = "Content to be deleted!";
    std::vector<uint8_t> data(content.begin(), content.end());
    engine->Write(handle, data).get();
    engine->Close(handle).get();

    // Step 2: Verify file exists by opening it
    auto verify_handle = engine->Open("/unlink_test.txt", OpenFlags::ReadOnly).get();
    ASSERT_NE(verify_handle, nullptr) << "File should exist before unlink";
    engine->Close(verify_handle).get();

    // Step 3: Call Unlink() on file path
    auto unlink_result = engine->Unlink("/unlink_test.txt").get();
    ASSERT_EQ(unlink_result, 0) << "Unlink should return 0 on success";

    // Step 4: Verify file no longer exists
    bool open_failed = false;
    try {
        auto reopened = engine->Open("/unlink_test.txt", OpenFlags::ReadOnly).get();
        // If open succeeds, file wasn't deleted
        if (reopened) {
            FAIL() << "File should not exist after unlink";
        }
    } catch (const std::exception& e) {
        open_failed = true;
        std::cout << "Expected open failure after unlink: " << e.what() << std::endl;
    }
    ASSERT_TRUE(open_failed) << "Opening unlinked file should fail";
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

TEST_F(FileOperationsE2ETest, OpenReturnsValidPath) {
    // file-005: Open returns FileHandle with valid path
    // Step 1: Call Open() with a path
    auto handle = engine->Open("/path_test.txt", OpenFlags::Create).get();

    // Step 2: Verify handle->GetPath() returns the original path
    ASSERT_NE(handle, nullptr) << "Open should return a valid handle";
    ASSERT_EQ(handle->GetPath(), "/path_test.txt") << "GetPath() should return the original path";

    // Step 3: Verify with different file paths
    auto handle2 = engine->Open("/another_file.txt", OpenFlags::Create).get();
    ASSERT_NE(handle2, nullptr) << "Open should return a valid handle";
    ASSERT_EQ(handle2->GetPath(), "/another_file.txt")
        << "GetPath() should return the original path";

    engine->Close(handle).get();
    engine->Close(handle2).get();
}

TEST_F(FileOperationsE2ETest, CloseFileHandle) {
    // file-006: Close releases file handle resources
    // Step 1: Open a file
    auto handle = engine->Open("/close_test.txt", OpenFlags::Create).get();
    ASSERT_NE(handle, nullptr) << "Open should return a valid handle";

    // Step 2: Call Close() with the handle
    auto close_result = engine->Close(handle).get();
    ASSERT_EQ(close_result, 0) << "Close should return 0 on success";

    // Step 3: Verify handle is invalidated (subsequent operations fail)
    // Try to write to the closed handle
    std::vector<uint8_t> write_data = {1, 2, 3, 4, 5};
    bool write_failed = false;
    try {
        engine->Write(handle, write_data).get();
    } catch (const std::exception& e) {
        write_failed = true;
        std::cout << "Expected write failure after close: " << e.what() << std::endl;
    }
    ASSERT_TRUE(write_failed) << "Write on closed handle should fail";

    // Step 4: Verify read also fails on closed handle
    std::vector<uint8_t> read_buf(100);
    bool read_failed = false;
    try {
        engine->Read(handle, read_buf, read_buf.size()).get();
    } catch (const std::exception& e) {
        read_failed = true;
        std::cout << "Expected read failure after close: " << e.what() << std::endl;
    }
    ASSERT_TRUE(read_failed) << "Read on closed handle should fail";
}

TEST_F(FileOperationsE2ETest, UnlinkNonExistentFile) {
    // file-008: Unlink on non-existent file returns error
    // Step 1: Call Unlink() on non-existent path
    bool unlink_failed = false;
    try {
        auto result = engine->Unlink("/non_existent_file.txt").get();
        // If unlink succeeds, that's unexpected
        FAIL() << "Unlink on non-existent file should fail";
    } catch (const std::exception& e) {
        unlink_failed = true;
        std::cout << "Expected unlink failure: " << e.what() << std::endl;
    }
    ASSERT_TRUE(unlink_failed) << "Unlink on non-existent file should return error";
}

TEST_F(FileOperationsE2ETest, MultipleFilesCoexist) {
    // TODO: Implement for multiple files
    GTEST_SKIP() << "File operations not yet implemented";
}
