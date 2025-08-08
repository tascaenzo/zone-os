#pragma once

#include <arch/x86_64/memory/memory.h>

/*
 *
 * @file mm/memory.h
 * @brief Interfaccia del layer memoria arch-indipendente
 *
 * Questo header definisce le funzioni offerte dal sottosistema memoria
 * a tutto il kernel. Tutte le implementazioni dipendono dalle funzioni
 * arch-specifiche dichiarate in arch/memory.h.
 */
extern memory_region_t *regions;
extern size_t region_count;

/**
 * @brief Inizializza il sottosistema memoria (arch + logica)
 */
void memory_init(void);

/**
 * @brief Completa l'inizializzazione dopo che lo heap è pronto
 */
void memory_late_init(void);

/**
 * @brief Stampa a schermo la mappa memoria rilevata (debug)
 */
void memory_print_map(void);

/**
 * @brief Ritorna un puntatore alle statistiche globali calcolate
 * @return Puntatore valido fintanto che il kernel è in esecuzione
 */
const memory_stats_t *memory_get_stats(void);

/**
 * @brief Cerca la regione USABLE più grande rilevata
 *
 * @param base Puntatore dove salvare l'indirizzo base
 * @param length Puntatore dove salvare la dimensione
 * @return true se trovata, false altrimenti
 */
bool memory_find_largest_region(u64 *base, u64 *length);

/**
 * @brief Verifica se una regione è completamente contenuta in memoria USABLE
 *
 * @param base Indirizzo base
 * @param length Lunghezza della regione
 * @return true se l'intera regione è usabile
 */
bool memory_is_region_usable(u64 base, u64 length);