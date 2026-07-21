# New OS Project — Documentation

This directory contains the initial design documents for a new educational operating-system project built from scratch after Zone OS.

The project has two equal goals:

1. build a small, understandable and progressively testable 64-bit operating system;
2. produce a complete Italian YouTube series that explains every important technical decision.

The first implementation target is x86_64, but architecture-specific mechanisms must remain behind small, explicit interfaces. Portability is treated as a design discipline rather than an immediate promise of multiple complete backends.

## Documents

### Direction and architecture

- [Vision and goals](VISION.md)
- [Architecture](ARCHITECTURE.md)
- [Portability and abstraction strategy](PORTABILITY_AND_ABSTRACTION.md)
- [Architecture decision records](DECISIONS.md)

### Planning and learning

- [Roadmap and milestones](ROADMAP.md)
- [Detailed milestone goals and checklists](MILESTONES_DETAILED.md)
- [OS development study plan](STUDY_PLAN.md)
- [YouTube series plan](YOUTUBE_SERIES.md)

### Engineering process

- [Build system](BUILD_SYSTEM.md)
- [C23 language policy](C23_POLICY.md)
- [Development workflow](DEVELOPMENT_WORKFLOW.md)
- [Testing strategy](TESTING.md)

## Working principles

- Correctness before features.
- Observability before complexity.
- One testable result per milestone.
- One focused change per pull request.
- The repository must remain understandable from the first episode to the latest one.
- Every major decision must be documented.
- The build must be incremental, reproducible and easy to explain.
- Generic code must not manipulate x86_64 registers or hardware formats directly.
- Architecture abstractions must be driven by real requirements, not speculative portability.
- Every milestone includes a study track, success tests and deliberate failure tests.

## Initial technical direction

- Architecture: x86_64, 64-bit mode only.
- Architecture strategy: generic kernel contracts with an initial x86_64 backend.
- Boot protocol: Limine adapter feeding kernel-owned boot information.
- Initial firmware target: UEFI.
- Kernel design: monolithic but modular.
- Language: ISO C23, freestanding.
- Compiler: Clang as primary compiler.
- Linker: LLD.
- Build system: Meson and Ninja.
- Emulator: QEMU.
- Debugger: GDB.
- Primary diagnostic output: serial console.
- Container usage: optional for development, required for reproducible CI images.

## Initial milestone sequence

1. Reproducible 64-bit workspace.
2. Controlled UEFI boot.
3. Diagnostics and CPU exceptions.
4. Physical memory ownership.
5. Virtual address spaces.
6. Kernel dynamic memory.
7. Interrupt delivery and time.
8. Kernel threads and scheduling.
9. User execution domain.
10. System-call boundary.
11. Initramfs and ELF64 program loading.
12. Minimal VFS.

Each milestone has detailed deliverables, verification checklists and exit criteria in [MILESTONES_DETAILED.md](MILESTONES_DETAILED.md). The associated knowledge path is defined in [STUDY_PLAN.md](STUDY_PLAN.md).

This documentation is intentionally stored in Zone OS temporarily. The final project should live in a separate repository once its name and repository are created.
