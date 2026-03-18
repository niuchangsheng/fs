# KVFS API 设计要点 (API Design Rationale)

`include/kvfs.h` 定义了 KVFS 存储引擎的核心 C++ 接口。该设计旨在平衡 **POSIX 兼容性**、**现代 C++ 安全性** 以及 **NVMe KV 硬件的高性能异步特性**。

## 1. POSIX 语义的现代抽象
传统的 POSIX `read`/`write` 接口存在一些局限性（如不透明的整数文件描述符、阻塞模式、容易发生隐式类型转换等）。

- **类型安全标志 (`enum class`):** 我们使用 `OpenFlags` 和 `Whence` 枚举类替代了 `O_RDONLY` 等位掩码。这可以有效防止逻辑错误，并增强代码的可读性。
- **句柄对象化 (`FileHandle`):** 通过 `std::shared_ptr<FileHandle>` 替代了整数文件描述符。这带来了自动的资源生命周期管理（基于引用计数），并且能够安全地存储每个打开文件的当前 Offset、元数据快照等上下文。

## 2. 核心异步化设计 (`std::future`)
NVMe KV 存储结合 SPDK 用户态驱动时，最大的性能优势在于通过**轮询 (Polling)** 和**异步提交**来消除等待中断的开销。

- **全异步 I/O:** 所有的主要操作（`Open`, `Read`, `Write`, `Stat`, `Fsync`）均返回 `std::future`。这使得上层应用可以无缝集成到基于 Event Loop 或协程 (Coroutines) 的异步架构中。
- **并发控制:** 应用层可以同时发起多个读写请求（Queue Depth > 1），引擎内部可以通过 SPDK 的 I/O 队列并发提交给硬件。

## 3. 现代 C++ 安全与零拷贝友好
- **`std::span` 的应用:** `Read` 和 `Write` 接口放弃了 `void*` 和 `size_t` 的组合，转而使用 `std::span<uint8_t>`。
    - **安全性:** 显式包含缓冲区长度，避免越界访问。
    - **灵活性:** 可以方便地传入 `std::vector`、固定数组或由 SPDK 分配的 Hugepage 内存。
- **资源隔离:** 通过抽象基类 `KVEngine` 和 `FileHandle` 实现接口与实现的完全分离，保证了外部 API 的稳定性。

## 4. 关键接口行为约定
- **`Read` / `Write` 操作:** 返回实际处理的字节数。为了最大化性能，调用者负责确保传入 `std::span` 的底层缓存在 `future` 就绪之前始终有效。
- **`Fsync`:** 提供强制刷盘语义。虽然 NVMe KV 内部有自身的持久化机制，但 `Fsync` 接口保证了所有已提交的 Chunk 和 Inode 元数据在硬件层面完全落盘，适用于关键事务场景。
- **`Stat`:** 返回标准的 `struct stat` 结构，方便现有工具和代码（如目录遍历、权限检查）直接集成。

## 5. 错误处理策略
- 异步操作失败时，相关的 `std::future::get()` 会抛出自定义异常（包含 errno 映射），以保持与现代 C++ 习惯一致，同时也方便与传统错误码对接。
