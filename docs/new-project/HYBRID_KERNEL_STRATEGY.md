# Strategia del kernel ibrido evolutivo

## Decisione

Il progetto adotta un'architettura **ibrida evolutiva**.

La prima implementazione sarà un kernel monolitico modulare, perché permette di raggiungere rapidamente boot, diagnostica, memoria, scheduler e user space. Fin dall'inizio, però, i sottosistemi saranno separati da confini espliciti, contratti piccoli, ownership documentata e strutture non condivise direttamente.

L'obiettivo non è fingere di avere un microkernel prima di costruire l'infrastruttura necessaria. L'obiettivo è rendere possibile spostare progressivamente driver, filesystem e altri servizi in user space senza una riscrittura completa.

## Principio guida

> Implementare localmente oggi, progettare il contratto come se domani potesse attraversare un confine IPC.

Questo non significa serializzare ogni chiamata fin dall'inizio. Significa evitare dipendenze che renderebbero impossibile introdurre quel confine in futuro.

## Fasi evolutive

### Fase A — Kernel monolitico modulare

I sottosistemi vengono eseguiti nello spazio del kernel, ma restano separati:

```text
Kernel
├── core
├── mm
├── scheduler
├── IPC
├── VFS
├── filesystem
├── driver
└── networking
```

Caratteristiche:

- chiamate dirette tra moduli;
- un solo spazio privilegiato;
- debug semplice;
- iterazione rapida;
- contratti già compatibili con handle e messaggi.

### Fase B — Kernel ibrido

I servizi selezionati possono essere eseguiti localmente oppure tramite un proxy IPC:

```text
Client kernel/user
       |
       v
Interfaccia del servizio
       |
       +--> backend locale nel kernel
       |
       +--> proxy IPC verso server user space
```

La scelta del backend non deve cambiare la semantica pubblica del servizio.

### Fase C — Servizi user space

Filesystem, driver o network stack possono diventare server isolati:

```text
Applicazione
    |
    v
Filesystem server
    |
    v
Block-device server
    |
    v
Microkernel core / kernel ibrido
```

Il kernel mantiene i meccanismi essenziali:

- scheduling;
- address space;
- IPC;
- gestione delle capability o degli handle;
- consegna degli interrupt;
- mapping e condivisione controllata della memoria;
- lifecycle dei task.

## Confini obbligatori fin dall'inizio

### Strutture private

Ogni sottosistema possiede le proprie strutture interne. Gli altri moduli non possono accedere direttamente ai campi privati.

```c
struct vfs_node;
struct process;
struct address_space;
struct block_device;
```

Gli oggetti vengono esposti come tipi opachi negli header pubblici.

### Handle invece di puntatori trasferibili

Le API che potrebbero attraversare un futuro confine di processo non devono dipendere da puntatori interni.

```c
typedef struct {
    uint64_t value;
} object_handle_t;
```

Un puntatore può essere usato internamente al backend locale, ma non deve diventare parte del protocollo pubblico.

### Ownership esplicita

Per ogni risorsa devono essere documentati:

- proprietario;
- lifetime;
- operazione di rilascio;
- comportamento in caso di crash;
- possibilità di trasferimento o condivisione;
- regole di concorrenza.

### Errori serializzabili

Gli errori pubblici devono essere valori stabili, non indirizzi, stringhe statiche o codici dipendenti dal backend.

```c
enum service_status {
    SERVICE_OK,
    SERVICE_INVALID_ARGUMENT,
    SERVICE_NOT_FOUND,
    SERVICE_PERMISSION_DENIED,
    SERVICE_UNAVAILABLE,
    SERVICE_IO_ERROR,
};
```

### Buffer descritti esplicitamente

Evitare API che assumono memoria condivisa implicita. Un buffer pubblico deve avere indirizzo, dimensione, direzione e ownership definiti.

Per IPC, i trasferimenti potranno usare:

- copia per messaggi piccoli;
- pagine condivise per dati grandi;
- trasferimento temporaneo di ownership;
- mapping read-only quando sufficiente.

## Regole per le API

Un'interfaccia candidata a diventare remota deve:

- usare tipi a dimensione stabile;
- non contenere puntatori interni;
- non dipendere dal layout delle strutture C private;
- definire timeout e cancellazione quando applicabili;
- descrivere gli errori parziali;
- evitare callback arbitrarie attraverso il confine;
- rendere esplicita la sincronia della chiamata;
- poter essere testata con un backend fake host-side.

## IPC come fondazione, non come dettaglio tardivo

L'IPC viene introdotto prima del filesystem persistente e prima di spostare driver in user space.

Primitive iniziali candidate:

```text
endpoint_create
send
receive
call
reply
notify
handle_transfer
shared_memory_create
shared_memory_map
```

La prima implementazione può essere semplice e sincrona. Ottimizzazioni come fast path, zero-copy e priority inheritance vengono introdotte solo dopo aver misurato i colli di bottiglia.

## Scheduler e IPC

Lo scheduler deve conoscere gli stati di attesa IPC senza conoscere il protocollo dei servizi.

Stati possibili:

```text
READY
RUNNING
BLOCKED_IPC_SEND
BLOCKED_IPC_RECEIVE
BLOCKED_REPLY
BLOCKED_EVENT
TERMINATED
```

Devono essere studiati e testati:

- deadlock;
- starvation;
- inversione di priorità;
- cancellazione di una richiesta;
- morte del server mentre esiste un client bloccato.

## VFS e filesystem

Il VFS definisce semantica e namespace. Il filesystem è un backend.

Prima fase:

```text
VFS -> backend initramfs nel kernel
```

Fase ibrida:

```text
VFS -> adapter locale
VFS -> proxy IPC filesystem server
```

Le API pubbliche devono usare handle di file, offset e buffer espliciti. Non devono restituire puntatori a vnode interni.

## Driver

I driver iniziali possono restare nel kernel, ma devono implementare interfacce orientate alle capacità:

- block device;
- character device;
- network device;
- clock source;
- interrupt source;
- framebuffer.

Un futuro driver server riceverà accesso soltanto alle risorse necessarie:

- regioni MMIO;
- porte IO, quando applicabile;
- interrupt assegnati;
- canali DMA controllati;
- endpoint IPC.

## Service manager

Quando compaiono i primi server user space servirà un service manager responsabile di:

- bootstrap dei servizi;
- registrazione dei nomi;
- distribuzione degli handle;
- dipendenze tra servizi;
- restart;
- health state;
- policy di recovery.

Il service discovery non deve essere integrato nel microkernel core.

## Criteri per spostare un modulo in user space

Un sottosistema è candidato quando:

- la sua interfaccia è stabile;
- può essere rappresentato tramite messaggi e handle;
- i buffer hanno ownership chiara;
- esistono test del backend locale;
- esistono test del proxy IPC;
- il costo dei context switch è accettabile;
- l'isolamento offre un beneficio concreto;
- è definito il comportamento in caso di crash del server.

## Ordine consigliato di estrazione

1. servizio di logging non critico;
2. filesystem initramfs o tmpfs sperimentale;
3. driver semplice e non essenziale;
4. block service;
5. filesystem persistente;
6. network stack;
7. driver più complessi.

PMM, VMM di basso livello, scheduler, interrupt core e primitive IPC rimangono inizialmente nel kernel.

## Test della capacità di refactoring

Per ogni servizio candidato devono esistere due implementazioni:

```text
backend_local
backend_ipc_proxy
```

La stessa suite di test contrattuale deve essere eseguibile contro entrambi.

Checklist:

- [ ] Nessun client include header privati del backend.
- [ ] Nessun protocollo contiene puntatori.
- [ ] Gli errori sono identici tra backend locale e remoto.
- [ ] Il backend remoto gestisce la morte del server.
- [ ] I test verificano messaggi malformati.
- [ ] I limiti delle dimensioni sono controllati.
- [ ] Ownership e rilascio degli handle sono verificati.
- [ ] Il passaggio locale/remoto non cambia la semantica osservabile.

## Non obiettivi iniziali

Non implementeremo subito:

- un capability system completo e formale;
- IPC zero-copy generalizzato;
- driver interamente in user space;
- service restart trasparente;
- distributed system semantics;
- compatibilità POSIX completa;
- separazione di ogni singolo sottosistema.

La priorità resta costruire un sistema funzionante e osservabile, preservando una direzione evolutiva reale.