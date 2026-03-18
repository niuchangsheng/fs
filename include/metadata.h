#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace kvfs {

// 16-byte Key Generation Helpers
inline std::string GenerateKey(char prefix, uint64_t oid) {
    std::string key(16, '\0');
    key[0] = prefix;
    // bytes 1-7 are padding/reserved
    std::memcpy(&key[8], &oid, sizeof(uint64_t));
    return key;
}

inline std::string GetSuperblockKey() {
    return GenerateKey('S', 0);
}

inline std::string GetInodeKey(uint64_t oid) {
    return GenerateKey('I', oid);
}

// Metadata Structures

#pragma pack(push, 1)

struct Superblock {
    uint32_t magic;           // 0x4B564653 ("KVFS")
    uint32_t version;         // 1
    uint32_t block_size;      // e.g., 4096
    uint64_t total_capacity;  // optional
    
    uint64_t root_inode_oid;  // OID of the root '/' directory
    uint64_t next_inode_oid;  // Counter for inode OIDs
    uint64_t next_chunk_oid;  // Counter for chunk OIDs

    // Serialize to vector
    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> data(sizeof(Superblock));
        std::memcpy(data.data(), this, sizeof(Superblock));
        return data;
    }

    // Deserialize from vector
    static Superblock Deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(Superblock)) {
            throw std::runtime_error("Invalid Superblock size");
        }
        Superblock sb;
        std::memcpy(&sb, data.data(), sizeof(Superblock));
        return sb;
    }
};

enum class FileType : uint8_t {
    RegularFile = 1,
    Directory = 2,
    Symlink = 3
};

struct Inode {
    uint64_t oid;
    FileType type;
    uint16_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    
    uint32_t link_count;

    bool is_inline;
    uint32_t inline_data_len;

    // For simplicity in this demo, we assume the rest of the payload 
    // is the inline data or the chunk map.
    // In a real implementation, we would append the dynamic sized data after this struct.

    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> data(sizeof(Inode));
        std::memcpy(data.data(), this, sizeof(Inode));
        return data;
    }

    static Inode Deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(Inode)) {
            throw std::runtime_error("Invalid Inode size");
        }
        Inode inode;
        std::memcpy(&inode, data.data(), sizeof(Inode));
        return inode;
    }
};

#pragma pack(pop)

} // namespace kvfs
