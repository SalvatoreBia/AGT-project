#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>



typedef struct
{
    uint64_t num_nodes;
    uint64_t num_edges;
    uint64_t *row_ptr; // offsets, size num_nodes + 1
    uint64_t *col_ind; // destinations, size num_edges
} graph;

typedef uint64_t player;

#define COST_SECURITY       1.0
#define PENALTY_UNSECURED   10.0

typedef struct
{
    graph *g;
    unsigned char *strategies; // Hot array (0 or 1)
    uint64_t num_players;
    uint64_t iteration;
} game_system;


graph* create_graph(uint64_t num_nodes, uint64_t num_edges);
// void add_edge(graph *g, uint64_t src, uint64_t dest); // Removed for CSR efficiency
uint64_t has_edge(graph *g, uint64_t u, uint64_t v);
void free_graph(graph *g);
void print_graph(graph *g);

void init_game(game_system *game, graph *g); 
void free_game(game_system *game);

graph* load_graph_from_file(const char *filename);
graph* generate_erdos_renyi(uint64_t num_nodes, double p);
graph* generate_random_regular(uint64_t num_nodes, uint64_t degree);

uint64_t save_graph_to_file(graph *g, const char *filename);

#endif /* DATA_STRUCTURES_H */