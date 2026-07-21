# Architecture Decision Records

This document lists the initial project decisions. As the project grows, each important decision may be moved into an individual file under `docs/adr/`.

## ADR-001 — Start a separate operating-system project

Status: Accepted

Decision:

The new project is not a continuation or rewrite-in-place of Zone OS. Zone OS remains available as a record of previous experience. The new system begins from an empty repository with newly documented constraints.

Reasoning:

- avoids inheriting accidental complexity;
- allows the build and teaching sequence to be designed from the beginning;
- preserves Zone OS as a historical and educational reference;
- prevents compatibility requirements with unfinished interfaces.

## ADR-002 — Target x86_64 only initially

Status: Accepted

Decision:

The initial kernel supports x86_64 only.

Reasoning:

Architecture abstraction is useful only after the real requirements of one implementation are understood. Premature multi-architecture interfaces can hide hardware behavior and complicate the educational path.

## ADR-003 — Use a modular monolithic kernel

Status: Accepted

Decision:

The first system uses a monolithic address space with internal module boundaries.

Reasoning:

This provides a direct path to memory management, scheduling and user space without introducing IPC and service isolation before the fundamentals are stable. A future microkernel experiment remains possible but is not an initial constraint.

## ADR-004 — Use Limine and begin with UEFI

Status: Accepted

Decision:

Limine provides the boot protocol and initial loading environment. The first supported firmware path is UEFI.

Reasoning:

- avoids writing a bootloader before writing the kernel;
- provides a modern and documented loading environment;
- reduces initial image-generation complexity;
- allows legacy BIOS support to be taught later as a separate topic.

## ADR-005 — Use ISO C23 in freestanding mode

Status: Accepted

Decision:

Kernel code uses `-std=c23` and `-ffreestanding`, with Clang as the primary compiler.

Reasoning:

C23 offers clearer attributes, static checks and modern language facilities while preserving a close relationship with the generated machine code. GNU extensions are isolated and used only when required.

## ADR-006 — Use Meson and Ninja

Status: Accepted

Decision:

Meson describes the build and Ninja executes it incrementally.

Reasoning:

The previous project relied on shell scripts, repeated source discovery and clean rebuilds. The new system requires explicit dependency tracking, fast incremental builds, multiple profiles and a workflow that remains understandable in videos.

## ADR-007 — Keep Docker optional during development

Status: Accepted

Decision:

Native tools are the preferred development path. A pinned container image provides reproducibility for CI and optional isolated development.

Reasoning:

Rebuilding or invoking an emulated x86_64 container for every source change creates unnecessary latency, especially on ARM hosts. Reproducibility remains important but should not make the local edit-build-run cycle inefficient.

## ADR-008 — Use serial output before framebuffer output

Status: Accepted

Decision:

The serial console is the first and primary diagnostic channel.

Reasoning:

Serial output is simple, deterministic, capturable by CI and available before graphics initialization. Framebuffer output is introduced later as a driver and presentation layer.

## ADR-009 — Build diagnostics before advanced memory management

Status: Accepted

Decision:

Panic handling, exception reporting, register dumps and debugger integration precede custom paging and dynamic allocation.

Reasoning:

Complex kernel subsystems are difficult to develop without trustworthy failure information. Diagnostic capability is treated as infrastructure, not polish.

## ADR-010 — Prefer simple allocators first

Status: Accepted

Decision:

The physical allocator begins as a bitmap. The kernel heap begins with an early bump allocator followed by a simple page-backed free-list allocator.

Reasoning:

Buddy and slab allocators are valuable but add metadata and invariants before the project has demonstrated a need for them. They may be introduced later with measurements and tests.

## ADR-011 — Make tests part of each milestone

Status: Accepted

Decision:

Every milestone defines observable completion criteria and automated tests where practical.

Reasoning:

The repository supports both long-term maintenance and a teaching series. Reproducible tests let viewers distinguish implementation errors from environment problems and prevent later episodes from silently breaking earlier work.

## ADR-012 — Align repository history with the video series

Status: Accepted

Decision:

Each episode has an issue, focused branch, notes, start tag and completion tag.

Reasoning:

A viewer must be able to reproduce the exact starting state, follow the implementation and compare the final result without interpreting unrelated later changes.

## ADR process

A new ADR is required when a decision:

- affects several subsystems;
- changes a public kernel interface or ABI;
- changes build or toolchain policy;
- changes the roadmap materially;
- introduces a difficult-to-reverse dependency;
- changes the educational sequence.

Each ADR should contain context, decision, consequences and rejected alternatives.
