#!/bin/bash
set -e

OUT_DIR=".build"
IMG="${OUT_DIR}/kernel.img"
KERNEL_ELF="${OUT_DIR}/kernel.elf"
OFFSET=$((2048 * 512)) # 1 MiB offset

echo "[+] Cleanup..."
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo "[+] Compilo arch/x86_64..."
make -C src/arch/x86_64

echo "[+] Compilo kernel..."
make -C src/kernel KERNEL_ELF="$(pwd)/$KERNEL_ELF"

echo "[+] Creo immagine FAT32 (GPT)..."
dd if=/dev/zero of="$IMG" bs=1M count=64
parted "$IMG" --script mklabel gpt
parted "$IMG" --script mkpart ESP fat32 2048s 100%
parted "$IMG" --script set 1 esp on

echo "[+] Format FAT32..."
mformat -i "$IMG"@@$OFFSET ::

echo "[+] Crea directory necessarie..."
mmd -i "$IMG"@@$OFFSET ::/boot || true
mmd -i "$IMG"@@$OFFSET ::/EFI || true
mmd -i "$IMG"@@$OFFSET ::/EFI/BOOT || true

echo "[+] Copia kernel.elf e limine.conf..."
mcopy -i "$IMG"@@$OFFSET "$KERNEL_ELF" ::/boot/kernel.elf
mcopy -i "$IMG"@@$OFFSET boot/limine.conf ::/boot/limine.conf

echo "[+] Copia payload Limine (BIOS + UEFI)..."
mcopy -i "$IMG"@@$OFFSET /opt/limine/limine-bios.sys ::/boot/
mcopy -i "$IMG"@@$OFFSET /opt/limine/limine-bios-cd.bin ::/boot/
mcopy -i "$IMG"@@$OFFSET /opt/limine/limine-uefi-cd.bin ::/boot/

echo "[+] Copia BOOTX64.EFI per UEFI..."
mcopy -i "$IMG"@@$OFFSET /opt/limine/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

echo "[+] Installa Limine (BIOS)..."
/opt/limine/limine bios-install "$IMG"

echo "[✓] Build completata: $IMG (UEFI + BIOS ready)"