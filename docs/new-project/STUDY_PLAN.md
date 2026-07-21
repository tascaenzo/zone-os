# OS Development Study Plan

This study plan follows the project milestones. It is not a prerequisite wall: study and implementation should alternate. Each topic should be learned deeply enough to explain it, implement a minimal version and debug common failures.

The implementation begins on **x86_64 in 64-bit mode**, while the study plan separates universal operating-system concepts from x86_64-specific mechanisms.

## Study method for every milestone

For each subject:

1. learn the architecture-neutral concept;
2. identify the contract the kernel needs;
3. study the x86_64 mechanism used for the first backend;
4. implement the smallest observable version;
5. create success and failure tests;
6. explain the result without hiding bootloader or hardware assumptions;
7. write down what would change on another architecture.

Each learning unit should produce:

- [ ] concise personal notes;
- [ ] a diagram;
- [ ] a glossary;
- [ ] one minimal experiment;
- [ ] one failure experiment;
- [ ] one YouTube episode outline;
- [ ] references to primary documentation.

---

# Foundation A — Modern freestanding C23

## Learn

- translation units;
- declarations and definitions;
- object lifetime and storage duration;
- integer conversion and promotion rules;
- pointer arithmetic;
- alignment and padding;
- strict aliasing and effective types;
- undefined, unspecified and implementation-defined behavior;
- volatile semantics and its limitations;
- atomics at a conceptual level;
- freestanding versus hosted implementations;
- compiler builtins and extensions;
- calling conventions and ABI boundaries.

## C23 features to use deliberately

- `static_assert`;
- standard attributes such as `[[noreturn]]`, `[[nodiscard]]` and `[[maybe_unused]]`;
- fixed-underlying-type enums where compiler support is verified;
- `nullptr` where supported by the selected compiler baseline;
- binary literals where they improve bit-field explanations;
- checked and explicit integer operations.

## Exercises

- [ ] Inspect structure layout with `sizeof`, `alignof` and `offsetof`.
- [ ] Write overflow-safe alignment helpers.
- [ ] Implement `memset`, `memcpy`, `memmove`, `memcmp` and `strlen`.
- [ ] Test overlapping memory moves.
- [ ] Demonstrate why signed overflow is not a wrapping operation.
- [ ] Compare generated assembly for `volatile` and non-volatile accesses.
- [ ] Create a compiler abstraction header for attributes and barriers.

## Completion signal

You can explain why code that looks correct in C may still be invalid for a kernel because of undefined behavior, ABI assumptions or optimizer transformations.

---

# Foundation B — Computer architecture

## Architecture-neutral topics

- privilege levels;
- instruction execution and pipeline basics;
- registers and processor state;
- interrupts and exceptions;
- virtual and physical addresses;
- caches and memory hierarchy;
- memory ordering;
- MMIO versus port IO;
- DMA concept;
- multiprocessor basics;
- firmware and hardware-description mechanisms.

## x86_64 focus

- general-purpose registers;
- `RIP`, `RSP`, `RFLAGS`;
- long mode;
- canonical addresses;
- control registers;
- model-specific registers;
- GDT, TSS and IDT;
- paging hierarchy;
- CPUID;
- APIC family;
- `syscall/sysret` and `iretq`;
- System V AMD64 calling convention as a reference, not an unquestioned kernel ABI.

## Exercises

- [ ] Read and annotate a register dump.
- [ ] Decode a canonical virtual address.
- [ ] Draw four-level x86_64 page translation.
- [ ] Explain exception versus hardware interrupt.
- [ ] Inspect CPUID output in a user-space experiment.
- [ ] Step through a C function prologue and epilogue in GDB.

## Completion signal

You can follow a CPU from kernel entry to a C function and explain where the stack, instruction pointer, page tables and privilege state come from.

---

# Foundation C — Toolchain, ELF and linking

## Learn

- preprocessing, compilation, assembly and linking;
- object files;
- symbols and relocations;
- ELF headers, sections and program headers;
- static versus dynamic linking;
- linker scripts;
- load memory address versus virtual memory address;
- debug information;
- symbol maps;
- compiler-generated runtime helpers;
- why freestanding code can still cause unresolved compiler builtins.

## Exercises

- [ ] Compile one C source to assembly.
- [ ] Inspect an object with `readelf`, `llvm-readobj` and `objdump`.
- [ ] Build a minimal ELF with a custom linker script.
- [ ] Find entry point, sections and load segments.
- [ ] Trigger and resolve a missing compiler-runtime symbol.
- [ ] Generate and inspect a link map.

## Completion signal

You can explain exactly how source files become a loadable kernel ELF and how the bootloader finds its entry point.

---

# M0 study — Build engineering

## Architecture-neutral topics

- dependency graphs;
- incremental builds;
- generated artifacts;
- hermetic and reproducible builds;
- build profiles;
- cross compilation;
- host tools versus target binaries;
- dependency version pinning;
- CI caching and artifact retention.

## Tools

- Meson;
- Ninja;
- Clang;
- LLD;
- Python only for orchestration tasks that Meson should not own;
- QEMU;
- GDB.

## Exercises

- [ ] Build two C translation units incrementally.
- [ ] Verify automatic header dependencies.
- [ ] Keep debug and release build directories side by side.
- [ ] Generate `compile_commands.json`.
- [ ] Time cold, incremental and no-op builds.
- [ ] Reproduce the build in a clean container.

## Questions to answer in the video

- Why is deleting the build directory on every compilation inefficient?
- What does Ninja know that a shell script does not?
- Which programs run on the host, and which artifacts target the kernel machine?

---

# M1 study — Firmware, boot and execution environment

## Architecture-neutral topics

- firmware responsibility;
- bootloader responsibility;
- kernel responsibility;
- boot protocol contracts;
- normalized boot information;
- early initialization ordering;
- stack requirements;
- controlled halt and failure reporting.

## x86_64 and UEFI topics

- UEFI concept and EFI System Partition;
- PE/COFF role for EFI applications;
- Limine loading an ELF64 kernel;
- long-mode assumptions supplied by the boot protocol;
- initial memory map;
- serial communication through legacy COM1 under QEMU.

## Exercises

- [ ] Boot a minimal kernel and stop at its entry symbol in GDB.
- [ ] Inspect the initial stack pointer alignment.
- [ ] Print normalized firmware and memory information.
- [ ] Remove one mandatory boot response and verify controlled failure.
- [ ] Compare UEFI boot with the conceptual legacy BIOS path.

## Abstraction reflection

Document what would change if the backend used ARM64 with UEFI or RISC-V with SBI/OpenSBI.

---

# M2 study — Exceptions, interrupt frames and diagnostics

## Architecture-neutral topics

- synchronous exceptions;
- asynchronous interrupts;
- saved execution contexts;
- reentrancy;
- fatal versus recoverable faults;
- panic design;
- emergency logging;
- stack unwinding fundamentals.

## x86_64 topics

- IDT gates;
- exception vectors;
- error-code exceptions;
- privilege transitions;
- TSS and IST;
- `iretq` frame;
- page-fault error code;
- CR2;
- double faults.

## Exercises

- [ ] Decode a manually constructed exception frame.
- [ ] Trigger divide-by-zero, invalid opcode and page fault.
- [ ] Verify the C and assembly context layouts using static assertions.
- [ ] Generate a stack trace using frame pointers.
- [ ] Trigger a nested failure and observe the emergency path.

## Abstraction reflection

Separate `exception_info` from the raw x86_64 interrupt frame. Record which fields are universal and which are platform-specific.

---

# M3 study — Physical memory management

## Architecture-neutral topics

- physical memory ownership;
- page frames;
- firmware memory maps;
- reserved, reclaimable and device memory;
- fragmentation;
- bitmap, free-list and buddy allocation;
- contiguous allocation;
- metadata placement;
- ownership invariants.

## x86_64 topics

- 4 KiB base pages;
- large-page awareness;
- memory map supplied through Limine;
- physical-address width discovery;
- MMIO regions.

## Exercises

- [ ] Normalize overlapping synthetic memory maps.
- [ ] Implement and host-test a bitmap allocator.
- [ ] Allocate all pages and free them in randomized order.
- [ ] Detect double free and unaligned free.
- [ ] Compare bitmap and buddy tradeoffs without implementing buddy yet.

## Abstraction reflection

The PMM must know page size and physical limits through platform configuration, not through scattered constants.

---

# M4 study — Virtual memory

## Architecture-neutral topics

- address spaces;
- virtual-to-physical translation;
- page permissions;
- user/kernel separation;
- demand versus eager mapping;
- copy-on-write concept;
- direct physical maps;
- TLBs;
- address-space switching;
- guard pages;
- executable versus writable memory.

## x86_64 topics

- PML4, PDPT, PD and PT;
- canonical addresses;
- page-table entry flags;
- CR3;
- NXE and NX;
- `invlpg`;
- PCID as a deferred optimization;
- higher-half kernel design.

## Exercises

- [ ] Walk a virtual address by hand.
- [ ] Implement map, resolve and unmap tests.
- [ ] Test mappings across every table boundary.
- [ ] Produce read-only and NX faults.
- [ ] Destroy an address space and verify frame reclamation.
- [ ] Add a guard page below a stack.

## Abstraction reflection

Generic VM flags should not reuse hardware bit positions. A backend must translate them.

---

# M5 study — Dynamic allocation

## Architecture-neutral topics

- allocation semantics;
- alignment;
- fragmentation;
- metadata corruption;
- free lists;
- boundary tags;
- splitting and coalescing;
- slab and object caches;
- allocation context restrictions;
- debug poisoning and canaries.

## Exercises

- [ ] Write a bump allocator.
- [ ] Write and host-test an aligned free-list allocator.
- [ ] Fuzz allocation/free sequences.
- [ ] Measure fragmentation.
- [ ] Compare free-list, buddy and slab use cases.
- [ ] Verify integer-overflow handling in size calculations.

## Abstraction reflection

Keep heap policy separate from the VM mechanism that supplies pages.

---

# M6 study — Hardware discovery, interrupts and clocks

## Architecture-neutral topics

- hardware-description tables;
- interrupt controllers;
- interrupt routing;
- masking and acknowledgement;
- edge versus level triggering;
- clock sources;
- clock events;
- monotonic time;
- calibration;
- timer deadlines versus periodic ticks.

## x86_64 topics

- ACPI RSDP, XSDT and checksums;
- MADT;
- Local APIC;
- IOAPIC;
- legacy PIC;
- LAPIC timer;
- HPET and TSC concepts;
- spurious interrupt vector.

## Exercises

- [ ] Parse synthetic ACPI tables with invalid lengths and checksums.
- [ ] Draw interrupt routing from device to kernel handler.
- [ ] Receive and acknowledge a timer interrupt.
- [ ] Mask and unmask an interrupt source.
- [ ] Compare clock source and clock event device.

## Abstraction reflection

Use controller and clock interfaces rather than naming APIC inside scheduler or generic time code.

---

# M7 study — Concurrency and scheduling

## Architecture-neutral topics

- execution context;
- kernel thread;
- process versus thread;
- cooperative and preemptive scheduling;
- scheduling policies;
- ready, blocked and terminated states;
- race conditions;
- critical sections;
- interrupt disabling versus locks;
- wait queues;
- lifetime and reclamation.

## x86_64 topics

- context-switch register set;
- stack switching;
- ABI-preserved registers;
- interrupt return into a selected context;
- per-CPU concepts, even though SMP is deferred.

## Exercises

- [ ] Implement a host-side scheduler queue model.
- [ ] Construct a new kernel-thread stack manually.
- [ ] Switch cooperatively between two threads.
- [ ] Add timer preemption.
- [ ] Block and wake a thread.
- [ ] Verify register preservation with known patterns.

## Abstraction reflection

The scheduler chooses a thread; architecture code performs the low-level context switch.

---

# M8 study — Protection and user mode

## Architecture-neutral topics

- protection domains;
- processes;
- privilege separation;
- kernel and user address ranges;
- fault containment;
- safe memory copying;
- time-of-check/time-of-use issues;
- process lifecycle.

## x86_64 topics

- Ring 0 and Ring 3;
- segment selectors in long mode;
- TSS `RSP0`;
- user/supervisor page bit;
- `iretq` transition;
- SMAP/SMEP as later hardening features.

## Exercises

- [ ] Enter a user function.
- [ ] Read the current privilege level.
- [ ] Attempt a privileged instruction from user mode.
- [ ] Attempt to modify a kernel page.
- [ ] Safely terminate only the faulty process.
- [ ] Test invalid ranges passed to copy helpers.

## Abstraction reflection

Model privilege as kernel/user execution domains rather than exposing x86 ring numbers to generic process code.

---

# M9 study — System calls and ABI design

## Architecture-neutral topics

- syscall purpose;
- ABI stability;
- argument passing;
- error conventions;
- capability and permission checks;
- pointer validation;
- blocking syscalls;
- restart semantics;
- versioning.

## x86_64 topics

- `syscall/sysret` registers and MSRs;
- `iretq` fallback path;
- `swapgs` concept for future per-CPU state;
- canonical return-address validation;
- calling-convention differences between user ABI and syscall ABI.

## Exercises

- [ ] Write an ABI table for three syscalls.
- [ ] Implement unknown-syscall handling.
- [ ] Test bad pointers and extreme lengths.
- [ ] Verify clobbered and preserved registers.
- [ ] Trace a syscall from user stub to kernel and back.

## Abstraction reflection

The syscall semantic layer should not depend on the machine entry instruction.

---

# M10 study — Archives, executable formats and process images

## Architecture-neutral topics

- byte-stream validation;
- archive formats;
- executable formats;
- segments versus sections;
- loader security;
- integer overflow during parsing;
- process image construction;
- initial stack and argument conventions.

## x86_64 topics

- ELF64;
- x86_64 machine identifier;
- little-endian encoding;
- loadable segments;
- executable entry point;
- static user binaries.

## Exercises

- [ ] Parse ELF64 host-side using only bounds-checked reads.
- [ ] Reject malformed headers and overflowing offsets.
- [ ] Map segments with correct permissions.
- [ ] Zero BSS.
- [ ] Build a tiny freestanding user program.
- [ ] Load and run it from initramfs.

## Abstraction reflection

Keep the generic executable loader separate from the architecture-specific validation and initial CPU-context creation.

---

# M11 study — Filesystems and VFS

## Architecture-neutral topics

- namespace;
- path resolution;
- files, directories and devices;
- inode/vnode concepts;
- mounts;
- file handles and descriptors;
- offsets;
- filesystem operations;
- caching and lifetime basics;
- permissions as a later extension.

## Exercises

- [ ] Build a host-side in-memory VFS prototype.
- [ ] Parse absolute paths safely.
- [ ] Handle `.`, `..` and repeated separators.
- [ ] Mount an initramfs root.
- [ ] Implement independent file offsets.
- [ ] Add a console device.

## Abstraction reflection

VFS clients must not know whether a file comes from TAR, FAT, ext2 or a device driver.

---

# Deferred study tracks

Study these only when the initial user-space cycle is stable.

## Synchronization and SMP

- atomic operations;
- memory models;
- spinlocks;
- mutexes;
- reader/writer locks;
- per-CPU data;
- inter-processor interrupts;
- TLB shootdowns;
- lock ordering;
- deadlock detection.

## Devices and buses

- PCI/PCIe configuration;
- MMIO;
- DMA;
- MSI/MSI-X;
- block-device abstraction;
- AHCI or NVMe;
- USB architecture.

## Persistent filesystems

- block cache;
- FAT or ext2;
- consistency;
- allocation maps;
- directory structures;
- crash behavior.

## Networking

- NIC drivers;
- Ethernet;
- ARP;
- IPv4/IPv6;
- routing;
- UDP and TCP;
- socket abstraction.

## Second architecture validation

After the generic contracts are stable, use a small second backend to test them. ARM64 or RISC-V are candidates, but the purpose is to validate abstractions, not to promise immediate feature parity.

---

# Recommended primary references

Prefer primary sources and specifications over tutorial copying:

- ISO C language material and compiler documentation;
- System V AMD64 ABI;
- Intel or AMD architecture manuals;
- UEFI specification;
- Limine protocol documentation;
- ELF specification;
- ACPI specification;
- Meson and Ninja documentation;
- QEMU and GDB manuals.

Secondary resources are useful for intuition, but each hardware-sensitive implementation should ultimately be checked against an authoritative specification.
