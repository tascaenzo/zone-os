# Nuovo progetto OS — Documentazione

Questa directory contiene i documenti iniziali di progettazione di un nuovo sistema operativo educativo costruito da zero dopo Zone OS.

Il progetto ha due obiettivi di pari importanza:

1. costruire un sistema operativo a 64 bit piccolo, comprensibile e verificabile in modo progressivo;
2. produrre una serie completa di video YouTube in italiano che spieghi ogni decisione tecnica importante.

La prima implementazione è destinata a x86_64, ma i meccanismi specifici dell’architettura devono rimanere dietro interfacce piccole ed esplicite. La portabilità viene trattata come disciplina progettuale, non come promessa immediata di più backend completi.

Il kernel segue una strategia **ibrida evolutiva**: parte come monolitico modulare per mantenere semplice lo sviluppo iniziale, ma separa fin dall'inizio strutture, ownership, API e protocolli in modo da poter spostare progressivamente filesystem, driver e altri servizi in user space.

## Documenti

### Direzione e architettura

- [Visione e obiettivi](VISION.md)
- [Architettura](ARCHITECTURE.md)
- [Strategia del kernel ibrido evolutivo](HYBRID_KERNEL_STRATEGY.md)
- [Strategia di portabilità e astrazione](PORTABILITY_AND_ABSTRACTION.md)
- [Decisioni architetturali](DECISIONS.md)

### Pianificazione e apprendimento

- [Roadmap e milestone](ROADMAP.md)
- [Obiettivi dettagliati e checklist delle milestone](MILESTONES_DETAILED.md)
- [Piano di studio per lo sviluppo OS](STUDY_PLAN.md)
- [Piano della serie YouTube](YOUTUBE_SERIES.md)

### Processo ingegneristico

- [Sistema di build](BUILD_SYSTEM.md)
- [Politica del linguaggio C23](C23_POLICY.md)
- [Flusso di sviluppo](DEVELOPMENT_WORKFLOW.md)
- [Strategia di test](TESTING.md)

## Principi operativi

- Correttezza prima delle funzionalità.
- Osservabilità prima della complessità.
- Un risultato verificabile per ogni milestone.
- Una modifica focalizzata per ogni pull request.
- Il repository deve restare comprensibile dal primo episodio all’ultimo.
- Ogni decisione importante deve essere documentata.
- La build deve essere incrementale, riproducibile e facile da spiegare.
- Il codice generico non deve manipolare direttamente registri o formati hardware x86_64.
- Le astrazioni devono derivare da requisiti reali, non da portabilità speculativa.
- Ogni milestone include un percorso di studio, test di successo e test deliberati di errore.
- Le strutture interne dei sottosistemi sono private e vengono esposte tramite tipi opachi.
- Le API candidate a diventare remote usano handle, errori stabili e buffer con ownership esplicita.
- Un servizio deve poter avere un backend locale e, in seguito, un proxy IPC con la stessa semantica.

## Direzione tecnica iniziale

- Architettura: x86_64, esclusivamente modalità a 64 bit.
- Strategia: contratti kernel generici con backend iniziale x86_64.
- Protocollo di boot: adapter Limine che produce informazioni di boot possedute dal kernel.
- Firmware iniziale: UEFI.
- Design del kernel: ibrido evolutivo, inizialmente monolitico modulare.
- IPC: introdotto prima del filesystem persistente e dell'estrazione dei driver.
- Linguaggio: ISO C23 freestanding.
- Compilatore principale: Clang.
- Linker: LLD.
- Build system: Meson e Ninja.
- Emulatore: QEMU.
- Debugger: GDB.
- Diagnostica primaria: console seriale.
- Container: opzionale nello sviluppo, richiesto come ambiente riproducibile di riferimento in CI.

## Sequenza iniziale delle milestone

1. Workspace a 64 bit riproducibile.
2. Boot UEFI controllato.
3. Diagnostica ed eccezioni CPU.
4. Proprietà della memoria fisica.
5. Spazi di indirizzamento virtuale.
6. Memoria dinamica del kernel.
7. Consegna degli interrupt e gestione del tempo.
8. Thread kernel e scheduling.
9. Dominio di esecuzione utente.
10. IPC, handle e condivisione controllata della memoria.
11. Confine delle chiamate di sistema.
12. Initramfs e caricamento di programmi ELF64.
13. VFS minimale con backend locale sostituibile.
14. Primo servizio user space sperimentale.

Ogni milestone possiede deliverable, checklist di verifica e criteri d’uscita in [MILESTONES_DETAILED.md](MILESTONES_DETAILED.md). Il percorso di conoscenze associato è definito in [STUDY_PLAN.md](STUDY_PLAN.md).

Questa documentazione è conservata temporaneamente in Zone OS. Il progetto definitivo dovrà vivere in un repository separato dopo la scelta del nome e la creazione del repository.