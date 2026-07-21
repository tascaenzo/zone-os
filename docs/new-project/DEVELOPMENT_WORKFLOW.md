# Development Workflow

## Repository strategy

The new operating system should ultimately live in a separate repository. Until that repository is created, these documents are staged in a dedicated Zone OS branch.

The main development branch remains releasable and each feature is introduced through a focused pull request.

## Branch naming

Recommended prefixes:

```text
build/
boot/
arch/
mm/
sched/
user/
fs/
test/
docs/
episode/
```

Examples:

```text
episode/03-incremental-build
mm/bitmap-page-allocation
arch/x86_64-page-fault-handler
```

## Commit policy

Commits should describe one logical change and use an imperative, subsystem-oriented subject:

```text
build: add x86_64 Meson cross file
boot: validate Limine memory-map response
serial: add COM1 polling output
mm: reserve bootloader-owned pages
test: add PMM double-free case
docs: explain high-half address layout
```

Avoid commits that mix formatting, refactoring, new behavior and documentation without a strong reason.

## Pull-request completion checklist

Every implementation PR should answer:

- What observable behavior changes?
- Which invariant or interface is introduced?
- How was it tested?
- What failure cases were tested?
- Does the design documentation need updating?
- Does this change belong to a YouTube episode?

Recommended checklist:

```markdown
- [ ] Debug build passes
- [ ] Release build passes
- [ ] Host-side tests pass
- [ ] QEMU tests pass
- [ ] Documentation updated
- [ ] No unrelated generated files committed
- [ ] Episode notes updated when applicable
```

## Episode workflow

Each episode uses a reproducible start and completion point.

```text
Issue:   Episode 03 — Incremental kernel build
Branch:  episode/03-incremental-build
Tag:     ep03-start
Tag:     ep03-complete
Notes:   docs/episodes/03-incremental-build.md
```

Viewers should be able to run:

```bash
git checkout ep03-start
git diff ep03-start ep03-complete
```

The video may show mistakes and debugging, while the final commit history should remain understandable.

## Definition of done

A feature is complete only when:

1. the code compiles without new warnings;
2. its expected behavior is observable;
3. relevant tests pass;
4. failure behavior is documented;
5. public APIs are documented;
6. the corresponding design document is updated;
7. temporary debug code is removed or intentionally gated.

## Review principles

Reviews should focus on:

- initialization order;
- ownership and lifetime;
- integer overflow;
- physical versus virtual address confusion;
- interrupt safety;
- reentrancy;
- lock ordering;
- user-pointer validation;
- behavior during partial initialization;
- testability and diagnostic quality.

## Generated files

Build artifacts, disk images, dependency caches and downloaded toolchains are not committed unless explicitly required for a release artifact.

The repository should contain checksums and scripts that recreate external dependencies rather than mutable binary copies without provenance.

## Documentation maintenance

Documentation is treated as part of the implementation. Architectural changes update the relevant document in the same PR.

Small local decisions may be documented in code. Decisions that affect multiple modules, the public ABI, the build system or the teaching sequence require an Architecture Decision Record.
