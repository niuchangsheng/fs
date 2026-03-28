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

TEST_F(FileOperationsE2ETest, WriteDataToFile) {
    // io-001: Write data to file at current offset
    // Step 1: Open file with WriteOnly or ReadWrite
    auto handle = engine->Open("/io_write_test.txt", OpenFlags::Create).get();
    ASSERT_NE(handle, nullptr) << "Open should return a valid handle";

    // Step 2: Prepare data buffer
    std::string content = "Hello, IO Operations!";
    std::vector<uint8_t> data(content.begin(), content.end());

    // Step 3: Call Write() with handle and data
    auto bytes_written = engine->Write(handle, data).get();

    // Step 4: Verify returned bytes written equals data size
    ASSERT_EQ(bytes_written, static_cast<ssize_t>(data.size()))
        << "Write should return the number of bytes written";

    // Step 5: Verify file offset is advanced
    uint64_t new_offset = handle->GetOffset();
    ASSERT_EQ(new_offset, static_cast<uint64_t>(data.size()))
        << "File offset should be advanced by the number of bytes written";

    engine->Close(handle).get();
}

TEST_F(FileOperationsE2ETest, ReadDataFromFile) {
    // io-002: Read data from file at current offset
    // Step 1: Create a file with known content
    auto write_handle = engine->Open("/io_read_test.txt", OpenFlags::Create).get();
    ASSERT_NE(write_handle, nullptr);

    std::string expected_content = "Hello, Read Test!";
    std::vector<uint8_t> write_data(expected_content.begin(), expected_content.end());
    engine->Write(write_handle, write_data).get();
    engine->Close(write_handle).get();

    // Step 2: Open file with ReadOnly
    auto read_handle = engine->Open("/io_read_test.txt", OpenFlags::ReadOnly).get();
    ASSERT_NE(read_handle, nullptr);

    // Step 3: Prepare read buffer
    std::vector<uint8_t> read_buf(200);

    // Step 4: Call Read() with handle and buffer
    auto bytes_read = engine->Read(read_handle, read_buf, read_buf.size()).get();

    // Step 5: Verify returned bytes read
    ASSERT_EQ(bytes_read, static_cast<ssize_t>(expected_content.size()))
        << "Read should return the number of bytes read";

    // Step 6: Verify buffer contains expected data
    std::string actual_content(read_buf.begin(), read_buf.begin() + bytes_read);
    ASSERT_EQ(actual_content, expected_content)
        << "Read content should match written data";

    // Step 7: Verify file offset is advanced
    uint64_t new_offset = read_handle->GetOffset();
    ASSERT_EQ(new_offset, static_cast<uint64_t>(bytes_read))
        << "File offset should be advanced by the number of bytes read";

    engine->Close(read_handle).get();
}

TEST_F(FileOperationsE2ETest, WriteReturnsActualBytesWritten) {
    // io-003: Write returns actual bytes written
    // Step 1: Open file for writing
    auto handle = engine->Open("/io_write_bytes_test.txt", OpenFlags::Create).get();
    ASSERT_NE(handle, nullptr);

    // Step 2: Write 100 bytes
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    // Step 3: Verify Write() returns 100
    auto bytes_written = engine->Write(handle, data).get();
    ASSERT_EQ(bytes_written, 100) << "Write should return 100 bytes written";

    engine->Close(handle).get();
}

TEST_F(FileOperationsE2ETest, ReadReturnsActualBytesRead) {
    // io-004: Read returns actual bytes read
    // Step 1: Create file with 100 bytes content
    auto write_handle = engine->Open("/io_read_bytes_test.txt", OpenFlags::Create).get();
    ASSERT_NE(write_handle, nullptr);

    std::vector<uint8_t> write_data(100);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i % 256);
    }
    engine->Write(write_handle, write_data).get();
    engine->Close(write_handle).get();

    // Step 2: Open file for reading
    auto read_handle = engine->Open("/io_read_bytes_test.txt", OpenFlags::ReadOnly).get();
    ASSERT_NE(read_handle, nullptr);

    // Step 3: Call Read() with 100 byte buffer
    std::vector<uint8_t> read_buf(100);
    auto bytes_read = engine->Read(read_handle, read_buf, read_buf.size()).get();

    // Step 4: Verify Read() returns 100
    ASSERT_EQ(bytes_read, 100) << "Read should return 100 bytes read";

    engine->Close(read_handle).get();
}

TEST_F(FileOperationsE2ETest, WriteUpdatesFileSize) {
    // io-005: Write updates file size in inode
    // Step 1: Create empty file
    auto handle = engine->Open("/io_filesize_test.txt", OpenFlags::Create).get();
    ASSERT_NE(handle, nullptr);
    engine->Close(handle).get();

    // Step 2: Verify initial size is 0
    auto stat_before = engine->Stat("/io_filesize_test.txt").get();
    ASSERT_EQ(stat_before.st_size, 0) << "Initial file size should be 0";

    // Step 3: Write 50 bytes
    auto write_handle = engine->Open("/io_filesize_test.txt", OpenFlags::ReadWrite).get();
    ASSERT_NE(write_handle, nullptr);

    std::vector<uint8_t> data(50);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    engine->Write(write_handle, data).get();
    engine->Close(write_handle).get();

    // Step 4: Call Stat() on file
    auto stat_after = engine->Stat("/io_filesize_test.txt").get();

    // Step 5: Verify size is 50
    ASSERT_EQ(stat_after.st_size, 50) << "File size should be 50 after writing 50 bytes";
}

TEST_F(FileOperationsE2ETest, WriteWithAppendFlag) {
    // io-006: Write with Append flag appends to end
    // Step 1: Create file with existing content
    auto write_handle = engine->Open("/io_append_test.txt", OpenFlags::Create).get();
    ASSERT_NE(write_handle, nullptr);

    std::string initial_content = "Initial content!";
    std::vector<uint8_t> initial_data(initial_content.begin(), initial_content.end());
    engine->Write(write_handle, initial_data).get();
    engine->Close(write_handle).get();

    // Step 2: Open with OpenFlags::Append
    auto append_handle = engine->Open("/io_append_test.txt", OpenFlags::Append).get();
    ASSERT_NE(append_handle, nullptr);

    // Step 3: Write new data
    std::string appended_content = " - Appended!";
    std::vector<uint8_t> append_data(appended_content.begin(), appended_content.end());
    engine->Write(append_handle, append_data).get();
    engine->Close(append_handle).get();

    // Step 4: Verify data is appended after original content
    auto read_handle = engine->Open("/io_append_test.txt", OpenFlags::ReadOnly).get();
    ASSERT_NE(read_handle, nullptr);

    auto stat = engine->Stat("/io_append_test.txt").get();
    size_t total_size = static_cast<size_t>(stat.st_size);

    std::vector<uint8_t> read_buf(total_size);
    auto bytes_read = engine->Read(read_handle, read_buf, total_size).get();
    ASSERT_EQ(bytes_read, static_cast<ssize_t>(total_size));

    std::string expected = initial_content + appended_content;
    std::string actual(read_buf.begin(), read_buf.end());
    ASSERT_EQ(actual, expected) << "Appended content should be at the end of original content";

    engine->Close(read_handle).get();
}

TEST_F(FileOperationsE2ETest, MultipleFilesCoexist) {
    // TODO: Implement for multiple files
    GTEST_SKIP() << "File operations not yet implemented";
}
