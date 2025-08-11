#include <limine.h>

__attribute__((used)) LIMINE_BASE_REVISION(2);
__attribute__((used)) LIMINE_REQUESTS_START_MARKER;

__attribute__((used)) volatile struct limine_memmap_request g_memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

__attribute__((used)) LIMINE_REQUESTS_END_MARKER;