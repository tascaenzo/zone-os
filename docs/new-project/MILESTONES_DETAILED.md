# Detailed Milestones and Completion Checklists

This document turns the high-level roadmap into executable project milestones.

The initial implementation target is **64-bit x86 (`x86_64`)**, but kernel-facing contracts should avoid unnecessary architecture assumptions. Platform-specific details belong behind explicit architecture interfaces.

## Milestone rules

Every milestone must satisfy all of these rules before it is considered complete:

- [ ] The result is observable in QEMU or through a host-side test.
- [ ] Failure paths are deliberately exercised.
- [ ] Public interfaces are documented.
- [ ] Architecture-specific code is isolated under `kernel/arch/x86_64/`.
- [ ] Generic kernel code does not directly access x86 registers, ports or descriptor tables.
- [ ] Debug and release builds compile without warnings.
- [ ] The CI pipeline executes the relevant tests.
- [ ] The documentation and architecture diagrams are updated.
- [ ] The milestone ends with a reproducible Git tag.
- [ ] The corresponding YouTube episode notes include start and completion references.

---

# M0 — Reproducible 64-bit development workspace

## Goal

Create a fast, understandable and reproducible workspace for a freestanding 64-bit kernel.

## Architecture-neutral contract

The build system must model the target platform through configuration rather than hard-coding x86 commands across scripts.

Expected concepts:

- target architecture;
- target machine word size;
- compiler and linker;
- platform sources;
- emulator backend;
- firmware and boot protocol;
- debug transport.

## x86_64 implementation

- Clang with an x86_64 freestanding target;
- LLD linker;
- Meson configuration;
- Ninja incremental build;
- QEMU `q35` machine;
- UEFI firmware;
- GDB remote debugging;
- serial terminal output.

## Deliverables

- [ ] Root `meson.build`.
- [ ] `meson_options.txt` for debug features and kernel tests.
- [ ] `config/x86_64.ini` cross file.
- [ ] Explicit compiler and linker flags.
- [ ] Debug and release build profiles.
- [ ] Dependency checking command.
- [ ] `tools/dev` command wrapper.
- [ ] QEMU launcher.
- [ ] GDB launcher and initial command file.
- [ ] CI workflow for configuration and compilation.
- [ ] Pinned tool versions or documented supported ranges.
- [ ] No mandatory Docker dependency for the local edit-build-run cycle.
- [ ] Optional container image for reproducibility and CI.

## Verification checklist

- [ ] A clean clone configures with one documented command.
- [ ] The source tree remains unchanged after a build.
- [ ] Touching one C file recompiles only one translation unit and relinks.
- [ ] Touching one header recompiles only dependent translation units.
- [ ] A no-op rebuild performs no compilation.
- [ ] Parallel builds succeed.
- [ ] Paths containing spaces do not break the build.
- [ ] Debug and release artifacts can coexist.
- [ ] Build failure messages identify the failed command.
- [ ] `compile_commands.json` is generated for editor tooling.

## Exit criteria

A contributor can clone, configure, compile, inspect the ELF and launch the debugger without manually editing any path.

---

# M1 — Controlled 64-bit boot

## Goal

Enter the kernel in a known 64-bit execution environment, validate boot information and halt safely.

## Architecture-neutral contract

Define a small boot-information structure owned by the kernel rather than leaking bootloader-specific structures throughout the codebase.

Suggested interface:

```c
struct boot_info;

[[nodiscard]] bool boot_capture_info(struct boot_info *out);
[[noreturn]] void platform_halt(void);
```

The generic kernel should receive normalized information such as:

- memory regions;
- kernel image location;
- firmware type;
- framebuffer description, when available;
- modules;
- command line;
- hardware-description roots.

## x86_64 implementation

- Limine protocol;
- UEFI boot;
- ELF64 kernel;
- higher-half virtual entry if selected by the linker design;
- serial port as the first diagnostic backend.

## Deliverables

- [ ] Linker script with documented sections.
- [ ] Explicit kernel entry symbol.
- [ ] Limine request declarations.
- [ ] Response validation before dereference.
- [ ] Conversion from Limine data to kernel-owned boot structures.
- [ ] Early serial driver.
- [ ] Controlled halt loop.
- [ ] Boot image generation as an incremental target.
- [ ] Script to inspect kernel ELF headers and sections.

## Verification checklist

- [ ] Kernel entry is reached under UEFI.
- [ ] The CPU is confirmed to be in 64-bit mode.
- [ ] Stack alignment matches the selected ABI contract.
- [ ] Bootloader responses are checked for null and count bounds.
- [ ] Missing optional information does not crash the kernel.
- [ ] Missing mandatory information produces a deterministic halt message.
- [ ] Kernel virtual and physical addresses are logged.
- [ ] Memory-map entry count is logged.
- [ ] QEMU exits or halts deterministically during automated tests.

## Observable result

```text
[boot] entered 64-bit kernel
[boot] boot information normalized
[boot] serial console ready
[ok] controlled boot complete
```

## Exit criteria

The kernel starts repeatedly with deterministic output and no direct bootloader dependency outside the boot adapter.

---

# M2 — Diagnostics, execution context and exceptions

## Goal

Build the diagnostic foundation required to debug every later subsystem.

## Architecture-neutral contract

Generic facilities:

- logging levels and sinks;
- panic reporting;
- architecture-neutral register/context presentation;
- exception classification;
- stack tracing interface;
- source-location macros.

Suggested boundaries:

```c
struct cpu_context;
struct exception_info;

void exception_dispatch(const struct exception_info *info,
                        const struct cpu_context *context);
[[noreturn]] void panic(const char *message);
```

## x86_64 implementation

- GDT;
- TSS;
- IDT;
- 256 interrupt stubs;
- exception error-code normalization;
- CR2 capture for page faults;
- frame-pointer stack trace;
- double-fault emergency stack through IST.

## Deliverables

- [ ] Structured log API.
- [ ] Serial log sink.
- [ ] Panic path without heap allocation.
- [ ] `[[noreturn]]` annotations.
- [ ] GDT setup.
- [ ] TSS setup.
- [ ] Dedicated stack for critical exceptions.
- [ ] IDT construction.
- [ ] Assembly entry stubs.
- [ ] Uniform saved-context structure.
- [ ] Static assertions for assembly/C layout agreement.
- [ ] Human-readable exception names.
- [ ] Page-fault bit decoding.
- [ ] Register dump.
- [ ] Basic stack trace.

## Verification checklist

- [ ] Divide-by-zero reaches the expected handler.
- [ ] Invalid opcode reaches the expected handler.
- [ ] Breakpoint exception can return safely.
- [ ] Page fault reports the faulting virtual address.
- [ ] Page-fault access type is decoded.
- [ ] Stack trace includes at least two known functions.
- [ ] Panic remains functional before memory allocation exists.
- [ ] Nested fatal faults terminate deterministically.
- [ ] C structure offsets match assembly constants.
- [ ] Exception tests run automatically in QEMU.

## Exit criteria

Every fatal CPU exception produces enough information to identify the instruction, saved context and likely cause.

---

# M3 — Physical memory ownership

## Goal

Represent physical memory safely and allocate page frames without hidden bootloader assumptions.

## Architecture-neutral contract

A physical-memory manager deals in page-frame identifiers or strongly named physical addresses. It must not expose x86 page-table formats.

Suggested interface:

```c
typedef struct { uintptr_t value; } paddr_t;

enum pmm_status {
    PMM_OK,
    PMM_OUT_OF_MEMORY,
    PMM_INVALID_ARGUMENT,
    PMM_NOT_OWNED,
    PMM_DOUBLE_FREE,
};

[[nodiscard]] enum pmm_status pmm_alloc_page(paddr_t *out);
[[nodiscard]] enum pmm_status pmm_free_page(paddr_t page);
```

## x86_64 implementation

- 4 KiB base pages initially;
- bitmap-backed page-frame allocator;
- Limine memory-map adapter;
- explicit reservations for kernel, boot structures, modules and framebuffer.

## Deliverables

- [ ] Normalized memory-region model.
- [ ] Region sorting and overlap validation.
- [ ] Page-alignment helpers.
- [ ] Page-frame database or bitmap.
- [ ] Reservation API.
- [ ] Single-page allocation.
- [ ] Multiple-page allocation with explicit semantics.
- [ ] Free operation.
- [ ] Statistics.
- [ ] Integrity checker.
- [ ] Debug ownership metadata where affordable.

## Verification checklist

- [ ] All non-usable regions remain unavailable.
- [ ] Kernel image pages are reserved.
- [ ] Boot structures remain reserved until explicitly released.
- [ ] Every returned page is aligned.
- [ ] No page is returned twice while allocated.
- [ ] Allocation exhaustion returns an error rather than panicking unexpectedly.
- [ ] Double free is detected in debug mode.
- [ ] Invalid and unaligned frees are rejected.
- [ ] Allocate-all/free-all restores the initial free-page count.
- [ ] Region normalization is host-tested with malformed maps.

## Exit criteria

The kernel can prove which physical pages it owns and can detect common ownership violations.

---

# M4 — Virtual address spaces

## Goal

Create an architecture-independent virtual-memory API backed initially by x86_64 page tables.

## Architecture-neutral contract

Generic code works with address-space objects and portable mapping permissions.

```c
typedef struct address_space address_space_t;

enum vm_flags {
    VM_READ    = 1u << 0,
    VM_WRITE   = 1u << 1,
    VM_EXECUTE = 1u << 2,
    VM_USER    = 1u << 3,
    VM_GLOBAL  = 1u << 4,
};
```

Architecture code translates generic permissions into hardware entries.

## x86_64 implementation

- four-level paging initially;
- canonical-address validation;
- page-table allocation through PMM;
- direct physical map;
- higher-half kernel;
- NX support;
- CR3 activation;
- `invlpg` and address-space TLB rules.

## Deliverables

- [ ] Address-space object.
- [ ] Page-table walker.
- [ ] Map operation.
- [ ] Unmap operation.
- [ ] Resolve/query operation.
- [ ] Permission conversion layer.
- [ ] Direct-map helpers.
- [ ] Kernel mapping inheritance policy.
- [ ] User/kernel address split policy.
- [ ] TLB invalidation abstraction.
- [ ] Page-table destruction.
- [ ] Debug dump and integrity checker.

## Verification checklist

- [ ] Mapping then resolving returns the expected physical address.
- [ ] Unmapping removes access.
- [ ] Duplicate mapping follows a documented policy.
- [ ] Non-canonical addresses are rejected.
- [ ] User mappings cannot overwrite protected kernel mappings.
- [ ] Read-only pages fault on writes.
- [ ] NX pages fault on instruction fetch when supported.
- [ ] Address-space destruction releases page-table frames.
- [ ] Failed partial mappings roll back correctly.
- [ ] Tests cover page-table boundaries.

## Exit criteria

The generic kernel can create, modify, activate and destroy an address space without manipulating x86_64 page-table entries directly.

---

# M5 — Kernel dynamic memory

## Goal

Provide reliable dynamic allocation for kernel objects without prematurely introducing complex allocators.

## Architecture-neutral contract

The kernel allocation API must define:

- alignment;
- zero-size behavior;
- out-of-memory behavior;
- ownership and lifetime;
- interrupt-context restrictions;
- debug guarantees.

## Initial implementation

- early bump allocator;
- page-backed heap expansion;
- aligned free-list allocator;
- optional allocation metadata in debug builds.

## Deliverables

- [ ] Early allocator with explicit retirement point.
- [ ] `kmalloc`, `kfree` and aligned allocation.
- [ ] Heap virtual-region manager.
- [ ] Page acquisition from PMM/VMM.
- [ ] Block splitting.
- [ ] Adjacent-block coalescing.
- [ ] Overflow-safe size calculations.
- [ ] Poison patterns in debug builds.
- [ ] Allocation statistics.
- [ ] Heap integrity walk.

## Verification checklist

- [ ] Common alignments are respected.
- [ ] Zero-size behavior is documented and tested.
- [ ] Allocation failure is recoverable where required.
- [ ] Split and merge behavior is host-tested.
- [ ] Repeated random allocation sequences preserve integrity.
- [ ] Double free and invalid free are diagnosed in debug builds.
- [ ] Heap expansion maps and releases pages correctly.
- [ ] The panic path does not depend on the heap.
- [ ] No allocator is called from unsupported interrupt contexts.

## Exit criteria

Kernel subsystems can allocate objects with a documented failure model, and allocator corruption is discoverable close to its source.

---

# M6 — Interrupt delivery and time

## Goal

Deliver hardware events and establish a monotonic kernel time source.

## Architecture-neutral contract

Define generic concepts:

- interrupt vector or event identifier;
- interrupt source;
- handler registration;
- acknowledgement;
- mask/unmask;
- monotonic clock;
- timer deadline or periodic tick.

Do not expose APIC register layouts to generic code.

## x86_64 implementation

- ACPI discovery sufficient for interrupt controllers;
- Local APIC;
- IOAPIC;
- legacy PIC disable;
- LAPIC or alternative timer calibration;
- periodic timer for the first scheduler.

## Deliverables

- [ ] Interrupt-controller interface.
- [ ] Timer/clock interface.
- [ ] ACPI table checksum and bounds validation.
- [ ] MADT parsing.
- [ ] Local APIC setup.
- [ ] IOAPIC redirection setup.
- [ ] Legacy PIC shutdown.
- [ ] IRQ registration and dispatch.
- [ ] Spurious interrupt handling.
- [ ] Monotonic tick counter.
- [ ] Timer calibration strategy.

## Verification checklist

- [ ] Timer interrupts arrive repeatedly.
- [ ] Each interrupt is acknowledged exactly once.
- [ ] Unknown interrupts are diagnosed safely.
- [ ] Masking prevents delivery.
- [ ] Unmasking restores delivery.
- [ ] Interrupt handlers do not allocate unless explicitly permitted.
- [ ] Time never moves backward.
- [ ] Spurious interrupts do not panic the system.
- [ ] ACPI parsing rejects invalid checksums and lengths.

## Exit criteria

The kernel receives deterministic timer events through an architecture-neutral dispatcher.

---

# M7 — Kernel threads and scheduler

## Goal

Run multiple independent kernel execution contexts on one CPU.

## Architecture-neutral contract

Define:

- thread identity;
- saved execution context;
- thread states;
- scheduling policy interface;
- blocking and wake-up;
- preemption boundary;
- architecture context-switch hooks.

## x86_64 implementation

- saved callee-preserved registers;
- stack switching;
- timer-driven preemption;
- single-core round-robin scheduler.

## Deliverables

- [ ] Thread object.
- [ ] Dedicated kernel stack.
- [ ] Initial context construction.
- [ ] Architecture context-switch routine.
- [ ] Ready queue.
- [ ] Idle thread.
- [ ] Cooperative yield.
- [ ] Timer preemption.
- [ ] Block and wake operations.
- [ ] Thread exit and resource reclamation.
- [ ] Scheduler invariants and tracing.

## Verification checklist

- [ ] Two threads alternate cooperatively.
- [ ] Two threads alternate through timer preemption.
- [ ] Register values survive context switches.
- [ ] Each thread uses its own stack.
- [ ] A blocked thread does not execute.
- [ ] A woken thread becomes runnable.
- [ ] Exiting threads are eventually reclaimed.
- [ ] Ready-queue corruption is detected.
- [ ] The idle thread runs only with no runnable work.

## Exit criteria

Multiple kernel threads run, block, resume and terminate predictably on a single x86_64 processor.

---

# M8 — User execution domain

## Goal

Execute untrusted code in a less-privileged address space and contain its faults.

## Architecture-neutral contract

Generic process concepts:

- process-owned address space;
- user virtual-memory range;
- user entry point and stack;
- safe user-memory access;
- fault ownership;
- process termination.

## x86_64 implementation

- Ring 3 segments;
- TSS kernel stack selection;
- `iretq` entry initially;
- user-accessible page permissions;
- supervisor/user fault distinction.

## Deliverables

- [ ] Process object.
- [ ] Per-process address space.
- [ ] User stack construction.
- [ ] Ring 3 transition.
- [ ] Return to kernel through a controlled mechanism.
- [ ] User pointer range checks.
- [ ] `copy_from_user` and `copy_to_user`.
- [ ] Process-fault termination.
- [ ] Kernel-fault distinction.

## Verification checklist

- [ ] A user function executes at reduced privilege.
- [ ] User code cannot write kernel pages.
- [ ] User code cannot execute privileged instructions.
- [ ] A user page fault terminates only the offending process.
- [ ] A kernel page fault still triggers kernel panic.
- [ ] Invalid user pointers are rejected.
- [ ] User stack alignment matches the selected ABI.
- [ ] Switching processes changes address spaces safely.

## Exit criteria

The kernel can execute and contain a deliberately faulty user program without losing control.

---

# M9 — System-call boundary

## Goal

Provide a documented and testable interface between user programs and the kernel.

## Architecture-neutral contract

The syscall layer defines:

- syscall numbering;
- argument types and ownership;
- return and error convention;
- restart/interruption policy;
- pointer validation;
- compatibility/versioning policy.

## x86_64 implementation

- start with a simple, debuggable entry mechanism;
- move to `syscall/sysret` after correctness is established;
- explicitly document register use and clobbers.

## Deliverables

- [ ] Syscall ABI document.
- [ ] Entry and return assembly.
- [ ] Dispatcher.
- [ ] Unknown-syscall error.
- [ ] `write`.
- [ ] `exit`.
- [ ] `yield`.
- [ ] Safe user-buffer handling.
- [ ] Per-syscall tracing in debug builds.

## Verification checklist

- [ ] Valid syscalls return expected results.
- [ ] Unknown syscall numbers return a stable error.
- [ ] Invalid user pointers cannot crash the kernel.
- [ ] Oversized lengths are rejected safely.
- [ ] ABI register preservation is tested.
- [ ] User code cannot select an arbitrary kernel return address.
- [ ] Process exit releases or schedules reclamation of resources.

## Exit criteria

A user program can print, yield and exit entirely through the documented syscall ABI.

---

# M10 — Initramfs and ELF64 program loading

## Goal

Load user programs from a boot-provided archive without depending on storage drivers.

## Architecture-neutral contract

Separate:

- boot module discovery;
- archive access;
- executable-format parsing;
- process image construction.

The generic loader consumes a byte source and produces validated load segments.

## x86_64 implementation

- Limine module adapter;
- TAR or USTAR initramfs;
- ELF64 little-endian executables for x86_64;
- initially static, non-dynamic executables.

## Deliverables

- [ ] Boot-module normalization.
- [ ] Bounds-checked archive reader.
- [ ] Path lookup inside initramfs.
- [ ] ELF header validation.
- [ ] Program-header validation.
- [ ] Segment mapping with permissions.
- [ ] BSS zeroing.
- [ ] Entry-point validation.
- [ ] Initial user stack.
- [ ] First freestanding user program.

## Verification checklist

- [ ] Valid executable starts.
- [ ] Wrong architecture is rejected.
- [ ] Truncated ELF files are rejected.
- [ ] Overflowing offsets and sizes are rejected.
- [ ] Overlapping invalid segments are rejected.
- [ ] Segment permissions match ELF flags.
- [ ] BSS is zero-filled.
- [ ] Entry point lies within a valid executable mapping.
- [ ] Malformed archive entries cannot read out of bounds.

## Observable result

```text
hello from userspace
```

## Exit criteria

The kernel creates a process from an initramfs ELF64 file and the program communicates through syscalls.

---

# M11 — Minimal Virtual File System

## Goal

Provide a generic namespace and file interface independent of the underlying filesystem.

## Architecture-neutral contract

Core concepts:

- vnode or inode-like object;
- mount;
- filesystem operations;
- file handle;
- descriptor table;
- path resolution;
- device nodes.

## Initial implementation

- read-only initramfs filesystem;
- `/dev/console` device;
- per-process file-descriptor table.

## Deliverables

- [ ] VFS object model.
- [ ] Mount table.
- [ ] Absolute path parser.
- [ ] Path-component traversal.
- [ ] File object and offset.
- [ ] Descriptor allocation.
- [ ] `open`, `read`, `write`, `close` kernel interfaces.
- [ ] Initramfs root mount.
- [ ] Console device node.
- [ ] Corresponding syscalls.

## Verification checklist

- [ ] Root path resolves.
- [ ] Existing initramfs files can be read.
- [ ] Missing paths return a stable error.
- [ ] Repeated separators and `.` follow documented behavior.
- [ ] `..` cannot escape the root.
- [ ] Descriptor limits are enforced.
- [ ] Closing an invalid descriptor is safe.
- [ ] Independent file handles maintain independent offsets.
- [ ] Console writes reach the selected log/terminal backend.

## Exit criteria

A user program can open and read a file from the initramfs and write its contents to `/dev/console`.

---

# Deferred milestones

The following are intentionally postponed until M0–M11 are stable:

- synchronization primitives beyond initial scheduler needs;
- SMP and per-CPU data;
- ACPI expansion;
- PCI discovery;
- storage drivers;
- FAT or ext2;
- pipes and IPC;
- networking;
- graphical output;
- USB;
- dynamic linking;
- POSIX compatibility;
- ARM64 or RISC-V backends.

When a second architecture is introduced, the abstraction boundaries created above must be validated by implementing the smallest possible second backend rather than redesigning the entire kernel in advance.
