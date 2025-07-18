# ===========================
# ZONE-OS Build System (Makefile)
# ===========================

# Immagine Docker
DOCKER_IMAGE = zone-os-dev
DOCKER_RUN = docker run --rm
DOCKER_RUN_IT = docker run --rm -it

# Arch compat
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
    DOCKER_PLATFORM = --platform linux/amd64
else ifeq ($(UNAME_M),aarch64)
    DOCKER_PLATFORM = --platform linux/amd64
else
    DOCKER_PLATFORM =
endif

# Output build
BUILD_DIR = .build
DISK_IMG = $(BUILD_DIR)/kernel.img

# ===========================
# Comandi Make
# ===========================

.DEFAULT_GOAL := help

help:
	@echo ""
	@echo "\033[1;34m=====================================\033[0m"
	@echo "\033[1;34m        ZONE-OS Build System             \033[0m"
	@echo "\033[1;34m=====================================\033[0m"
	@echo ""
	@echo "\033[1;33mComandi disponibili:\033[0m"
	@echo "  \033[0;32mmake setup\033[0m       Prepara ambiente Docker + struttura progetto"
	@echo "  \033[0;32mmake build\033[0m       Compila il kernel in ambiente Docker"
	@echo "  \033[0;32mmake dev\033[0m         Avvia modalità sviluppo interattiva"
	@echo "  \033[0;32mmake run\033[0m         Avvia QEMU in modalità host"
	@echo "  \033[0;32mmake clean\033[0m       Rimuove la directory di build"
	@echo "  \033[0;32mmake clean-all\033[0m   Rimuove anche l'immagine Docker"
	@echo ""

setup:
	@echo "\033[1;34m>>> Setup ambiente Docker\033[0m"
	@./scripts/setup.sh

build:
	@echo "\033[1;34m>>> Build kernel\033[0m"
	@$(DOCKER_RUN) $(DOCKER_PLATFORM) \
		-v "$(PWD):/workspace" \
		$(DOCKER_IMAGE) \
		/workspace/scripts/build.sh

run:
	@echo "\033[1;34m>>> Esecuzione kernel in QEMU\033[0m"
	@HOST_RUN=1 ./scripts/run.sh

dev:
	@echo "\033[1;34m>>> Modalità sviluppo (host nativo)\033[0m"
	@./scripts/dev.sh

clean:
	@echo "\033[1;34m>>> Clean\033[0m"
	@rm -rf $(BUILD_DIR)
	@echo "\033[0;32m✓ Pulizia completata\033[0m"

clean-all: clean
	@echo "\033[1;34m>>> Clean immagine Docker\033[0m"
	@docker rmi -f $(DOCKER_IMAGE) 2>/dev/null || true
	@echo "\033[0;32m✓ Tutto pulito\033[0m"
