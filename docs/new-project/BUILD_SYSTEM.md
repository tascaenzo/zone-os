# Sistema di build

## Obiettivi

La build deve essere abbastanza veloce per lo sviluppo quotidiano, abbastanza semplice da spiegare in un video e abbastanza riproducibile per la CI.

Il sistema di build deve:

- compilare in modo incrementale;
- tracciare correttamente le dipendenze dagli header;
- supportare la compilazione parallela;
- separare sorgenti e file generati;
- offrire configurazioni debug, release e test;
- mantenere separata la generazione dell'immagine dalla compilazione del kernel;
- evitare di richiedere Docker a ogni modifica;
- fissare le versioni degli strumenti esterni quando pratico.

## Strumenti scelti

- Meson: configurazione del progetto e grafo delle dipendenze.
- Ninja: esecuzione incrementale della build.
- Clang: compilatore C23 principale.
- LLD: linker principale.
- Python: piccoli strumenti portabili di orchestrazione.
- QEMU: emulazione e test di integrazione.
- GDB: debugging a livello sorgente.
- Limine: protocollo di boot e bootloader.

## Fasi della build

```text
Sorgenti C e assembly
        |
        v
File oggetto
        |
        v
kernel.elf
        |
        +----> simboli di debug e map file
        |
        v
albero di boot preparato
        |
        v
immagine UEFI
        |
        v
QEMU
```

Ogni fase deve avere input e output espliciti. Una fase viene eseguita solo quando cambiano i suoi input.

## Comandi previsti

L'interfaccia pubblica deve restare piccola:

```bash
./tools/dev setup
./tools/dev build
./tools/dev run
./tools/dev debug
./tools/dev test
./tools/dev clean
```

`tools/dev` è soltanto un dispatcher sottile. Non deve reimplementare la logica delle dipendenze del compilatore.

I comandi di livello inferiore restano documentati:

```bash
meson setup build/debug --cross-file config/x86_64.ini --buildtype=debug
meson compile -C build/debug
```

## Profili di build

### Debug

- informazioni di debug;
- frame pointer;
- assertion;
- logging dettagliato;
- controlli di integrità degli allocator;
- ottimizzazione minima.

### Release

- kernel ottimizzato;
- logging ridotto;
- simboli separati o conservati per il debugging post-mortem;
- nessuna dipendenza di correttezza da assertion disabilitate.

### Test

- registro dei test abilitato;
- configurazione QEMU deterministica;
- codice di uscita leggibile dalla macchina;
- target opzionali per fault injection.

## Politica di generazione dell'immagine

La compilazione del kernel e la costruzione dell'immagine di boot sono target separati. Ricompilare un sorgente non deve ripartizionare o riformattare un'immagine, salvo che `kernel.elf` sia realmente cambiato.

La prima release supporta solo UEFI. Il BIOS legacy è una funzionalità successiva e isolata.

L'albero di boot preparato deve essere simile a:

```text
build/debug/sysroot/
├── boot/
│   ├── kernel.elf
│   └── limine.conf
└── EFI/
    └── BOOT/
        └── BOOTX64.EFI
```

## Gestione delle dipendenze

Gli artefatti esterni del bootloader devono essere fissati a una versione nota. I download devono essere verificati con checksum. Il repository non deve dipendere da file mutabili installati in percorsi come `/opt/limine`.

Le dipendenze host vengono controllate da `tools/dev setup`. Il comando segnala gli strumenti mancanti e non installa silenziosamente pacchetti di sistema.

## Politica dei container

Lo sviluppo locale usa la toolchain nativa per velocità. Un'immagine container fornisce un ambiente di riferimento riproducibile per CI e per chi preferisce l'isolamento.

Il container non deve essere ricostruito a ogni modifica dei sorgenti. Le directory del progetto possono essere montate in un'immagine di sviluppo già costruita.

## Aspettative prestazionali

Dopo la configurazione iniziale:

- una build senza modifiche deve terminare quasi subito;
- modificare un file C deve compilare un solo oggetto e rilinkare;
- le modifiche alla documentazione non devono ricostruire il kernel;
- l'avvio di QEMU non deve forzare una clean build;
- la selezione dei test deve evitare suite non correlate.

## Osservabilità della build

La build deve essere ispezionabile con gli strumenti standard di Meson e Ninja. Gli script personalizzati devono stampare il comando esatto fallito e restituirne invariato il codice di uscita.