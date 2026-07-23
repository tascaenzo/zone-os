# Libro del progetto — Sistemi operativi dalla teoria al kernel

## Scopo

Questa sezione trasforma la documentazione tecnica del progetto in un percorso editoriale simile a un manuale universitario di sistemi operativi, ma collegato direttamente a un kernel reale e progressivamente eseguibile.

Il libro non sostituisce roadmap, ADR e specifiche API. Le organizza in una sequenza didattica composta da:

1. teoria generale dei sistemi operativi;
2. meccanismi indipendenti dall'architettura;
3. implementazione iniziale x86_64;
4. API pubbliche del kernel;
5. sorgenti commentati;
6. esperimenti e casi di fallimento;
7. collegamenti alle milestone e agli episodi video.

## Struttura editoriale

Ogni capitolo deve contenere:

- obiettivi di apprendimento;
- prerequisiti;
- modello concettuale;
- distinzione tra meccanismo e policy;
- astrazione generica;
- implementazione x86_64;
- API esposte dal kernel;
- sorgenti commentati;
- invarianti e ownership;
- errori comuni;
- test ed esperimenti;
- domande di riepilogo;
- esercizi;
- collegamento alla milestone;
- sequenza video associata.

## Parti del libro

### Parte I — Fondamenti

- Che cos'è un sistema operativo
- Kernel, user space e privilegi
- C23 freestanding
- Toolchain, ELF e linking
- Architettura dei calcolatori
- Firmware, bootloader e bootstrap

### Parte II — Nucleo del kernel

- Diagnostica, logging e panic
- Eccezioni e contesto di esecuzione
- Memoria fisica
- Memoria virtuale
- Heap del kernel
- Interrupt e tempo

### Parte III — Esecuzione e isolamento

- Thread
- Scheduler
- Processi
- User mode
- Syscall ABI
- Handle e ownership
- IPC e memoria condivisa

### Parte IV — Servizi di sistema

- Initramfs
- ELF64 e caricamento dei programmi
- VFS
- File descriptor
- Driver e device model
- Kernel ibrido evolutivo
- Primo servizio user space

### Parte V — Evoluzione

- Sincronizzazione
- SMP
- Storage persistente
- Networking
- Sicurezza e capability
- Porting verso una seconda architettura

## Relazione con gli altri documenti

- `../MILESTONES_DETAILED.md`: definisce cosa deve essere completato.
- `../API_ROADMAP.md`: elenca le firme previste delle API.
- `../STUDY_PLAN.md`: definisce cosa studiare.
- `../YOUTUBE_SERIES.md`: descrive la serie ad alto livello.
- `VIDEO_CURRICULUM.md`: collega capitoli, esperimenti e video.
- `ANNOTATED_SOURCE_GUIDE.md`: definisce come presentare i sorgenti commentati.
- `KERNEL_API_REFERENCE_STYLE.md`: definisce il formato della documentazione API.
- `CHAPTER_TEMPLATE.md`: modello obbligatorio per ogni nuovo capitolo.

## Regola editoriale principale

> Ogni concetto teorico deve terminare in un artefatto osservabile: un'API, una struttura dati, un esperimento, un test o un comportamento del kernel.

Il lettore deve poter studiare il capitolo senza guardare il video, mentre il video deve poter usare il capitolo come scaletta e materiale di approfondimento.