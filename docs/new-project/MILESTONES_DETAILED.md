# Milestone dettagliate e checklist di completamento

Questo documento trasforma la roadmap generale in milestone operative. Il primo target è **x86_64 a 64 bit**, ma i contratti visibili al kernel devono evitare assunzioni architetturali non necessarie.

## Regole comuni

Ogni milestone è completa solo quando:

- [ ] il risultato è osservabile in QEMU o con un test host-side;
- [ ] i percorsi di errore vengono provocati intenzionalmente;
- [ ] le interfacce pubbliche sono documentate;
- [ ] il codice specifico resta sotto `kernel/arch/x86_64/`;
- [ ] il codice generico non accede direttamente a registri, porte o descriptor table x86;
- [ ] build debug e release compilano senza warning;
- [ ] la CI esegue i test rilevanti;
- [ ] documentazione e diagrammi sono aggiornati;
- [ ] viene creato un tag Git riproducibile;
- [ ] le note dell’episodio indicano stato iniziale e finale.

---

# M0 — Workspace di sviluppo a 64 bit riproducibile

## Obiettivo

Creare un ambiente rapido, comprensibile e riproducibile per un kernel freestanding a 64 bit.

## Contratto indipendente dall’architettura

La build descrive tramite configurazione: architettura target, word size, compilatore, linker, sorgenti di piattaforma, emulatore, firmware, protocollo di boot e trasporto di debug.

## Implementazione x86_64

Clang, LLD, Meson, Ninja, QEMU `q35`, UEFI, GDB remoto e terminale seriale.

## Deliverable

- [ ] `meson.build` principale;
- [ ] opzioni Meson per diagnostica e test;
- [ ] `config/x86_64.ini`;
- [ ] flag di compilazione e link espliciti;
- [ ] profili debug e release;
- [ ] controllo delle dipendenze;
- [ ] wrapper `tools/dev` sottile;
- [ ] launcher QEMU e GDB;
- [ ] CI di configurazione e build;
- [ ] versioni supportate documentate;
- [ ] Docker non obbligatorio nel ciclo locale;
- [ ] container opzionale e riproducibile.

## Verifica

- [ ] un clone pulito si configura con un comando;
- [ ] la build non modifica i sorgenti;
- [ ] una modifica C ricompila una sola translation unit;
- [ ] una modifica a un header ricompila solo i dipendenti;
- [ ] una build no-op non compila nulla;
- [ ] debug e release possono coesistere;
- [ ] gli errori mostrano il comando fallito;
- [ ] viene generato `compile_commands.json`.

## Criterio d’uscita

Un collaboratore può clonare, configurare, compilare, ispezionare l’ELF e avviare il debugger senza modificare percorsi a mano.

---

# M1 — Boot controllato a 64 bit

## Obiettivo

Entrare nel kernel in un ambiente noto, validare le informazioni di boot e arrestarsi in sicurezza.

## Contratto generico

Il kernel possiede una struttura `boot_info` normalizzata. Le strutture Limine restano confinate nell’adapter.

```c
struct boot_info;
[[nodiscard]] bool boot_capture_info(struct boot_info *out);
[[noreturn]] void platform_halt(void);
```

Le informazioni includono regioni di memoria, posizione del kernel, firmware, framebuffer opzionale, moduli, command line e descrizione hardware.

## Implementazione x86_64

Limine, UEFI, ELF64, entry virtuale high-half se prevista e seriale come primo backend.

## Checklist

- [ ] linker script documentato;
- [ ] simbolo di entry esplicito;
- [ ] richieste Limine validate prima dell’uso;
- [ ] conversione verso strutture del kernel;
- [ ] driver seriale early;
- [ ] halt controllato;
- [ ] immagine incrementale;
- [ ] strumenti per ispezionare header e sezioni ELF;
- [ ] conferma modalità 64 bit e allineamento dello stack;
- [ ] assenza di crash con informazioni opzionali mancanti;
- [ ] errore deterministico se manca un’informazione obbligatoria.

Risultato:

```text
[boot] ingresso nel kernel a 64 bit
[boot] informazioni normalizzate
[boot] console seriale pronta
[ok] boot controllato completato
```

---

# M2 — Diagnostica, contesto di esecuzione ed eccezioni

## Obiettivo

Costruire gli strumenti necessari a diagnosticare tutti i sottosistemi successivi.

## Contratto generico

Logging, panic, classificazione delle eccezioni, vista diagnostica del contesto CPU, stack trace e informazioni sulla sorgente.

```c
struct cpu_context;
struct exception_info;
void exception_dispatch(const struct exception_info *info,
                        const struct cpu_context *context);
[[noreturn]] void panic(const char *message);
```

## Backend x86_64

GDT, TSS, IDT, stub dei 256 vettori, normalizzazione dell’error code, CR2, frame-pointer stack trace e stack IST per double fault.

## Checklist

- [ ] logger strutturato e sink seriale;
- [ ] panic privo di allocazioni;
- [ ] GDT, TSS e IDT;
- [ ] stack dedicato alle eccezioni critiche;
- [ ] contesto salvato uniforme;
- [ ] `static_assert` tra layout C e assembly;
- [ ] nomi leggibili delle eccezioni;
- [ ] decodifica page fault;
- [ ] dump registri e stack trace;
- [ ] test divisione per zero, invalid opcode, breakpoint e page fault;
- [ ] gestione deterministica di fault annidati.

## Criterio d’uscita

Ogni eccezione fatale produce istruzione, contesto salvato e informazioni sufficienti a individuare la causa probabile.

---

# M3 — Proprietà della memoria fisica

## Obiettivo

Rappresentare la memoria fisica e allocare frame senza dipendenze nascoste dal bootloader.

## Contratto generico

```c
typedef struct { uintptr_t value; } paddr_t;

enum pmm_status {
    PMM_OK,
    PMM_OUT_OF_MEMORY,
    PMM_INVALID_ARGUMENT,
    PMM_NOT_OWNED,
    PMM_DOUBLE_FREE,
};
```

Il PMM non espone il formato delle page table x86.

## Implementazione iniziale

Pagine base da 4 KiB, allocator bitmap, adapter della memory map Limine, riserve esplicite per kernel, boot, moduli e framebuffer.

## Checklist

- [ ] modello normalizzato delle regioni;
- [ ] ordinamento e controllo sovrapposizioni;
- [ ] helper di allineamento;
- [ ] bitmap dei frame;
- [ ] API di riserva, allocazione e free;
- [ ] statistiche e integrity checker;
- [ ] nessuna regione non usabile viene allocata;
- [ ] ogni pagina restituita è allineata e unica;
- [ ] esaurimento restituisce errore;
- [ ] double-free e free non valido rilevati in debug;
- [ ] allocate-all/free-all ripristina il conteggio;
- [ ] normalizzazione testata sull’host con mappe malformate.

---

# M4 — Spazi di indirizzamento virtuale

## Obiettivo

Creare un’API di memoria virtuale generica sostenuta inizialmente dalle page table x86_64.

```c
typedef struct address_space address_space_t;

enum vm_flags {
    VM_READ    = 1u << 0,
    VM_WRITE   = 1u << 1,
    VM_EXECUTE = 1u << 2,
    VM_USER    = 1u << 3,
    VM_GLOBAL  = 1u << 4,
};
```

## Backend x86_64

Paging a quattro livelli, indirizzi canonici, direct map, kernel high-half, NX, CR3 e invalidazione TLB.

## Checklist

- [ ] oggetto address space;
- [ ] page-table walker;
- [ ] map, unmap e query;
- [ ] traduzione dei permessi;
- [ ] helper direct map;
- [ ] separazione kernel/utente;
- [ ] distruzione delle page table;
- [ ] dump e integrity checker;
- [ ] map/resolve e unmap verificati;
- [ ] indirizzi non canonici rifiutati;
- [ ] pagine read-only e NX realmente protette;
- [ ] rollback dei mapping parziali;
- [ ] test ai confini di ogni livello.

---

# M5 — Memoria dinamica del kernel

## Obiettivo

Offrire allocazione dinamica affidabile senza introdurre subito allocator complessi.

## Implementazione

Bump allocator early, espansione sostenuta da pagine e free list allineata.

## Checklist

- [ ] retirement esplicito dell’allocator early;
- [ ] `kmalloc`, `kfree` e allocazione allineata;
- [ ] gestione della regione virtuale dell’heap;
- [ ] splitting e coalescing;
- [ ] calcoli delle dimensioni protetti da overflow;
- [ ] poisoning e statistiche debug;
- [ ] integrity walk;
- [ ] allineamenti e zero-size documentati;
- [ ] test randomizzati host-side;
- [ ] double-free e invalid-free diagnosticati;
- [ ] il panic non dipende dall’heap.

---

# M6 — Interrupt hardware e tempo

## Obiettivo

Consegnare eventi hardware e stabilire un tempo monotono del kernel.

## Contratto generico

Sorgente interrupt, registrazione handler, acknowledgement, mask/unmask, clock monotono e clock event. Nessun registro APIC è visibile al codice generico.

## Backend x86_64

Scoperta ACPI, Local APIC, IOAPIC, disattivazione PIC legacy e timer calibrato.

## Checklist

- [ ] interfacce controller e timer;
- [ ] checksum e limiti ACPI validati;
- [ ] parsing MADT;
- [ ] configurazione LAPIC e IOAPIC;
- [ ] gestione interrupt spuri;
- [ ] contatore monotono;
- [ ] interrupt periodici deterministici;
- [ ] acknowledgement eseguito una sola volta;
- [ ] mask/unmask verificati;
- [ ] handler senza allocazione non autorizzata;
- [ ] il tempo non torna indietro.

---

# M7 — Thread kernel e scheduler

## Obiettivo

Eseguire più contesti kernel indipendenti su una CPU.

Il livello generico possiede thread, stati, ready queue, politica, blocco e risveglio. Il backend possiede la costruzione del contesto e lo switch low-level.

## Checklist

- [ ] oggetto thread e stack dedicato;
- [ ] contesto iniziale;
- [ ] routine di context switch;
- [ ] ready queue e idle thread;
- [ ] yield cooperativo;
- [ ] preemption da timer;
- [ ] block/wake;
- [ ] terminazione e reclamazione;
- [ ] invarianti e tracing;
- [ ] registri preservati tra switch;
- [ ] thread bloccati non eseguiti;
- [ ] idle eseguito solo in assenza di lavoro.

---

# M8 — Dominio di esecuzione utente

## Obiettivo

Eseguire codice non fidato in uno spazio meno privilegiato e contenerne i fault.

## Backend x86_64

Ring 3, TSS con stack kernel, ingresso iniziale tramite `iretq`, permessi user/supervisor.

## Checklist

- [ ] oggetto processo e address space dedicato;
- [ ] stack utente;
- [ ] transizione a Ring 3;
- [ ] ritorno controllato al kernel;
- [ ] controlli dei range utente;
- [ ] `copy_from_user` e `copy_to_user`;
- [ ] terminazione del solo processo che genera fault;
- [ ] distinzione tra fault kernel e utente;
- [ ] impossibilità di scrivere pagine kernel o eseguire istruzioni privilegiate;
- [ ] allineamento ABI dello stack.

---

# M9 — Confine delle chiamate di sistema

## Obiettivo

Fornire un’interfaccia documentata tra programmi utente e kernel.

## Checklist

- [ ] documento ABI;
- [ ] assembly di ingresso e ritorno;
- [ ] dispatcher;
- [ ] errore stabile per syscall sconosciute;
- [ ] `write`, `exit`, `yield`;
- [ ] validazione dei buffer utente;
- [ ] tracing debug;
- [ ] test di puntatori errati e lunghezze estreme;
- [ ] registri preservati secondo ABI;
- [ ] indirizzo di ritorno non controllabile arbitrariamente dall’utente.

---

# M10 — Initramfs e caricamento ELF64

## Obiettivo

Caricare programmi utente da un archivio fornito al boot senza dipendere da driver storage.

## Contratti separati

Scoperta dei moduli, accesso all’archivio, parsing dell’eseguibile e costruzione dell’immagine del processo.

## Implementazione iniziale

Adapter Limine, TAR/USTAR, ELF64 little-endian x86_64 e binari statici.

## Checklist

- [ ] normalizzazione dei moduli;
- [ ] lettore archivio bounds-checked;
- [ ] lookup dei percorsi;
- [ ] validazione header e program header ELF;
- [ ] mapping dei segmenti con permessi corretti;
- [ ] azzeramento BSS;
- [ ] validazione entry point;
- [ ] stack utente iniziale;
- [ ] rifiuto di architettura errata, file troncati, overflow e segmenti invalidi.

Risultato:

```text
ciao dallo spazio utente
```

---

# M11 — Virtual File System minimale

## Obiettivo

Fornire namespace e interfaccia file indipendenti dal filesystem sottostante.

## Implementazione iniziale

Filesystem initramfs read-only, `/dev/console` e tabella descriptor per processo.

## Checklist

- [ ] modello VFS;
- [ ] mount table;
- [ ] parser dei percorsi assoluti;
- [ ] attraversamento dei componenti;
- [ ] oggetto file e offset;
- [ ] allocazione descriptor;
- [ ] interfacce `open`, `read`, `write`, `close`;
- [ ] mount root dell’initramfs;
- [ ] dispositivo console;
- [ ] gestione documentata di `.`, `..` e separatori ripetuti;
- [ ] limiti dei descriptor;
- [ ] offset indipendenti per handle distinti.

## Criterio d’uscita

Un programma utente apre un file nell’initramfs e ne scrive il contenuto su `/dev/console`.

---

# Milestone rimandate

Dopo M0–M11: sincronizzazione avanzata, SMP e dati per-CPU, PCI, storage, FAT/ext2, pipe, rete, grafica, USB, dynamic linking, compatibilità POSIX e backend ARM64/RISC-V.

Una seconda architettura dovrà validare i confini esistenti con il backend minimo possibile, non provocare una riprogettazione speculativa dell’intero kernel.