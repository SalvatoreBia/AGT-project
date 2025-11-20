#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "data_structures.h"



#define MAX_IT  1000

double calculate_utility(game_system *game, int player_id, int strategy);
int    run_best_response_iteration(game_system *game);

#endif /* ALGORITHM_H */