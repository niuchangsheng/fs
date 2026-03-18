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

## 三、 详细架构与演进方向

本项目的分层架构设计、元数据（Inode/Dentry/Superblock）管理细节、KV 大小限制的应对策略，以及工程落地中面临的核心挑战（如 RMW 写放大、WAL 瓶颈、内存开销等）和相应的演进方案，已详细记录在专门的设计文档中。

👉 **[请参阅详细架构设计文档：DESIGN.md](DESIGN.md)**
