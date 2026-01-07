#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include "../include/algorithm.h"
#include "../include/logging.h"

static double get_random_double()
{
    return (double)rand() / (double)RAND_MAX;
}

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


double calculate_utility(game_system *game, int player_id, int strategy)
{
    if (strategy == 1)
        return -COST_SECURITY;

    double curr_payoff = 0.0;
    int start = game->g->row_ptr[player_id];
    int end = game->g->row_ptr[player_id + 1];

    for (int i = start; i < end; ++i)
    {
        int neighbor_id = game->g->col_ind[i];
        if (game->strategies[neighbor_id] == 0)
        {
            curr_payoff -= PENALTY_UNSECURED;
        }
    }
    return curr_payoff;
}

int run_best_response_iteration(game_system *game)
{
    int change_occurred = 0;

    for (int i = 0; i < game->num_players; ++i)
    {
        int curr_strategy = game->strategies[i];
        double u_out = calculate_utility(game, i, 0);
        double u_in = calculate_utility(game, i, 1);

        int best_strategy = curr_strategy;

        if (u_in > u_out)
            best_strategy = 1;
        else if (u_out > u_in)
            best_strategy = 0;

        if (best_strategy != curr_strategy)
        {
            game->strategies[i] = best_strategy;
            change_occurred = 1;
            LOG_NODE_UPDATE(i, curr_strategy, best_strategy,
                           (best_strategy == 1) ? u_in : u_out);
        }
    }

    return change_occurred;
}

void init_regret_system(game_system *game)
{
    game->rs.regrets = (double *)calloc(game->num_players * 2, sizeof(double));
    game->rs.probs = (double *)calloc(game->num_players * 2, sizeof(double));

    for (int i = 0; i < game->num_players * 2; ++i)
    {
        game->rs.probs[i] = 0.5;
    }
}

void free_regret_system(game_system *game)
{
    free(game->rs.regrets);
    free(game->rs.probs);
}

int run_regret_matching_iteration(game_system *game)
{
    int nnodes = game->num_players;

    for (int i = 0; i < nnodes; ++i)
    {
        double prob_1 = game->rs.probs[2 * i + 1];
        int old_s = game->strategies[i];
        game->strategies[i] = (get_random_double() < prob_1) ? 1 : 0;
        if (game->strategies[i] != old_s) {
            LOG_NODE_UPDATE(i, old_s, game->strategies[i], 0.0);
        }
    }

    int is_nash = 1;

    for (int i = 0; i < nnodes; ++i)
    {
        double u0 = calculate_utility(game, i, 0);
        double u1 = calculate_utility(game, i, 1);
        double u_real = (game->strategies[i] == 1) ? u1 : u0;

        double r0 = u0 - u_real;
        double r1 = u1 - u_real;

        if (r0 > 1e-9 || r1 > 1e-9)
        {
            is_nash = 0;
        }

        game->rs.regrets[2 * i] += r0;
        game->rs.regrets[2 * i + 1] += r1;

        double r0_pos = (game->rs.regrets[2 * i] > 0) ? game->rs.regrets[2 * i] : 0.0;
        double r1_pos = (game->rs.regrets[2 * i + 1] > 0) ? game->rs.regrets[2 * i + 1] : 0.0;
        double sum = r0_pos + r1_pos;

        if (sum > 1e-9)
        {
            game->rs.probs[2 * i] = r0_pos / sum;
            game->rs.probs[2 * i + 1] = r1_pos / sum;
        }
        else
        {
            game->rs.probs[2 * i] = 0.5;
            game->rs.probs[2 * i + 1] = 0.5;
        }
    }

    return !is_nash;

}



void reset_fictitious_system(game_system *game)
{

    game->fs.turn = 100;



    for (int i = 0; i < game->num_players; ++i)
    {

        int variance = rand() % 11;

        game->fs.counts[i] = 90 + variance;

        game->fs.believes[i] = (double)game->fs.counts[i] / (double)game->fs.turn;

        game->strategies[i] = (rand() % 2);
    }
}

void init_fictitious_system(game_system *game)
{
    game->fs.counts = (int *)calloc(game->num_players, sizeof(int));
    game->fs.believes = (double *)calloc(game->num_players, sizeof(double));

    reset_fictitious_system(game);
}

void free_fictitious_system(game_system *game)
{
    free(game->fs.counts);
    free(game->fs.believes);
}

int run_fictitious_play_iteration(game_system *game)
{

    int n = game->num_players;

    for (int i = 0; i < n; ++i)
    {

        game->fs.believes[i] = (double)game->fs.counts[i] / (double)game->fs.turn;
    }

    int change_occurred = 0;
    unsigned char *next_strategies = malloc(n * sizeof(unsigned char));

    for (int i = 0; i < n; ++i)
    {

        double eu_1 = -COST_SECURITY;

        double eu_0 = 0.0;

        int start = game->g->row_ptr[i];
        int end = game->g->row_ptr[i + 1];

        for (int k = start; k < end; ++k)
        {
            int neighbor = game->g->col_ind[k];

            double prob_neighbor_0 = 1.0 - game->fs.believes[neighbor];
            eu_0 -= PENALTY_UNSECURED * prob_neighbor_0;
        }



        if (eu_1 > eu_0)
        {
            next_strategies[i] = 1;
        }
        else
        {
            next_strategies[i] = 0;
        }

        if (next_strategies[i] != game->strategies[i])
        {
            change_occurred = 1;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        int old_s = game->strategies[i];
        game->strategies[i] = next_strategies[i];
        if (game->strategies[i] != old_s) {
            LOG_NODE_UPDATE(i, old_s, game->strategies[i], 0.0);
        }

        if (game->strategies[i] == 1)
        {
            game->fs.counts[i]++;
        }
    }
    game->fs.turn++;

    free(next_strategies);
    return change_occurred;
}



int is_valid_cover(game_system *game)
{
    graph *g = game->g;
    for (int u = 0; u < g->num_nodes; ++u)
    {
        if (game->strategies[u] == 1)
            continue;

        for (int k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            int v = g->col_ind[k];
            if (u > v)
                continue;

            if (game->strategies[v] == 0)
            {

                return 0;
            }
        }
    }
    return 1;
}

int is_minimal(game_system *game)
{
    graph *g = game->g;
    int n = game->num_players;
    unsigned char *has_private = (unsigned char *)calloc(n, sizeof(unsigned char));

    if (!has_private)
        return 0;

    for (int u = 0; u < n; ++u)
    {
        for (int k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            int v = g->col_ind[k];
            if (u >= v)
                continue;

            unsigned char su = game->strategies[u];
            unsigned char sv = game->strategies[v];

            if (su && !sv)
                has_private[u] = 1;
            else if (!su && sv)
                has_private[v] = 1;
        }
    }

    int minimal = 1;
    for (int i = 0; i < n; ++i)
    {
        if (game->strategies[i] == 1 && !has_private[i])
        {
            minimal = 0;
            break;
        }
    }

    free(has_private);
    return minimal;
}



int run_simulation(game_system *game, int algorithm, int max_it, int verbose)
{
    int converged = 0;
    int no_change_streak = 0;
    int last_restart_it = 0;
    const int restart_interval = 1000;

    while (game->iteration < max_it)
    {

        if (algorithm == ALGO_FP &&
            (game->iteration - last_restart_it) >= restart_interval)
        {
            if (verbose)
                printf("[INFO] Iteration %d: Random restart triggered\n", game->iteration);
            reset_fictitious_system(game);
            last_restart_it = game->iteration;
            no_change_streak = 0;
        }

        if (verbose && game->iteration % 100 == 0)
        {
            printf("[INFO] Iteration %d\n", game->iteration + 1);
        }

        const char *algo_name = "UNKNOWN";
        if (algorithm == ALGO_BRD) algo_name = "BRD";
        else if (algorithm == ALGO_RM) algo_name = "RM";
        else if (algorithm == ALGO_FP) algo_name = "FP";
        (void)algo_name;


        LOG_STEP_BEGIN(game->iteration, algo_name);

        int change = 0;

        if (algorithm == ALGO_BRD)
        {
            change = run_best_response_iteration(game);
        }
        else if (algorithm == ALGO_RM)
        {
            change = run_regret_matching_iteration(game);
        }
        else if (algorithm == ALGO_FP)
        {
            change = run_fictitious_play_iteration(game);
        }

        LOG_STEP_END();

        if (!change)
        {
            no_change_streak++;
        }
        else
        {
            no_change_streak = 0;
        }

        if (no_change_streak >= 500)

        {
            converged = 1;
            if (verbose)
                printf("[OK] Convergence reached at iteration %d\n", game->iteration);
            break;
        }

        game->iteration++;
    }

    return converged ? (int)game->iteration : -1;
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

    // Puntatore alla funzione caratteristica da usare
    double (*char_func)(graph *, int *, size_t) = NULL;

    if (version == 1)
        char_func = characteristic_function_v1;
    else if (version == 2)
        char_func = characteristic_function_v2;
    else if (version == 3)
        char_func = characteristic_function_v3;
    else
    {
        fprintf(stderr, "Errore: versione funzione caratteristica non valida (%d)\n", version);
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
