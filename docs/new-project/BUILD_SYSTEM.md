# Build System

## Objectives

The build must be fast enough for daily development, simple enough to explain in a video and reproducible enough for CI.

The build system must:

- compile incrementally;
- track header dependencies correctly;
- support parallel compilation;
- separate source and generated files;
- provide debug, release and test configurations;
- keep image generation separate from kernel compilation;
- avoid requiring Docker for every edit;
- pin external tool versions where practical.

## Selected tools

- Meson: project configuration and dependency graph.
- Ninja: incremental build execution.
- Clang: primary C23 compiler.
- LLD: primary linker.
- Python: small portable orchestration tools.
- QEMU: emulation and integration testing.
- GDB: source-level debugging.
- Limine: boot protocol and bootloader.

## Build stages

```text
C and assembly sources
        |
        v
Object files
        |
        v
kernel.elf
        |
        +----> debug symbols and map file
        |
        v
staged boot tree
        |
        v
UEFI image
        |
        v
QEMU
```

Each stage must be represented by explicit inputs and outputs. A stage runs only when its inputs change.

## Expected commands

The public interface should remain small:

```bash
./tools/dev setup
./tools/dev build
./tools/dev run
./tools/dev debug
./tools/dev test
./tools/dev clean
```

`tools/dev` is only a thin command dispatcher. It must not reimplement compiler dependency logic.

Equivalent lower-level commands remain documented:

```bash
meson setup build/debug --cross-file config/x86_64.ini --buildtype=debug
meson compile -C build/debug
```

## Build profiles

### Debug

- debug information;
- frame pointers;
- assertions;
- detailed logging;
- allocator integrity checks;
- minimal optimization.

### Release

- optimized kernel;
- reduced logging;
- separate or preserved symbols for postmortem debugging;
- no correctness dependency on disabled assertions.

### Test

- test registry enabled;
- deterministic QEMU configuration;
- machine-readable exit code;
- optional fault-injection targets.

## Image-generation policy

Kernel compilation and boot-image construction are separate targets. Recompiling a source file must not repartition or reformat an image unless the kernel ELF actually changed.

The first release targets UEFI only. Legacy BIOS support is a later, isolated feature.

The staged boot tree should look like:

```text
build/debug/sysroot/
├── boot/
│   ├── kernel.elf
│   └── limine.conf
└── EFI/
    └── BOOT/
        └── BOOTX64.EFI
```

## Dependency management

External bootloader artifacts must be pinned to a known version. Downloads should be verified with a checksum. The repository should not depend on mutable files installed at paths such as `/opt/limine`.

Host dependencies are checked by `tools/dev setup`. That command reports missing tools and does not silently install system packages.

## Container policy

Local development uses the native toolchain for speed. A container image provides a reproducible reference environment for CI and users who prefer isolation.

The container must not be rebuilt for each source change. Source directories may be mounted into a previously built development image.

## Performance expectations

After the initial configuration:

- no-op build should complete almost immediately;
- editing one C file should compile one object and relink;
- documentation changes should not rebuild the kernel;
- QEMU launch should not trigger a clean build;
- test selection should avoid running unrelated suites.

## Build observability

The build should be inspectable with standard Meson and Ninja tooling. Custom scripts must print the exact failed command and return its exit status unchanged.
