# Architecture

## Overview

The first version is a modular monolithic kernel targeting x86_64. Architecture-specific code is isolated, but portability is not allowed to complicate the first implementation.

```text
Bootloader
    |
    v
Architecture bootstrap
    |
    +--> Serial diagnostics
    +--> CPU tables and exceptions
    +--> Physical memory manager
    +--> Virtual memory manager
    +--> Kernel heap
    +--> Interrupt controller and timer
    +--> Scheduler
    +--> User mode and system calls
    +--> VFS, initramfs and ELF loader
```

## Proposed source tree

```text
kernel/
├── arch/
│   └── x86_64/
│       ├── boot/
│       ├── cpu/
│       ├── interrupts/
│       ├── memory/
│       └── platform/
├── core/
│   ├── init/
│   ├── panic/
│   ├── log/
│   └── scheduler/
├── mm/
│   ├── pmm/
│   ├── vmm/
│   └── heap/
├── drivers/
│   ├── serial/
│   ├── timer/
│   └── console/
├── fs/
│   ├── vfs/
│   └── initramfs/
├── process/
├── syscall/
└── lib/
include/
├── kernel/
└── arch/x86_64/
tests/
├── host/
└── kernel/
tools/
docs/
```

## Layering rules

- Generic kernel code must not include private architecture headers.
- Architecture code may depend on generic kernel interfaces where initialization order permits it.
- Hardware drivers must expose small interfaces and avoid leaking register layouts into unrelated modules.
- Memory managers must not print directly; they report errors through status values or the logging interface.
- The panic path must avoid dynamic allocation.
- Boot-time code must validate all bootloader responses before dereferencing them.

## Boot sequence

1. Limine loads the kernel ELF and transfers control.
2. The entry code establishes the minimum required CPU state.
3. The serial port is initialized.
4. Bootloader responses and memory-map information are validated.
5. GDT, TSS and IDT are installed.
6. CPU exception handlers are enabled.
7. The physical memory manager is initialized.
8. Kernel-owned page tables are established.
9. The kernel heap is initialized.
10. Interrupt controller and timer are configured.
11. The scheduler starts the idle thread and initial kernel tasks.
12. The first user process is loaded when user mode becomes available.

## Memory architecture

The early design uses:

- 4 KiB base pages;
- a high-half kernel;
- an explicit direct physical-memory map;
- a bitmap physical page allocator;
- separate address-space objects;
- non-executable data, heap and stack pages where supported;
- guard pages for important stacks;
- explicit mapping, unmapping and address-resolution APIs.

Physical and virtual addresses should use distinct project types such as `paddr_t` and `vaddr_t`. Conversions must be explicit.

## Execution model

The initial scheduler is single-core and preemptive. It first supports kernel threads, then processes with independent address spaces.

Initial thread states:

```text
NEW -> READY -> RUNNING -> BLOCKED
                 |           |
                 +--> READY <-+
                 |
                 +--> DEAD
```

## System-call boundary

The system-call ABI is introduced only after Ring 3 works reliably. All user pointers must be validated before use. The initial ABI should stay intentionally small and versioned.

Candidate first calls:

- `write`
- `exit`
- `yield`
- `mmap`
- `munmap`
- `spawn`

## Error-handling policy

Recoverable operations return typed status values. Fatal invariants invoke `panic()` with source location and machine state when available.

Assertions are enabled in debug builds. Release builds may remove expensive diagnostics but must not depend on assertions for correctness.
