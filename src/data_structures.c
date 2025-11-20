#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/data_structures.h"

// +-----------------+
// | Graph functions |
// +-----------------+

graph* create_graph(uint64_t num_nodes, uint64_t num_edges)
{
    if (num_nodes <= 0) return NULL;

    graph *g = (graph*)malloc(sizeof(graph));
    if (g == NULL) return NULL;

    g->num_nodes = num_nodes;
    g->num_edges = num_edges;

    g->row_ptr = (uint64_t*)malloc((num_nodes + 1) * sizeof(uint64_t));
    g->col_ind = (uint64_t*)malloc(num_edges * sizeof(uint64_t));

    if (g->row_ptr == NULL || g->col_ind == NULL)
    {
        free_graph(g);
        return NULL;
    }

    // Initialize row_ptr to 0 (optional, but good practice)
    // For regular graphs, we will overwrite this.
    memset(g->row_ptr, 0, (num_nodes + 1) * sizeof(uint64_t));
    
    return g;
}

void free_graph(graph *g)
{
    if (g == NULL) return;
    if (g->row_ptr) free(g->row_ptr);
    if (g->col_ind) free(g->col_ind);
    free(g);
}

// Helper: Check if edge u-v exists in CSR
// Assumes degree is small, so linear scan is fine.
uint64_t has_edge(graph *g, uint64_t u, uint64_t v)
{
    if (g == NULL) return 0;
    
    // Iterate over neighbors of u
    uint64_t start = g->row_ptr[u];
    uint64_t end = g->row_ptr[u+1]; // Note: during construction, this might be the *capacity* end, not current end.
    // BUT: For generate_random_regular, we will use a separate 'current_degree' array to track valid edges.
    // So this function should be used carefully during construction.
    // Actually, during construction, we can't use row_ptr[u+1] as the limit if we haven't filled it yet.
    // We need to pass the current count.
    // However, to keep signature simple, let's assume this is used on a fully built graph OR
    // we handle construction differently.
    
    // WAIT: In generate_random_regular, we fill the graph. We need to check duplicates.
    // We can pass a 'current_degree' array to a helper, or just scan up to 'current_degree[u]'.
    // Since has_edge is public, it assumes valid CSR.
    // I will make a local helper for construction.
    
    for (uint64_t i = start; i < end; ++i)
    {
        if (g->col_ind[i] == v) return 1;
    }
    return 0;
}

void print_graph(graph *g)
{
    if (g == NULL) return;
    printf("Graph CSR (%d nodes, %d edges)\n", g->num_nodes, g->num_edges);
    // Only print first few nodes to avoid spam
    uint64_t limit = g->num_nodes > 10 ? 10 : g->num_nodes;
    for (uint64_t i = 0; i < limit; ++i)
    {
        printf("%d: ", i);
        for (uint64_t j = g->row_ptr[i]; j < g->row_ptr[i+1]; ++j)
        {
            printf("%d ", g->col_ind[j]);
        }
        printf("\n");
    }
}

// Helper: Shuffle an array
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

// Helper for construction: check if v is in u's current list
static uint64_t has_edge_partial(graph *g, uint64_t u, uint64_t v, uint64_t current_u_degree)
{
    uint64_t start = g->row_ptr[u];
    for (uint64_t i = 0; i < current_u_degree; ++i)
    {
        if (g->col_ind[start + i] == v) return 1;
    }
    return 0;
}

graph* generate_random_regular(uint64_t num_nodes, uint64_t degree)
{
    if ((num_nodes * degree) % 2 != 0) return NULL;
    if (degree >= num_nodes || degree < 0) return NULL;

    uint64_t num_edges = num_nodes * degree; // Total directed edges in CSR (since undirected graph stores both u->v and v->u)
    // Wait, num_edges in CSR usually means total entries in col_ind.
    // For undirected graph with N nodes and degree D, we have N*D entries.
    
    uint64_t total_stubs = num_nodes * degree;
    uint64_t *stubs = (uint64_t*)malloc(total_stubs * sizeof(uint64_t));
    uint64_t *current_degree = (uint64_t*)malloc(num_nodes * sizeof(uint64_t));
    
    if (!stubs || !current_degree) {
        if(stubs) free(stubs);
        if(current_degree) free(current_degree);
        return NULL;
    }

    graph *g = NULL;
    uint64_t success = 0;
    
    while (!success)
    {
        if (g != NULL) free_graph(g);
        g = create_graph(num_nodes, num_edges);
        if (g == NULL) break;

        // Setup row_ptr for regular graph
        // row_ptr[i] starts at i * degree
        for (uint64_t i = 0; i <= num_nodes; ++i)
        {
            g->row_ptr[i] = i * degree;
        }

        // Reset current degree counters
        memset(current_degree, 0, num_nodes * sizeof(uint64_t));

        // Fill stubs
        uint64_t k = 0;
        for (uint64_t i = 0; i < num_nodes; ++i) {
            for (uint64_t j = 0; j < degree; ++j) {
                stubs[k++] = i;
            }
        }

        shuffle_array(stubs, total_stubs);

        uint64_t collision_detected = 0;
        for (uint64_t i = 0; i < total_stubs; i += 2)
        {
            uint64_t u = stubs[i];
            uint64_t v = stubs[i+1];

            if (u == v || has_edge_partial(g, u, v, current_degree[u]))
            {
                collision_detected = 1;
                break;
            }
            
            // Add edge u->v
            g->col_ind[g->row_ptr[u] + current_degree[u]] = v;
            current_degree[u]++;

            // Add edge v->u
            g->col_ind[g->row_ptr[v] + current_degree[v]] = u;
            current_degree[v]++;
        }

        if (!collision_detected) {
            success = 1;
        }
    }

    free(stubs);
    free(current_degree);
    return g;
}

// Not implemented for CSR optimization task
graph* load_graph_from_file(const char *filename) { return NULL; }
graph* generate_erdos_renyi(uint64_t num_nodes, double p) { return NULL; }


// +-----------------+
// | Game functions  |
// +-----------------+

void init_game(game_system *game, graph *g)
{
    game->g = g;
    game->num_players = g->num_nodes;
    
    // Allocate strategies (SoA)
    // Using calloc to init to 0, but we will randomize.
    game->strategies = (unsigned char*)malloc(game->num_players * sizeof(unsigned char));
    game->iteration = 0;

    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        game->strategies[i] = rand() % 2;
    }
}

void free_game(game_system *game)
{
    if (game->strategies) free(game->strategies);
}


// Saves graph in binary format: 
// [num_nodes][num_edges][row_ptr array][col_ind array]
uint64_t save_graph_to_file(graph *g, const char *filename)
{
    if (g == NULL || filename == NULL) return 0;

    FILE *f = fopen(filename, "wb"); // Write Binary
    if (f == NULL) {
        perror("Error opening file for writing");
        return 0;
    }

    // 1. Write Header info
    fwrite(&g->num_nodes, sizeof(uint64_t), 1, f);
    fwrite(&g->num_edges, sizeof(uint64_t), 1, f);

    // 2. Write CSR Arrays
    // row_ptr size is num_nodes + 1
    fwrite(g->row_ptr, sizeof(uint64_t), g->num_nodes + 1, f);
    
    // col_ind size is num_edges
    fwrite(g->col_ind, sizeof(uint64_t), g->num_edges, f);

    fclose(f);
    printf("Graph saved to %s (Binary format)\n", filename);
    return 1;
}


// +---------------------------------------------------------+
// | Add these implementations to src/data_structures.c      |
// +---------------------------------------------------------+

#include <time.h> // Required for time() if not already included

graph* load_graph_from_file(const char *filename)
{
    if (filename == NULL) return NULL;

    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        perror("Error opening file for reading");
        return NULL;
    }

    uint64_t num_nodes = 0;
    uint64_t num_edges = 0;

    // 1. Read Header info
    if (fread(&num_nodes, sizeof(uint64_t), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }
    if (fread(&num_edges, sizeof(uint64_t), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }

    // 2. Allocate Graph structure
    graph *g = create_graph(num_nodes, num_edges);
    if (g == NULL)
    {
        fclose(f);
        return NULL;
    }

    // 3. Read CSR Arrays directly into the allocated memory
    // Read row_ptr (size is num_nodes + 1)
    if (fread(g->row_ptr, sizeof(uint64_t), num_nodes + 1, f) != num_nodes + 1)
    {
        fprintf(stderr, "Error reading row_ptr\n");
        free_graph(g);
        fclose(f);
        return NULL;
    }

    // Read col_ind (size is num_edges)
    if (fread(g->col_ind, sizeof(uint64_t), num_edges, f) != num_edges)
    {
        fprintf(stderr, "Error reading col_ind\n");
        free_graph(g);
        fclose(f);
        return NULL;
    }

    fclose(f);
    printf("Graph loaded from %s (%lu nodes, %lu edges)\n", filename, num_nodes, num_edges);
    return g;
}

graph* generate_erdos_renyi(uint64_t num_nodes, double p)
{
    if (num_nodes == 0) return NULL;
    if (p < 0.0 || p > 1.0) return NULL;

    // We use a Two-Pass algorithm to build CSR directly without dynamic lists.
    // Pass 1: Count degrees to determine graph size and row_ptr.
    // Pass 2: Fill col_ind.
    // We must use the SAME seed for both passes to generate the same edges.
    
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)rand();
    
    // --- PASS 1: Calculate Degrees ---
    srand(seed);
    
    uint64_t *degrees = (uint64_t*)calloc(num_nodes, sizeof(uint64_t));
    if (!degrees) return NULL;

    uint64_t total_edges_count = 0;

    for (uint64_t i = 0; i < num_nodes; ++i)
    {
        for (uint64_t j = i + 1; j < num_nodes; ++j)
        {
            // Standard check: rand / RAND_MAX < p
            if ((double)rand() / RAND_MAX < p)
            {
                degrees[i]++;
                degrees[j]++;
                total_edges_count += 2; // Undirected = 2 directed edges in CSR
            }
        }
    }

    // --- ALLOCATION ---
    graph *g = create_graph(num_nodes, total_edges_count);
    if (g == NULL)
    {
        free(degrees);
        return NULL;
    }

    // Build row_ptr (prefix sum of degrees)
    g->row_ptr[0] = 0;
    for (uint64_t i = 0; i < num_nodes; ++i)
    {
        g->row_ptr[i+1] = g->row_ptr[i] + degrees[i];
    }

    // Create a temporary array to track current insert position for each node
    // because we add edges for 'u' and 'v' simultaneously.
    uint64_t *current_offsets = (uint64_t*)malloc(num_nodes * sizeof(uint64_t));
    if (!current_offsets)
    {
        free(degrees);
        free_graph(g);
        return NULL;
    }
    // Initialize offsets to the start of each row
    memcpy(current_offsets, g->row_ptr, num_nodes * sizeof(uint64_t));

    // --- PASS 2: Fill Edges ---
    // RESET SEED to replicate the exact same sequence of probabilities
    srand(seed); 

    for (uint64_t i = 0; i < num_nodes; ++i)
    {
        for (uint64_t j = i + 1; j < num_nodes; ++j)
        {
            if ((double)rand() / RAND_MAX < p)
            {
                // Add edge i -> j
                uint64_t pos_i = current_offsets[i]++;
                g->col_ind[pos_i] = j;

                // Add edge j -> i
                uint64_t pos_j = current_offsets[j]++;
                g->col_ind[pos_j] = i;
            }
        }
    }

    free(degrees);
    free(current_offsets);
    
    return g;
}