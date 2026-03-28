/**
 * @file metadata_test.cpp
 * @brief 单元测试：Metadata 结构 (Superblock, Inode)
 *
 * 测试元数据结构的序列化和反序列化
 */

#include "metadata.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace kvfs;

void test_superblock_key_generation() {
    std::cout << "[TEST] Superblock key generation" << std::endl;

    std::string key = GetSuperblockKey();
    assert(key.size() == 16);
    assert(key[0] == 'S');

    std::cout << "  PASSED" << std::endl;
}

void test_inode_key_generation() {
    std::cout << "[TEST] Inode key generation" << std::endl;

    std::string key1 = GetInodeKey(1);
    std::string key2 = GetInodeKey(2);

    assert(key1.size() == 16);
    assert(key2.size() == 16);
    assert(key1[0] == 'I');
    assert(key2[0] == 'I');
    assert(key1 != key2);

    std::cout << "  PASSED" << std::endl;
}

void test_superblock_serialize_deserialize() {
    std::cout << "[TEST] Superblock serialize/deserialize" << std::endl;

    Superblock original{};
    original.magic = 0x4B564653;  // "KVFS"
    original.version = 1;
    original.block_size = 4096;
    original.total_capacity = 1024 * 1024 * 1024;
    original.root_inode_oid = 1;
    original.next_inode_oid = 2;
    original.next_chunk_oid = 100;

    // Serialize
    auto data = original.Serialize();
    assert(data.size() == sizeof(Superblock));

    // Deserialize
    Superblock restored = Superblock::Deserialize(data);

    assert(restored.magic == original.magic);
    assert(restored.version == original.version);
    assert(restored.block_size == original.block_size);
    assert(restored.total_capacity == original.total_capacity);
    assert(restored.root_inode_oid == original.root_inode_oid);
    assert(restored.next_inode_oid == original.next_inode_oid);
    assert(restored.next_chunk_oid == original.next_chunk_oid);

    std::cout << "  PASSED" << std::endl;
}

void test_inode_serialize_deserialize() {
    std::cout << "[TEST] Inode serialize/deserialize" << std::endl;

    Inode original{};
    original.oid = 42;
    original.type = FileType::RegularFile;
    original.mode = 0644;
    original.uid = 1000;
    original.gid = 1000;
    original.size = 1024;
    original.atime = 1711600000;
    original.mtime = 1711600000;
    original.ctime = 1711600000;
    original.link_count = 1;
    original.is_inline = true;
    original.inline_data_len = 100;

    // Serialize
    auto data = original.Serialize();
    assert(data.size() == sizeof(Inode));

    // Deserialize
    Inode restored = Inode::Deserialize(data);

    assert(restored.oid == original.oid);
    assert(restored.type == original.type);
    assert(restored.mode == original.mode);
    assert(restored.uid == original.uid);
    assert(restored.gid == original.gid);
    assert(restored.size == original.size);
    assert(restored.is_inline == original.is_inline);

    std::cout << "  PASSED" << std::endl;
}

void test_file_type_enum() {
    std::cout << "[TEST] FileType enum values" << std::endl;

    assert(static_cast<uint8_t>(FileType::RegularFile) == 1);
    assert(static_cast<uint8_t>(FileType::Directory) == 2);
    assert(static_cast<uint8_t>(FileType::Symlink) == 3);

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== Metadata Unit Tests ===" << std::endl;
    std::cout << std::endl;

    test_superblock_key_generation();
    test_inode_key_generation();
    test_superblock_serialize_deserialize();
    test_inode_serialize_deserialize();
    test_file_type_enum();

    std::cout << std::endl;
    std::cout << "=== All Unit Tests Passed ===" << std::endl;
    return 0;
}
