#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/data_structures.h"

// --- GESTIONE GRAFO ---

graph *create_graph(uint64_t num_nodes, uint64_t num_edges)
{
    if (num_nodes == 0)
        return NULL;

    graph *g = (graph *)malloc(sizeof(graph));
    if (!g)
        return NULL;

    g->num_nodes = num_nodes;
    g->num_edges = num_edges;

    // Uso calloc per row_ptr per sicurezza (inizializza a 0)
    g->row_ptr = (uint64_t *)calloc((num_nodes + 1), sizeof(uint64_t));
    g->col_ind = (uint64_t *)malloc(num_edges * sizeof(uint64_t));

    if (!g->row_ptr || !g->col_ind)
    {
        free_graph(g);
        return NULL;
    }

    return g;
}

void free_graph(graph *g)
{
    if (!g)
        return;
    if (g->row_ptr)
        free(g->row_ptr);
    if (g->col_ind)
        free(g->col_ind);
    free(g);
}

void print_graph(graph *g)
{
    if (!g)
        return;
    printf("Graph CSR (%lu nodes, %lu edges)\n", g->num_nodes, g->num_edges);

    uint64_t limit = g->num_nodes > 10 ? 10 : g->num_nodes;
    for (uint64_t i = 0; i < limit; ++i)
    {
        printf("%lu: ", i);
        for (uint64_t j = g->row_ptr[i]; j < g->row_ptr[i + 1]; ++j)
        {
            printf("%lu ", g->col_ind[j]);
        }
        printf("\n");
    }
}

// --- FILE I/O ---

uint64_t save_graph_to_file(graph *g, const char *filename)
{
    if (!g || !filename)
        return 0;

    FILE *f = fopen(filename, "wb");
    if (!f)
    {
        perror("Error opening file for writing");
        return 0;
    }

    fwrite(&g->num_nodes, sizeof(uint64_t), 1, f);
    fwrite(&g->num_edges, sizeof(uint64_t), 1, f);
    fwrite(g->row_ptr, sizeof(uint64_t), g->num_nodes + 1, f);
    fwrite(g->col_ind, sizeof(uint64_t), g->num_edges, f);

    fclose(f);
    printf("Graph saved to %s\n", filename);
    return 1;
}

graph *load_graph_from_file(const char *filename)
{
    if (!filename)
        return NULL;

    FILE *f = fopen(filename, "rb");
    if (!f)
        return NULL; // Silenzioso se non esiste, gestito dal main

    uint64_t num_nodes = 0, num_edges = 0;

    if (fread(&num_nodes, sizeof(uint64_t), 1, f) != 1 ||
        fread(&num_edges, sizeof(uint64_t), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }

    graph *g = create_graph(num_nodes, num_edges);
    if (!g)
    {
        fclose(f);
        return NULL;
    }

    if (fread(g->row_ptr, sizeof(uint64_t), num_nodes + 1, f) != num_nodes + 1 ||
        fread(g->col_ind, sizeof(uint64_t), num_edges, f) != num_edges)
    {
        free_graph(g);
        fclose(f);
        return NULL;
    }

    fclose(f);
    printf("Graph loaded from %s (%lu nodes, %lu edges)\n", filename, num_nodes, num_edges);
    return g;
}

// --- GENERATORI GRAFI ---

// Helper: Shuffle array per randomizzazione archi
static void shuffle_array(uint64_t *array, uint64_t n)
{
    for (uint64_t i = n - 1; i > 0; i--)
    {
        uint64_t j = rand() % (i + 1);
        uint64_t temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Helper: Verifica parziale durante la costruzione per evitare duplicati
static uint64_t has_edge_partial(graph *g, uint64_t u, uint64_t v, uint64_t current_u_degree)
{
    uint64_t start = g->row_ptr[u];
    for (uint64_t i = 0; i < current_u_degree; ++i)
    {
        if (g->col_ind[start + i] == v)
            return 1;
    }
    return 0;
}

graph *generate_random_regular(uint64_t num_nodes, uint64_t degree)
{
    if ((num_nodes * degree) % 2 != 0 || degree >= num_nodes)
        return NULL;

    uint64_t num_edges = num_nodes * degree;
    uint64_t total_stubs = num_nodes * degree;

    uint64_t *stubs = (uint64_t *)malloc(total_stubs * sizeof(uint64_t));
    uint64_t *current_degree = (uint64_t *)calloc(num_nodes, sizeof(uint64_t));

    if (!stubs || !current_degree)
    {
        free(stubs);
        free(current_degree);
        return NULL;
    }

    graph *g = NULL;
    uint64_t success = 0;

    while (!success)
    {
        if (g)
            free_graph(g);
        g = create_graph(num_nodes, num_edges);
        if (!g)
            break;

        // Inizializza row_ptr per grafo regolare
        for (uint64_t i = 0; i <= num_nodes; ++i)
            g->row_ptr[i] = i * degree;

        // Reset gradi correnti per il tentativo
        memset(current_degree, 0, num_nodes * sizeof(uint64_t));

        // Riempi stubs: [0,0,0, 1,1,1, ...]
        uint64_t k = 0;
        for (uint64_t i = 0; i < num_nodes; ++i)
        {
            for (uint64_t j = 0; j < degree; ++j)
                stubs[k++] = i;
        }

        shuffle_array(stubs, total_stubs);

        uint64_t collision = 0;
        for (uint64_t i = 0; i < total_stubs; i += 2)
        {
            uint64_t u = stubs[i];
            uint64_t v = stubs[i + 1];

            if (u == v || has_edge_partial(g, u, v, current_degree[u]))
            {
                collision = 1;
                break;
            }

            g->col_ind[g->row_ptr[u] + current_degree[u]++] = v;
            g->col_ind[g->row_ptr[v] + current_degree[v]++] = u;
        }

        if (!collision)
            success = 1;
    }

    free(stubs);
    free(current_degree);
    return g;
}

// --- GESTIONE GIOCO ---

void init_game(game_system *game, graph *g)
{
    game->g = g;
    game->num_players = g->num_nodes;
    game->strategies = (unsigned char *)malloc(game->num_players * sizeof(unsigned char));
    game->iteration = 0;

    // Inizializza puntatori RS a NULL per sicurezza
    game->rs.regrets = NULL;
    game->rs.probs = NULL;

    // Strategie iniziali random
    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        game->strategies[i] = rand() % 2;
    }
}

void free_game(game_system *game)
{
    if (game->strategies)
        free(game->strategies);
}