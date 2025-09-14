#pragma once
#include <arch/x86_64/memory/paging_defs.h>
#include <lib/types.h>

/* Geometria pagina (kernel-common) */
#define PAGE_SHIFT (ARCH_PAGE_SHIFT)
#define PAGE_SIZE (1ull << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SIZE - 1ull)

/* Utility allineamento/indirizzamento */
#define PAGE_ALIGN_DOWN(addr) ((u64)(addr) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(addr) (((u64)(addr) + PAGE_MASK) & ~PAGE_MASK)
#define IS_PAGE_ALIGNED(addr) ((((u64)(addr)) & PAGE_MASK) == 0)

#define ADDR_TO_PAGE(addr) ((u64)(addr) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(page) ((u64)(page) << PAGE_SHIFT)

typedef u64 pfn_t;

/* Taglie standard */
#define KB (1024ull)
#define MB (1024ull * KB)
#define GB (1024ull * MB)
