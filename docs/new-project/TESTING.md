# Strategia di test

## Obiettivi

I test devono rendere più sicuro lo sviluppo low-level senza fingere che i test host-side sostituiscano il comportamento reale dell’hardware.

Il progetto usa più livelli complementari:

1. unit test eseguiti sull’host;
2. self-test del kernel in QEMU;
3. test di boot e integrazione;
4. assert e controlli d’integrità nelle build debug;
5. validazione manuale con debugger per il comportamento specifico dell’architettura.

## Test host-side

La logica pura deve essere progettata, quando possibile, per essere compilata ed eseguita come normale programma dell’host.

Candidati adatti:

- bitmap;
- liste intrusive;
- ring buffer;
- funzioni stringa e memoria;
- formattazione;
- validazione ELF;
- parsing TAR;
- risoluzione dei percorsi VFS;
- logica dei metadati degli allocator;
- operazioni sulle code dello scheduler.

I test host non devono dipendere silenziosamente da comportamenti assenti nel kernel freestanding. Il codice condiviso deve usare confini di compatibilità piccoli ed espliciti.

## Self-test del kernel

I test kernel vengono eseguiti in QEMU e validano codice dipendente da stato CPU, page table o interrupt.

Suite iniziali:

```text
boot
exceptions
pmm
vmm
heap
interrupts
scheduler
userspace
syscalls
elf
```

I test devono essere selezionabili:

```bash
./tools/dev test pmm
./tools/dev test vmm
./tools/dev test all
```

## Protocollo di uscita da QEMU

Il kernel di test deve terminare QEMU tramite un dispositivo debug-exit deterministico o un altro meccanismo documentato. La CI deve ricevere uno stato significativo di successo o errore, non limitarsi a interpretare l’output testuale.

La seriale resta disponibile per la diagnostica:

```text
[test] pmm.allocate_single_page ... PASS
[test] pmm.reject_double_free ... PASS
[test] pmm.reserve_kernel_range ... PASS
[summary] 3 superati, 0 falliti
```

## Test con fault intenzionali

Devono essere verificati deliberatamente:

- divisione per zero;
- opcode non valido;
- general-protection fault;
- page fault su pagina non presente;
- page fault per protezione in scrittura;
- accesso utente a memoria supervisor.

Un test passa solo quando vengono osservati vettore, codice d’errore e comportamento diagnostico corretti.

## Test della memoria

I test dei memory manager devono verificare più della semplice allocazione riuscita:

- comportamento a memoria esaurita;
- allineamento;
- protezione delle regioni riservate;
- rilevamento double-free;
- rifiuto di mapping sovrapposti;
- politica di sostituzione dei mapping;
- applicazione dei permessi;
- aggiornamenti visibili alla TLB;
- pulizia dopo fallimenti parziali.

## Strumentazione debug

Le build debug possono abilitare:

- poisoning delle allocazioni;
- red zone;
- canary;
- controlli d’integrità delle liste;
- verifica delle page table;
- controlli di proprietà dei lock;
- report panic dettagliati.

Questi controlli devono essere protetti da opzioni di build e non devono cambiare l’API esterna prevista.

## Integrazione continua

La pipeline CI iniziale comprende:

```text
controlli di formato o stile
configurazione Meson
build debug Clang
build release Clang
unit test host
smoke test di boot QEMU
test kernel selezionati
```

Una build di compatibilità GCC può essere aggiunta dopo la stabilizzazione della toolchain Clang.

## Determinismo

I test QEMU automatici devono usare configurazione macchina, quantità di RAM e modello CPU espliciti e stabili quando possibile. I test devono evitare assunzioni sul tempo reale, salvo quando verificano specificamente il timekeeping.

## Proprietà dei test

Ogni milestone definisce i propri test di completamento. Una PR che corregge un bug deve aggiungere un test di regressione quando il fallimento è riproducibile in modo deterministico.