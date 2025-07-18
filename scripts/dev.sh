#!/bin/bash
set -e

echo "[*] Modalità sviluppo attiva (mtime + build Docker + QEMU nativo)..."

# Rileva il sistema operativo
OS_TYPE="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS_TYPE="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS_TYPE="linux"
fi

echo "[*] Sistema rilevato: $OS_TYPE"

# Inizializza a 0 per forzare build iniziale
LAST_MTIME=0

# Funzione timeout universale (Linux + macOS)
run_with_timeout() {
    local timeout_duration=$1
    shift
    
    # Su Linux, usa timeout se disponibile
    if [[ "$OS_TYPE" == "linux" ]] && command -v timeout >/dev/null 2>&1; then
        timeout "$timeout_duration" "$@"
        return $?
    fi
    
    # Su macOS o se timeout non è disponibile, usa implementazione custom
    "$@" &
    local cmd_pid=$!
    
    # Avvia timeout in background
    (
        sleep "$timeout_duration"
        if kill -0 "$cmd_pid" 2>/dev/null; then
            echo "[!] Timeout raggiunto (${timeout_duration}s), terminando build..."
            kill "$cmd_pid" 2>/dev/null
        fi
    ) &
    local timeout_pid=$!
    
    # Aspetta che il comando finisca
    wait "$cmd_pid"
    local exit_code=$?
    
    # Termina il timeout se il comando è finito prima
    kill "$timeout_pid" 2>/dev/null || true
    wait "$timeout_pid" 2>/dev/null || true
    
    return $exit_code
}

# Funzione per ottenere l'mtime più recente
get_latest_mtime() {
    local latest=0
    
    # Verifica che le directory esistano
    for dir in src; do
        if [[ ! -d "$dir" ]]; then
            echo "[!] Directory $dir non trovata!"
            return 1
        fi
    done
    
    # Trova il file più recente usando find con gestione errori
    while IFS= read -r -d '' file; do
        if [[ -f "$file" ]]; then
            # Gestione stat universale (Linux + macOS)
            if [[ "$OS_TYPE" == "linux" ]]; then
                mtime=$(stat -c %Y "$file" 2>/dev/null)
            elif [[ "$OS_TYPE" == "macos" ]]; then
                mtime=$(stat -f %m "$file" 2>/dev/null)
            else
                # Fallback: prova entrambi
                mtime=$(stat -c %Y "$file" 2>/dev/null || stat -f %m "$file" 2>/dev/null)
            fi
            
            if [[ -n "$mtime" ]] && [[ $mtime -gt $latest ]]; then
                latest=$mtime
            fi
        fi
    done < <(find src/ -type f -print0 2>/dev/null)
    
    echo "$latest"
}

# Variabile per tracciare se QEMU è in esecuzione
QEMU_PID=""

# Funzione per terminare QEMU se in esecuzione
cleanup_qemu() {
    if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
        echo "[*] Terminando QEMU (PID: $QEMU_PID)..."
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
        QEMU_PID=""
    fi
}

# Gestione segnali per cleanup
trap cleanup_qemu EXIT INT TERM

echo "[*] Monitoraggio avviato. Premi Ctrl+C per fermare."

while true; do
    CURRENT_MTIME=$(get_latest_mtime)
    
    # Verifica se get_latest_mtime ha avuto successo
    if [[ $? -ne 0 ]]; then
        echo "[!] Errore nel controllo dei file. Attendo 5 secondi..."
        sleep 5
        continue
    fi
    
    if [[ "$CURRENT_MTIME" -gt "$LAST_MTIME" ]]; then
        echo "[+] Modifica rilevata ($(date)). Terminando QEMU precedente..."
        cleanup_qemu
        
        echo "[+] Avvio make build (Docker)..."
        
        # Esegui build con timeout di 5 minuti
        if run_with_timeout 300 make build; then
            echo "[✓] Build Docker completata. Avvio QEMU..."
            
            # Avvia QEMU in background e salva il PID
            ./scripts/run.sh &
            QEMU_PID=$!
            
            echo "[*] QEMU avviato (PID: $QEMU_PID)"
        else
            echo "[!] Build fallita o timeout (5 minuti)."
        fi
        
        LAST_MTIME="$CURRENT_MTIME"
    fi
    
    # Sleep più lungo per ridurre carico CPU
    sleep 2
done