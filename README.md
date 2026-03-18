# NVMe KV POSIX File System / Blob Engine (fs)

## 简介 (Introduction)
本项目致力于构建一个极具野心、在特定高性能场景下极具潜力的下一代存储引擎。它基于 NVMe Key-Value (KV) 指令集，采用 C++ 编写，通过完全绕过传统的块设备层 (Block Layer) 和 VFS，直接将 POSIX 文件语义映射到扁平的底层硬件 KV 空间中，以实现极致的 I/O 吞吐量和微秒级延迟。

## 一、 核心价值：打破传统的“存储税” (Breaking the Storage Tax)
传统的文件系统栈（VFS -> Ext4/XFS -> Block Layer -> NVMe Driver -> SSD FTL）极其冗长。每一层都在做地址映射和元数据管理，这被称为“Storage Tax”。本项目通过以下设计打破这一限制：

1. **消除双重映射 (Eliminating Double Indirection):** 传统的 SSD 内部有一个 FTL (Flash Translation Layer) 将 LBA (逻辑块地址) 映射到物理 NAND 页。而主机端的文件系统又将 File Offset 映射到 LBA。我们直接使用 NVMe KV，**将文件语义直接交给设备硬件管理**，消除了主机端的 LBA 映射层。
2. **释放 CPU 算力:** 通过 SPDK 用户态轮询和直接的 KV 语义，我们将原本用于处理复杂 POSIX 文件系统 B-Tree、日志和中断上下文切换的 CPU 周期释放出来，让业务应用获得更多的算力。
3. **更优的设备级垃圾回收 (Device-side GC):** 传统的块设备不知道哪些 LBA 是属于被删除文件的，必须依赖主机发送 TRIM 指令。在 NVMe KV 中，删除一个 Key，SSD 的控制器立刻就知道这部分 NAND 空间可以回收，极大降低了写放大 (Write Amplification)。

## 二、 技术亮点 (Technical Highlights)

1. **扁平空间重构树形语义:** 巧妙地利用 Hash(Path) 或内部 OID，在扁平的、无序的 KV 空间中“长出”了严格的 POSIX 目录树 (Inode/Dentry)。这是一种典型的 Log-Structured 思想的变种。
2. **基于单 KV CAS 的无锁并发:** 传统文件系统极度依赖复杂的并发锁（如 VFS 的 i_mutex）。我们的设计亮点在于：通过分配新的 Chunk OID 和写时复制 (Copy-on-Write, CoW)，将对文件数据的就地修改 (In-place update) 转化为对 Inode 映射表指针的原子替换 (CAS)。这在多核并发场景下能极大地提升吞吐量。
3. **极致的 I/O 路径:** 结合 SPDK 和大页内存，数据的流动是从用户态应用的 Buffer 直接 DMA 到底层 NVMe 设备的 KV 槽位中，做到了真正的 Zero-Copy。

## 三、 架构设计的挑战与演进方向

在将此概念模型落地为工业级存储产品时，我们面临以下核心挑战，这也是后续设计的重点方向：

### 1. RMW (读-改-写) 带来的写放大灾难
*   **挑战:** 如果底层 NVMe KV 强制要求 4KB 对齐或更糟的 1MB/4MB Value 大小，当用户仅调用 `write(fd, buf, 10)` 修改 10 个字节时，我们需要读出整个 Chunk，修改 10 字节，再申请一个新 OID 写回，最后 CAS 更新 Inode。
*   **演进方案:** 必须在内存中引入精密的 **Page Cache / Buffer Pool 设计**。吸收碎片化的随机写，合并为顺序的大块 KV Put，这不仅关乎性能，更关乎 SSD 的寿命。

### 2. 崩溃一致性 (Crash Consistency) 与复杂命名空间操作
*   **挑战:** 用 Write-Ahead Logging (WAL) 来解决 `Rename` 等涉及多个 KV 操作的原子性时，如果 WAL 是一个固定的 Key，多线程高并发写 WAL 时可能成为单点瓶颈。
*   **演进方案:** 引入基于 Per-Core 的 Ring Buffer Log 机制，或者借鉴 LFS (Log-structured File System) 的思想，将元数据更新的 Intent 附加在数据写入的同一个 Batch 中。

### 3. 内存消耗与元数据索引 (Memory Footprint)
*   **挑战:** 在一个包含十亿个小文件的文件系统中，如果在内存中缓存完整的 Inode 和 Dentry 映射，内存开销巨大。
*   **演进方案:** 设计一套高效的元数据缓存淘汰策略 (LRU/LFU)，并处理好内存元数据与 KV 设备上元数据的同步一致性。

### 4. 硬件特性的不确定性 (Hardware Quirks)
*   **挑战:** NVMe KV 标准在不同硬件厂商（Samsung, WDC 等）上的实现差异巨大，例如对范围扫描 (Scan) 的支持和 Value 大小限制各不相同。
*   **演进方案:** 在 I/O 引擎之上构建一个 **HAL (Hardware Abstraction Layer)**，将特定的硬件怪癖屏蔽掉，避免核心的文件映射逻辑强绑定某一种 SSD 的特性。

---
*更多详细架构设计请参阅 `DESIGN.md`*