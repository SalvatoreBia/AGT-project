#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"
#include <stdint.h>

#define ALGO_BRD 1
#define ALGO_RM 2
#define ALGO_FP 3

int64_t run_simulation(game_system *game, int algorithm, uint64_t max_it);

double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy);

uint64_t run_best_response_iteration(game_system *game);

uint64_t run_regret_matching_iteration(game_system *game);

uint64_t run_fictious_play_iteration(game_system *game);

int is_valid_cover(game_system *game);

int is_minimal(game_system *game);

void init_regret_system(game_system *game);
void free_regret_system(game_system *game);

void init_fictious_system(game_system *game);
void free_fictious_system(game_system *game);

#endif /* ALGORITHM_H */