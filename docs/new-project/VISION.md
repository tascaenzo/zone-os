# Vision and Goals

## Project statement

The project is a small educational operating system for x86_64, developed in public through a structured Italian YouTube series.

It is not intended to compete with Linux, provide daily-driver functionality or support many architectures in its early stages. Its purpose is to demonstrate how a modern operating system grows from a controlled boot environment into a kernel capable of running isolated user programs.

## Primary goals

### Technical goals

- Boot reliably through Limine on UEFI systems.
- Provide strong diagnostics from the first milestone.
- Implement physical and virtual memory management.
- Handle CPU exceptions and hardware interrupts correctly.
- Support kernel threads and preemptive scheduling.
- Enter x86_64 user mode.
- Provide a small and documented system-call ABI.
- Load ELF programs from an initramfs.
- Introduce a simple VFS and console device.

### Educational goals

- Explain each component before implementing it.
- Keep commits and pull requests aligned with individual lessons.
- Provide start and completion tags for every episode.
- Show debugging and failures, not only the final working code.
- Document trade-offs and rejected alternatives.
- Make the repository usable without watching every video.

## Non-goals for the first development cycle

The following areas are intentionally postponed:

- symmetric multiprocessing;
- ARM or RISC-V ports;
- USB;
- networking;
- graphical desktop environments;
- advanced storage drivers;
- POSIX compatibility;
- self-hosting;
- a custom filesystem;
- a strict microkernel architecture.

## Definition of success

The first major project cycle is successful when the system can:

1. boot reproducibly in QEMU;
2. report failures through serial diagnostics;
3. manage physical and virtual memory safely;
4. schedule multiple kernel threads;
5. enter Ring 3;
6. load and execute an ELF program from an initramfs;
7. allow that program to write to the console and exit through system calls;
8. run automated host-side and QEMU integration tests in CI.

## Design philosophy

The system should prefer simple implementations that are easy to inspect and verify. More sophisticated data structures and optimizations are introduced only when a demonstrated limitation requires them.

A bitmap physical allocator is preferred before a buddy allocator. A simple kernel heap is preferred before a slab allocator. Single-core scheduling is preferred before SMP. UEFI is preferred before adding legacy BIOS support.

The project values a stable development process more than rapid accumulation of features.
