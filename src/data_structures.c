#include <stdio.h>
#include <stdlib.h>
#include "../include/data_structures.h"


// +-----------------+
// | Graph functions |
// +-----------------+

graph* create_graph(int num_nodes)
{
    if (num_nodes <= 0) return NULL;

    graph *g = (graph*)malloc(sizeof(graph));
    if (g == NULL) return NULL;

    g->num_nodes = num_nodes;
    g->nodes = (adj_node**)malloc(num_nodes * sizeof(adj_node*));
    if (g->nodes == NULL)
    {
        free((void*)g);
        return NULL;
    }

    for (int i = 0; i < num_nodes; ++i)
    {
        g->nodes[i] = NULL;
    }

    return g;
}

adj_node* new_adj_node(int dest)
{
    adj_node *node = (adj_node*)malloc(sizeof(adj_node));
    if (node == NULL) return NULL;

    node->dest = dest;
    node->next = NULL;
    return node;
} 

void add_edge(graph *g, int src, int dest)
{
    if (g == NULL) return;
    if ((src < 0 || src >= g->num_nodes) || (dest < 0 || dest >= g->num_nodes)) return;

    adj_node *new_node = new_adj_node(dest);
    new_node->next = g->nodes[src];
    g->nodes[src] = new_node;

    new_node = new_adj_node(src);
    new_node->next = g->nodes[dest];
    g->nodes[dest] = new_node;
}

graph* load_graph_from_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Errore nell'apertura del file.");
        return NULL;
    }

    int n, u, v;

    fscanf(file, "%d", &n);
    graph *g = create_graph(n);

    while (fscanf(file, "%d %d", &u, &v) != EOF) add_edge(g, u, v);

    fclose(file);
    return g;
}


// +-----------------+
// | Game functions  |
// +-----------------+

void init_game(game_system *game, graph *g)
{
    game->g = g; // Collega il grafo al sistema
    game->num_players = g->num_nodes;
    
    // Ora alloca i giocatori in base alla dimensione del grafo
    game->players = (player*)malloc(game->num_players * sizeof(player));
    game->iteration = 0;

    for (int i = 0; i < game->num_players; ++i)
    {
        game->players[i].id = i;
        game->players[i].current_strategy = rand() % 2;
        
        game->players[i].history_count[0] = 0;
        game->players[i].history_count[1] = 0;
        game->players[i].regret_sum[0] = 0.0;
        game->players[i].regret_sum[1] = 0.0;
    }
}