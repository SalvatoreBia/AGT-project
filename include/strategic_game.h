#ifndef STRATEGIC_GAME_H
#define STRATEGIC_GAME_H

#include "data_structures.h"
#include <stdint.h>

#define ALGO_BRD 1
#define ALGO_RM  2
#define ALGO_FP  3
#define ALGO_FP_ASYNC 5

double calculate_utility(game_system *game, int player_id, int strategy);

int run_simulation(game_system *game, int algorithm, int max_it, int verbose);

int run_best_response_iteration(game_system *game);
int run_regret_matching_iteration(game_system *game);

int run_fictitious_play_iteration(game_system *game);
int run_async_fictitious_play_iteration(game_system *game);

int is_valid_cover(game_system *game);
int is_minimal(game_system *game);

void init_regret_system(game_system *game);
void free_regret_system(game_system *game);

void init_fictitious_system(game_system *game);
void free_fictitious_system(game_system *game);

#endif
