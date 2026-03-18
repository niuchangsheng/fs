# NVMe KV Blob Engine - Required Skills

## Core C++ Engineering
- **Modern C++ (C++20/C++23):** Expertise in advanced features, move semantics, smart pointers, templates, and standard library.
- **Memory Management:** Zero-copy techniques, direct I/O, NUMA awareness, and custom memory allocators.
- **Concurrency & Async I/O:** Multithreading, lock-free programming, `io_uring`, or SPDK for highly concurrent I/O operations.

## Storage Domain Knowledge
- **NVMe KV Standard:** Deep understanding of the NVMe Key-Value Command Set, KV pairs, command formats, and device behavior.
- **Blob Engine Architecture:** Design of blob storage systems, metadata management, garbage collection, and space allocation.
- **Performance Optimization:** Latency reduction, throughput maximization, caching strategies (DRAM/NVDIMM), and profiling tools (perf, eBPF).

## System Programming (Linux)
- **Linux Kernel Interfaces:** Block layer, NVMe drivers, PCIe architecture.
- **Direct Device Access:** Interacting with character devices or bypassing the kernel using frameworks like SPDK (Storage Performance Development Kit).

## Architecture & System Design
- **Scalability & Reliability:** Designing for high availability, fault tolerance, and large-scale data sets.
- **API Design:** Clean, efficient C++ interfaces for client applications to interact with the blob engine.
