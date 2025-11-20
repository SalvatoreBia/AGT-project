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

graph* generate_erdos_renyi(int num_nodes, double p)
{
    // 1. Creiamo il grafo vuoto con N nodi
    graph *g = create_graph(num_nodes);
    if (g == NULL) return NULL;

    // 2. Iteriamo su tutte le coppie uniche di nodi.
    // i va da 0 a N
    // j va da i+1 a N
    // In questo modo controlliamo la coppia (0,1) ma NON la (1,0) e NON la (0,0).
    // Dato che il tuo add_edge aggiunge già entrambe le direzioni, questo è corretto.
    for (int i = 0; i < num_nodes; ++i)
    {
        for (int j = i + 1; j < num_nodes; ++j)
        {
            // Generiamo un float random tra 0.0 e 1.0
            double r = (double)rand() / (double)RAND_MAX;

            // Se il numero estratto è minore della probabilità p, creiamo l'arco
            if (r < p)
            {
                add_edge(g, i, j);
            }
        }
    }

    return g;
}

// Helper: controlla se esiste l'arco u-v
int has_edge(graph *g, int u, int v)
{
    if (g == NULL || g->nodes == NULL) return 0;
    
    adj_node *curr = g->nodes[u];
    while (curr != NULL)
    {
        if (curr->dest == v) return 1;
        curr = curr->next;
    }
    return 0;
}

// Helper: Mescola un array (Fisher-Yates shuffle)
static void shuffle_array(int *array, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Funzione Principale
graph* generate_random_regular(int num_nodes, int degree)
{
    // 1. Controlli di validità matematica
    // La somma dei gradi deve essere pari (Handshake Lemma)
    if ((num_nodes * degree) % 2 != 0) return NULL;
    if (degree >= num_nodes || degree < 0) return NULL;

    int total_stubs = num_nodes * degree;
    int *stubs = (int*)malloc(total_stubs * sizeof(int));
    
    graph *g = NULL;
    int success = 0;
    
    // Loop di tentativi (per evitare self-loops o archi doppi)
    while (!success)
    {
        // Se avevamo un grafo fallito da un giro precedente, puliamo
        if (g != NULL) {
             // Qui dovresti chiamare free_graph(g). 
             // Per ora assumo che tu lo faccia o ricarichi la struct.
             // Implementazione rapida di pulizia per non avere memory leak nel loop:
             for(int i=0; i<g->num_nodes; i++) {
                 adj_node *curr = g->nodes[i];
                 while(curr) { adj_node* tmp=curr; curr=curr->next; free(tmp); }
             }
             free(g->nodes);
             free(g);
        }

        g = create_graph(num_nodes);
        
        // 2. Riempiamo l'array degli stub:
        // [0,0,0, 1,1,1, 2,2,2 ...] se grado è 3
        int k = 0;
        for (int i = 0; i < num_nodes; ++i) {
            for (int j = 0; j < degree; ++j) {
                stubs[k++] = i;
            }
        }

        // 3. Mescoliamo gli stub
        shuffle_array(stubs, total_stubs);

        // 4. Proviamo a collegarli a coppie
        int collision_detected = 0;
        for (int i = 0; i < total_stubs; i += 2)
        {
            int u = stubs[i];
            int v = stubs[i+1];

            // Check 1: Self-loop (u collegato a se stesso)
            // Check 2: Multi-edge (arco u-v già esistente)
            if (u == v || has_edge(g, u, v))
            {
                collision_detected = 1;
                break; // Fallito, riprova tutto
            }
            
            add_edge(g, u, v);
        }

        if (!collision_detected) {
            success = 1;
        }
    }

    free(stubs);
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