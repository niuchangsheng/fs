/**
 * @file metadata_test.cpp
 * @brief 单元测试：Metadata 结构
 */

#include "metadata.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace kvfs;

TEST(MetadataTest, SuperblockKeyGeneration) {
    std::string key = GetSuperblockKey();
    ASSERT_EQ(key.size(), 16);
    ASSERT_EQ(key[0], 'S');
}

TEST(MetadataTest, InodeKeyGeneration) {
    std::string key1 = GetInodeKey(1);
    std::string key2 = GetInodeKey(2);

    ASSERT_EQ(key1.size(), 16);
    ASSERT_EQ(key2.size(), 16);
    ASSERT_EQ(key1[0], 'I');
    ASSERT_EQ(key2[0], 'I');
    ASSERT_NE(key1, key2);
}

TEST(MetadataTest, SuperblockSerializeDeserialize) {
    Superblock original{};
    original.magic = 0x4B564653;
    original.version = 1;
    original.block_size = 4096;
    original.total_capacity = 1024 * 1024 * 1024;
    original.root_inode_oid = 1;
    original.next_inode_oid = 2;
    original.next_chunk_oid = 100;

    auto data = original.Serialize();
    ASSERT_EQ(data.size(), sizeof(Superblock));

    Superblock restored = Superblock::Deserialize(data);

    ASSERT_EQ(restored.magic, original.magic);
    ASSERT_EQ(restored.version, original.version);
    ASSERT_EQ(restored.block_size, original.block_size);
    ASSERT_EQ(restored.total_capacity, original.total_capacity);
    ASSERT_EQ(restored.root_inode_oid, original.root_inode_oid);
    ASSERT_EQ(restored.next_inode_oid, original.next_inode_oid);
    ASSERT_EQ(restored.next_chunk_oid, original.next_chunk_oid);
}

TEST(MetadataTest, InodeSerializeDeserialize) {
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

    auto data = original.Serialize();
    ASSERT_EQ(data.size(), sizeof(Inode));

    Inode restored = Inode::Deserialize(data);

    ASSERT_EQ(restored.oid, original.oid);
    ASSERT_EQ(restored.type, original.type);
    ASSERT_EQ(restored.mode, original.mode);
    ASSERT_EQ(restored.uid, original.uid);
    ASSERT_EQ(restored.gid, original.gid);
    ASSERT_EQ(restored.size, original.size);
    ASSERT_EQ(restored.is_inline, original.is_inline);
}

TEST(MetadataTest, FileTypeEnumValues) {
    ASSERT_EQ(static_cast<uint8_t>(FileType::RegularFile), 1);
    ASSERT_EQ(static_cast<uint8_t>(FileType::Directory), 2);
    ASSERT_EQ(static_cast<uint8_t>(FileType::Symlink), 3);
}
