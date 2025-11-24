#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"
#include <stdint.h>

// --- FUNZIONI CORE ---

// Calcola il payoff per un giocatore data la sua strategia e quella dei vicini
double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy);

// Esegue un'iterazione di Best Response Dynamics
// Ritorna 1 se almeno un giocatore ha cambiato strategia, 0 altrimenti (Nash Eq.)
uint64_t run_best_response_iteration(game_system *game);

// Esegue un'iterazione di Regret Matching
// Ritorna il numero di nodi attivi
uint64_t run_regret_matching_iteration(game_system *game);

// --- ANALISI RISULTATI ---

// Verifica se la copertura attuale è valida (tutti gli archi coperti)
int is_valid_cover(game_system *game);

// Verifica se la soluzione è un minimo locale (nessun nodo ridondante)
int is_minimal(game_system *game);

// --- GESTIONE MEMORIA REGRET ---

void init_regret_system(game_system *game);
void free_regret_system(game_system *game);

#endif /* ALGORITHM_H */