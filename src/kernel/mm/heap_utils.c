#include <mm/heap_utils.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <klib/klog.h>

/*
 * ===========================================================================
 * HEAP UTILS - SUPPORTO BASICO PER L'HEAP DEL KERNEL
 * ===========================================================================
 *
 * Queste funzioni offrono meccanismi elementari per gestire lo spazio
 * virtuale dedicato all'heap del kernel. Non implementano un vero
 * allocatore: forniscono solo la riserva dell'area virtuale e la mappatura
 * di pagine fisiche tramite il PMM/VMM.
 */

/* Puntatore alla prossima area virtuale disponibile nell'heap */
static u64 next_heap_virtual = KERNEL_HEAP_BASE;

/**
 * @brief Riserva un range virtuale per l'heap
 *
 * Nessuna pagina fisica viene allocata: semplicemente si avanza il
 * puntatore next_heap_virtual all'interno della zona heap.
 */
void *heap_reserve_virtual_range(size_t size) {
    size = PAGE_ALIGN_UP(size);
    if (next_heap_virtual + size > KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE) {
        klog_error("heap_utils: Spazio heap virtuale esaurito");
        return NULL;
    }
    void *res = (void *)next_heap_virtual;
    next_heap_virtual += size;
    klog_debug("heap_utils: Riservato range virtuale 0x%lx - 0x%lx (%zu KB)",
               (u64)res, (u64)res + size, size / 1024);
    return res;
}

/**
 * @brief Placeholder per la futura liberazione di range virtuali
 */
void heap_release_virtual_range(void *base, size_t size) {
    (void)base;
    (void)size;
}

/**
 * @brief Mappa pagine fisiche in un range virtuale dell'heap
 */
bool heap_map_physical_pages(void *virt_base, size_t page_count) {
    if (!heap_address_valid(virt_base)) {
        return false;
    }
    u64 virt_addr = (u64)virt_base;
    void *phys_pages = pmm_alloc_pages(page_count);
    if (!phys_pages) {
        klog_error("heap_utils: PMM allocation failed for %zu pages", page_count);
        return false;
    }
    u64 flags = VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_GLOBAL;
    if (!vmm_map(vmm_kernel_space(), virt_addr, (u64)phys_pages, page_count, flags)) {
        klog_error("heap_utils: VMM mapping failed");
        pmm_free_pages(phys_pages, page_count);
        return false;
    }
    klog_debug("heap_utils: Mapped %zu pages: virt=0x%lx -> phys=0x%lx",
               page_count, virt_addr, (u64)phys_pages);
    return true;
}

/**
 * @brief Rimuove i mapping fisici da un range dell'heap
 */
void heap_unmap_physical_pages(void *virt_base, size_t page_count) {
    if (!heap_address_valid(virt_base)) {
        return;
    }
    vmm_unmap(vmm_kernel_space(), (u64)virt_base, page_count);
}

/**
 * @brief Controlla se un indirizzo rientra nella zona heap
 */
bool heap_address_valid(const void *addr) {
    u64 a = (u64)addr;
    return IS_KERNEL_HEAP(a);
}
