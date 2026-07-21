# Roadmap and Milestones

Each milestone must end with an observable result, automated verification where practical, updated documentation and a tagged repository state.

## M0 — Reproducible workspace

Deliverables:

- Meson cross file for x86_64;
- Ninja incremental build;
- Clang and LLD toolchain configuration;
- dependency checker;
- debug and release build directories;
- QEMU launcher;
- GDB launcher;
- CI build job.

Completion criteria:

- a clean clone can be configured with one documented command;
- changing one C source recompiles only that translation unit and relinks;
- build output never modifies the source tree.

## M1 — Controlled UEFI boot

Deliverables:

- Limine configuration;
- kernel ELF;
- UEFI boot image;
- validated bootloader requests;
- serial initialization;
- controlled halt path.

Observable result:

```text
[boot] kernel entered
[boot] Limine responses validated
[ok] controlled boot complete
```

## M2 — Diagnostics and CPU exceptions

Deliverables:

- structured logging;
- panic interface;
- GDT and TSS;
- IDT;
- handlers for CPU exceptions;
- register dump;
- page-fault error decoding;
- basic stack trace.

Completion criteria:

- intentional divide-by-zero and page-fault tests produce deterministic diagnostics;
- panic does not allocate memory.

## M3 — Physical memory management

Deliverables:

- normalized memory map;
- page reservation rules;
- bitmap allocator;
- single-page and contiguous-page allocation as separate APIs;
- double-free detection in debug builds;
- statistics and integrity checks.

## M4 — Virtual memory management

Deliverables:

- page-table walking;
- map, unmap and resolve operations;
- direct physical map;
- high-half kernel mappings;
- NX support;
- TLB invalidation;
- independent address-space object.

## M5 — Kernel dynamic memory

Deliverables:

- early bump allocator;
- page-backed heap;
- simple free-list allocation;
- alignment support;
- debug poisoning and guard checks;
- host-side allocator tests where possible.

Advanced buddy and slab allocators are postponed until measurements justify them.

## M6 — Hardware interrupts and time

Deliverables:

- local APIC initialization;
- IOAPIC routing;
- legacy PIC disable path;
- timer calibration;
- periodic timer interrupts;
- interrupt registration API.

## M7 — Kernel threads

Deliverables:

- thread representation;
- per-thread kernel stack;
- context switching;
- ready queue;
- idle thread;
- round-robin scheduling;
- preemption;
- blocked and terminated states.

## M8 — User mode

Deliverables:

- Ring 3 entry and return;
- user stack;
- separate address space;
- safe copy-to-user and copy-from-user primitives;
- intentional user-fault isolation test.

## M9 — System calls

Deliverables:

- documented syscall ABI;
- syscall dispatch;
- `write`, `exit` and `yield`;
- pointer and length validation;
- unknown-syscall behavior.

## M10 — Initramfs and ELF programs

Deliverables:

- Limine module loading;
- TAR initramfs reader;
- ELF64 validation and loader;
- initial process creation;
- first user program.

Observable result:

```text
hello from userspace
```

## M11 — Minimal VFS

Deliverables:

- vnode model;
- path lookup;
- file descriptors;
- `/dev/console`;
- initramfs mounted as root;
- basic `open`, `read`, `write` and `close` interfaces.

## Later milestones

Potential later work includes synchronization primitives, pipes, SMP, storage, FAT or ext2, networking, a shell and graphics. These must not be scheduled until the initial user-space cycle is stable.
