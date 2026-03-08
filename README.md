# FOS-OS-Implementation

> A custom educational operating system focused on kernel design, memory management, multitasking, and system-level correctness.

Building an operating system is the ultimate systems engineering challenge. **BetterThanWindows-OS** replaces high-level abstractions with direct control over hardware, memory, and the CPU, providing a hands-on exploration of the full computing stack.

---

##  Project Overview

This OS was developed in **three major phases**, each progressively transforming the kernel from a minimal core into a multitasking, stress-tested system.

The project emphasizes:
- Low-level systems programming
- Memory safety and performance
- Process isolation and scheduling
- Real-world kernel validation under load

---

##  Architecture & Features

### Phase I — Core Kernel Architecture
Focused on building a stable and extensible kernel foundation.

- **Dynamic Memory Allocator**
  - handling kernel-side dynamic memory allocation and ensuring safe and efficient memory usage.

- **Kernel Heap**
  - Dedicated heap for supervisor-mode operations
  - Strict isolation from user-space memory

- **Page Fault Handling**
  -  to correctly manage memory faults and ensure proper page placement.

---

### Phase II — Advanced System Services
Transitioned the kernel into a fully multitasking environment.

- **Page Fault Replacement**
  - Implemented the first part of the Fault Handling mechanism, focusing on detecting and managing memory access violations during execution.

- **User Heap**
  - handling dynamic memory allocation in user space and ensuring correct interaction with the kernel memory system.

- **Shared Memory Management**
  - enabling processes to safely create, attach, and manage shared memory segments with proper synchronization and protection.

- **CPU Scheduling**
  - Process lifecycle management (ready, running, blocked, terminated)
  - efficient CPU utilization.

- **Kernel Protection**
  - enforcing strict separation between user mode and kernel mode to protect critical kernel resources.

---

### Phase III — Validation & Testing
Ensured correctness, robustness, and performance.

- **Automated Test Suites**
  - Detects race conditions
  - Identifies synchronization issues
  - Verifies memory safety and leak prevention

---

##  Technologies & Concepts

- C / Low-level systems programming
- Paging & virtual memory
- Kernel heaps and allocators
- Process scheduling & context switching
- Page fault handling

---

##  Testing

- Each kernel module is tested independently
- Stress tests simulate high memory pressure and heavy multitasking
- Emphasis on correctness, synchronization, and stability

---

##  Learning Outcomes

This project provided deep, practical insight into:

- How operating systems manage memory and CPU resources
- Trade-offs between performance, security, and complexity
- Debugging at the kernel and hardware-interaction level
- Designing systems that remain stable under extreme load

---

##  Contributing

Contributions, issues, and suggestions are welcome.
Feel free to fork the repository and experiment.

---


###  If you find this project interesting, consider giving it a star!
