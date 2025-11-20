#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"
#include <stdint.h>



#define MAX_IT  1000

double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy);
uint64_t    run_best_response_iteration(game_system *game);

// uint64_t    is_solution_minimal(game_system *game);
int         is_minimal(game_system *game);


#endif /* ALGORITHM_H */