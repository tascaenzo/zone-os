# C23 Language Policy

## Standard

The kernel is written in ISO C23 and compiled in freestanding mode.

Primary compiler mode:

```text
-std=c23 -ffreestanding
```

GNU language modes such as `gnu23` are not the default. Compiler extensions may be used only when they solve a concrete low-level requirement and are hidden behind project macros or architecture-specific interfaces.

## Compiler policy

- Clang is the primary compiler.
- LLD is the primary linker.
- A secondary GCC build should be added to CI when the initial toolchain is stable.
- The minimum supported compiler version must be documented and pinned in CI.

## Useful C23 features

The project may use modern features where they improve clarity or static verification:

```c
static_assert(sizeof(struct interrupt_frame) == EXPECTED_SIZE);

[[noreturn]]
void panic(const char *message);

[[nodiscard]]
pmm_status_t pmm_alloc_page(paddr_t *out_page);

struct page *page = nullptr;
```

Fixed underlying types for enumerations may be used for hardware flags and ABI values when compiler support is verified.

## Freestanding environment

Selecting C23 does not provide a hosted C library. The project may use compiler-provided freestanding headers where supported, including:

- `<stddef.h>`
- `<stdint.h>`
- `<stdbool.h>`
- `<stdarg.h>`
- `<stdalign.h>`
- `<limits.h>`

The kernel provides the runtime functions it requires, such as:

- `memcpy`
- `memmove`
- `memset`
- `memcmp`
- `strlen`
- formatting and logging primitives

No kernel source may accidentally depend on the host libc.

## Type policy

Prefer explicit standard-width types:

```c
uint8_t
uint16_t
uint32_t
uint64_t
uintptr_t
size_t
```

Project-specific semantic aliases are encouraged where they prevent address-space confusion:

```c
typedef uintptr_t paddr_t;
typedef uintptr_t vaddr_t;
```

Avoid globally defined ambiguous aliases such as `u8`, `u32` and `ulong` in educational-facing APIs.

## Compiler abstraction

Compiler-specific syntax belongs in one small header, for example:

```c
#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define K_PACKED __attribute__((packed))
#define K_ALIGNED(value) __attribute__((aligned(value)))
#define K_SECTION(name) __attribute__((section(name)))
#else
#error "Unsupported compiler"
#endif
```

Architecture headers may use these wrappers but should not duplicate raw attributes throughout the source tree.

## Warning policy

Debug and CI builds should begin with a strict warning set:

```text
-Wall
-Wextra
-Wpedantic
-Werror
-Wconversion
-Wshadow
-Wundef
-Wmissing-prototypes
-Wstrict-prototypes
```

Warnings that are unsuitable for a specific low-level file should be disabled narrowly, with a comment explaining why. Global suppression is discouraged.

## Style principles

- Functions and variables use `snake_case`.
- Types use descriptive names ending in `_t` only for project typedefs where appropriate.
- Constants and macros use `UPPER_SNAKE_CASE`.
- Public headers expose minimal APIs.
- Functions return typed status values for recoverable failures.
- Output parameters are validated.
- Pointer arithmetic is isolated and commented.
- Integer casts must make truncation or reinterpretation explicit.

## Assembly boundary

Assembly is kept in separate `.S` files when possible. Inline assembly is reserved for small CPU primitives where the constraints are reviewed carefully.

Every assembly interface must document:

- inputs and outputs;
- clobbered registers;
- stack layout;
- calling convention;
- alignment assumptions.

## Feature adoption rule

The project uses C23 to improve correctness and readability, not to maximize novelty. A language feature is adopted only when:

1. it is supported by the pinned primary compiler;
2. it has a clear benefit;
3. it can be explained to the audience;
4. it does not obscure the generated machine-level behavior relevant to the lesson.
