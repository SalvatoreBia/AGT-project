#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>
#include <stdio.h> // Necessario per FILE*

// --- COSTANTI DI GIOCO ---
#define COST_SECURITY       1.0
#define PENALTY_UNSECURED   10.0

// --- STRUTTURE ---

typedef struct
{
    uint64_t num_nodes;
    uint64_t num_edges;
    uint64_t *row_ptr; // CSR: offsets, size num_nodes + 1
    uint64_t *col_ind; // CSR: destinations, size num_edges
} graph;

typedef struct
{
    double *regrets; // Array dei rimpianti cumulativi
    double *probs;   // Array delle probabilità attuali
} regret_system;

typedef struct
{
    graph *g;
    unsigned char *strategies; // Array "hot" (0 o 1) per le mosse attuali
    
    regret_system rs;          // Sistema per Regret Matching (usato solo se attivo)

    uint64_t num_players;
    uint64_t iteration;
} game_system;

// --- PROTOTIPI ---

// Gestione Grafo
graph* create_graph(uint64_t num_nodes, uint64_t num_edges);
void free_graph(graph *g);
void print_graph(graph *g);
uint64_t has_edge(graph *g, uint64_t u, uint64_t v);

// Caricamento / Salvataggio / Generazione
graph* load_graph_from_file(const char *filename);
uint64_t save_graph_to_file(graph *g, const char *filename);
graph* generate_erdos_renyi(uint64_t num_nodes, double p);
graph* generate_random_regular(uint64_t num_nodes, uint64_t degree);

// Gestione Gioco
void init_game(game_system *game, graph *g); 
void free_game(game_system *game);

#endif /* DATA_STRUCTURES_H */