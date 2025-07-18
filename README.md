# Zone OS

**The Zone Operating System** - Un sistema operativo minimalista x86_64 sviluppato da zero.

```
 ______                    ____   _____
|___  /                   / __ \ / ____|
   / / ___  _ __   ___   | |  | | (___
  / / / _ \| '_ \ / _ \  | |  | |\___ \
 / /_| (_) | | | |  __/  | |__| |____) |
/_____\___/|_| |_|\___|   \____/|_____/
```

## 🎯 Obiettivi del Progetto

Zone OS è un progetto educativo per comprendere i fondamenti dei sistemi operativi moderni:

- **Architettura pulita**: Codice organizzato e modulare
- **Boot moderno**: Supporto UEFI e BIOS legacy tramite Limine
- **Sviluppo agile**: Build automatizzata con Docker e hot-reload
- **x86_64 nativo**: Pieno supporto per architetture a 64-bit

## 🏗️ Architettura

```
Zone OS
├── Bootloader (Limine v9)
├── Kernel Core (C + Assembly)
├── Memory Management
└── Hardware Abstraction Layer
```

### Struttura del Progetto

```
zone-os/
├── src/
│   ├── arch/x86_64/        # Codice specifico architettura
│   │   └── boot/boot.s     # Entry point assembly
│   ├── kernel/             # Kernel core
│   │   ├── main.c          # Kernel main
│   │   ├── print.c         # Output primitivo
│   │   └── Makefile        # Build kernel
│   └── boot/               # File assembly boot (se presenti)
├── boot/                   # Configurazione bootloader
|   |── Dockerfile          # Ambiente di build
│   └── limine.conf         # Config Limine v9
├── tools/
│   └── linker.ld           # Linker script
├── scripts/
│   ├── build.sh           # Script di build
│   ├── run.sh             # Esecuzione QEMU
│   └── dev.sh             # Modalità sviluppo
└── .build/                # Output compilazione (generata)
```

## 🚀 Quick Start

### Prerequisiti

- **Docker** (per build isolata)
- **QEMU** (per testing)
- **Make** (orchestrazione build)

### Build e Run

```bash
# Build completa
make build

# Esecuzione in QEMU
make run

# Modalità sviluppo (auto-rebuild + hot-reload)
make dev
```

### Build Manuale (senza Docker)

```bash
# Requisiti: clang, nasm, ld.lld, mtools, parted
./scripts/build.sh
```

## 🛠️ Toolchain

- **Bootloader**: Limine v9.x (UEFI + BIOS)
- **Compiler**: Clang (target x86_64-pc-none-elf)
- **Assembler**: NASM
- **Linker**: LLD
- **Build**: Docker + Make
- **Emulator**: QEMU

## 🧪 Features Implementate

### ✅ Completate

- [x] Boot UEFI e BIOS
- [x] Kernel entry point x86_64
- [x] Output text basilare
- [x] Build system automatizzata
- [x] Hot-reload development

### 🚧 In Sviluppo

- [ ] Memory management (paging)
- [ ] Interrupt handling (IDT)
- [ ] Keyboard input
- [ ] VGA/Framebuffer graphics

### 📋 Roadmap

- [ ] Process management
- [ ] File system
- [ ] Network stack
- [ ] User space

## 🔧 Sviluppo

### Modalità Sviluppo

```bash
# Avvia il watcher automatico
make dev
```

Il sistema monitora modifiche ai file sorgente e:

1. Rebuilda automaticamente
2. Riavvia QEMU con la nuova immagine
3. Mostra output in tempo reale

### Build Targets

```bash
make build          # Build completa in Docker
make run            # Esegui l'ultima build
make clean          # Pulizia file temporanei
make dev            # Modalità sviluppo
```

## 📖 Documentazione Tecnica

### Boot Process

1. **UEFI/BIOS** carica Limine
2. **Limine** legge configurazione e carica kernel
3. **boot.s** prepara ambiente x86_64
4. **main.c** inizializza kernel core

## 🤝 Contributi

Zone OS è un progetto personale educativo, ma feedback e suggerimenti sono sempre benvenuti!

### Coding Style

- **C**: Stile GNU con indentazione a 4 spazi
- **Assembly**: Sintassi Intel (NASM)
- **Naming**: snake_case per funzioni, UPPER_CASE per costanti

## 📄 Licenza

Questo progetto è rilasciato sotto licenza MIT.

---

**Zone OS** - Sviluppato con ❤️ da Enzo
