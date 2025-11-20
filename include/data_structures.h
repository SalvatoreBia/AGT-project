#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H


typedef struct adjacent_node
{
    int dest;
    struct adjacent_node *next;
} adj_node;

typedef struct
{
    int num_nodes;
    adj_node **nodes;
} graph;

typedef struct
{
    int id;
    int current_strategy;
    int history_count[2]; 
    double regret_sum[2]; 
    double strategy_sum[2];
} player;

#define COST_SECURITY       1.0
#define PENALTY_UNSECURED   10.0

typedef struct
{
    graph *g;
    player *players;
    int num_players;
    int iteration;
} game_system;


graph* create_graph(int num_nodes);
void add_edge(graph *g, int src, int dest);
void free_graph(graph *g);
void print_graph(graph *g);

void init_game(game_system *game, graph *g); 
void free_game(game_system *game);

graph* load_graph_from_file(const char *filename);

#endif /* DATA_STRUCTURES_H */