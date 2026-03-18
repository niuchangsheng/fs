# KVFS 元数据结构与键空间设计 (Metadata & Key Space Design)

本文档详细定义了 KVFS（基于 NVMe KV 的文件系统）的核心元数据结构以及在扁平的 KV 存储中如何划分和管理 Key 空间。

## 1. 键空间布局 (Key Space Layout)

NVMe KV 设备要求提供固定或有最大长度限制的 Key。为了统一管理，我们采用 **16 字节 (128-bit)** 的定长 Key。所有文件系统对象（超级块、Inode、数据块）都映射到这个 16 字节的键空间中。

**Key 编码格式:**
`[ Type Prefix (1 Byte) ] [ Padding/Reserved (7 Bytes) ] [ Object ID / OID (8 Bytes) ]`

*   **Type Prefix:**
    *   `'S'` (0x53): Superblock (超级块)
    *   `'I'` (0x49): Inode (索引节点)
    *   `'C'` (0x43): Data Chunk (数据分块)
*   **OID (uint64_t):** 对象的全局唯一标识符。

*示例:*
*   Superblock Key: `['S', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]` (OID = 0)
*   Inode 5 Key: `['I', 0, 0, 0, 0, 0, 0, 0, (uint64_t)5_BE]`

---

## 2. 核心数据结构 (Core C++ Structures)

所有元数据结构在写入 KV 设备前，都需要序列化为二进制字节流 (Byte Stream)。

### 2.1 Superblock (超级块)
超级块是文件系统的全局入口点，其 OID 固定为 0。

```cpp
struct Superblock {
    uint32_t magic;           // 魔数，例如 0x4B564653 ("KVFS")
    uint32_t version;         // 文件系统版本
    uint32_t block_size;      // 逻辑块大小 (Chunk Size)，如 4096 (4KB) 或 1MB
    uint64_t total_capacity;  // 底层 KV 设备总容量
    
    uint64_t root_inode_oid;  // 根目录 ('/') 的 Inode ID
    uint64_t next_inode_oid;  // 用于分配下一个 Inode ID 的全局计数器
    uint64_t next_chunk_oid;  // 用于分配下一个 Data Chunk ID 的全局计数器

    uint8_t padding[...];     // 填充至适当大小（如 512 字节或 4KB）
};
```

### 2.2 Inode (索引节点)
每个文件、目录或符号链接对应一个 Inode。

```cpp
enum class FileType : uint8_t {
    RegularFile = 1,
    Directory = 2,
    Symlink = 3
};

struct Inode {
    uint64_t oid;             // 自身的 Inode ID
    FileType type;            // 文件类型
    uint16_t mode;            // POSIX 权限位 (e.g., 0755)
    uint32_t uid;             // 所有者 User ID
    uint32_t gid;             // 所有者 Group ID
    uint64_t size;            // 文件逻辑总大小 (字节)
    
    uint64_t atime;           // 访问时间
    uint64_t mtime;           // 修改时间
    uint64_t ctime;           // 状态改变时间
    
    uint32_t link_count;      // 硬链接数

    // --- 数据布局 (Data Layout) ---
    // 方案 A: 如果文件非常小，直接将数据存储在 Inode 的 Value 中（Inline Data）
    bool is_inline;
    uint32_t inline_data_len;
    // uint8_t inline_data[MAX_INLINE_SIZE];
    
    // 方案 B: 如果文件较大，这里存储逻辑 Chunk Index 到 物理 Chunk OID 的映射表
    // 实际实现中，这部分可能是一个动态数组，序列化时紧跟在基础属性之后
    // std::map<uint32_t /* chunk_index */, uint64_t /* chunk_oid */> chunk_map;
};
```
*(注：序列化时，Inode 的 Value = 基础固定属性 + (Inline 数据 或 Chunk 映射表) )*

### 2.3 Dentry (目录项)
目录在底层被视为一种特殊的 `FileType::Directory` 文件。它的内容（Data）就是目录项列表。为了便于序列化和遍历，目录的数据块内容布局如下：

```cpp
// 目录数据块中的单条记录结构
struct DentryRecord {
    uint64_t inode_oid;       // 子文件/目录的 Inode ID
    uint8_t  type;            // 子节点类型 (FileType)
    uint16_t name_len;        // 文件名长度
    // char name[name_len];   // 实际文件名字符串（紧跟其后）
};
```
**目录数据块的内存流格式:**
`[Dentry 1] [Name 1] | [Dentry 2] [Name 2] | ... | [Dentry N] [Name N]`

---

## 3. 操作示例：打开文件解析路径 (Path Resolution)

假设用户调用 `open("/usr/bin/app", O_RDONLY)`，引擎内部的元数据查找流程如下：

1. **读取 Superblock:** 从固定的 Superblock Key 中读取数据，获取 `root_inode_oid`。
2. **读取 Root Inode:** 使用 `['I', ..., root_inode_oid]` 作为 Key 读取根目录 (`/`) 的 Inode。
3. **遍历 Root 目录数据:** 根据 Root Inode 中的 `chunk_map` 获取其数据分块，在分块数据（Dentry 列表）中寻找名为 `"usr"` 的条目，获取其 Inode ID (`usr_oid`)。
4. **递归查找:** 
   - 读取 `usr` 的 Inode。
   - 遍历 `usr` 的 Dentry，找到 `"bin"`，获取 `bin_oid`。
   - 读取 `bin` 的 Inode。
   - 遍历 `bin` 的 Dentry，找到 `"app"`，获取 `app_oid`。
5. **打开目标文件:** 读取并缓存 `app_oid` 对应的 Inode，并在内存中构造 `FileHandle`，初始读写偏移量 (Offset) 设为 0。

---

## 4. 碎片化与序列化挑战
*   **字节序:** OID 和其它整数类型在作为 Key 或持久化 Value 时，必须使用**小端序 (Little-Endian) 或大端序 (Big-Endian)** 中的一种固定格式（通常推荐 Little-Endian 以匹配 x86 架构的内存原生布局，避免 CPU 转换开销）。
*   **KV 容量管理:** 序列化后的 Inode 加上其 Chunk 映射表，不能超过底层 NVMe KV 允许的最大 Value 尺寸。如果映射表极大，需要引入 **间接块 (Indirect Blocks)** 机制。
