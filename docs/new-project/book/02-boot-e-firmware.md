# Capitolo 2 — Dal firmware al primo byte del kernel

> "Prima di esistere un sistema operativo deve esistere un modo per caricarlo."

## Obiettivi

In questo capitolo comprenderemo l'intera catena di bootstrap di un PC moderno: dall'accensione della macchina fino all'ingresso nella funzione `kernel_main()`. Non implementeremo ancora il boot, ma costruiremo il modello mentale necessario per progettare correttamente il nostro kernel.

## Domanda iniziale

Quando premiamo il pulsante di accensione, come fa il processore a sapere dove si trova il nostro kernel?

La risposta è: **non lo sa**. Esiste una sequenza di software che prepara l'ambiente e passa il controllo al kernel.

## Il percorso di avvio

```text
Power On
   │
   ▼
Firmware (UEFI)
   │
   ▼
Boot Manager
   │
   ▼
Limine
   │
   ▼
Boot Adapter
   │
   ▼
boot_info
   │
   ▼
kernel_main()
```

## 💡 Idea

Il kernel non dovrebbe conoscere il bootloader. Deve conoscere soltanto una struttura dati stabile (`boot_info`) costruita da un adapter.

## 📜 Contesto storico

I primi PC usavano il BIOS, progettato in un'epoca di CPU a 16 bit. Oggi useremo UEFI perché offre servizi moderni, supporto a sistemi a 64 bit e una gestione più ricca della memoria disponibile al boot.

## 🧠 Pensare come il kernel

Perché non includere direttamente `limine.h` in tutto il kernel?

Perché il bootloader è un dettaglio di implementazione. Se domani decidessimo di usare un altro bootloader, dovremmo modificare un solo componente: il boot adapter.

## API previste

```c
struct boot_info;

[[nodiscard]]
const struct boot_info *boot_get_info(void);

void boot_early_init(void);
```

Queste API rappresentano il contratto tra il codice di bootstrap e il resto del kernel.

## 🛠️ Implementazione

Il primo punto d'ingresso del nostro kernel sarà concettualmente:

```c
void kernel_main(const struct boot_info *boot);
```

Nessun altro modulo dovrà conoscere come tali informazioni sono state ottenute.

## 🔬 Esperimento

Durante i primi video utilizzeremo QEMU e GDB per osservare:

- il passaggio dal firmware al bootloader;
- il caricamento dell'immagine ELF;
- il salto al punto di ingresso del kernel.

## Errori comuni

- Mescolare il codice del bootloader con quello del kernel.
- Esporre tipi specifici di Limine nelle API pubbliche.
- Assumere che i servizi UEFI siano disponibili dopo il bootstrap.

## Evoluzione futura

In seguito il boot adapter potrà essere sostituito senza modificare PMM, VMM, scheduler o altri sottosistemi. Questo è il primo esempio concreto di separazione tra dipendenze esterne e API interne.

## Cosa abbiamo costruito

- Un modello mentale del bootstrap.
- La distinzione tra firmware, bootloader e kernel.
- La motivazione del boot adapter.
- Il primo contratto API del progetto.

## Collegamenti

- Milestone M1.
- Capitolo successivo: Architettura x86_64.
