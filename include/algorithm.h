#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"
#include <stdint.h>

#define ALGO_BRD 1
#define ALGO_RM  2
#define ALGO_FP  3

double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy);

int64_t run_simulation(game_system *game, int algorithm, uint64_t max_it);

uint64_t run_best_response_iteration(game_system *game);
uint64_t run_regret_matching_iteration(game_system *game);
uint64_t run_fictious_play_iteration(game_system *game);

int is_valid_cover(game_system *game);
int is_minimal(game_system *game);

void init_regret_system(game_system *game);
void free_regret_system(game_system *game);

void init_fictious_system(game_system *game);
void free_fictious_system(game_system *game);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

double characteristic_function_v1(graph *g, uint64_t *coalition, size_t coalition_size);
double characteristic_function_v2(graph *g, uint64_t *coalition, size_t coalition_size);
double characteristic_function_v3(graph *g, uint64_t *coalition, size_t coalition_size);

double* calculate_shapley_values(graph *g, int iterations, int version);

unsigned char* build_security_set_from_shapley(graph *g, double *shapley_values);

int count_covered_edges(graph *g, uint64_t *coalition, size_t coalition_size);
int is_coalition_valid_cover(graph *g, uint64_t *coalition, size_t coalition_size);
int is_coalition_minimal(graph *g, uint64_t *coalition, size_t coalition_size);

#endif /* ALGORITHM_H */
