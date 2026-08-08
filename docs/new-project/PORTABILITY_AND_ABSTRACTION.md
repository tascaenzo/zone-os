# Strategia di portabilità e astrazione

Il progetto parte da **x86_64 in modalità a 64 bit**, ma il codice non deve trasformare i meccanismi x86_64 in concetti universali del kernel.

L’obiettivo non è supportare subito più architetture. L’obiettivo è mantenere confini chiari, così che un backend futuro possa essere aggiunto senza riscrivere il kernel generico.

## Principio fondamentale

> Astrarre i concetti stabili, non l’hardware ipotetico.

Non va progettata in anticipo una HAL universale e molto estesa. È preferibile isolare l’accesso diretto all’hardware, esporre contratti piccoli e modificarli solo quando requisiti concreti lo richiedono.

## Confine nell’albero dei sorgenti

```text
kernel/
├── arch/
│   └── x86_64/
│       ├── boot/
│       ├── cpu/
│       ├── interrupts/
│       ├── memory/
│       ├── time/
│       └── context/
├── platform/
│   ├── firmware/
│   ├── interrupt_controller/
│   └── timer/
├── core/
├── mm/
├── sched/
├── process/
├── fs/
└── lib/
```

Regole di proprietà:

- `arch/x86_64` possiede istruzioni, registri, descriptor table e formati delle page table;
- la memoria generica possiede politiche di allocazione e mapping;
- lo scheduler generico possiede stati e politiche dei thread;
- il backend architetturale possiede il context switch low-level;
- gli adapter di boot possiedono le strutture Limine;
- il kernel generico consuma informazioni di boot normalizzate;
- il codice indipendente dai device usa interfacce, non registri APIC o COM1.

## Elementi specifici dell’architettura

- assembly di ingresso;
- rilevamento delle feature CPU;
- accesso a control register e MSR;
- stub di eccezioni e interrupt;
- costruzione di GDT, TSS e IDT;
- codifica delle entry delle page table;
- istruzioni di invalidazione TLB;
- assembly di context switch;
- transizioni di privilegio;
- ingresso delle syscall;
- port I/O;
- memory barrier specifiche;
- accesso ai registri APIC.

## Elementi generici

- formattazione e routing dei log;
- politica di panic;
- reporting normalizzato delle eccezioni;
- proprietà delle pagine fisiche;
- politiche di allocazione;
- ciclo di vita degli address space;
- permessi di mapping generici;
- heap;
- code e politica dello scheduler;
- stato di thread e processi;
- semantica delle syscall;
- framework di parsing ELF;
- VFS e file descriptor;
- gestione dell’archivio initramfs;
- test di algoritmi e parser puri.

## Tipi di indirizzo

Evitare interi privi di significato quando il dominio dell’indirizzo è importante:

```c
typedef struct { uintptr_t value; } paddr_t;
typedef struct { uintptr_t value; } vaddr_t;
```

Le operazioni devono essere esplicite:

```c
[[nodiscard]] bool paddr_add(paddr_t base, size_t offset, paddr_t *out);
[[nodiscard]] bool vaddr_add(vaddr_t base, size_t offset, vaddr_t *out);
```

## Contesto CPU

Non imporre un’unica struttura universale dei registri. Usare due livelli:

1. un contesto grezzo posseduto dall’architettura;
2. una vista diagnostica generica.

```c
struct arch_cpu_context;

struct execution_view {
    vaddr_t instruction_pointer;
    vaddr_t stack_pointer;
    uintptr_t flags;
    bool from_user;
};
```

Il backend conserva sempre le informazioni specifiche complete.

## Eccezioni

Il codice generico ragiona per categorie:

```c
enum exception_class {
    EXCEPTION_ARITHMETIC,
    EXCEPTION_INVALID_INSTRUCTION,
    EXCEPTION_MEMORY_ACCESS,
    EXCEPTION_BREAKPOINT,
    EXCEPTION_HARDWARE_FAILURE,
    EXCEPTION_UNKNOWN,
};
```

Il backend x86_64 conserva vettore e codice d’errore specifico.

## Memoria virtuale

I permessi generici non devono coincidere accidentalmente con i bit x86_64:

```c
enum vm_permission {
    VM_PERMISSION_READ    = 1u << 0,
    VM_PERMISSION_WRITE   = 1u << 1,
    VM_PERMISSION_EXECUTE = 1u << 2,
    VM_PERMISSION_USER    = 1u << 3,
    VM_PERMISSION_GLOBAL  = 1u << 4,
};
```

Il backend traduce questi valori nei bit present, writable, user, global e NX. La dimensione delle pagine deve provenire dal contratto dell’architettura o dalla configurazione del target, non da costanti duplicate.

## Interrupt

Il dispatch generico non deve assumere che ogni piattaforma usi vettori IDT numerati.

```c
struct interrupt_source {
    uintptr_t platform_id;
};
```

Registrazione, masking e acknowledgement sono operazioni del backend del controller. Il primo backend può mappare direttamente l’identificatore su vettori x86_64 e route IOAPIC.

## Tempo

Separare:

- **clock source**: valore che avanza continuamente;
- **clock event device**: programma un interrupt futuro;
- **timekeeping del kernel**: converte e accumula il tempo;
- **politica dello scheduler**: decide come usare il tempo.

Lo scheduler non deve leggere direttamente LAPIC o TSC.

## Scheduler

Il livello generico possiede stati, run queue, politica, blocco/risveglio e durata dei thread. Il backend architetturale possiede contesto iniziale, switch low-level e transizioni kernel/utente.

## Protocolli di boot

Limine è un adapter, non il modello interno del kernel:

```text
strutture Limine
      ↓ validazione e normalizzazione
boot_info del kernel
      ↓
PMM, framebuffer, moduli, firmware
```

Nessun sottosistema esterno all’adapter deve includere `limine.h`.

## Firmware e descrizione hardware

UEFI viene usato per il boot iniziale; ACPI descrive l’hardware x86_64 rilevante. I parser di ACPI producono modelli validati e normalizzati: i driver dei controller non devono analizzare direttamente le tabelle firmware.

## Driver

Preferire interfacce basate sulle capacità:

- sink di output byte;
- controller interrupt;
- clock source;
- clock event device;
- framebuffer;
- block device;
- network device.

Il driver seriale iniziale può essere specifico di x86_64/QEMU, mentre il logger vede soltanto un sink di byte.

## Astrazione del build system

La configurazione seleziona:

- sorgenti dell’architettura;
- flag specifici;
- linker script;
- strategia dell’immagine di boot;
- argomenti dell’emulatore;
- architettura del debugger;
- asset firmware.

Una nuova architettura deve aggiungere configurazione e backend, non duplicare tutta la build.

## Checklist di revisione delle astrazioni

- [ ] Il codice generico include inutilmente header specifici?
- [ ] Bit hardware trapelano in enum generici?
- [ ] Una struttura del bootloader sopravvive oltre l’early boot?
- [ ] Un indirizzo fisico viene trattato come puntatore dereferenziabile?
- [ ] Scheduler o VM generici eseguono assembly x86?
- [ ] Un nome specifico viene usato per un concetto generico?
- [ ] L’interfaccia risponde a un requisito reale?
- [ ] Un backend finto host-side può testare la logica?
- [ ] La semantica degli errori è documentata?
- [ ] L’astrazione è più piccola dell’implementazione che nasconde?

## Evitare la falsa portabilità

Non bisogna:

- creare directory ARM64 o RISC-V vuote solo per dichiarare portabilità;
- aggiungere callback per ogni feature immaginabile;
- disseminare `#ifdef` nel codice generico;
- fingere che segmentazione x86 e livelli di eccezione ARM siano identici;
- eliminare informazioni specifiche utili dalla diagnostica;
- ottimizzare per una seconda architettura prima che la prima sia corretta.

## Validazione futura

Dopo la stabilizzazione di M0–M11 verrà scelto un target minimo:

- ARM64 sotto QEMU con UEFI, oppure
- RISC-V 64 sotto QEMU con SBI/OpenSBI.

Il secondo backend dovrà inizialmente raggiungere solo build, boot, seriale, eccezioni e scoperta della memoria fisica. Qualunque astrazione renda il secondo backend più difficile senza chiarire il primo dovrà essere rivalutata.