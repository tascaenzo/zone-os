#!/bin/bash
# Usage: ./scripts/clean.sh

# Cancella file locali
rm -rf .build

# Pulisci eventuali residui Docker
docker run --rm \
  -v "$(pwd):/workspace" \
  -w /workspace \
  zone-os-dev \
  make clean

echo "✅ Pulizia completata"