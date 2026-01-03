#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include "../include/data_structures.h"

// Helper struct for temporary edge storage
typedef struct {
    uint64_t u;
    uint64_t v;
} edge_t;

graph *create_graph(uint64_t num_nodes, uint64_t num_edges)
{
    if (num_nodes == 0)
        return NULL;

    graph *g = (graph *)malloc(sizeof(graph));
    if (!g)
        return NULL;

    g->num_nodes = num_nodes;
    g->num_edges = num_edges;

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
    printf("Graph CSR (%" PRIu64 " nodes, %" PRIu64 " edges)\n", g->num_nodes, g->num_edges);

    uint64_t limit = g->num_nodes > 10 ? 10 : g->num_nodes;
    for (uint64_t i = 0; i < limit; ++i)
    {
        printf("%" PRIu64 ": ", i);
        for (uint64_t j = g->row_ptr[i]; j < g->row_ptr[i + 1]; ++j)
        {
            printf("%" PRIu64 " ", g->col_ind[j]);
        }
        printf("\n");
    }
}


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
        return NULL;

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
    printf("Graph loaded from %s (%" PRIu64 " nodes, %" PRIu64 " edges)\n", filename, num_nodes, num_edges);
    return g;
}


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

        for (uint64_t i = 0; i <= num_nodes; ++i)
            g->row_ptr[i] = i * degree;

        memset(current_degree, 0, num_nodes * sizeof(uint64_t));

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

// ---------------------------------------------------------
// NEW: Erdos-Renyi Generator
// ---------------------------------------------------------
graph *generate_erdos_renyi(uint64_t num_nodes, double p) {
    // Estimating number of edges for allocation (could overflow if p is large, using dynamic array is safer)
    // To keep it simple in C, we'll use a dynamic growing array of edges.
    
    size_t capacity = num_nodes * 10; 
    size_t count = 0;
    edge_t *edge_list = (edge_t *)malloc(capacity * sizeof(edge_t));
    if(!edge_list) return NULL;

    uint64_t *degrees = (uint64_t *)calloc(num_nodes, sizeof(uint64_t));
    if(!degrees) { free(edge_list); return NULL; }

    // Generate Edges
    for (uint64_t i = 0; i < num_nodes; ++i) {
        for (uint64_t j = i + 1; j < num_nodes; ++j) {
            double r = (double)rand() / (double)RAND_MAX;
            if (r < p) {
                // Add edge
                if (count >= capacity) {
                    capacity *= 2;
                    edge_t *temp = realloc(edge_list, capacity * sizeof(edge_t));
                    if (!temp) {
                        free(edge_list);
                        free(degrees);
                        return NULL;
                    }
                    edge_list = temp;
                }
                edge_list[count].u = i;
                edge_list[count].v = j;
                count++;
                degrees[i]++;
                degrees[j]++;
            }
        }
    }

    // Convert to CSR
    // Total directed edges in CSR = 2 * undirected edges
    graph *g = create_graph(num_nodes, count * 2);
    if (!g) {
        free(edge_list);
        free(degrees);
        return NULL;
    }

    // Fill row_ptr (prefix sum)
    g->row_ptr[0] = 0;
    for (uint64_t i = 0; i < num_nodes; ++i) {
        g->row_ptr[i+1] = g->row_ptr[i] + degrees[i];
    }

    // Temporary array to track insertion position for each node
    uint64_t *current_pos = (uint64_t *)malloc(num_nodes * sizeof(uint64_t));
    for (uint64_t i = 0; i < num_nodes; ++i) {
        current_pos[i] = g->row_ptr[i];
    }

    // Fill col_ind
    for (size_t i = 0; i < count; ++i) {
        uint64_t u = edge_list[i].u;
        uint64_t v = edge_list[i].v;

        g->col_ind[current_pos[u]++] = v;
        g->col_ind[current_pos[v]++] = u;
    }

    free(current_pos);
    free(edge_list);
    free(degrees);

    return g;
}

// ---------------------------------------------------------
// NEW: Barabasi-Albert Generator
// ---------------------------------------------------------
graph *generate_barabasi_albert(uint64_t num_nodes, uint64_t m) {
    if (m < 1 || m >= num_nodes) return NULL;

    // We start with m+1 nodes fully connected (clique) to ensure we have a valid start
    // Number of edges in initial clique: (m+1)*m / 2
    // Number of edges added by remaining nodes: (num_nodes - (m+1)) * m
    
    uint64_t init_nodes = m + 1;
    uint64_t approx_edges = (init_nodes * m) / 2 + (num_nodes - init_nodes) * m;
    
    // Allocate space for edges
    edge_t *edge_list = (edge_t *)malloc(approx_edges * sizeof(edge_t));
    uint64_t edge_count = 0;

    // Repeated nodes array for preferential attachment (stores node ID proportional to degree)
    // Size approx 2 * num_edges
    uint64_t *repeated_nodes = (uint64_t *)malloc(approx_edges * 2 * sizeof(uint64_t));
    uint64_t repeated_count = 0;

    // 1. Initialize with a clique of size m+1
    for (uint64_t i = 0; i < init_nodes; ++i) {
        for (uint64_t j = i + 1; j < init_nodes; ++j) {
            edge_list[edge_count].u = i;
            edge_list[edge_count].v = j;
            edge_count++;

            repeated_nodes[repeated_count++] = i;
            repeated_nodes[repeated_count++] = j;
        }
    }

    // 2. Add remaining nodes
    uint64_t *targets = (uint64_t *)malloc(m * sizeof(uint64_t));

    for (uint64_t i = init_nodes; i < num_nodes; ++i) {
        // Select m distinct nodes from repeated_nodes
        uint64_t added = 0;
        while (added < m) {
            uint64_t r_idx = rand() % repeated_count;
            uint64_t target = repeated_nodes[r_idx];
            
            // Check for duplicates
            int duplicate = 0;
            for (uint64_t k = 0; k < added; ++k) {
                if (targets[k] == target) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate) {
                targets[added++] = target;
            }
        }

        // Add edges connecting new node i to targets
        for (uint64_t k = 0; k < m; ++k) {
            uint64_t target = targets[k];
            edge_list[edge_count].u = i;
            edge_list[edge_count].v = target;
            edge_count++;

            repeated_nodes[repeated_count++] = i;
            repeated_nodes[repeated_count++] = target;
        }
    }

    free(targets);
    free(repeated_nodes);

    // 3. Convert to CSR (Calculate degrees, alloc, fill)
    uint64_t *degrees = (uint64_t *)calloc(num_nodes, sizeof(uint64_t));
    for (uint64_t i = 0; i < edge_count; ++i) {
        degrees[edge_list[i].u]++;
        degrees[edge_list[i].v]++;
    }

    graph *g = create_graph(num_nodes, edge_count * 2);
    
    // Row pointers
    g->row_ptr[0] = 0;
    for (uint64_t i = 0; i < num_nodes; ++i) {
        g->row_ptr[i+1] = g->row_ptr[i] + degrees[i];
    }

    // Fill col_ind
    uint64_t *current_pos = (uint64_t *)malloc(num_nodes * sizeof(uint64_t));
    for (uint64_t i = 0; i < num_nodes; ++i) {
        current_pos[i] = g->row_ptr[i];
    }

    for (uint64_t i = 0; i < edge_count; ++i) {
        uint64_t u = edge_list[i].u;
        uint64_t v = edge_list[i].v;

        g->col_ind[current_pos[u]++] = v;
        g->col_ind[current_pos[v]++] = u;
    }

    free(current_pos);
    free(degrees);
    free(edge_list);

    return g;
}


void init_game(game_system *game, graph *g)
{
    game->g = g;
    game->num_players = g->num_nodes;
    game->strategies = (unsigned char *)malloc(game->num_players * sizeof(unsigned char));
    game->iteration = 0;

    game->rs.regrets = NULL;
    game->rs.probs = NULL;

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