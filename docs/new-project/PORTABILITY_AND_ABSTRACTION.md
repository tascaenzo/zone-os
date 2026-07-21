# Portability and Abstraction Strategy

The project starts on **x86_64 in 64-bit mode**, but the codebase should avoid turning x86_64 mechanisms into universal kernel concepts.

The goal is not immediate multi-architecture support. The goal is to preserve clear boundaries so that a future backend can be added without rewriting the generic kernel.

## Core principle

> Abstract stable concepts, not hypothetical hardware.

We should not invent a large universal HAL before understanding the needs of the first implementation. We should instead isolate direct hardware access, expose small contracts and revise those contracts only when concrete evidence requires it.

## Source-tree boundary

```text
kernel/
├── arch/
│   └── x86_64/
│       ├── boot/
│       ├── cpu/
│       ├── interrupts/
│       ├── memory/
│       ├── time/
│       └── context/
├── platform/
│   ├── firmware/
│   ├── interrupt_controller/
│   └── timer/
├── core/
├── mm/
├── sched/
├── process/
├── fs/
└── lib/
```

The exact layout may evolve, but these ownership rules should remain:

- `arch/x86_64` owns instructions, registers, descriptor tables and page-table formats;
- generic memory management owns allocation and mapping policy;
- generic scheduling owns thread states and policy;
- architecture code owns low-level context switching;
- boot-protocol adapters own Limine structures;
- generic kernel code consumes normalized boot information;
- device-independent code consumes interfaces, not APIC or COM1 registers.

## What must remain architecture-specific

- entry assembly;
- CPU feature detection;
- control-register access;
- MSR access;
- exception and interrupt stubs;
- GDT, TSS and IDT construction;
- page-table entry encoding;
- TLB invalidation instructions;
- context-switch assembly;
- privilege transition assembly;
- syscall entry assembly;
- port IO;
- architecture-specific barriers;
- APIC register access.

## What should be generic

- log formatting and routing;
- panic policy;
- normalized exception reporting;
- physical-page ownership;
- allocation policy;
- address-space lifecycle;
- generic mapping permissions;
- heap allocation;
- scheduler policy and queues;
- thread and process state;
- syscall semantics;
- ELF parsing framework;
- VFS;
- file descriptors;
- initramfs archive handling;
- tests for pure algorithms and parsers.

## Address types

Avoid passing unlabelled integers when the address domain matters.

```c
typedef struct {
    uintptr_t value;
} paddr_t;

typedef struct {
    uintptr_t value;
} vaddr_t;
```

These wrappers prevent accidental mixing and allow architecture-specific validation at boundaries.

Operations should be explicit:

```c
[[nodiscard]] bool paddr_add(paddr_t base, size_t offset, paddr_t *out);
[[nodiscard]] bool vaddr_add(vaddr_t base, size_t offset, vaddr_t *out);
```

## CPU context

Do not force one universal register structure on every subsystem.

Use two layers:

1. an architecture-owned raw context containing exact saved registers;
2. a generic diagnostic view containing common fields and accessors.

```c
struct arch_cpu_context;

struct execution_view {
    vaddr_t instruction_pointer;
    vaddr_t stack_pointer;
    uintptr_t flags;
    bool from_user;
};
```

The architecture backend converts or exposes accessors without discarding raw information.

## Exceptions

Generic code should reason about categories:

```c
enum exception_class {
    EXCEPTION_ARITHMETIC,
    EXCEPTION_INVALID_INSTRUCTION,
    EXCEPTION_MEMORY_ACCESS,
    EXCEPTION_BREAKPOINT,
    EXCEPTION_HARDWARE_FAILURE,
    EXCEPTION_UNKNOWN,
};
```

The x86_64 backend retains the vector number and architecture-specific error code.

## Virtual memory

Generic VM permissions must not match x86_64 page-table bits by accident.

```c
enum vm_permission {
    VM_PERMISSION_READ    = 1u << 0,
    VM_PERMISSION_WRITE   = 1u << 1,
    VM_PERMISSION_EXECUTE = 1u << 2,
    VM_PERMISSION_USER    = 1u << 3,
    VM_PERMISSION_GLOBAL  = 1u << 4,
};
```

The x86_64 backend translates these into present, writable, user, global and NX semantics.

Page size should be obtained through the architecture contract or build-time target description rather than repeated as a magic constant.

## Interrupts

Generic interrupt dispatch should not assume that every platform uses numbered IDT vectors.

A generic interrupt source can contain an opaque platform identifier:

```c
struct interrupt_source {
    uintptr_t platform_id;
};
```

Registration, masking and acknowledgement are operations supplied by the controller backend.

The first backend may map this directly to x86_64 vectors and IOAPIC entries.

## Time

Separate:

- **clock source**: reads a continuously advancing value;
- **clock event device**: asks hardware to interrupt at or after a deadline;
- **kernel timekeeping**: converts and accumulates time;
- **scheduler tick policy**: decides how the scheduler uses time.

Do not let the scheduler read LAPIC or TSC registers directly.

## Scheduler

The generic scheduler owns:

- thread states;
- run queues;
- policy;
- block/wake transitions;
- lifetime rules.

The architecture backend owns:

- initial register context;
- low-level switch;
- interrupt-return mechanics;
- user/kernel transition details.

## Boot protocols

Limine is an adapter, not the kernel's internal boot model.

```text
Limine structures
      ↓ validate and normalize
kernel boot_info
      ↓ consumed by
PMM, framebuffer, modules, firmware discovery
```

No subsystem outside the boot adapter should include `limine.h`.

## Firmware and hardware description

UEFI is used for the initial boot, while ACPI will describe relevant x86_64 hardware.

Generic code should receive discovered resources through normalized models. For example, the interrupt-controller subsystem should not parse ACPI itself. ACPI parsing discovers controllers and passes validated descriptions to their drivers.

## Drivers

Prefer capability-oriented interfaces over architecture names.

Examples:

- byte-output sink;
- interrupt controller;
- clock source;
- clock event device;
- framebuffer;
- block device;
- network device.

The early serial driver may be x86_64/QEMU-specific, but logging only sees a byte-output sink.

## Build-system abstraction

The build configuration should select:

- architecture sources;
- architecture compiler flags;
- linker script;
- boot image strategy;
- emulator arguments;
- debugger architecture;
- firmware assets.

A future architecture should add configuration and backend code rather than fork all build logic.

## Abstraction review checklist

Before merging a subsystem:

- [ ] Does generic code include an architecture-specific header unnecessarily?
- [ ] Are hardware bit positions leaking into generic enums?
- [ ] Is a bootloader structure stored beyond early boot?
- [ ] Is a physical address being treated as a dereferenceable pointer?
- [ ] Does a scheduler or VM policy function execute x86 assembly directly?
- [ ] Is a platform-specific name used for a generic concept?
- [ ] Is the proposed interface based on a real current requirement?
- [ ] Could a host-side fake backend test the generic logic?
- [ ] Are failure semantics documented?
- [ ] Is the abstraction smaller than the implementation it hides?

## Avoid false portability

Do not:

- implement empty ARM64 or RISC-V directories merely to claim portability;
- add callbacks for every imaginable architecture feature;
- use preprocessor conditionals throughout generic code;
- pretend x86 segmentation and ARM exception levels are identical;
- erase useful platform information from diagnostics;
- optimize for a second architecture before the first backend is correct.

## Future validation strategy

After M0–M11 are stable, choose one minimal validation target:

- ARM64 under QEMU with UEFI, or
- RISC-V 64 under QEMU with SBI/OpenSBI.

The validation target only needs to reach selected milestones initially:

1. build;
2. boot;
3. serial output;
4. exceptions;
5. physical memory discovery.

Any abstraction that makes the second backend harder without making the first backend clearer should be reconsidered.
