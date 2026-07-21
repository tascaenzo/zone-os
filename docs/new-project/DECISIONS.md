# Decisioni architetturali

Questo documento raccoglie le decisioni iniziali del progetto. Con la crescita del sistema, ogni decisione importante potrà essere spostata in un file dedicato sotto `docs/adr/`.

## ADR-001 — Avviare un nuovo progetto di sistema operativo

**Stato:** Accettata

**Decisione:**

Il nuovo progetto non è una continuazione né una riscrittura nello stesso repository di Zone OS. Zone OS resta disponibile come testimonianza dell’esperienza precedente. Il nuovo sistema parte da un repository vuoto e da vincoli documentati da zero.

**Motivazioni:**

- evita di ereditare complessità accidentale;
- permette di progettare fin dall’inizio build e percorso didattico;
- conserva Zone OS come riferimento storico;
- evita requisiti di compatibilità con interfacce incomplete.

## ADR-002 — Supportare inizialmente solo x86_64

**Stato:** Accettata

**Decisione:**

Il kernel iniziale supporta solo x86_64 in modalità a 64 bit.

**Motivazioni:**

Le astrazioni architetturali sono utili solo dopo aver compreso i requisiti reali di almeno un’implementazione. Interfacce multiarchitettura premature possono nascondere il comportamento hardware e complicare il percorso didattico.

## ADR-003 — Usare un kernel monolitico modulare

**Stato:** Accettata

**Decisione:**

Il primo sistema usa un unico spazio di indirizzamento kernel con confini modulari interni.

**Motivazioni:**

Fornisce un percorso diretto verso memoria, scheduling e user space senza introdurre IPC e isolamento dei servizi prima che le fondamenta siano stabili. Un futuro esperimento microkernel resta possibile, ma non è un vincolo iniziale.

## ADR-004 — Usare Limine e iniziare da UEFI

**Stato:** Accettata

**Decisione:**

Limine fornisce protocollo di boot e ambiente di caricamento. Il primo percorso firmware supportato è UEFI.

**Motivazioni:**

- evita di scrivere un bootloader prima del kernel;
- offre un ambiente moderno e documentato;
- riduce la complessità iniziale di generazione delle immagini;
- consente di trattare BIOS legacy come argomento separato.

## ADR-005 — Usare ISO C23 in modalità freestanding

**Stato:** Accettata

**Decisione:**

Il codice kernel usa `-std=c23` e `-ffreestanding`, con Clang come compilatore principale.

**Motivazioni:**

C23 offre attributi più chiari, verifiche statiche e strumenti moderni mantenendo una relazione stretta con il codice macchina generato. Le estensioni GNU vengono isolate e usate solo quando necessarie.

## ADR-006 — Usare Meson e Ninja

**Stato:** Accettata

**Decisione:**

Meson descrive la build e Ninja la esegue incrementalmente.

**Motivazioni:**

Il progetto precedente dipendeva da script shell, scansioni ripetute dei sorgenti e ricompilazioni pulite. Il nuovo sistema richiede dipendenze esplicite, build incrementali rapide, profili multipli e un flusso spiegabile nei video.

## ADR-007 — Rendere Docker opzionale nello sviluppo

**Stato:** Accettata

**Decisione:**

Gli strumenti nativi sono il percorso di sviluppo preferito. Un’immagine container fissata garantisce riproducibilità in CI e sviluppo isolato opzionale.

**Motivazioni:**

Ricostruire o invocare un container x86_64 emulato a ogni modifica introduce latenza inutile, soprattutto su host ARM. La riproducibilità resta importante, ma non deve penalizzare il ciclo modifica-build-run.

## ADR-008 — Usare la seriale prima del framebuffer

**Stato:** Accettata

**Decisione:**

La console seriale è il primo e principale canale diagnostico.

**Motivazioni:**

È semplice, deterministica, catturabile dalla CI e disponibile prima dell’inizializzazione grafica. Il framebuffer verrà introdotto successivamente come driver e livello di presentazione.

## ADR-009 — Costruire la diagnostica prima della memoria avanzata

**Stato:** Accettata

**Decisione:**

Panic, eccezioni, dump dei registri e integrazione con il debugger precedono paging personalizzato e allocazione dinamica.

**Motivazioni:**

I sottosistemi complessi sono difficili da sviluppare senza informazioni affidabili sui fallimenti. La diagnostica è infrastruttura, non rifinitura.

## ADR-010 — Preferire inizialmente allocator semplici

**Stato:** Accettata

**Decisione:**

L’allocator fisico parte da una bitmap. L’heap parte da un bump allocator iniziale seguito da una free list sostenuta da pagine.

**Motivazioni:**

Buddy e slab sono utili, ma introducono metadati e invarianti prima che il progetto ne dimostri la necessità. Potranno essere aggiunti in seguito con misure e test.

## ADR-011 — Integrare i test in ogni milestone

**Stato:** Accettata

**Decisione:**

Ogni milestone definisce criteri osservabili di completamento e test automatici quando praticabile.

**Motivazioni:**

Il repository deve sostenere manutenzione e didattica. Test riproducibili aiutano a distinguere errori di implementazione da problemi ambientali e impediscono agli episodi successivi di rompere silenziosamente quelli precedenti.

## ADR-012 — Allineare la storia Git alla serie video

**Stato:** Accettata

**Decisione:**

Ogni episodio possiede issue, branch focalizzato, note, tag iniziale e tag finale.

**Motivazioni:**

Chi segue deve poter riprodurre lo stato iniziale esatto, seguire l’implementazione e confrontare il risultato senza interpretare modifiche successive non correlate.

## Processo ADR

È richiesta una nuova ADR quando una decisione:

- coinvolge più sottosistemi;
- modifica un’interfaccia pubblica o un’ABI;
- modifica build o toolchain;
- cambia significativamente la roadmap;
- introduce una dipendenza difficile da invertire;
- cambia l’ordine didattico.

Ogni ADR deve contenere contesto, decisione, conseguenze e alternative scartate.