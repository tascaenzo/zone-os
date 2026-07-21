# YouTube Series Plan

## Series purpose

The series documents the construction of a modern educational x86_64 operating system from an empty repository to the execution of user-space programs.

The audience should learn both operating-system concepts and the engineering process used to build, debug and test low-level software.

## Episode structure

Each episode should contain:

1. the problem being solved;
2. the theory required to understand it;
3. the design chosen for the project;
4. implementation in small steps;
5. at least one failure or diagnostic path;
6. an observable result;
7. a test;
8. a start and completion tag.

Recommended duration is flexible. Topics should not be stretched or compressed merely to match a fixed video length.

## Season 0 — Foundations and build system

### Episode 0 — Why build another operating system?

- lessons learned from Zone OS;
- scope and non-goals;
- roadmap;
- educational repository strategy.

Result: project vision and initial repository.

### Episode 1 — Hosted and freestanding C

- what an operating system kernel is;
- hosted versus freestanding execution;
- why C23;
- what the compiler does and does not provide.

Result: first freestanding object file.

### Episode 2 — Cross-compilation with Clang

- target triples;
- ABI assumptions;
- compiler flags;
- red zone and code model;
- inspecting generated object files.

Result: verified x86_64 kernel objects.

### Episode 3 — Meson and Ninja

- build graphs;
- incremental compilation;
- header dependencies;
- debug and release profiles;
- why Zone OS rebuilt too much.

Result: fast incremental kernel build.

### Episode 4 — Linking a kernel ELF

- sections;
- symbols;
- linker script;
- virtual and physical addresses;
- map files and ELF inspection.

Result: valid kernel ELF.

### Episode 5 — Limine and UEFI image

- firmware and bootloaders;
- Limine protocol;
- staging tree;
- reproducible UEFI image.

Result: QEMU transfers control to the kernel.

### Episode 6 — QEMU, serial and GDB

- serial output;
- QEMU options;
- GDB remote debugging;
- breakpoints and register inspection.

Result: debuggable controlled boot.

## Season 1 — CPU foundations

Topics:

- kernel entry and boot response validation;
- logging and panic;
- x86_64 execution environment;
- GDT;
- TSS;
- IDT;
- CPU exception stubs;
- interrupt frames;
- page-fault diagnostics;
- stack tracing.

Season result: deliberate CPU faults produce reliable diagnostic reports.

## Season 2 — Memory management

Topics:

- physical memory map;
- page terminology;
- reserved regions;
- bitmap PMM;
- x86_64 page tables;
- high-half kernel;
- direct map;
- mapping APIs;
- NX and permissions;
- early allocator;
- kernel heap.

Season result: tested physical allocation, virtual mappings and dynamic kernel memory.

## Season 3 — Interrupts and scheduling

Topics:

- legacy PIC and APIC concepts;
- local APIC;
- IOAPIC;
- timers;
- interrupt-safe code;
- thread representation;
- context switching;
- ready queues;
- idle thread;
- preemption;
- synchronization basics.

Season result: multiple kernel threads run preemptively.

## Season 4 — User space

Topics:

- privilege rings;
- user address spaces;
- user stacks;
- transitions to Ring 3;
- safe user-memory access;
- syscall ABI;
- `write`, `exit` and `yield`;
- process lifecycle.

Season result: a user program prints text through a syscall and exits.

## Season 5 — Programs and files

Topics:

- initramfs;
- TAR format;
- ELF64 program loading;
- virtual filesystem concepts;
- vnode and file descriptors;
- console device;
- initial process;
- simple command execution.

Season result: multiple user programs are loaded from the initramfs.

## Supporting material

Each episode should publish:

- episode notes;
- diagrams where useful;
- commands shown in the video;
- references;
- start and completion tags;
- test instructions;
- known limitations;
- optional exercises.

Suggested note template:

```markdown
# Episode NN — Title

## Learning objectives
## Starting point
## Concepts
## Implementation steps
## Commands
## Tests
## Common errors
## Exercises
## Final state
```

## Teaching principles

- Never hide a required compiler or linker flag without explaining it.
- Distinguish hardware rules, ABI rules, compiler behavior and project conventions.
- Explain undefined behavior when it affects kernel code.
- Show how to inspect binaries with tools rather than treating the build as magic.
- Prefer diagrams for stack layouts, page tables and transitions.
- Keep old episode tags buildable whenever practical.
- Correct mistakes publicly in documentation and follow-up notes.

## Community workflow

Questions and corrections should be directed to GitHub Discussions or episode-specific issues once the new repository supports them. Bugs should include the episode tag, host environment, command used and serial output.
