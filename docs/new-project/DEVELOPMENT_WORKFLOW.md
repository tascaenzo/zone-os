# Flusso di sviluppo

## Strategia del repository

Il nuovo sistema operativo dovrà vivere in un repository separato. Fino alla sua creazione, questi documenti restano su un branch dedicato di Zone OS.

Il branch principale deve rimanere rilasciabile e ogni funzionalità viene introdotta tramite una pull request focalizzata.

## Nomi dei branch

Prefissi consigliati:

```text
build/
boot/
arch/
mm/
sched/
user/
fs/
test/
docs/
episode/
```

Esempi:

```text
episode/03-build-incrementale
mm/allocazione-pagine-bitmap
arch/x86_64-page-fault-handler
```

## Politica dei commit

Ogni commit deve descrivere una singola modifica logica e usare un oggetto imperativo orientato al sottosistema:

```text
build: aggiungi il cross file Meson x86_64
boot: valida la risposta della memory map Limine
serial: aggiungi output polling su COM1
mm: riserva le pagine possedute dal bootloader
test: aggiungi il caso PMM double-free
docs: spiega il layout high-half
```

Evitare commit che mescolano formattazione, refactoring, nuovo comportamento e documentazione senza una ragione forte.

## Checklist di completamento delle pull request

Ogni PR di implementazione deve rispondere a queste domande:

- Quale comportamento osservabile cambia?
- Quale invariante o interfaccia viene introdotta?
- Come è stata testata?
- Quali casi di errore sono stati provati?
- La documentazione di progetto deve essere aggiornata?
- La modifica appartiene a un episodio YouTube?

Checklist consigliata:

```markdown
- [ ] Build debug completata
- [ ] Build release completata
- [ ] Test host completati
- [ ] Test QEMU completati
- [ ] Documentazione aggiornata
- [ ] Nessun artefatto generato non correlato incluso
- [ ] Note dell'episodio aggiornate, quando applicabile
```

## Flusso degli episodi

Ogni episodio usa un punto iniziale e finale riproducibile.

```text
Issue:   Episodio 03 — Build incrementale del kernel
Branch:  episode/03-build-incrementale
Tag:     ep03-start
Tag:     ep03-complete
Note:    docs/episodes/03-build-incrementale.md
```

Chi segue deve poter eseguire:

```bash
git checkout ep03-start
git diff ep03-start ep03-complete
```

Il video può mostrare errori e debugging; la cronologia finale deve comunque restare comprensibile.

## Definizione di completato

Una funzionalità è completa solo quando:

1. compila senza nuovi warning;
2. il comportamento atteso è osservabile;
3. i test rilevanti passano;
4. il comportamento in caso di errore è documentato;
5. le API pubbliche sono documentate;
6. il documento di design corrispondente è aggiornato;
7. il codice di debug temporaneo è rimosso o protetto da un’opzione esplicita.

## Principi di revisione

Le review devono concentrarsi su:

- ordine di inizializzazione;
- proprietà e durata degli oggetti;
- overflow interi;
- confusione tra indirizzi fisici e virtuali;
- sicurezza rispetto agli interrupt;
- rientranza;
- ordine dei lock;
- validazione dei puntatori utente;
- comportamento durante inizializzazioni parziali;
- testabilità e qualità della diagnostica.

## File generati

Artefatti di build, immagini disco, cache e toolchain scaricate non vengono versionati, salvo espliciti artefatti di release.

Il repository deve contenere checksum e strumenti per ricreare le dipendenze esterne, non copie binarie mutevoli prive di provenienza.

## Manutenzione della documentazione

La documentazione fa parte dell’implementazione. Le modifiche architetturali aggiornano il documento pertinente nella stessa PR.

Le decisioni locali possono essere commentate nel codice. Le decisioni che coinvolgono più moduli, ABI pubblica, build system o percorso didattico richiedono una Architecture Decision Record.