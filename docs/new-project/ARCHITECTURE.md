# Architettura

## Panoramica

La prima versione è un kernel monolitico modulare destinato a x86_64. Il codice specifico dell'architettura è isolato, ma la portabilità non deve complicare la prima implementazione.

```text
Bootloader
    |
    v
Bootstrap dell'architettura
    |
    +--> Diagnostica seriale
    +--> Tabelle CPU ed eccezioni
    +--> Gestore della memoria fisica
    +--> Gestore della memoria virtuale
    +--> Heap del kernel
    +--> Controller degli interrupt e timer
    +--> Scheduler
    +--> Modalità utente e chiamate di sistema
    +--> VFS, initramfs e caricatore ELF
```

## Struttura proposta dei sorgenti

```text
kernel/
├── arch/
│   └── x86_64/
│       ├── boot/
│       ├── cpu/
│       ├── interrupts/
│       ├── memory/
│       └── platform/
├── core/
│   ├── init/
│   ├── panic/
│   ├── log/
│   └── scheduler/
├── mm/
│   ├── pmm/
│   ├── vmm/
│   └── heap/
├── drivers/
│   ├── serial/
│   ├── timer/
│   └── console/
├── fs/
│   ├── vfs/
│   └── initramfs/
├── process/
├── syscall/
└── lib/
include/
├── kernel/
└── arch/x86_64/
tests/
├── host/
└── kernel/
tools/
docs/
```

## Regole di stratificazione

- Il codice generico del kernel non deve includere header privati dell'architettura.
- Il codice dell'architettura può dipendere dalle interfacce generiche del kernel quando l'ordine di inizializzazione lo consente.
- I driver hardware devono esporre interfacce piccole ed evitare di propagare il layout dei registri in moduli non correlati.
- I gestori della memoria non devono stampare direttamente; riportano gli errori tramite valori di stato o l'interfaccia di logging.
- Il percorso di panic non deve usare allocazione dinamica.
- Il codice di boot deve validare tutte le risposte del bootloader prima di dereferenziarle.

## Sequenza di boot

1. Limine carica il kernel ELF e trasferisce il controllo.
2. Il codice di ingresso stabilisce lo stato minimo richiesto della CPU.
3. Viene inizializzata la porta seriale.
4. Vengono validate le risposte del bootloader e le informazioni della mappa di memoria.
5. Vengono installate GDT, TSS e IDT.
6. Vengono abilitati gli handler delle eccezioni CPU.
7. Viene inizializzato il gestore della memoria fisica.
8. Vengono create le page table di proprietà del kernel.
9. Viene inizializzato l'heap del kernel.
10. Vengono configurati controller degli interrupt e timer.
11. Lo scheduler avvia il thread idle e i task iniziali del kernel.
12. Il primo processo utente viene caricato quando la modalità utente è disponibile.

## Architettura della memoria

Il progetto iniziale usa:

- pagine base da 4 KiB;
- kernel high-half;
- direct map esplicita della memoria fisica;
- allocatore di pagine fisiche basato su bitmap;
- oggetti address space separati;
- pagine dati, heap e stack non eseguibili quando supportato;
- guard page per gli stack importanti;
- API esplicite di mapping, unmapping e risoluzione degli indirizzi.

Gli indirizzi fisici e virtuali devono usare tipi distinti del progetto, come `paddr_t` e `vaddr_t`. Le conversioni devono essere esplicite.

## Modello di esecuzione

Lo scheduler iniziale è single-core e preemptive. Supporta prima i kernel thread, poi i processi con address space indipendenti.

Stati iniziali dei thread:

```text
NEW -> READY -> RUNNING -> BLOCKED
                 |           |
                 +--> READY <-+
                 |
                 +--> DEAD
```

## Confine delle chiamate di sistema

L'ABI delle chiamate di sistema viene introdotta solo dopo che Ring 3 funziona in modo affidabile. Tutti i puntatori provenienti dallo user space devono essere validati prima dell'uso. L'ABI iniziale deve restare volutamente piccola e versionata.

Prime chiamate candidate:

- `write`
- `exit`
- `yield`
- `mmap`
- `munmap`
- `spawn`

## Politica di gestione degli errori

Le operazioni recuperabili restituiscono valori di stato tipizzati. Le violazioni fatali delle invarianti invocano `panic()` con posizione nel sorgente e stato della macchina, quando disponibili.

Le assertion sono abilitate nelle build debug. Le build release possono rimuovere diagnostica costosa, ma non devono dipendere dalle assertion per la correttezza.