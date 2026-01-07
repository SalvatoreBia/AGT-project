#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include "../include/coalitional_game.h"

static void shuffle_array(int *array, size_t n)
{
    if (n > 1)
    {
        for (size_t i = 0; i < n - 1; i++)
        {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

typedef struct
{
    int u;
    int v;
} edge;

static guint edge_hash(gconstpointer key)
{
    const edge *e = (const edge *)key;
    int hash = e->u ^ (e->v << 1);
    return (guint)(hash % UINT32_MAX);
}

static gboolean edge_equal(gconstpointer a, gconstpointer b)
{
    const edge *e1 = (const edge *)a;
    const edge *e2 = (const edge *)b;
    return (e1->u == e2->u) && (e1->v == e2->v);
}

int count_covered_edges(graph *g, int *coalition, size_t coalition_size)
{
    GHashTable *edges = g_hash_table_new_full(edge_hash, edge_equal, g_free, NULL);
    if (edges == NULL)
        return -1;

    edge temp_edge;

    for (size_t i = 0; i < coalition_size; ++i)
    {
        int curr = coalition[i];
        int start = g->row_ptr[curr];
        int end = g->row_ptr[curr + 1];

        for (int j = start; j < end; ++j)
        {
            int neighbor = g->col_ind[j];

            if (curr < neighbor)
            {
                temp_edge.u = curr;
                temp_edge.v = neighbor;
            }
            else
            {
                temp_edge.u = neighbor;
                temp_edge.v = curr;
            }

            if (!g_hash_table_contains(edges, &temp_edge))
            {
                edge *new_e = g_new(edge, 1);
                new_e->u = temp_edge.u;
                new_e->v = temp_edge.v;
                g_hash_table_add(edges, new_e);
            }
        }
    }

    int size = (int)g_hash_table_size(edges);
    g_hash_table_destroy(edges);
    return size;
}

int is_coalition_valid_cover(graph *g, int *coalition, size_t coalition_size)
{
    unsigned char *in_coalition = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));
    if (!in_coalition)
        return 0;

    for (size_t i = 0; i < coalition_size; ++i)
    {
        in_coalition[coalition[i]] = 1;
    }

    int is_valid = 1;
    for (int u = 0; u < g->num_nodes; ++u)
    {
        for (int k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            int v = g->col_ind[k];
            if (u >= v)
                continue;

            if (!in_coalition[u] && !in_coalition[v])
            {
                is_valid = 0;
                break;
            }
        }
        if (!is_valid)
            break;
    }

    free(in_coalition);
    return is_valid;
}

int is_coalition_minimal(graph *g, int *coalition, size_t coalition_size)
{
    unsigned char *in_coalition = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));
    unsigned char *has_private = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));

    if (!in_coalition || !has_private)
    {
        free(in_coalition);
        free(has_private);
        return 0;
    }

    for (size_t i = 0; i < coalition_size; ++i)
    {
        in_coalition[coalition[i]] = 1;
    }

    for (int u = 0; u < g->num_nodes; ++u)
    {
        for (int k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            int v = g->col_ind[k];
            if (u >= v)
                continue;


            if (in_coalition[u] && !in_coalition[v])
            {
                has_private[u] = 1;
            }
            else if (!in_coalition[u] && in_coalition[v])
            {
                has_private[v] = 1;
            }
        }
    }

    int minimal = 1;
    for (size_t i = 0; i < coalition_size; ++i)
    {
        if (!has_private[coalition[i]])
        {
            minimal = 0;
            break;
        }
    }

    free(in_coalition);
    free(has_private);
    return minimal;
}



double characteristic_function_v1(graph *g, int *coalition, size_t coalition_size)
{
    if (coalition_size == 0)
        return 0.0;

    int covered = count_covered_edges(g, coalition, coalition_size);
    if (covered < 0)
        return 0.0;

    double fraction = (double)covered / (double)g->num_edges;
    double value = fraction * 100.0;

    if (is_coalition_valid_cover(g, coalition, coalition_size))
    {
        if (!is_coalition_minimal(g, coalition, coalition_size))
        {
            value -= 10.0;
        }
    }

    return value;
}


double characteristic_function_v2(graph *g, int *coalition, size_t coalition_size)
{
    if (coalition_size == 0)
        return 0.0;

    int covered = count_covered_edges(g, coalition, coalition_size);
    if (covered < 0)
        return 0.0;

    double value = (double)covered;

    if (is_coalition_valid_cover(g, coalition, coalition_size))
    {
        value += 100.0;

        if (is_coalition_minimal(g, coalition, coalition_size))
        {
            value += 50.0;
        }
    }

    return value;
}


double characteristic_function_v3(graph *g, int *coalition, size_t coalition_size)
{
    if (coalition_size == 0)
        return 0.0;

    int covered = count_covered_edges(g, coalition, coalition_size);
    if (covered < 0)
        return 0.0;

    double value = (double)covered - (double)coalition_size * 0.5;

    if (is_coalition_valid_cover(g, coalition, coalition_size))
    {
        value += 50.0;

        if (is_coalition_minimal(g, coalition, coalition_size))
        {
            value += 30.0;
        }
    }

    return value;
}


double *calculate_shapley_values(graph *g, int iterations, int version)
{
    size_t n = g->num_nodes;

    double *shapley_values = calloc(n, sizeof(double));

    int *permutation = malloc(n * sizeof(int));

    int *coalition = malloc(n * sizeof(int));

    for (size_t i = 0; i < n; i++)
    {
        permutation[i] = i;
    }

    double (*char_func)(graph *, int *, size_t) = NULL;

    if (version == 1)
        char_func = characteristic_function_v1;
    else if (version == 2)
        char_func = characteristic_function_v2;
    else if (version == 3)
        char_func = characteristic_function_v3;
    else
    {
        fprintf(stderr, "Error: invalid characteristic function version (%d)\n", version);
        free(shapley_values);
        free(permutation);
        free(coalition);
        return NULL;
    }
    printf("[INFO] Starting optimized Shapley calculation (%d iterations)...\n", iterations);

    for (int iter = 0; iter < iterations; ++iter)
    {

        shuffle_array(permutation, n);

        for (size_t i = 0; i < n; ++i)
        {
            int curr_node = permutation[i];

            size_t coalition_size = 0;
            for (size_t j = 0; j < i; ++j)
            {
                coalition[coalition_size++] = permutation[j];
            }

            double value_without = 0.0;
            if (coalition_size > 0)
            {
                value_without = char_func(g, coalition, coalition_size);
            }

            coalition[coalition_size] = curr_node;

            double value_with = char_func(g, coalition, coalition_size + 1);

            double marginal_contribution = value_with - value_without;

            shapley_values[curr_node] += marginal_contribution;
        }

        if (iter % 100 == 0 && iter > 0)
            printf("[INFO] Iteration %d/%d completed\n", iter, iterations);
    }

    for (size_t i = 0; i < n; i++)
    {
        shapley_values[i] /= iterations;
    }

    free(permutation);
    free(coalition);
    printf("[OK] Shapley calculation completed\n");
    return shapley_values;
}

static int compare_node_values(const void *a, const void *b)
{
    typedef struct
    {
        int node_id;
        double shapley_value;
    } node_value_pair;

    const node_value_pair *na = (const node_value_pair *)a;
    const node_value_pair *nb = (const node_value_pair *)b;

    double diff = nb->shapley_value - na->shapley_value;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}


unsigned char *build_security_set_from_shapley(graph *g, double *shapley_values)
{
    size_t n = g->num_nodes;

    typedef struct
    {
        int node_id;
        double shapley_value;
    } node_value_pair;

    node_value_pair *sorted_nodes = malloc(n * sizeof(node_value_pair));
    for (size_t i = 0; i < n; i++)
    {
        sorted_nodes[i].node_id = i;
        sorted_nodes[i].shapley_value = shapley_values[i];
    }
    qsort(sorted_nodes, n, sizeof(node_value_pair), compare_node_values);

    printf("\n[INFO] Building minimal security set from Shapley values...\n");

    unsigned char *security_set = calloc(n, sizeof(unsigned char));
    for (size_t i = 0; i < n; i++)
        security_set[i] = 1;

    printf("[INFO] Attempting minimization (reverse delete order)...\n");
    size_t removed_count = 0;

    for (size_t i = n; i > 0; i--)
    {
        size_t index = i - 1;
        int candidate_node = sorted_nodes[index].node_id;

        security_set[candidate_node] = 0;

        int still_covered = 1;
        int start = g->row_ptr[candidate_node];
        int end = g->row_ptr[candidate_node + 1];

        for (int j = start; j < end; ++j)
        {
            int neighbor = g->col_ind[j];
            if (security_set[neighbor] == 0)
            {
                still_covered = 0;
                break;
            }
        }

        if (still_covered)
        {
            removed_count++;
        }
        else
        {

            security_set[candidate_node] = 1;
        }
    }

    printf("[INFO] Nodes removed: %zu. Final set size: %zu\n", removed_count, n - removed_count);

    printf("[INFO] Final minimality check (using is_minimal logic)...\n");

    int changed = 1;
    int extra_removed = 0;

    while (changed)
    {
        changed = 0;
        unsigned char *has_private = calloc(n, sizeof(unsigned char));

        for (size_t u = 0; u < n; ++u)
        {
            for (int k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
            {
                int v = g->col_ind[k];
                if ((int)u >= v)
                    continue;

                if (security_set[u] && !security_set[v])
                {
                    has_private[u] = 1;
                }
                else if (!security_set[u] && security_set[v])
                {
                    has_private[v] = 1;
                }
            }
        }

        for (size_t i = 0; i < n; i++)
        {
            if (security_set[i] && !has_private[i])
            {
                security_set[i] = 0;
                extra_removed++;
                changed = 1;
                printf("  Removed node %d (no private edge)\n", (int)i);
                break;
            }
        }

        free(has_private);
    }

    if (extra_removed > 0)
    {
        printf("[INFO] Removed %d additional nodes to ensure minimality\n", extra_removed);
    }
    else
    {
        printf("[OK] Set was already minimal after greedy phase\n");
    }

    size_t final_size = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (security_set[i])
            final_size++;
    }
    printf("[OK] Final minimal set: %zu nodes\n", final_size);

    free(sorted_nodes);
    return security_set;
}
