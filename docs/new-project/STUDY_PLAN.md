# Piano di studio per lo sviluppo di sistemi operativi

Questo piano segue le milestone del progetto. Non è un muro di prerequisiti: studio e implementazione devono alternarsi. Ogni argomento va compreso abbastanza da poterlo spiegare, implementare in forma minima e debuggare nei casi di errore più comuni.

L’implementazione parte da **x86_64 in modalità a 64 bit**, distinguendo sempre i concetti universali dei sistemi operativi dai meccanismi specifici della prima architettura.

## Metodo di studio per ogni milestone

Per ogni argomento:

1. studiare il concetto indipendente dall’architettura;
2. identificare il contratto richiesto dal kernel;
3. studiare il meccanismo x86_64 usato dal primo backend;
4. implementare la versione minima osservabile;
5. creare test di successo e di errore;
6. spiegare il risultato senza nascondere assunzioni hardware o del bootloader;
7. annotare cosa cambierebbe su un’altra architettura.

Ogni unità produce:

- [ ] note personali concise;
- [ ] un diagramma;
- [ ] un glossario;
- [ ] un esperimento minimo;
- [ ] un esperimento di fallimento;
- [ ] una scaletta per un episodio;
- [ ] riferimenti a documentazione primaria.

---

# Fondamenta A — C23 moderno freestanding

## Studiare

- translation unit, dichiarazioni e definizioni;
- durata e lifetime degli oggetti;
- conversioni e promozioni intere;
- aritmetica dei puntatori;
- allineamento e padding;
- strict aliasing ed effective type;
- comportamento undefined, unspecified e implementation-defined;
- semantica e limiti di `volatile`;
- concetti base degli atomici;
- ambiente hosted e freestanding;
- builtin ed estensioni del compilatore;
- calling convention e confini ABI.

## C23 da usare consapevolmente

- `static_assert`;
- `[[noreturn]]`, `[[nodiscard]]`, `[[maybe_unused]]`;
- enum con tipo sottostante fisso quando verificato;
- `nullptr` quando supportato dalla baseline;
- letterali binari quando chiariscono i bit;
- operazioni intere esplicite e controllate.

## Esercizi

- [ ] ispezionare layout con `sizeof`, `alignof` e `offsetof`;
- [ ] scrivere helper di allineamento protetti da overflow;
- [ ] implementare `memset`, `memcpy`, `memmove`, `memcmp`, `strlen`;
- [ ] testare copie sovrapposte;
- [ ] dimostrare perché l’overflow signed non è wrapping garantito;
- [ ] confrontare assembly con e senza `volatile`;
- [ ] creare un header di astrazione del compilatore.

**Segnale di completamento:** saper spiegare perché codice C apparentemente corretto può essere invalido in un kernel per UB, ABI o trasformazioni dell’ottimizzatore.

---

# Fondamenta B — Architettura dei calcolatori

## Concetti generali

- livelli di privilegio;
- esecuzione delle istruzioni e basi della pipeline;
- registri e stato del processore;
- interrupt ed eccezioni;
- indirizzi virtuali e fisici;
- cache e gerarchia di memoria;
- ordinamento della memoria;
- MMIO e port I/O;
- DMA;
- basi del multiprocessore;
- firmware e descrizione dell’hardware.

## Focus x86_64

- registri generali, `RIP`, `RSP`, `RFLAGS`;
- long mode e indirizzi canonici;
- control register e MSR;
- GDT, TSS e IDT;
- gerarchia del paging;
- CPUID;
- famiglia APIC;
- `syscall/sysret` e `iretq`;
- System V AMD64 ABI come riferimento, non come ABI kernel imposta.

## Esercizi

- [ ] leggere e annotare un dump dei registri;
- [ ] decodificare un indirizzo canonico;
- [ ] disegnare la traduzione a quattro livelli;
- [ ] distinguere eccezione e interrupt hardware;
- [ ] ispezionare CPUID da user space;
- [ ] seguire prologo ed epilogo di una funzione in GDB.

---

# Fondamenta C — Toolchain, ELF e linking

## Studiare

- preprocessing, compilazione, assembly e link;
- object file;
- simboli e rilocazioni;
- header, sezioni e program header ELF;
- link statico e dinamico;
- linker script;
- indirizzo di caricamento e indirizzo virtuale;
- informazioni di debug e map file;
- helper runtime generati dal compilatore.

## Esercizi

- [ ] compilare C in assembly;
- [ ] ispezionare oggetti con `readelf`, `llvm-readobj`, `objdump`;
- [ ] costruire un ELF minimo con linker script;
- [ ] individuare entry, sezioni e segmenti;
- [ ] provocare e risolvere un simbolo runtime mancante;
- [ ] generare e leggere una link map.

---

# Studio M0 — Ingegneria della build

## Concetti

Grafi delle dipendenze, build incrementali, artefatti generati, riproducibilità, profili, cross-compilazione, strumenti host e binari target, version pinning, cache CI.

## Strumenti

Meson, Ninja, Clang, LLD, Python solo per orchestrazione, QEMU e GDB.

## Esercizi

- [ ] compilare incrementalmente due translation unit;
- [ ] verificare dipendenze automatiche dagli header;
- [ ] mantenere debug e release affiancate;
- [ ] generare `compile_commands.json`;
- [ ] misurare build cold, incrementale e no-op;
- [ ] riprodurre tutto in un container pulito.

Domande da spiegare: perché cancellare la directory di build è inefficiente? Cosa conosce Ninja che uno script shell non conosce? Quali programmi girano sull’host e quali artefatti sono per il target?

---

# Studio M1 — Firmware, boot e ambiente di esecuzione

## Concetti generali

Responsabilità di firmware, bootloader e kernel; contratto di boot; normalizzazione delle informazioni; ordine dell’early init; requisiti dello stack; halt controllato.

## Focus x86_64/UEFI

UEFI ed EFI System Partition, ruolo PE/COFF, Limine e kernel ELF64, assunzioni del long mode, memory map iniziale, seriale COM1 in QEMU.

## Esercizi

- [ ] fermarsi all’entry point con GDB;
- [ ] verificare l’allineamento iniziale dello stack;
- [ ] stampare firmware e memoria normalizzati;
- [ ] rimuovere una risposta obbligatoria e verificare l’errore controllato;
- [ ] confrontare concettualmente UEFI e BIOS legacy.

Riflessione: cosa cambierebbe con ARM64/UEFI o RISC-V/SBI?

---

# Studio M2 — Eccezioni, interrupt frame e diagnostica

## Concetti generali

Eccezioni sincrone, interrupt asincroni, contesti salvati, rientranza, fault recuperabili e fatali, panic, logging di emergenza, stack unwinding.

## Focus x86_64

Gate IDT, vettori, eccezioni con error code, transizioni di privilegio, TSS e IST, frame `iretq`, page-fault code, CR2 e double fault.

## Esercizi

- [ ] decodificare un frame costruito manualmente;
- [ ] provocare divide-by-zero, invalid opcode e page fault;
- [ ] verificare layout C/assembly con `static_assert`;
- [ ] produrre uno stack trace con frame pointer;
- [ ] testare un fallimento annidato.

---

# Studio M3 — Memoria fisica

## Concetti

Proprietà della memoria, frame, memory map firmware, regioni riservate o reclamabili, frammentazione, bitmap/free-list/buddy, allocazione contigua, metadati e invarianti.

## Focus x86_64

Pagine da 4 KiB, consapevolezza delle large page, map Limine, larghezza dell’indirizzo fisico e MMIO.

## Esercizi

- [ ] normalizzare mappe sintetiche sovrapposte;
- [ ] implementare e testare un allocator bitmap;
- [ ] allocare tutte le pagine e liberarle in ordine casuale;
- [ ] rilevare double-free e free non allineato;
- [ ] confrontare bitmap e buddy senza implementare subito buddy.

---

# Studio M4 — Memoria virtuale

## Concetti

Address space, traduzione, permessi, separazione kernel/utente, mapping eager e demand, copy-on-write come concetto, direct map, TLB, cambio address space, guard page, W^X.

## Focus x86_64

PML4/PDPT/PD/PT, indirizzi canonici, flag delle entry, CR3, NXE/NX, `invlpg`, PCID rimandato e kernel high-half.

## Esercizi

- [ ] attraversare manualmente un indirizzo virtuale;
- [ ] implementare test map/resolve/unmap;
- [ ] testare ogni confine delle tabelle;
- [ ] provocare fault read-only e NX;
- [ ] distruggere un address space verificando il rilascio dei frame;
- [ ] aggiungere una guard page sotto uno stack.

---

# Studio M5 — Allocazione dinamica

## Concetti

Semantica di allocazione, allineamento, frammentazione, corruzione dei metadati, free list, boundary tag, splitting, coalescing, slab, contesti di allocazione, poisoning e canary.

## Esercizi

- [ ] bump allocator;
- [ ] free-list allineata testata sull’host;
- [ ] fuzz di sequenze alloc/free;
- [ ] misura della frammentazione;
- [ ] confronto tra free-list, buddy e slab;
- [ ] verifica degli overflow nei calcoli delle dimensioni.

---

# Studio M6 — Scoperta hardware, interrupt e clock

## Concetti

Tabelle di descrizione, controller interrupt, routing, masking, acknowledgement, edge/level trigger, clock source, clock event, tempo monotono, calibrazione, deadline e tick periodico.

## Focus x86_64

ACPI RSDP/XSDT e checksum, MADT, LAPIC, IOAPIC, PIC legacy, timer LAPIC, HPET e TSC, vettore spurio.

## Esercizi

- [ ] analizzare tabelle ACPI sintetiche malformate;
- [ ] disegnare il percorso device→handler;
- [ ] ricevere e riconoscere un timer interrupt;
- [ ] mask/unmask di una sorgente;
- [ ] distinguere clock source e clock event.

---

# Studio M7 — Concorrenza e scheduling

## Concetti

Contesto di esecuzione, thread e processo, scheduling cooperativo/preemptive, stati, race, sezioni critiche, interrupt disabling e lock, wait queue, lifetime e reclamazione.

## Focus x86_64

Set di registri del context switch, cambio stack, registri preservati dall’ABI, ritorno da interrupt, concetti per-CPU pur rimandando SMP.

## Esercizi

- [ ] modello host-side della ready queue;
- [ ] costruzione manuale dello stack di un nuovo thread;
- [ ] switch cooperativo tra due thread;
- [ ] preemption da timer;
- [ ] block/wake;
- [ ] verifica dei registri con pattern noti.

---

# Studio M8 — Protezione e modalità utente

## Concetti

Domini di protezione, processi, separazione dei privilegi, range kernel/utente, contenimento dei fault, copie sicure, TOCTOU e ciclo di vita.

## Focus x86_64

Ring 0/Ring 3, selector in long mode, `RSP0`, bit user/supervisor, `iretq`, SMEP/SMAP come hardening futuro.

## Esercizi

- [ ] entrare in una funzione utente;
- [ ] verificare il livello di privilegio;
- [ ] tentare un’istruzione privilegiata;
- [ ] tentare di modificare una pagina kernel;
- [ ] terminare solo il processo colpevole;
- [ ] testare range invalidi nei copy helper.

---

# Studio M9 — Syscall e design ABI

## Concetti

Scopo delle syscall, stabilità ABI, passaggio argomenti, convenzioni degli errori, permessi, puntatori utente, syscall bloccanti, restart e versioning.

## Focus x86_64

Registri e MSR di `syscall/sysret`, fallback `iretq`, concetto di `swapgs`, validazione dell’indirizzo di ritorno, differenza tra ABI utente e ABI syscall.

## Esercizi

- [ ] tabella ABI per tre syscall;
- [ ] syscall sconosciuta;
- [ ] puntatori errati e lunghezze estreme;
- [ ] registri clobbered e preservati;
- [ ] tracing completo user→kernel→user.

---

# Studio M10 — Archivi, ELF e immagini di processo

## Concetti

Validazione di byte stream, archivi, formati eseguibili, segmenti e sezioni, sicurezza del loader, overflow nel parsing, immagine del processo e stack iniziale.

## Focus x86_64

ELF64, machine identifier x86_64, little endian, segmenti loadable, entry point e binari statici.

## Esercizi

- [ ] parser ELF64 host-side con letture bounds-checked;
- [ ] rifiuto di header malformati e offset in overflow;
- [ ] mapping con permessi corretti;
- [ ] azzeramento BSS;
- [ ] piccolo programma freestanding utente;
- [ ] caricamento ed esecuzione da initramfs.

---

# Studio M11 — Filesystem e VFS

## Concetti

Namespace, path resolution, file, directory e device, inode/vnode, mount, file handle e descriptor, offset, operazioni del filesystem, lifetime e caching di base.

## Esercizi

- [ ] prototipo VFS in memoria sull’host;
- [ ] parser sicuro dei percorsi assoluti;
- [ ] gestione di `.`, `..` e separatori ripetuti;
- [ ] mount root dell’initramfs;
- [ ] offset indipendenti;
- [ ] device console.

---

# Percorsi rimandati

## Sincronizzazione e SMP

Atomici, memory model, spinlock, mutex, reader/writer lock, dati per-CPU, IPI, TLB shootdown, ordine dei lock e deadlock.

## Device e bus

PCI/PCIe, MMIO, DMA, MSI/MSI-X, block device, AHCI/NVMe e USB.

## Filesystem persistenti

Block cache, FAT/ext2, consistenza, bitmap di allocazione, directory e comportamento dopo crash.

## Networking

Driver NIC, Ethernet, ARP, IPv4/IPv6, routing, UDP/TCP e socket.

## Seconda architettura

Dopo la stabilizzazione dei contratti generici, un backend minimo ARM64 o RISC-V dovrà validarli senza promettere parità immediata di funzionalità.

---

# Riferimenti primari consigliati

Preferire specifiche e documentazione primaria:

- materiale ISO C e documentazione dei compilatori;
- System V AMD64 ABI;
- manuali Intel o AMD;
- specifica UEFI;
- documentazione del protocollo Limine;
- specifica ELF;
- specifica ACPI;
- documentazione Meson e Ninja;
- manuali QEMU e GDB.

Le risorse secondarie sono utili per l’intuizione, ma ogni implementazione sensibile all’hardware deve essere verificata contro una fonte autorevole.