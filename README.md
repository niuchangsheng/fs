# NVMe KV POSIX File System / Blob Engine (fs)

## 简介 (Introduction)
本项目致力于构建一个极具野心、在特定高性能场景下极具潜力的下一代存储引擎。它基于 NVMe Key-Value (KV) 指令集，采用 C++ 编写，通过完全绕过传统的块设备层 (Block Layer) 和 VFS，直接将 POSIX 文件语义映射到扁平的底层硬件 KV 空间中，以实现极致的 I/O 吞吐量和微秒级延迟。

---

## 📖 项目背景 (Background)

### 什么是 NVMe KV?
传统的 SSD 遵循 **LBA (Logical Block Addressing)** 模式，将存储空间视为一连串连续的逻辑块。主机端的文件系统（如 Ext4/XFS）必须维护复杂的 B-Tree 或位图来映射文件偏移量到 LBA。

**NVMe KV** 是存储标准化组织 (SNIA) 推出的新一代标准。它允许主机直接通过变量长度的 **Key** 来存取变量长度的 **Value**，而无需关心底层的物理地址。这意味着原本运行在主机 CPU 上的键值映射逻辑被下放（Offload）到了 SSD 内部的控制器中执行。

### 为什么选择这个项目?
本项目旨在消除主机端文件系统与 SSD 内部 FTL (Flash Translation Layer) 之间的冗余映射。通过直接对接 NVMe KV 语义，我们不仅能够获得极高的性能，还能显著降低 CPU 在 I/O 管理上的开销。

---

## 🚀 目标应用场景 (Target Use Cases)

1. **AI 训练样本加载 (Dataset Loading):** 
   - 在处理海量小文件（如亿级图片、音频片段）时，传统文件系统的 Inode 查找和锁竞争是严重的性能瓶颈。本项目能提供接近硬件极限的随机读取速度。
2. **高性能数据库底层存储:** 
   - 为 LSM-Tree 架构的数据库（如 RocksDB 的变体）提供直接的对象存储后端，消除文件系统层的干扰和日志冗余。
3. **极低延迟存储系统:** 
   - 适用于高频交易 (HFT) 或实时流处理，通过用户态轮询 (SPDK Polling) 和零拷贝 (Zero-Copy) 技术，消除微秒级的内核上下文切换延迟。

---

## 💎 核心价值 (Core Value)

1. **消除双重映射 (No Double Indirection):** 直接使用 NVMe KV 语义，消除了主机端 LBA 映射层，让 SSD 控制器直接管理数据布局。
2. **释放 CPU 算力:** 绕过 VFS、B-Tree 查找、块层分配及中断处理，将宝贵的 CPU 周期留给业务逻辑。
3. **优化设备级 GC:** 硬件原生感知数据删除，显著降低写放大 (Write Amplification)，延长 SSD 寿命。

---

## ✨ 技术亮点 (Technical Highlights)

1. **扁平空间重构树形语义:** 巧妙地利用内部 OID，在扁平的 KV 空间中实现了严格的 POSIX 目录树。
2. **基于单 KV CAS 的无锁并发:** 采用写时复制 (CoW) 策略，通过单次原子操作 (CAS) 更新 Inode，实现高并发下的无锁数据修改。
3. **极致的用户态 I/O 路径:** 深度结合 SPDK、大页内存 (Hugepages) 与用户态轮询，构建全路径零拷贝的 DMA 传输。

---

## 🛠️ 环境要求与预备条件 (Prerequisites)

*   **硬件支持:** 需要支持 NVMe KV 指令集的固态硬盘（如 Samsung PM983 KV 等）。
*   **模拟环境:** 若无实体硬件，可使用集成 NVMe KV 补丁的 QEMU 或 SPDK 模拟器进行开发调试。
*   **软件环境:** 
    *   现代 C++ 编译器 (支持 C++20/C++23)
    *   Linux 操作系统（需配置 Hugepages 和 VFIO/UIO 驱动）
    *   SPDK (Storage Performance Development Kit)

---

## 📚 详细设计文档
关于分层架构、元数据映射、写放大处理及崩溃一致性（WAL）的深度分析，请查阅：
👉 **[详细架构设计文档：DESIGN.md](docs/DESIGN.md)**