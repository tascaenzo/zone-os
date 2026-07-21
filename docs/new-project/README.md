# New OS Project — Documentation

This directory contains the initial design documents for a new educational operating-system project built from scratch after Zone OS.

The project has two equal goals:

1. build a small, understandable and progressively testable x86_64 operating system;
2. produce a complete Italian YouTube series that explains every important technical decision.

## Documents

- [Vision and goals](VISION.md)
- [Architecture](ARCHITECTURE.md)
- [Roadmap and milestones](ROADMAP.md)
- [Build system](BUILD_SYSTEM.md)
- [C23 language policy](C23_POLICY.md)
- [Development workflow](DEVELOPMENT_WORKFLOW.md)
- [Testing strategy](TESTING.md)
- [YouTube series plan](YOUTUBE_SERIES.md)
- [Architecture decision records](DECISIONS.md)

## Working principles

- Correctness before features.
- Observability before complexity.
- One testable result per milestone.
- One focused change per pull request.
- The repository must remain understandable from the first episode to the latest one.
- Every major decision must be documented.
- The build must be incremental, reproducible and easy to explain.

## Initial technical direction

- Architecture: x86_64 only.
- Boot protocol: Limine.
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

This documentation is intentionally stored in Zone OS temporarily. The final project should live in a separate repository once its name and repository are created.
