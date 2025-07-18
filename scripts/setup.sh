#!/bin/bash
set -e

# Colori ANSI
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

DOCKER_IMAGE="zone-os-dev"

echo -e "${BLUE}=== ZONE-OS Setup Script ===${NC}\n"

# Verifica Docker installato
echo -e "${YELLOW}Verifica Docker...${NC}"
if ! command -v docker &> /dev/null; then
    echo -e "${RED}❌ Docker non è installato!${NC}"
    echo "Installa Docker da: https://www.docker.com/products/docker-desktop"
    exit 1
fi

# Verifica Docker attivo
if ! docker info > /dev/null 2>&1; then
    echo -e "${RED}❌ Docker non è in esecuzione!${NC}"
    echo "Avvia Docker Desktop e riprova."
    exit 1
fi

echo -e "${GREEN}✓ Docker OK${NC}"

# Detect architettura host
ARCH=$(uname -m)
echo -e "${YELLOW}Architettura rilevata: ${ARCH}${NC}"

if [[ "$ARCH" == "arm64" || "$ARCH" == "aarch64" ]]; then
    echo -e "${YELLOW}⚠️  Architettura ARM: uso emulazione x86_64 via --platform${NC}"
    PLATFORM_FLAG="--platform linux/amd64"
else
    PLATFORM_FLAG=""
fi

# Build Docker (forza rebuild se modificato)
echo -e "\n${YELLOW}Costruzione immagine Docker...${NC}"
echo -e "${YELLOW}Questo potrebbe richiedere alcuni minuti la prima volta${NC}"
docker build $PLATFORM_FLAG -t "$DOCKER_IMAGE" tools/

echo -e "${GREEN}✓ Immagine Docker costruita con successo!${NC}"

# Crea directory progetto minime se mancano
echo -e "\n${YELLOW}Verifica struttura directory...${NC}"
mkdir -p kernel boot scripts .build

echo -e "${GREEN}✓ Directory verificate${NC}"

echo -e "\n${BLUE}=== Setup completato. Ora puoi lanciare: make build ===${NC}"
