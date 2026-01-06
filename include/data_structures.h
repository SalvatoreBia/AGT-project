#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>
#include <stdio.h>

#define COST_SECURITY       1.0
#define PENALTY_UNSECURED   10.0


typedef struct
{
    int num_nodes;
    int num_edges;
    int *row_ptr;
    int *col_ind;
} graph;

typedef struct
{
    double *regrets;
    double *probs;
} regret_system;

typedef struct
{
    int *counts;
    double *believes;
    int turn;
} fictitious_system;


typedef struct
{
    graph *g;
    unsigned char *strategies;
    
    regret_system rs;
    
    fictitious_system fs;

    int num_players;
    int iteration;
} game_system;


graph* create_graph(int num_nodes, int num_edges);
void free_graph(graph *g);
void print_graph(graph *g);

graph* load_graph_from_file(const char *filename);
int save_graph_to_file(graph *g, const char *filename);

graph* generate_random_regular(int num_nodes, int degree);
graph* generate_erdos_renyi(int num_nodes, double p);
graph* generate_barabasi_albert(int num_nodes, int m);

void init_game(game_system *game, graph *g); 
void free_game(game_system *game);

#endif /* DATA_STRUCTURES_H */