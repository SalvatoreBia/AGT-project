#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>
#include <stdio.h>

#define COST_SECURITY       1.0
#define PENALTY_UNSECURED   10.0


typedef struct
{
    uint64_t num_nodes;
    uint64_t num_edges;
    uint64_t *row_ptr;
    uint64_t *col_ind;
} graph;

typedef struct
{
    double *regrets;
    double *probs;
} regret_system;

typedef struct
{
    graph *g;
    unsigned char *strategies;
    
    regret_system rs;

    uint64_t num_players;
    uint64_t iteration;
} game_system;


graph* create_graph(uint64_t num_nodes, uint64_t num_edges);
void free_graph(graph *g);
void print_graph(graph *g);
uint64_t has_edge(graph *g, uint64_t u, uint64_t v);

graph* load_graph_from_file(const char *filename);
uint64_t save_graph_to_file(graph *g, const char *filename);
graph* generate_erdos_renyi(uint64_t num_nodes, double p);
graph* generate_random_regular(uint64_t num_nodes, uint64_t degree);

void init_game(game_system *game, graph *g); 
void free_game(game_system *game);

#endif /* DATA_STRUCTURES_H */