#ifndef COALITIONAL_GAME_H
#define COALITIONAL_GAME_H

#include "data_structures.h"
#include <stddef.h>

double characteristic_function_v1(graph *g, int *coalition, size_t coalition_size);
double characteristic_function_v2(graph *g, int *coalition, size_t coalition_size);
double characteristic_function_v3(graph *g, int *coalition, size_t coalition_size);

double* calculate_shapley_values(graph *g, int iterations, int version);

unsigned char* build_security_set_from_shapley(graph *g, double *shapley_values);

int count_covered_edges(graph *g, int *coalition, size_t coalition_size);
int is_coalition_valid_cover(graph *g, int *coalition, size_t coalition_size);
int is_coalition_minimal(graph *g, int *coalition, size_t coalition_size);

#endif
