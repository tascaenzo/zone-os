# Roadmap delle API per milestone

## Scopo

Questo documento raccoglie le API previste per ogni milestone. Le firme sono contratti di progetto iniziali: guidano lo studio, la progettazione e i test, ma possono evolvere tramite ADR quando l'implementazione dimostra requisiti diversi.

Ogni milestone deve produrre:

- header pubblici minimi;
- implementazioni private;
- documentazione di ownership e lifetime;
- codici di errore stabili;
- test delle API;
- almeno un caso di fallimento deliberato;
- separazione tra contratto generico e backend x86_64;
- nessuna esposizione accidentale di strutture private.

## Convenzioni comuni

### Tipi opachi

```c
struct address_space;
struct thread;
struct process;
struct ipc_endpoint;
struct file;
struct vnode;
```

La definizione completa resta nel modulo proprietario.

### Handle

```c
typedef struct {
    uint64_t value;
} object_handle_t;
```

Gli handle sono preferiti nelle API destinate a poter attraversare un futuro confine IPC.

### Indirizzi tipizzati

```c
typedef struct {
    uintptr_t value;
} paddr_t;

typedef struct {
    uintptr_t value;
} vaddr_t;
```

Le conversioni tra indirizzi fisici, virtuali e puntatori devono essere esplicite.

### Risultati

Le operazioni recuperabili restituiscono enum tipizzati. Gli output vengono restituiti tramite parametri `out` validati.

```c
enum kernel_status {
    KERNEL_OK,
    KERNEL_INVALID_ARGUMENT,
    KERNEL_OUT_OF_MEMORY,
    KERNEL_NOT_FOUND,
    KERNEL_PERMISSION_DENIED,
    KERNEL_BUSY,
    KERNEL_UNSUPPORTED,
    KERNEL_IO_ERROR,
};
```

Non tutte le API devono usare un unico enum globale. I sottosistemi possono definire errori più precisi, purché siano convertibili in uno stato comune ai confini pubblici.

---

# M0 — Workspace e build

Questa milestone non introduce ancora API runtime del kernel. Introduce però l'interfaccia degli strumenti di sviluppo.

## Comandi pubblici

```text
./tools/dev setup
./tools/dev configure [debug|release|test]
./tools/dev build [target]
./tools/dev run [--no-build]
./tools/dev debug
./tools/dev test [suite]
./tools/dev inspect
./tools/dev clean
```

## Contratto dello strumento

```text
setup      -> valida le dipendenze senza installarle silenziosamente
configure  -> crea una directory Meson riproducibile
build      -> esegue una build incrementale
run        -> avvia QEMU con configurazione deterministica
debug      -> avvia QEMU e collega GDB
test       -> esegue test host o kernel selezionati
inspect    -> mostra proprietà ELF e dipendenze
clean      -> rimuove solo artefatti generati
```

## API di configurazione build

Opzioni Meson candidate:

```text
-Darchitecture=x86_64
-Dfirmware=uefi
-Dboot_protocol=limine
-Dkernel_tests=true|false
-Ddiagnostics=basic|full
-Dtoolchain=clang|zig
```

---

# M1 — Boot controllato

## Header pubblici candidati

```text
kernel/include/boot/info.h
kernel/include/core/halt.h
kernel/include/drivers/byte_sink.h
```

## Informazioni di boot normalizzate

```c
enum boot_memory_type {
    BOOT_MEMORY_USABLE,
    BOOT_MEMORY_RESERVED,
    BOOT_MEMORY_RECLAIMABLE,
    BOOT_MEMORY_KERNEL,
    BOOT_MEMORY_MODULE,
    BOOT_MEMORY_FRAMEBUFFER,
    BOOT_MEMORY_MMIO,
};

struct boot_memory_region {
    paddr_t base;
    uint64_t length;
    enum boot_memory_type type;
};

struct boot_module {
    paddr_t physical_base;
    uint64_t size;
    const char *name;
};

struct boot_info;
```

## API generiche

```c
[[nodiscard]]
bool boot_info_capture(struct boot_info *out_info);

[[nodiscard]]
size_t boot_info_memory_region_count(const struct boot_info *info);

[[nodiscard]]
bool boot_info_memory_region_at(
    const struct boot_info *info,
    size_t index,
    struct boot_memory_region *out_region
);

[[nodiscard]]
size_t boot_info_module_count(const struct boot_info *info);

[[nodiscard]]
bool boot_info_module_at(
    const struct boot_info *info,
    size_t index,
    struct boot_module *out_module
);

[[noreturn]]
void platform_halt(void);
```

## Output diagnostico iniziale

```c
struct byte_sink;

enum byte_sink_status {
    BYTE_SINK_OK,
    BYTE_SINK_UNAVAILABLE,
    BYTE_SINK_IO_ERROR,
};

[[nodiscard]]
enum byte_sink_status byte_sink_write(
    struct byte_sink *sink,
    const void *data,
    size_t size
);
```

## Backend x86_64

```c
[[nodiscard]] bool x86_64_limine_capture(struct boot_info *out_info);
[[nodiscard]] bool x86_64_serial_init(struct byte_sink **out_sink);
[[noreturn]] void x86_64_halt(void);
```

Il codice generico non include `limine.h`.

---

# M2 — Logging, panic ed eccezioni

## Logging

```c
enum log_level {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL,
};

struct log_record {
    enum log_level level;
    const char *component;
    const char *message;
};

void log_set_sink(struct byte_sink *sink);

void log_write(
    enum log_level level,
    const char *component,
    const char *format,
    ...
);
```

## Panic

```c
struct source_location {
    const char *file;
    const char *function;
    uint32_t line;
};

[[noreturn]]
void panic_at(
    struct source_location location,
    const char *format,
    ...
);

#define PANIC(...) \
    panic_at(       \
        (struct source_location){__FILE__, __func__, __LINE__}, \
        __VA_ARGS__ \
    )
```

## Eccezioni

```c
enum exception_class {
    EXCEPTION_ARITHMETIC,
    EXCEPTION_INVALID_INSTRUCTION,
    EXCEPTION_MEMORY_ACCESS,
    EXCEPTION_BREAKPOINT,
    EXCEPTION_PROTECTION,
    EXCEPTION_HARDWARE_FAILURE,
    EXCEPTION_UNKNOWN,
};

struct execution_view {
    vaddr_t instruction_pointer;
    vaddr_t stack_pointer;
    uintptr_t flags;
    bool from_user;
};

struct exception_info {
    enum exception_class class;
    uint32_t architecture_code;
    uint64_t architecture_error;
    vaddr_t fault_address;
};

void exception_dispatch(
    const struct exception_info *info,
    const struct execution_view *view,
    const void *architecture_context
);
```

## Backend x86_64

```c
void x86_64_gdt_init(void);
void x86_64_tss_init(void);
void x86_64_idt_init(void);
void x86_64_exceptions_enable(void);

[[nodiscard]]
bool x86_64_context_make_view(
    const struct x86_64_interrupt_context *context,
    struct execution_view *out_view
);
```

---

# M3 — Gestione della memoria fisica

## Tipi

```c
typedef struct {
    uint64_t value;
} page_frame_t;

enum pmm_status {
    PMM_OK,
    PMM_INVALID_ARGUMENT,
    PMM_OUT_OF_MEMORY,
    PMM_NOT_OWNED,
    PMM_ALREADY_RESERVED,
    PMM_DOUBLE_FREE,
};

struct pmm_statistics {
    uint64_t total_pages;
    uint64_t usable_pages;
    uint64_t free_pages;
    uint64_t reserved_pages;
};
```

## API

```c
[[nodiscard]]
enum pmm_status pmm_init(const struct boot_info *boot_info);

[[nodiscard]]
enum pmm_status pmm_reserve_range(
    paddr_t base,
    uint64_t length
);

[[nodiscard]]
enum pmm_status pmm_release_range(
    paddr_t base,
    uint64_t length
);

[[nodiscard]]
enum pmm_status pmm_alloc_page(paddr_t *out_page);

[[nodiscard]]
enum pmm_status pmm_alloc_pages(
    size_t count,
    size_t alignment_pages,
    paddr_t *out_base
);

[[nodiscard]]
enum pmm_status pmm_free_page(paddr_t page);

[[nodiscard]]
enum pmm_status pmm_free_pages(
    paddr_t base,
    size_t count
);

void pmm_get_statistics(struct pmm_statistics *out_stats);

[[nodiscard]]
bool pmm_validate_integrity(void);
```

---

# M4 — Gestione della memoria virtuale

## Tipi

```c
struct address_space;

enum vm_permission {
    VM_READ    = 1u << 0,
    VM_WRITE   = 1u << 1,
    VM_EXECUTE = 1u << 2,
    VM_USER    = 1u << 3,
    VM_GLOBAL  = 1u << 4,
};

enum vm_status {
    VM_OK,
    VM_INVALID_ARGUMENT,
    VM_INVALID_ADDRESS,
    VM_ALREADY_MAPPED,
    VM_NOT_MAPPED,
    VM_OUT_OF_MEMORY,
    VM_PERMISSION_ERROR,
};

struct vm_mapping {
    vaddr_t virtual_address;
    paddr_t physical_address;
    size_t page_count;
    uint32_t permissions;
};
```

## API generiche

```c
[[nodiscard]]
enum vm_status address_space_create(
    struct address_space **out_space
);

void address_space_retain(struct address_space *space);
void address_space_release(struct address_space *space);

[[nodiscard]]
enum vm_status address_space_activate(
    struct address_space *space
);

[[nodiscard]]
enum vm_status vm_map(
    struct address_space *space,
    const struct vm_mapping *mapping
);

[[nodiscard]]
enum vm_status vm_unmap(
    struct address_space *space,
    vaddr_t virtual_address,
    size_t page_count
);

[[nodiscard]]
enum vm_status vm_query(
    const struct address_space *space,
    vaddr_t virtual_address,
    struct vm_mapping *out_mapping
);

[[nodiscard]]
enum vm_status vm_protect(
    struct address_space *space,
    vaddr_t virtual_address,
    size_t page_count,
    uint32_t permissions
);
```

## Backend architetturale

```c
struct arch_address_space;

[[nodiscard]]
enum vm_status arch_vm_space_create(
    struct arch_address_space **out_space
);

void arch_vm_space_destroy(struct arch_address_space *space);

[[nodiscard]]
enum vm_status arch_vm_activate(
    struct arch_address_space *space
);

void arch_vm_invalidate_page(vaddr_t address);
void arch_vm_invalidate_space(struct arch_address_space *space);
```

Il backend x86_64 traduce `vm_permission` nei bit delle page table.

---

# M5 — Heap del kernel

## Tipi

```c
enum heap_status {
    HEAP_OK,
    HEAP_INVALID_ARGUMENT,
    HEAP_OUT_OF_MEMORY,
    HEAP_CORRUPTED,
    HEAP_INVALID_POINTER,
    HEAP_DOUBLE_FREE,
};

struct heap_statistics {
    size_t committed_bytes;
    size_t allocated_bytes;
    size_t free_bytes;
    size_t allocation_count;
};
```

## API

```c
[[nodiscard]]
enum heap_status heap_init(void);

[[nodiscard]]
void *kmalloc(size_t size);

[[nodiscard]]
void *kcalloc(size_t count, size_t size);

[[nodiscard]]
void *krealloc(void *pointer, size_t new_size);

[[nodiscard]]
void *kmalloc_aligned(size_t size, size_t alignment);

void kfree(void *pointer);

void heap_get_statistics(struct heap_statistics *out_stats);

[[nodiscard]]
bool heap_validate_integrity(void);
```

Per le API interne critiche può essere preferibile una variante esplicita:

```c
[[nodiscard]]
enum heap_status heap_allocate(
    size_t size,
    size_t alignment,
    void **out_pointer
);
```

---

# M6 — Interrupt e tempo

## Interrupt

```c
typedef struct {
    uintptr_t value;
} interrupt_source_t;

typedef void (*interrupt_handler_fn)(
    interrupt_source_t source,
    void *context
);

enum interrupt_status {
    INTERRUPT_OK,
    INTERRUPT_INVALID_SOURCE,
    INTERRUPT_ALREADY_REGISTERED,
    INTERRUPT_NOT_REGISTERED,
    INTERRUPT_UNSUPPORTED,
};

[[nodiscard]]
enum interrupt_status interrupt_register(
    interrupt_source_t source,
    interrupt_handler_fn handler,
    void *context
);

[[nodiscard]]
enum interrupt_status interrupt_unregister(
    interrupt_source_t source
);

[[nodiscard]]
enum interrupt_status interrupt_mask(interrupt_source_t source);
[[nodiscard]]
enum interrupt_status interrupt_unmask(interrupt_source_t source);
void interrupt_acknowledge(interrupt_source_t source);
```

## Tempo

```c
typedef struct {
    uint64_t nanoseconds;
} duration_t;

typedef struct {
    uint64_t nanoseconds;
} monotonic_time_t;

[[nodiscard]]
monotonic_time_t clock_monotonic_now(void);

[[nodiscard]]
bool timer_set_deadline(monotonic_time_t deadline);

void timer_cancel_deadline(void);
```

## Backend

```c
struct interrupt_controller_ops {
    enum interrupt_status (*mask)(interrupt_source_t source);
    enum interrupt_status (*unmask)(interrupt_source_t source);
    void (*acknowledge)(interrupt_source_t source);
};

struct clock_source_ops {
    uint64_t (*read_ticks)(void);
    uint64_t frequency_hz;
};
```

---

# M7 — Thread e scheduler

## Tipi

```c
struct thread;

typedef uint64_t thread_id_t;

typedef void (*thread_entry_fn)(void *argument);

enum thread_state {
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED,
};

enum scheduler_status {
    SCHEDULER_OK,
    SCHEDULER_INVALID_ARGUMENT,
    SCHEDULER_OUT_OF_MEMORY,
    SCHEDULER_INVALID_STATE,
    SCHEDULER_NOT_FOUND,
};
```

## API

```c
[[nodiscard]]
enum scheduler_status scheduler_init(void);

[[nodiscard]]
enum scheduler_status thread_create_kernel(
    thread_entry_fn entry,
    void *argument,
    struct thread **out_thread
);

thread_id_t thread_id(const struct thread *thread);
enum thread_state thread_state_get(const struct thread *thread);

void thread_retain(struct thread *thread);
void thread_release(struct thread *thread);

void scheduler_start(void);
void scheduler_yield(void);

[[nodiscard]]
enum scheduler_status thread_block(void *wait_object);

[[nodiscard]]
enum scheduler_status thread_wake(struct thread *thread);

[[noreturn]]
void thread_exit(int exit_code);
```

## Backend del context switch

```c
struct arch_thread_context;

[[nodiscard]]
bool arch_thread_context_create(
    struct arch_thread_context *out_context,
    vaddr_t stack_top,
    thread_entry_fn entry,
    void *argument
);

void arch_context_switch(
    struct arch_thread_context *from,
    const struct arch_thread_context *to
);
```

---

# M8 — Processi e user mode

## Tipi

```c
struct process;

typedef uint64_t process_id_t;

enum process_state {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED,
};

enum process_status {
    PROCESS_OK,
    PROCESS_INVALID_ARGUMENT,
    PROCESS_OUT_OF_MEMORY,
    PROCESS_INVALID_ADDRESS,
    PROCESS_PERMISSION_DENIED,
    PROCESS_INVALID_STATE,
};
```

## API

```c
[[nodiscard]]
enum process_status process_create(
    struct process **out_process
);

process_id_t process_id(const struct process *process);

void process_retain(struct process *process);
void process_release(struct process *process);

[[nodiscard]]
enum process_status process_set_image(
    struct process *process,
    struct address_space *address_space,
    vaddr_t entry_point,
    vaddr_t stack_pointer
);

[[nodiscard]]
enum process_status process_start(struct process *process);

[[nodiscard]]
enum process_status process_terminate(
    struct process *process,
    int exit_code
);

[[nodiscard]]
enum process_status copy_from_user(
    void *kernel_destination,
    vaddr_t user_source,
    size_t size
);

[[nodiscard]]
enum process_status copy_to_user(
    vaddr_t user_destination,
    const void *kernel_source,
    size_t size
);
```

## Backend x86_64

```c
[[nodiscard]]
bool x86_64_user_context_create(
    struct arch_thread_context *out_context,
    vaddr_t entry_point,
    vaddr_t user_stack,
    vaddr_t kernel_stack
);

[[noreturn]]
void x86_64_enter_user(
    const struct arch_thread_context *context
);
```

---

# M9 — IPC, handle e syscall

Questa milestone viene introdotta prima dei servizi filesystem persistenti per sostenere l'evoluzione ibrida.

## Handle table

```c
enum handle_right {
    HANDLE_RIGHT_READ     = 1u << 0,
    HANDLE_RIGHT_WRITE    = 1u << 1,
    HANDLE_RIGHT_TRANSFER = 1u << 2,
    HANDLE_RIGHT_MAP      = 1u << 3,
    HANDLE_RIGHT_SIGNAL   = 1u << 4,
};

[[nodiscard]]
enum kernel_status handle_create(
    struct process *owner,
    void *object,
    uint32_t rights,
    object_handle_t *out_handle
);

[[nodiscard]]
enum kernel_status handle_close(
    struct process *owner,
    object_handle_t handle
);

[[nodiscard]]
enum kernel_status handle_duplicate(
    struct process *owner,
    object_handle_t source,
    uint32_t reduced_rights,
    object_handle_t *out_handle
);
```

## Endpoint e messaggi

```c
#define IPC_INLINE_DATA_MAX 128u
#define IPC_HANDLE_MAX 8u

struct ipc_message {
    uint32_t protocol;
    uint32_t operation;
    uint64_t request_id;
    uint32_t flags;
    uint32_t data_size;
    uint8_t data[IPC_INLINE_DATA_MAX];
    object_handle_t handles[IPC_HANDLE_MAX];
    uint32_t handle_count;
};

enum ipc_status {
    IPC_OK,
    IPC_INVALID_ARGUMENT,
    IPC_WOULD_BLOCK,
    IPC_PEER_CLOSED,
    IPC_MESSAGE_TOO_LARGE,
    IPC_PERMISSION_DENIED,
    IPC_INTERRUPTED,
};

[[nodiscard]]
enum ipc_status ipc_endpoint_create(
    object_handle_t *out_endpoint_a,
    object_handle_t *out_endpoint_b
);

[[nodiscard]]
enum ipc_status ipc_send(
    object_handle_t endpoint,
    const struct ipc_message *message
);

[[nodiscard]]
enum ipc_status ipc_receive(
    object_handle_t endpoint,
    struct ipc_message *out_message
);

[[nodiscard]]
enum ipc_status ipc_call(
    object_handle_t endpoint,
    const struct ipc_message *request,
    struct ipc_message *out_reply
);

[[nodiscard]]
enum ipc_status ipc_reply(
    object_handle_t endpoint,
    const struct ipc_message *reply
);
```

## Memoria condivisa

```c
[[nodiscard]]
enum kernel_status shared_memory_create(
    size_t size,
    object_handle_t *out_handle
);

[[nodiscard]]
enum kernel_status shared_memory_map(
    object_handle_t handle,
    struct address_space *space,
    vaddr_t preferred_address,
    uint32_t permissions,
    vaddr_t *out_address
);
```

## Syscall ABI iniziale

```c
enum syscall_number : uint32_t {
    SYS_WRITE,
    SYS_EXIT,
    SYS_YIELD,
    SYS_HANDLE_CLOSE,
    SYS_IPC_SEND,
    SYS_IPC_RECEIVE,
    SYS_IPC_CALL,
};
```

L'entry assembly è specifica dell'architettura; la semantica del dispatcher è generica.

```c
struct syscall_request;
struct syscall_result;

void syscall_dispatch(
    const struct syscall_request *request,
    struct syscall_result *out_result
);
```

---

# M10 — Initramfs e caricatore ELF64

## Sorgenti di byte

```c
struct byte_source {
    const void *context;
    enum kernel_status (*read)(
        const void *context,
        uint64_t offset,
        void *destination,
        size_t size
    );
    uint64_t size;
};
```

## Initramfs

```c
struct archive;
struct archive_entry;

enum archive_status {
    ARCHIVE_OK,
    ARCHIVE_INVALID_FORMAT,
    ARCHIVE_NOT_FOUND,
    ARCHIVE_OUT_OF_BOUNDS,
};

[[nodiscard]]
enum archive_status archive_open(
    const struct byte_source *source,
    struct archive **out_archive
);

void archive_close(struct archive *archive);

[[nodiscard]]
enum archive_status archive_lookup(
    const struct archive *archive,
    const char *path,
    struct archive_entry *out_entry
);
```

## ELF

```c
struct executable_image;

struct executable_segment {
    vaddr_t virtual_address;
    uint64_t file_offset;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t permissions;
};

enum elf_status {
    ELF_OK,
    ELF_INVALID_MAGIC,
    ELF_UNSUPPORTED_CLASS,
    ELF_UNSUPPORTED_MACHINE,
    ELF_INVALID_HEADER,
    ELF_INVALID_SEGMENT,
    ELF_OUT_OF_BOUNDS,
};

[[nodiscard]]
enum elf_status elf64_parse(
    const struct byte_source *source,
    struct executable_image **out_image
);

void executable_image_destroy(struct executable_image *image);

[[nodiscard]]
enum process_status process_load_executable(
    struct process *process,
    const struct executable_image *image
);
```

---

# M11 — VFS e filesystem locale

Le API devono essere implementabili sia da un backend locale sia da un futuro proxy IPC.

## Tipi

```c
struct vfs;
struct file;

typedef struct {
    uint64_t value;
} file_handle_t;

enum file_type {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_CHARACTER_DEVICE,
    FILE_TYPE_BLOCK_DEVICE,
};

enum fs_status {
    FS_OK,
    FS_INVALID_ARGUMENT,
    FS_NOT_FOUND,
    FS_ALREADY_EXISTS,
    FS_NOT_DIRECTORY,
    FS_IS_DIRECTORY,
    FS_PERMISSION_DENIED,
    FS_IO_ERROR,
    FS_END_OF_FILE,
};
```

## API pubbliche

```c
[[nodiscard]]
enum fs_status vfs_init(struct vfs **out_vfs);

[[nodiscard]]
enum fs_status vfs_mount(
    struct vfs *vfs,
    const char *path,
    object_handle_t filesystem
);

[[nodiscard]]
enum fs_status file_open(
    struct vfs *vfs,
    const char *path,
    uint32_t flags,
    file_handle_t *out_file
);

[[nodiscard]]
enum fs_status file_read(
    file_handle_t file,
    uint64_t offset,
    void *buffer,
    size_t size,
    size_t *out_read
);

[[nodiscard]]
enum fs_status file_write(
    file_handle_t file,
    uint64_t offset,
    const void *buffer,
    size_t size,
    size_t *out_written
);

[[nodiscard]]
enum fs_status file_close(file_handle_t file);
```

## Contratto del backend

```c
struct filesystem_ops {
    enum fs_status (*open)(
        void *context,
        const char *path,
        uint32_t flags,
        object_handle_t *out_file
    );

    enum fs_status (*read)(
        void *context,
        object_handle_t file,
        uint64_t offset,
        void *buffer,
        size_t size,
        size_t *out_read
    );

    enum fs_status (*write)(
        void *context,
        object_handle_t file,
        uint64_t offset,
        const void *buffer,
        size_t size,
        size_t *out_written
    );

    enum fs_status (*close)(
        void *context,
        object_handle_t file
    );
};
```

Prima implementazione:

```text
client VFS -> backend initramfs locale
```

Evoluzione:

```text
client VFS -> proxy IPC -> filesystem server
```

---

# M12 — Estrazione del primo servizio user space

Questa milestone valida l'architettura ibrida evolutiva.

## Service discovery

```c
typedef struct {
    uint64_t value;
} service_id_t;

enum service_status {
    SERVICE_OK,
    SERVICE_NOT_FOUND,
    SERVICE_ALREADY_REGISTERED,
    SERVICE_UNAVAILABLE,
    SERVICE_PERMISSION_DENIED,
};

[[nodiscard]]
enum service_status service_register(
    const char *name,
    object_handle_t endpoint,
    service_id_t *out_service
);

[[nodiscard]]
enum service_status service_connect(
    const char *name,
    object_handle_t *out_endpoint
);

[[nodiscard]]
enum service_status service_unregister(service_id_t service);
```

## Protocollo filesystem iniziale

```c
enum fs_protocol_operation : uint32_t {
    FS_PROTOCOL_OPEN,
    FS_PROTOCOL_READ,
    FS_PROTOCOL_WRITE,
    FS_PROTOCOL_CLOSE,
};

struct fs_open_request {
    uint32_t flags;
    uint32_t path_length;
    char path[];
};

struct fs_open_reply {
    enum fs_status status;
    object_handle_t file;
};
```

Le strutture a dimensione variabile dovranno essere validate con helper espliciti e non dereferenziate direttamente senza controllo dei limiti.

## API del client

Le API `file_open`, `file_read`, `file_write` e `file_close` non cambiano. Cambia soltanto il backend:

```text
filesystem_ops locale
        ↓
filesystem_ops proxy IPC
```

## Criterio di completamento

La stessa suite contrattuale deve passare contro:

```text
backend_initramfs_local
backend_filesystem_ipc
```

---

# Modello di documentazione per ogni API

Ogni API pubblica deve documentare:

```text
Nome
Scopo
Milestone di introduzione
Header pubblico
Contesto consentito
Parametri
Output
Ownership
Lifetime
Thread safety
Interrupt safety
Possibili errori
Effetti collaterali
Backend iniziale
Possibile backend remoto
Test richiesti
```

Esempio:

```c
/**
 * @brief Alloca una pagina fisica libera.
 *
 * Contesto: thread kernel; non consentita in NMI.
 * Ownership: in caso di successo la pagina passa al chiamante.
 * Errori: PMM_OUT_OF_MEMORY, PMM_INVALID_ARGUMENT.
 * Thread safety: sì, dopo l'inizializzazione dello scheduler.
 *
 * @param out_page Riceve l'indirizzo fisico della pagina.
 * @return Stato dell'operazione.
 */
[[nodiscard]]
enum pmm_status pmm_alloc_page(paddr_t *out_page);
```

# Regole di evoluzione

- Una firma pubblica non viene modificata senza aggiornare questa roadmap.
- Una modifica incompatibile richiede ADR.
- Le firme rappresentano intenzioni progettuali, non un vincolo immutabile.
- Le API non ancora implementate devono essere marcate come pianificate, non aggiunte come stub fittizi.
- Ogni milestone deve revisionare la propria sezione prima dell'implementazione.
- Dopo la milestone, le firme effettive devono sostituire quelle proposte.
- Le API condivise tra kernel e user space devono vivere in header di protocollo separati.
- I dettagli x86_64 non devono apparire nei contratti generici salvo che siano parte esplicita di un backend architetturale.