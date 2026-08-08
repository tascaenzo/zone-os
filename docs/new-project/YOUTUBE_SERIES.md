# Piano della serie YouTube

## Scopo della serie

La serie documenta la costruzione di un sistema operativo educativo moderno x86_64, da un repository vuoto fino all’esecuzione di programmi nello spazio utente.

Il pubblico deve apprendere sia i concetti dei sistemi operativi sia il processo ingegneristico usato per costruire, debuggare e testare software low-level.

## Struttura degli episodi

Ogni episodio deve contenere:

1. il problema da risolvere;
2. la teoria necessaria;
3. il design scelto;
4. l’implementazione per piccoli passi;
5. almeno un percorso di errore o diagnostica;
6. un risultato osservabile;
7. un test;
8. un tag iniziale e uno finale.

La durata è flessibile. Gli argomenti non devono essere allungati o compressi solo per rispettare una durata prefissata.

## Stagione 0 — Fondamenta e sistema di build

### Episodio 0 — Perché costruire un altro sistema operativo?

- lezioni apprese da Zone OS;
- ambito e non-obiettivi;
- roadmap;
- strategia didattica del repository.

**Risultato:** visione del progetto e repository iniziale.

### Episodio 1 — C hosted e freestanding

- cos’è un kernel;
- esecuzione hosted e freestanding;
- perché C23;
- cosa fornisce e cosa non fornisce il compilatore.

**Risultato:** primo object file freestanding.

### Episodio 2 — Cross-compilazione con Clang

- target triple;
- assunzioni ABI;
- flag del compilatore;
- red zone e code model;
- ispezione degli object file generati.

**Risultato:** object file kernel x86_64 verificati.

### Episodio 3 — Meson e Ninja

- grafi di build;
- compilazione incrementale;
- dipendenze dagli header;
- profili debug e release;
- perché Zone OS ricompilava troppo.

**Risultato:** build incrementale rapida del kernel.

### Episodio 4 — Link di un kernel ELF

- sezioni;
- simboli;
- linker script;
- indirizzi virtuali e fisici;
- map file e ispezione ELF.

**Risultato:** kernel ELF valido.

### Episodio 5 — Limine e immagine UEFI

- firmware e bootloader;
- protocollo Limine;
- albero di staging;
- immagine UEFI riproducibile.

**Risultato:** QEMU trasferisce il controllo al kernel.

### Episodio 6 — QEMU, seriale e GDB

- output seriale;
- opzioni QEMU;
- debugging remoto GDB;
- breakpoint e ispezione dei registri.

**Risultato:** boot controllato e debuggabile.

## Stagione 1 — Fondamenta della CPU

Argomenti:

- entry point e validazione delle risposte di boot;
- logging e panic;
- ambiente di esecuzione x86_64;
- GDT;
- TSS;
- IDT;
- stub delle eccezioni CPU;
- interrupt frame;
- diagnostica dei page fault;
- stack trace.

**Risultato della stagione:** fault CPU intenzionali producono report diagnostici affidabili.

## Stagione 2 — Gestione della memoria

Argomenti:

- memory map fisica;
- terminologia delle pagine;
- regioni riservate;
- PMM bitmap;
- page table x86_64;
- kernel high-half;
- direct map;
- API di mapping;
- NX e permessi;
- allocator iniziale;
- heap kernel.

**Risultato:** allocazione fisica, mapping virtuali e memoria dinamica testati.

## Stagione 3 — Interrupt e scheduling

Argomenti:

- PIC legacy e APIC;
- Local APIC;
- IOAPIC;
- timer;
- codice interrupt-safe;
- rappresentazione dei thread;
- context switching;
- ready queue;
- idle thread;
- preemption;
- basi della sincronizzazione.

**Risultato:** più thread kernel vengono eseguiti in modo preemptive.

## Stagione 4 — Spazio utente

Argomenti:

- livelli di privilegio;
- address space utente;
- stack utente;
- transizione a Ring 3;
- accesso sicuro alla memoria utente;
- ABI syscall;
- `write`, `exit` e `yield`;
- ciclo di vita dei processi.

**Risultato:** un programma utente stampa tramite syscall e termina.

## Stagione 5 — Programmi e file

Argomenti:

- initramfs;
- formato TAR;
- caricamento ELF64;
- concetti VFS;
- vnode e file descriptor;
- dispositivo console;
- processo iniziale;
- esecuzione semplice di comandi.

**Risultato:** più programmi utente vengono caricati dall’initramfs.

## Materiale di supporto

Ogni episodio deve pubblicare:

- note;
- diagrammi quando utili;
- comandi mostrati nel video;
- riferimenti;
- tag iniziale e finale;
- istruzioni di test;
- limitazioni note;
- esercizi facoltativi.

Template consigliato:

```markdown
# Episodio NN — Titolo

## Obiettivi di apprendimento
## Punto di partenza
## Concetti
## Passi di implementazione
## Comandi
## Test
## Errori comuni
## Esercizi
## Stato finale
```

## Principi didattici

- Non nascondere un flag del compilatore o linker senza spiegarlo.
- Distinguere regole hardware, ABI, comportamento del compilatore e convenzioni del progetto.
- Spiegare l’undefined behavior quando influenza il kernel.
- Mostrare come ispezionare i binari invece di trattare la build come magia.
- Preferire diagrammi per stack, page table e transizioni.
- Mantenere compilabili i tag dei vecchi episodi quando praticabile.
- Correggere pubblicamente gli errori nella documentazione e nelle note successive.

## Flusso della community

Domande e correzioni dovranno essere indirizzate alle GitHub Discussions o alle issue specifiche degli episodi quando il nuovo repository le supporterà. Le segnalazioni di bug devono includere tag dell’episodio, ambiente host, comando eseguito e output seriale.