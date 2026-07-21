# Testing Strategy

## Goals

Testing must make low-level development safer without pretending that host-side tests replace real hardware behavior.

The project uses several complementary test layers:

1. host-side unit tests;
2. kernel self-tests in QEMU;
3. boot and integration tests;
4. debug assertions and integrity checks;
5. manual debugger-driven validation for architecture-specific behavior.

## Host-side tests

Pure logic should be designed so it can be compiled and executed as a normal host program.

Good candidates include:

- bitmaps;
- intrusive lists;
- ring buffers;
- string and memory functions;
- formatting;
- ELF validation;
- TAR parsing;
- VFS path resolution;
- allocator metadata logic;
- scheduler queue operations.

Host tests must not silently rely on behavior unavailable in the freestanding kernel. Shared code should use small compatibility boundaries.

## Kernel self-tests

Kernel tests execute inside QEMU and validate code that depends on CPU state, page tables or interrupts.

Initial suites:

```text
boot
exceptions
pmm
vmm
heap
interrupts
scheduler
userspace
syscalls
elf
```

Tests should be selectable:

```bash
./tools/dev test pmm
./tools/dev test vmm
./tools/dev test all
```

## QEMU exit protocol

The test kernel should terminate QEMU through a deterministic debug-exit device or another documented mechanism. CI must receive a meaningful success or failure status instead of parsing only human-readable output.

Serial output remains available for diagnostics:

```text
[test] pmm.allocate_single_page ... PASS
[test] pmm.reject_double_free ... PASS
[test] pmm.reserve_kernel_range ... PASS
[summary] 3 passed, 0 failed
```

## Fault-injection tests

Expected CPU faults should be tested deliberately:

- divide by zero;
- invalid opcode;
- general-protection fault;
- non-present page fault;
- write-protection fault;
- user access to supervisor memory.

A test passes only when the correct vector, error code and diagnostic behavior are observed.

## Memory tests

Memory-manager tests must verify more than successful allocation.

Required cases include:

- exhausted allocator behavior;
- alignment;
- reserved-region protection;
- double-free detection;
- overlapping mapping rejection;
- mapping replacement policy;
- permission enforcement;
- TLB-visible updates;
- cleanup after partial failure.

## Debug instrumentation

Debug builds may enable:

- allocation poisoning;
- red zones;
- canaries;
- list integrity checks;
- page-table verification;
- lock ownership checks;
- verbose panic reports.

These checks must be isolated behind build options and must not alter the intended external API.

## Continuous integration

The initial CI pipeline should include:

```text
format or style checks
Meson configure
Clang debug build
Clang release build
host unit tests
QEMU boot smoke test
selected kernel tests
```

A GCC compatibility build can be added after the primary Clang toolchain is stable.

## Determinism

Automated QEMU tests should use a fixed machine configuration, fixed memory size and explicit CPU model where practical. Tests must avoid real-time assumptions unless they are specifically testing timekeeping.

## Test ownership

Every milestone defines its own completion tests. A pull request that fixes a bug should add a regression test whenever the failure can be reproduced deterministically.
