#!/bin/bash
set -e

# Base dinamica
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(realpath "$SCRIPT_DIR/..")"
IMG="$PROJECT_ROOT/.build/kernel.img"
CODE="$PROJECT_ROOT/boot/OVMF_CODE.fd"

# Check
if [[ ! -f "$IMG" ]]; then
  echo "❌ Immagine $IMG non trovata. Esegui prima: make build"
  exit 1
fi

if [[ ! -f "$CODE" ]]; then
  echo "❌ OVMF_CODE.fd non trovato in: $CODE"
  exit 1
fi

# QEMU avvio (UEFI con -bios, semplice)
echo "▶️ Avvio QEMU UEFI (via -bios)..."
exec qemu-system-x86_64 \
  -m 512M \
  -machine q35 \
  -drive format=raw,file="$IMG" \
  -bios "$CODE" \
  -vga std \
  -serial mon:stdio \
  -no-reboot \
  -no-shutdown \
  -boot menu=on

