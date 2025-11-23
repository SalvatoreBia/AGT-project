#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"
#include <stdint.h>


double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy);
uint64_t    run_best_response_iteration(game_system *game);

// uint64_t    is_solution_minimal(game_system *game);
int         is_minimal(game_system *game);

void     init_cumulative_regrets(game_system *game);
void     free_cumulative_regrets(game_system *game);
uint64_t run_regret_matching_iteration(game_system *game);

#endif /* ALGORITHM_H */