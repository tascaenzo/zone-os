# Visione e obiettivi

## Dichiarazione del progetto

Il progetto è un piccolo sistema operativo educativo per x86_64, sviluppato pubblicamente attraverso una serie YouTube strutturata in italiano.

Non vuole competere con Linux, diventare un sistema operativo di uso quotidiano o supportare molte architetture nelle prime fasi. Il suo scopo è mostrare come un sistema operativo moderno cresca da un ambiente di boot controllato fino a un kernel capace di eseguire programmi utente isolati.

## Obiettivi principali

### Obiettivi tecnici

- Avviare il sistema in modo affidabile tramite Limine su UEFI.
- Offrire diagnostica robusta fin dalla prima milestone.
- Implementare gestione della memoria fisica e virtuale.
- Gestire correttamente eccezioni CPU e interrupt hardware.
- Supportare thread kernel e scheduling preemptive.
- Entrare nella modalità utente x86_64.
- Definire una piccola ABI per le chiamate di sistema.
- Caricare programmi ELF da un’initramfs.
- Introdurre una VFS semplice e un dispositivo console.

### Obiettivi didattici

- Spiegare ogni componente prima di implementarlo.
- Allineare commit e pull request alle singole lezioni.
- Fornire tag iniziale e finale per ogni episodio.
- Mostrare debugging e fallimenti, non solo il risultato funzionante.
- Documentare compromessi e alternative scartate.
- Rendere il repository utilizzabile anche senza guardare ogni video.

## Non-obiettivi del primo ciclo

Sono intenzionalmente rimandati:

- multiprocessore simmetrico;
- porting ARM o RISC-V;
- USB;
- rete;
- desktop grafico;
- driver storage avanzati;
- compatibilità POSIX;
- self-hosting;
- filesystem personalizzato;
- architettura strettamente microkernel.

## Definizione di successo

Il primo grande ciclo è completato quando il sistema può:

1. avviarsi in modo riproducibile in QEMU;
2. riportare i fallimenti tramite diagnostica seriale;
3. gestire in sicurezza memoria fisica e virtuale;
4. schedulare più thread kernel;
5. entrare in Ring 3;
6. caricare ed eseguire un programma ELF da initramfs;
7. permettere al programma di scrivere sulla console e terminare tramite syscall;
8. eseguire in CI test host-side e integrazione QEMU.

## Filosofia progettuale

Il sistema deve preferire implementazioni semplici, facili da ispezionare e verificare. Strutture dati e ottimizzazioni più sofisticate vengono introdotte solo quando un limite dimostrato le rende necessarie.

Una bitmap è preferita a un buddy allocator nella prima fase. Un heap semplice precede lo slab allocator. Lo scheduling single-core precede SMP. UEFI precede il supporto BIOS legacy.

Il progetto attribuisce più valore a un processo di sviluppo stabile che all’accumulo rapido di funzionalità.