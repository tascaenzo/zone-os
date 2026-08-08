# Politica del linguaggio C23

## Standard

Il kernel è scritto in ISO C23 e compilato in modalità freestanding.

Modalità principale del compilatore:

```text
-std=c23 -ffreestanding
```

Le modalità GNU come `gnu23` non sono il default. Le estensioni del compilatore possono essere usate solo quando risolvono un requisito concreto di basso livello e devono essere nascoste dietro macro del progetto o interfacce specifiche dell'architettura.

## Politica del compilatore

- Clang è il compilatore principale.
- LLD è il linker principale.
- Una build secondaria con GCC deve essere aggiunta alla CI quando la toolchain iniziale è stabile.
- La versione minima supportata del compilatore deve essere documentata e fissata nella CI.

## Funzionalità C23 utili

Il progetto può usare funzionalità moderne quando migliorano chiarezza o verifica statica:

```c
static_assert(sizeof(struct interrupt_frame) == EXPECTED_SIZE);

[[noreturn]]
void panic(const char *message);

[[nodiscard]]
pmm_status_t pmm_alloc_page(paddr_t *out_page);

struct page *page = nullptr;
```

I tipi sottostanti fissi per le enumerazioni possono essere usati per flag hardware e valori ABI quando il supporto del compilatore è stato verificato.

## Ambiente freestanding

Selezionare C23 non fornisce una libreria C hosted. Il progetto può usare gli header freestanding forniti dal compilatore, quando supportati, tra cui:

- `<stddef.h>`
- `<stdint.h>`
- `<stdbool.h>`
- `<stdarg.h>`
- `<stdalign.h>`
- `<limits.h>`

Il kernel implementa le funzioni runtime che gli servono, come:

- `memcpy`
- `memmove`
- `memset`
- `memcmp`
- `strlen`
- primitive di formattazione e logging

Nessun sorgente del kernel deve dipendere accidentalmente dalla libc dell'host.

## Politica dei tipi

Preferire tipi standard a larghezza esplicita:

```c
uint8_t
uint16_t
uint32_t
uint64_t
uintptr_t
size_t
```

Sono incoraggiati alias semantici specifici del progetto quando prevengono confusione tra spazi di indirizzamento:

```c
typedef uintptr_t paddr_t;
typedef uintptr_t vaddr_t;
```

Evitare alias globali ambigui come `u8`, `u32` e `ulong` nelle API rivolte alla didattica.

## Astrazione del compilatore

La sintassi specifica del compilatore deve risiedere in un piccolo header, per esempio:

```c
#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define K_PACKED __attribute__((packed))
#define K_ALIGNED(value) __attribute__((aligned(value)))
#define K_SECTION(name) __attribute__((section(name)))
#else
#error "Compilatore non supportato"
#endif
```

Gli header dell'architettura possono usare questi wrapper, ma non devono duplicare attributi grezzi in tutto l'albero dei sorgenti.

## Politica dei warning

Le build debug e CI devono partire da un insieme rigoroso di warning:

```text
-Wall
-Wextra
-Wpedantic
-Werror
-Wconversion
-Wshadow
-Wundef
-Wmissing-prototypes
-Wstrict-prototypes
```

I warning non adatti a uno specifico file low-level devono essere disabilitati nel punto più ristretto possibile, con un commento che ne spieghi il motivo. Le soppressioni globali sono sconsigliate.

## Principi di stile

- Funzioni e variabili usano `snake_case`.
- I tipi usano nomi descrittivi con suffisso `_t` solo per typedef del progetto, quando appropriato.
- Costanti e macro usano `UPPER_SNAKE_CASE`.
- Gli header pubblici espongono API minime.
- Le funzioni restituiscono valori di stato tipizzati per gli errori recuperabili.
- I parametri di output vengono validati.
- L'aritmetica dei puntatori è isolata e commentata.
- I cast interi devono rendere esplicita la troncatura o la reinterpretazione.

## Confine con l'assembly

L'assembly viene mantenuto in file `.S` separati quando possibile. L'inline assembly è riservato a piccole primitive CPU i cui constraint siano stati revisionati con attenzione.

Ogni interfaccia assembly deve documentare:

- input e output;
- registri clobbered;
- layout dello stack;
- calling convention;
- requisiti di allineamento.

## Regola di adozione delle funzionalità

Il progetto usa C23 per migliorare correttezza e leggibilità, non per massimizzare la novità. Una funzionalità del linguaggio viene adottata solo quando:

1. è supportata dal compilatore principale fissato;
2. offre un beneficio chiaro;
3. può essere spiegata al pubblico;
4. non nasconde il comportamento a livello macchina rilevante per la lezione.