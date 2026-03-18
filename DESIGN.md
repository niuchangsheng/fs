# NVMe KV 文件系统/Blob 引擎 - 高层架构设计

## 1. 概述
本项目旨在基于 NVMe Key-Value (KV) 指令集构建一个提供 POSIX 文件语义的高性能存储引擎。由于底层物理存储是扁平的键值对结构，而非传统的线性逻辑块地址（LBA），系统的核心挑战在于如何将层次化的文件系统（目录、文件、偏移量）高效映射到受限的 KV 空间中，并保证操作的原子性和数据一致性。

## 2. 系统分层架构
系统自上而下分为四个主要层次：

1. **VFS / API 层:**
   - 向上提供标准的 POSIX 接口（`open`, `read`, `write`, `mkdir`, `rename` 等）。
   - 管理内存中的文件描述符（FD）状态、读写偏移量（Offset）和并发访问控制。
2. **文件系统逻辑层 (Namespace & Metadata):**
   - 维护层次化的目录树结构（Namespace）。
   - 管理超级块（Superblock）、目录项（Dentry）和索引节点（Inode）。
3. **KV 映射与分块层 (Mapping & Chunking):**
   - 解决底层 NVMe KV 设备的容量和大小限制。
   - 将逻辑上的大块连续 I/O 拆分为适合底层 KV 设备大小的 Chunk。
   - 负责数据的读-改-写（RMW, Read-Modify-Write）逻辑。
4. **NVMe KV 物理驱动层:**
   - 使用 SPDK 提供的用户态驱动，通过轮询模式与 NVMe 设备通信，实现零拷贝和低延迟访问。

## 3. 元数据管理
在扁平的 KV 存储中构建文件系统，需要对传统的元数据结构进行适配改造。我们将内部对象统一编址为 64-bit 或 128-bit 的唯一 ID (OID)，并将其直接映射为底层 NVMe 的 Key。

### 3.1 Superblock (超级块)
- **存储方式:** 存储在具有固定全局已知 Key 的 KV 对中（如 `Key = "SUPERBLOCK____00"`）。
- **内容:** 包含文件系统的全局状态，如 Magic Number、版本号、块大小、总容量、**根目录的 Inode ID (Root OID)** 以及用于 ID 分配的全局计数器。
- **作用:** 系统挂载 (Mount) 时的唯一入口点。

### 3.2 Inode (索引节点)
每个文件或目录拥有一个唯一的 Inode OID，映射为一个 KV 对。
- **Key:** `Inode_Prefix | Inode_OID`
- **内容:** 
  - POSIX 属性：大小 (Size)、权限 (Mode)、所有者 (UID/GID)、时间戳 (mtime, ctime, atime)。
  - 类型：普通文件 (File)、目录 (Directory) 或符号链接 (Symlink)。
  - **数据索引 (Data Layout):** 记录该文件的逻辑块 (Logical Offset) 到底层数据块 OID (Chunk OID) 的映射表。对于巨型文件，该映射表可能也是间接块形式。
  - **内联数据 (Inline Data):** 对于极小的文件（例如仅几十字节），数据直接保存在 Inode 的 Value 中，以节省一次 KV 访问。

### 3.3 Dentry (目录项)
目录在本质上是一个特殊的 Inode，其数据内容是该目录下的所有子文件和子目录的映射表。
- **内容格式:** 序列化后的 `<Filename, Inode_OID, Type>` 列表。
- 为了加速大型目录的查找，可以将目录的 Dentry 数据组织为基于 Hash 或 B-Tree 的结构，并分块存储在多个 KV 中。

## 4. 应对 Key / Value 大小限制
NVMe KV 硬件通常对 Key 的长度（如 16B/32B）和 Value 的大小（如最大 1MB/4MB，最小可能要求 4KB 对齐）有严格限制。

### 4.1 Key 大小限制 (太长或不定长)
- 用户提供的路径（如 `/usr/local/bin/app`）长度不定。
- **处理方式:** 我们不直接使用路径作为底层 Key。相反，系统为每个 Inode 和 Chunk 分配内部的、固定长度的 64-bit/128-bit OID，并将其打包填充至底层要求的长度（如 16 字节）。目录树解析（Path Resolution）的过程就是将字符串路径逐层转换为 OID 的过程。

### 4.2 Value 大小限制 (文件太大)
- **处理方式:** **分块 (Chunking)**。将大文件按最大支持的 Value Size（或更小的逻辑块大小，如 128KB/1MB）进行切分。
- 文件偏移量计算出 Chunk Index，去 Inode 的数据索引表中查找对应的 Chunk OID，最后用该 OID 组装出底层的 Key 存取数据。

### 4.3 Value 大小限制 (数据太小)
- **处理方式:**
  1. **内联 (Inlining):** 小于 Inode 剩余空间的小数据直接与 Inode Metadata 打包存在一起。
  2. **合并与对齐 (Padding/Packing):** 如果硬件要求最小写入单元，对于独立的小数据块，不足部分用 0 填充。若产生大量空间浪费，可考虑在映射层引入 Sub-block Allocation 机制，将多个小 Chunk 打包写入同一个 KV（但这会增加 RMW 开销）。

## 5. 原子性与一致性保证
假设底层 NVMe KV 仅支持**单个 KV 级别的 Compare-And-Swap (CAS)** 操作。由于文件系统操作（如写入数据、重命名文件）通常涉及多个 KV（如修改数据 Chunk、修改 Inode 属性、修改 Dentry），我们需要软件机制来保证多 KV 操作的原子性。

### 5.1 数据写入 (Copy-on-Write 机制)
- 不覆盖写入（Out-of-place update）。
- 当向已有文件写入数据时：
  1. 系统分配一个新的 Chunk OID，并将新数据（包含修改部分和原部分的合并，即 RMW 结果）作为一个全新的 KV 对写入设备。
  2. 使用 CAS 操作更新该文件的 Inode，将对应位置的数据索引指针指向新的 Chunk OID，同时更新 mtime 和 file size。
  3. 只要 Inode 的 CAS 成功，写入即生效；若失败，说明有并发修改，则重新读取 Inode，重试合并和 CAS 步骤。
  4. 旧的 Chunk 变为孤儿，通过后台垃圾回收（GC）异步删除。

### 5.2 复杂命名空间操作 (如 Rename)
Rename 操作（例如将 `dir_A/file_1` 移动到 `dir_B/file_2`）需要同时修改两个 Dentry (dir_A 和 dir_B)，仅靠单个 CAS 无法做到原子性。
- **解决方案：Write-Ahead Logging (WAL) 或 Intent Log**
  1. 系统保留一个特殊的 KV 区域（或环形缓冲区）作为日志区。
  2. 记录 Intent（意图）：准备将 `file_1` 从 `dir_A` 移出并加入 `dir_B`。
  3. 执行操作：依次更新 `dir_A` 和 `dir_B` 的 Dentry KV（可以通过 CAS 保证各个 Dentry 更新的串行化）。
  4. 清除日志。
- **崩溃恢复:** 挂载时，检查日志区。如果存在未完成的 Intent，根据其阶段进行重做（Redo）或回滚（Undo），以保证命名空间的一致性。