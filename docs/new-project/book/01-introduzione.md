# Capitolo 1 — Che cos'è un sistema operativo

## Obiettivi

Alla fine di questo capitolo sarai in grado di:

- definire cosa è un sistema operativo;
- distinguere tra kernel e user space;
- comprendere il ruolo del kernel come intermediario tra hardware e software;
- capire perché esistono diversi tipi di kernel;
- collegare la teoria al nostro progetto concreto.

---

## 1. Il problema fondamentale

Un computer, senza sistema operativo, è solo hardware.

CPU, RAM, dischi, periferiche: tutto è presente, ma nulla è coordinato.

Per esempio:

```text
Applicazione
   │
   ▼
???
   │
   ▼
Hardware
```

Domande fondamentali:

- Come accede un programma alla memoria?
- Come scrive su disco?
- Come usa la CPU?
- Come interagisce con altre applicazioni?

Senza un sistema operativo, ogni programma dovrebbe:

- conoscere l'hardware;
- gestire direttamente periferiche;
- evitare conflitti con altri programmi;
- implementare il proprio scheduler;
- gestire la memoria.

Questo è impraticabile.

---

## 2. Definizione di sistema operativo

Un sistema operativo è uno strato software che:

- astrae l'hardware;
- gestisce le risorse;
- fornisce servizi ai programmi;
- impone regole di sicurezza e isolamento.

Possiamo rappresentarlo così:

```text
Applicazioni (user space)
        │
        ▼
Sistema operativo
        │
        ▼
Hardware
```

---

## 3. Il kernel

Il kernel è la parte centrale del sistema operativo.

È l'unico componente che:

- gira con privilegi elevati;
- può accedere direttamente all'hardware;
- controlla memoria, CPU e dispositivi.

Nel nostro progetto:

```text
User space
    │
    ▼
Kernel
    │
    ▼
Hardware
```

Il kernel è responsabile di:

- scheduling (chi usa la CPU);
- gestione memoria;
- gestione processi;
- I/O;
- interrupt;
- comunicazione tra programmi.

---

## 4. Kernel vs user space

Una distinzione fondamentale:

```text
User space (applicazioni)
Kernel space (privilegiato)
```

Il kernel:

- è protetto;
- non può essere modificato direttamente dalle applicazioni;
- espone API controllate.

Le applicazioni comunicano con il kernel tramite:

```text
syscall
```

Esempio:

```text
Applicazione → write() → kernel → disco
```

---

## 5. Tipi di kernel

### Kernel monolitico

Tutto gira nel kernel:

```text
Kernel
├── scheduler
├── memoria
├── filesystem
├── driver
```

Vantaggi:

- veloce;
- semplice da implementare.

Svantaggi:

- meno isolamento;
- bug critici.

---

### Microkernel

Il kernel è minimale:

```text
Kernel
├── scheduling
├── IPC
└── memoria

User space
├── filesystem
├── driver
├── rete
```

Vantaggi:

- isolamento;
- modularità.

Svantaggi:

- complessità;
- overhead IPC.

---

### Kernel ibrido evolutivo (nostro approccio)

```text
Fase iniziale:
Kernel monolitico modulare

Fase avanzata:
Servizi spostabili in user space
```

Questo ci permette di:

- imparare velocemente;
- mantenere flessibilità architetturale.

---

## 6. Meccanismo vs policy

Principio fondamentale:

- Meccanismo → cosa è possibile fare
- Policy → come viene usato

Esempio:

```text
Meccanismo: allocare memoria
Policy: quale processo la ottiene
```

Il kernel deve implementare meccanismi.

---

## 7. Dal concetto al codice

Nel nostro progetto ogni concetto teorico diventerà:

- un modulo;
- un insieme di API;
- una struttura dati;
- test verificabili.

Esempio:

```text
Memoria fisica → modulo PMM
Processi → modulo process
IPC → modulo ipc
```

---

## 8. Prime API (anticipazione)

Anche nel primo capitolo iniziamo a ragionare in termini concreti.

Esempio di API futura:

```c
enum pmm_status pmm_alloc_page(paddr_t *out_page);
```

Domande:

- Chi possiede la pagina?
- Quando viene liberata?
- Cosa succede se fallisce?

Questo tipo di ragionamento sarà centrale in tutto il libro.

---

## 9. Come lo fanno gli altri OS

### Linux

- kernel monolitico;
- altamente modulare;
- driver nel kernel.

### Minix

- microkernel;
- servizi in user space.

### seL4

- microkernel formale;
- forte isolamento.

### Windows

- kernel ibrido.

---

## 10. Collegamento alla milestone

Questo capitolo è collegato a:

```text
M0 — Setup progetto
M1 — Boot
```

---

## 11. Collegamento ai video

Episodi suggeriti:

1. Cos'è un sistema operativo
2. Kernel vs user space
3. Tipi di kernel
4. Architettura del progetto

---

## 12. Esercizi

1. Disegna lo stack completo di esecuzione da applicazione a hardware.
2. Spiega perché un programma non può accedere direttamente alla memoria fisica.
3. Confronta monolitico e microkernel.
4. Descrivi cosa succede durante una syscall.

---

## 13. Domande di riepilogo

- Cos'è un kernel?
- Perché serve un sistema operativo?
- Qual è la differenza tra meccanismo e policy?
- Qual è il ruolo delle syscall?

---

## 14. Prossimo capitolo

Nel prossimo capitolo vedremo:

> Come un sistema operativo viene caricato: firmware, bootloader e bootstrap del kernel.
