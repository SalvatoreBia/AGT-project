#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include "../include/data_structures.h"

// Helper struct for temporary edge storage
typedef struct {
    int u;
    int v;
} edge_t;

graph *create_graph(int num_nodes, int num_edges)
{
    if (num_nodes == 0)
        return NULL;

    graph *g = (graph *)malloc(sizeof(graph));
    if (!g)
        return NULL;

    g->num_nodes = num_nodes;
    g->num_edges = num_edges;

    g->row_ptr = (int *)calloc((num_nodes + 1), sizeof(int));
    g->col_ind = (int *)malloc(num_edges * sizeof(int));

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
    printf("Graph CSR (%d nodes, %d edges)\n", g->num_nodes, g->num_edges);

    int limit = g->num_nodes > 10 ? 10 : g->num_nodes;
    for (int i = 0; i < limit; ++i)
    {
        printf("%d: ", i);
        for (int j = g->row_ptr[i]; j < g->row_ptr[i + 1]; ++j)
        {
            printf("%d ", g->col_ind[j]);
        }
        printf("\n");
    }
}


int save_graph_to_file(graph *g, const char *filename)
{
    if (!g || !filename)
        return 0;

    FILE *f = fopen(filename, "wb");
    if (!f)
    {
        perror("Error opening file for writing");
        return 0;
    }

    fwrite(&g->num_nodes, sizeof(int), 1, f);
    fwrite(&g->num_edges, sizeof(int), 1, f);
    fwrite(g->row_ptr, sizeof(int), g->num_nodes + 1, f);
    fwrite(g->col_ind, sizeof(int), g->num_edges, f);

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

    int num_nodes = 0, num_edges = 0;

    if (fread(&num_nodes, sizeof(int), 1, f) != 1 ||
        fread(&num_edges, sizeof(int), 1, f) != 1)
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

    if (fread(g->row_ptr, sizeof(int), num_nodes + 1, f) != num_nodes + 1 ||
        fread(g->col_ind, sizeof(int), num_edges, f) != num_edges)
    {
        free_graph(g);
        fclose(f);
        return NULL;
    }

    fclose(f);
    printf("Graph loaded from %s (%d nodes, %d edges)\n", filename, num_nodes, num_edges);
    return g;
}


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

static int has_edge_partial(graph *g, int u, int v, int current_u_degree)
{
    int start = g->row_ptr[u];
    for (int i = 0; i < current_u_degree; ++i)
    {
        if (g->col_ind[start + i] == v)
            return 1;
    }
    return 0;
}

graph *generate_random_regular(int num_nodes, int degree)
{
    if ((num_nodes * degree) % 2 != 0 || degree >= num_nodes)
        return NULL;

    int num_edges = num_nodes * degree;
    int total_stubs = num_nodes * degree;

    int *stubs = (int *)malloc(total_stubs * sizeof(int));
    int *current_degree = (int *)calloc(num_nodes, sizeof(int));

    if (!stubs || !current_degree)
    {
        free(stubs);
        free(current_degree);
        return NULL;
    }

    graph *g = NULL;
    int success = 0;

    while (!success)
    {
        if (g)
            free_graph(g);
        g = create_graph(num_nodes, num_edges);
        if (!g)
            break;

        for (int i = 0; i <= num_nodes; ++i)
            g->row_ptr[i] = i * degree;

        memset(current_degree, 0, num_nodes * sizeof(int));

        int k = 0;
        for (int i = 0; i < num_nodes; ++i)
        {
            for (int j = 0; j < degree; ++j)
                stubs[k++] = i;
        }

        shuffle_array(stubs, total_stubs);

        int collision = 0;
        for (int i = 0; i < total_stubs; i += 2)
        {
            int u = stubs[i];
            int v = stubs[i + 1];

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
graph *generate_erdos_renyi(int num_nodes, double p) {
    // Estimating number of edges for allocation (could overflow if p is large, using dynamic array is safer)
    // To keep it simple in C, we'll use a dynamic growing array of edges.
    
    size_t capacity = num_nodes * 10; 
    size_t count = 0;
    edge_t *edge_list = (edge_t *)malloc(capacity * sizeof(edge_t));
    if(!edge_list) return NULL;

    int *degrees = (int *)calloc(num_nodes, sizeof(int));
    if(!degrees) { free(edge_list); return NULL; }

    // Generate Edges
    for (int i = 0; i < num_nodes; ++i) {
        for (int j = i + 1; j < num_nodes; ++j) {
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
    for (int i = 0; i < num_nodes; ++i) {
        g->row_ptr[i+1] = g->row_ptr[i] + degrees[i];
    }

    // Temporary array to track insertion position for each node
    int *current_pos = (int *)malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; ++i) {
        current_pos[i] = g->row_ptr[i];
    }

    // Fill col_ind
    for (size_t i = 0; i < count; ++i) {
        int u = edge_list[i].u;
        int v = edge_list[i].v;

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
graph *generate_barabasi_albert(int num_nodes, int m) {
    if (m < 1 || m >= num_nodes) return NULL;

    // We start with m+1 nodes fully connected (clique) to ensure we have a valid start
    // Number of edges in initial clique: (m+1)*m / 2
    // Number of edges added by remaining nodes: (num_nodes - (m+1)) * m
    
    int init_nodes = m + 1;
    int approx_edges = (init_nodes * m) / 2 + (num_nodes - init_nodes) * m;
    
    // Allocate space for edges
    edge_t *edge_list = (edge_t *)malloc(approx_edges * sizeof(edge_t));
    int edge_count = 0;

    // Repeated nodes array for preferential attachment (stores node ID proportional to degree)
    // Size approx 2 * num_edges
    int *repeated_nodes = (int *)malloc(approx_edges * 2 * sizeof(int));
    int repeated_count = 0;

    // 1. Initialize with a clique of size m+1
    for (int i = 0; i < init_nodes; ++i) {
        for (int j = i + 1; j < init_nodes; ++j) {
            edge_list[edge_count].u = i;
            edge_list[edge_count].v = j;
            edge_count++;

            repeated_nodes[repeated_count++] = i;
            repeated_nodes[repeated_count++] = j;
        }
    }

    // 2. Add remaining nodes
    int *targets = (int *)malloc(m * sizeof(int));

    for (int i = init_nodes; i < num_nodes; ++i) {
        // Select m distinct nodes from repeated_nodes
        int added = 0;
        while (added < m) {
            int r_idx = rand() % repeated_count;
            int target = repeated_nodes[r_idx];
            
            // Check for duplicates
            int duplicate = 0;
            for (int k = 0; k < added; ++k) {
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
        for (int k = 0; k < m; ++k) {
            int target = targets[k];
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
    int *degrees = (int *)calloc(num_nodes, sizeof(int));
    for (int i = 0; i < edge_count; ++i) {
        degrees[edge_list[i].u]++;
        degrees[edge_list[i].v]++;
    }

    graph *g = create_graph(num_nodes, edge_count * 2);
    
    // Row pointers
    g->row_ptr[0] = 0;
    for (int i = 0; i < num_nodes; ++i) {
        g->row_ptr[i+1] = g->row_ptr[i] + degrees[i];
    }

    // Fill col_ind
    int *current_pos = (int *)malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; ++i) {
        current_pos[i] = g->row_ptr[i];
    }

    for (int i = 0; i < edge_count; ++i) {
        int u = edge_list[i].u;
        int v = edge_list[i].v;

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

    for (int i = 0; i < game->num_players; ++i)
    {
        game->strategies[i] = rand() % 2;
    }
}

void free_game(game_system *game)
{
    if (game->strategies)
        free(game->strategies);
}