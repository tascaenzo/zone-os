# Roadmap e milestone

Ogni milestone deve terminare con un risultato osservabile, una verifica automatizzata quando possibile, documentazione aggiornata e uno stato del repository contrassegnato da un tag.

## M0 — Workspace riproducibile

Deliverable:

- cross file Meson per x86_64;
- build incrementale con Ninja;
- configurazione della toolchain Clang e LLD;
- controllo delle dipendenze;
- directory di build debug e release;
- launcher QEMU;
- launcher GDB;
- job CI di compilazione.

Criteri di completamento:

- un clone pulito può essere configurato con un singolo comando documentato;
- modificare un sorgente C ricompila solo la relativa translation unit e rilinka;
- gli output di build non modificano mai l’albero dei sorgenti.

## M1 — Boot UEFI controllato

Deliverable:

- configurazione Limine;
- kernel ELF;
- immagine di boot UEFI;
- richieste al bootloader validate;
- inizializzazione seriale;
- percorso di halt controllato.

Risultato osservabile:

```text
[boot] ingresso nel kernel
[boot] risposte Limine validate
[ok] boot controllato completato
```

## M2 — Diagnostica ed eccezioni CPU

Deliverable:

- logging strutturato;
- interfaccia panic;
- GDT e TSS;
- IDT;
- handler delle eccezioni CPU;
- dump dei registri;
- decodifica degli errori di page fault;
- stack trace basilare.

Criteri di completamento:

- test intenzionali di divisione per zero e page fault producono diagnostica deterministica;
- il panic non alloca memoria.

## M3 — Gestione della memoria fisica

Deliverable:

- memory map normalizzata;
- regole di riserva delle pagine;
- allocator bitmap;
- API separate per pagina singola e pagine contigue;
- rilevamento double-free nelle build debug;
- statistiche e controlli d’integrità.

## M4 — Gestione della memoria virtuale

Deliverable:

- attraversamento delle page table;
- operazioni map, unmap e resolve;
- direct physical map;
- mapping high-half del kernel;
- supporto NX;
- invalidazione TLB;
- oggetto address space indipendente.

## M5 — Memoria dinamica del kernel

Deliverable:

- bump allocator iniziale;
- heap sostenuto da pagine;
- allocator semplice a free list;
- supporto all’allineamento;
- poisoning e controlli guard nelle build debug;
- test host-side dell’allocator quando possibile.

Buddy e slab allocator avanzati vengono rimandati finché misure concrete non ne giustificano l’introduzione.

## M6 — Interrupt hardware e tempo

Deliverable:

- inizializzazione Local APIC;
- routing IOAPIC;
- disattivazione del PIC legacy;
- calibrazione del timer;
- interrupt periodici del timer;
- API di registrazione degli interrupt.

## M7 — Thread kernel

Deliverable:

- rappresentazione dei thread;
- stack kernel per thread;
- context switching;
- ready queue;
- idle thread;
- scheduling round-robin;
- preemption;
- stati bloccato e terminato.

## M8 — Modalità utente

Deliverable:

- ingresso e ritorno da Ring 3;
- stack utente;
- address space separato;
- primitive sicure `copy_to_user` e `copy_from_user`;
- test intenzionale di isolamento di un fault utente.

## M9 — Chiamate di sistema

Deliverable:

- ABI syscall documentata;
- dispatch delle syscall;
- `write`, `exit` e `yield`;
- validazione di puntatori e lunghezze;
- comportamento per syscall sconosciute.

## M10 — Initramfs e programmi ELF

Deliverable:

- caricamento moduli Limine;
- lettore initramfs TAR;
- validazione e loader ELF64;
- creazione del processo iniziale;
- primo programma utente.

Risultato osservabile:

```text
ciao dallo spazio utente
```

## M11 — VFS minimale

Deliverable:

- modello vnode;
- risoluzione dei percorsi;
- file descriptor;
- `/dev/console`;
- initramfs montata come root;
- interfacce basilari `open`, `read`, `write` e `close`.

## Milestone successive

Il lavoro futuro può includere primitive di sincronizzazione, pipe, SMP, storage, FAT o ext2, networking, shell e grafica. Queste attività non devono essere pianificate finché il ciclo iniziale dello user space non è stabile.