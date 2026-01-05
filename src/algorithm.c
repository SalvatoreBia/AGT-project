#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <glib.h>
#include "../include/algorithm.h"
#include "../include/logging.h"

/* ============================================================================
 * PRIVATE HELPER FUNCTIONS
 * ============================================================================ */

static double get_random_double()
{
    return (double)rand() / (double)RAND_MAX;
}

static void shuffle_array(uint64_t *array, size_t n)
{
    if (n > 1)
    {
        for (size_t i = 0; i < n - 1; i++)
        {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            uint64_t t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

/* ============================================================================
 * STRATEGIC GAME: UTILITY CALCULATION
 * ============================================================================ */

double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy)
{
    if (strategy == 1)
        return -COST_SECURITY;

    // Strategy 0: pay penalty for each unsecured neighbor
    double curr_payoff = 0.0;
    uint64_t start = game->g->row_ptr[player_id];
    uint64_t end = game->g->row_ptr[player_id + 1];

    for (uint64_t i = start; i < end; ++i)
    {
        uint64_t neighbor_id = game->g->col_ind[i];
        if (game->strategies[neighbor_id] == 0)
        {
            curr_payoff -= PENALTY_UNSECURED;
        }
    }
    return curr_payoff;
}

/* ============================================================================
 * STRATEGIC GAME: BEST RESPONSE DYNAMICS (BRD)
 * ============================================================================ */

uint64_t run_best_response_iteration(game_system *game)
{
    uint64_t change_occurred = 0;

    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        uint64_t curr_strategy = game->strategies[i];
        double u_out = calculate_utility(game, i, 0);
        double u_in = calculate_utility(game, i, 1);

        uint64_t best_strategy = curr_strategy;

        if (u_in > u_out)
            best_strategy = 1;
        else if (u_out > u_in)
            best_strategy = 0;

        if (best_strategy != curr_strategy)
        {
            game->strategies[i] = best_strategy;
            change_occurred = 1;
            double chosen_u = (best_strategy == 1) ? u_in : u_out;
            LOG_NODE_UPDATE(i, curr_strategy, best_strategy, chosen_u);
        }
    }

    return change_occurred;
}

/* ============================================================================
 * STRATEGIC GAME: REGRET MATCHING (RM)
 * ============================================================================ */

void init_regret_system(game_system *game)
{
    game->rs.regrets = (double *)calloc(game->num_players * 2, sizeof(double));
    game->rs.probs = (double *)calloc(game->num_players * 2, sizeof(double));

    // Initialize with uniform probabilities
    for (uint64_t i = 0; i < game->num_players * 2; ++i)
    {
        game->rs.probs[i] = 0.5;
    }
}

void free_regret_system(game_system *game)
{
    free(game->rs.regrets);
    free(game->rs.probs);
}

uint64_t run_regret_matching_iteration(game_system *game)
{
    uint64_t nnodes = game->num_players;

    // Step 1: Sample strategies based on probabilities from previous iteration
    for (uint64_t i = 0; i < nnodes; ++i)
    {
        double prob_1 = game->rs.probs[2 * i + 1];
        uint64_t old_s = game->strategies[i];
        game->strategies[i] = (get_random_double() < prob_1) ? 1 : 0;
        if (game->strategies[i] != old_s) {
            LOG_NODE_UPDATE(i, old_s, game->strategies[i], 0.0);
        }
    }

    uint64_t is_nash = 1;

    // Step 2: Calculate regrets and update probabilities for next iteration
    for (uint64_t i = 0; i < nnodes; ++i)
    {
        double u0 = calculate_utility(game, i, 0);
        double u1 = calculate_utility(game, i, 1);
        double u_real = (game->strategies[i] == 1) ? u1 : u0;

        double r0 = u0 - u_real;
        double r1 = u1 - u_real;

        // Check for Nash equilibrium (no incentive to deviate)
        if (r0 > 1e-9 || r1 > 1e-9)
        {
            is_nash = 0;
        }

        // Update cumulative regrets
        game->rs.regrets[2 * i] += r0;
        game->rs.regrets[2 * i + 1] += r1;

        // Calculate positive regrets
        double r0_pos = (game->rs.regrets[2 * i] > 0) ? game->rs.regrets[2 * i] : 0.0;
        double r1_pos = (game->rs.regrets[2 * i + 1] > 0) ? game->rs.regrets[2 * i + 1] : 0.0;
        double sum = r0_pos + r1_pos;

        // Update probabilities using regret matching
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

    return !is_nash; // Return 1 if not converged
}

/* ============================================================================
 * STRATEGIC GAME: FICTITIOUS PLAY (FP)
 * ============================================================================ */

void reset_fictitious_system(game_system *game)
{
    // Set a fictitious history of length 100 to break symmetry
    game->fs.turn = 100;
    
    // Initialize with random counts to create heterogeneous priors around the critical threshold
    // Threshold is approx 0.975 for Cost=1, Penalty=10, Degree=4.
    // We use turn=100 and counts in [90, 100] to seed some below and some above/near.
    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        // Random count between 90 and 100
        uint64_t variance = rand() % 11; // 0 to 10
        game->fs.counts[i] = 90 + variance;
        
        // Calculate initial belief based on this fictitious history
        game->fs.believes[i] = (double)game->fs.counts[i] / (double)game->fs.turn;

        // Also randomize initial strategy to help escape basin
        game->strategies[i] = (rand() % 2);
    }
}

void init_fictitious_system(game_system *game)
{
    game->fs.counts = (uint64_t *)calloc(game->num_players, sizeof(uint64_t));
    game->fs.believes = (double *)calloc(game->num_players, sizeof(double));
    
    reset_fictitious_system(game);
}

void free_fictitious_system(game_system *game)
{
    free(game->fs.counts);
    free(game->fs.believes);
}

uint64_t run_fictitious_play_iteration(game_system *game)
{
    // printf("DEBUG: FP iteration start, turn=%" PRIu64 "\n", game->fs.turn);
    uint64_t n = game->num_players;
    
    // 1. Update Beliefs
    // Update beliefs based on accumulated history
    for (uint64_t i = 0; i < n; ++i)
    {
        // counts[i] stores how many times player i played Strategy 1 (Security)
        game->fs.believes[i] = (double)game->fs.counts[i] / (double)game->fs.turn;
    }

    uint64_t change_occurred = 0;
    unsigned char *next_strategies = malloc(n * sizeof(unsigned char));

    // 2. Best Response against Beliefs
    for (uint64_t i = 0; i < n; ++i)
    {
        // Expected Utility of playing Strategy 1 (Security)
        // Cost is fixed -1.0
        double eu_1 = -COST_SECURITY; 

        // Expected Utility of playing Strategy 0 (No Security)
        // Sum of (-PENALTY_UNSECURED * Prop(Neighbor plays 0))
        double eu_0 = 0.0;
        
        uint64_t start = game->g->row_ptr[i];
        uint64_t end = game->g->row_ptr[i + 1];

        for (uint64_t k = start; k < end; ++k)
        {
            uint64_t neighbor = game->g->col_ind[k];
            // Probability neighbor plays 0 = 1.0 - Probability neighbor plays 1 (belief)
            double prob_neighbor_0 = 1.0 - game->fs.believes[neighbor];
            eu_0 -= PENALTY_UNSECURED * prob_neighbor_0;
        }

        // Choose best strategy
        // If tied, sticking to current or default preference (usually 1 if equal avoids penalty?)
        // Here strict inequality for change:
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

    // 3. Update State
    // Apply strategies and update history counts
    for (uint64_t i = 0; i < n; ++i)
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

/* ============================================================================
 * STRATEGIC GAME: VALIDATION HELPERS
 * ============================================================================ */

int is_valid_cover(game_system *game)
{
    graph *g = game->g;
    for (uint64_t u = 0; u < g->num_nodes; ++u)
    {
        if (game->strategies[u] == 1)
            continue;

        // Se sono spento, controllo i vicini
        for (uint64_t k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            uint64_t v = g->col_ind[k];
            if (u > v)
                continue;

            if (game->strategies[v] == 0)
            {
                // Trovato arco scoperto
                return 0;
            }
        }
    }
    return 1;
}

int is_minimal(game_system *game)
{
    graph *g = game->g;
    uint64_t n = game->num_players;
    unsigned char *has_private = (unsigned char *)calloc(n, sizeof(unsigned char));

    if (!has_private)
        return 0;

    // Identifica nodi con archi privati
    for (uint64_t u = 0; u < n; ++u)
    {
        for (uint64_t k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            uint64_t v = g->col_ind[k];
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
    for (uint64_t i = 0; i < n; ++i)
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

/* ============================================================================
 * STRATEGIC GAME: SIMULATION RUNNER
 * ============================================================================ */

int64_t run_simulation(game_system *game, int algorithm, uint64_t max_it, int verbose)
{
    uint64_t converged = 0;
    int no_change_streak = 0;
    uint64_t last_restart_it = 0;
    const uint64_t restart_interval = 1000;

    while (game->iteration < max_it)
    {
        // Check for Random Restart if not converged for a long time
        if (algorithm == ALGO_FP && 
            (game->iteration - last_restart_it) >= restart_interval)
        {
            if (verbose)
                printf("--- It %" PRIu64 " : Random Restart Triggered! ---\n", game->iteration);
            reset_fictitious_system(game);
            last_restart_it = game->iteration;
            no_change_streak = 0;
        }
        
        if (verbose && game->iteration % 1000 == 0)
        {
            printf("--- It %" PRIu64 " ---\n", game->iteration + 1);
        }

        const char *algo_name = "UNKNOWN";
        if (algorithm == ALGO_BRD) algo_name = "BRD";
        else if (algorithm == ALGO_RM) algo_name = "RM";
        else if (algorithm == ALGO_FP) algo_name = "FP";

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

        if (no_change_streak >= 500) // Require streak of 500 unchanged iterations to allow beliefs to saturate
        {
            converged = 1;
            if (verbose)
                printf("Convergence reached at it %" PRIu64 "\n", game->iteration);
            break;
        }

        game->iteration++;
    }

    return converged ? (int64_t)game->iteration : -1;
}

/* ============================================================================
 * COALITIONAL GAME: EDGE STRUCTURE AND HASH FUNCTIONS
 * ============================================================================ */

typedef struct
{
    uint64_t u;
    uint64_t v;
} edge;

static guint edge_hash(gconstpointer key)
{
    const edge *e = (const edge *)key;
    uint64_t hash = e->u ^ (e->v << 1);
    return (guint)(hash % UINT32_MAX);
}

static gboolean edge_equal(gconstpointer a, gconstpointer b)
{
    const edge *e1 = (const edge *)a;
    const edge *e2 = (const edge *)b;
    return (e1->u == e2->u) && (e1->v == e2->v);
}

int count_covered_edges(graph *g, uint64_t *coalition, size_t coalition_size)
{
    GHashTable *edges = g_hash_table_new_full(edge_hash, edge_equal, g_free, NULL);
    if (edges == NULL)
        return -1;

    edge temp_edge;

    for (size_t i = 0; i < coalition_size; ++i)
    {
        uint64_t curr = coalition[i];
        uint64_t start = g->row_ptr[curr];
        uint64_t end = g->row_ptr[curr + 1];

        for (uint64_t j = start; j < end; ++j)
        {
            uint64_t neighbor = g->col_ind[j];

            // Normalize edge (ensure u < v)
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

            // Add edge if not already in set
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

int is_coalition_valid_cover(graph *g, uint64_t *coalition, size_t coalition_size)
{
    unsigned char *in_coalition = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));
    if (!in_coalition)
        return 0;

    // Mark nodes in coalition
    for (size_t i = 0; i < coalition_size; ++i)
    {
        in_coalition[coalition[i]] = 1;
    }

    // Check if all edges are covered
    int is_valid = 1;
    for (uint64_t u = 0; u < g->num_nodes; ++u)
    {
        for (uint64_t k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            uint64_t v = g->col_ind[k];
            if (u >= v)
                continue; // Process each edge once

            // Edge (u,v) is uncovered if both endpoints are outside the coalition
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

int is_coalition_minimal(graph *g, uint64_t *coalition, size_t coalition_size)
{
    unsigned char *in_coalition = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));
    unsigned char *has_private = (unsigned char *)calloc(g->num_nodes, sizeof(unsigned char));

    if (!in_coalition || !has_private)
    {
        free(in_coalition);
        free(has_private);
        return 0;
    }

    // Mark nodes in coalition
    for (size_t i = 0; i < coalition_size; ++i)
    {
        in_coalition[coalition[i]] = 1;
    }

    // Identify nodes with private edges
    // A private edge for node u is an edge (u,v) where u is in the coalition but v is not
    for (uint64_t u = 0; u < g->num_nodes; ++u)
    {
        for (uint64_t k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
        {
            uint64_t v = g->col_ind[k];
            if (u >= v)
                continue; // Process each edge once

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

    // A coalition is minimal if every node in it has at least one private edge
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


/**
 * funzione caratteristica 1:
 *      per tutti i nodi nella coalizione, conta il numero di archi
 *      unici coperti, fa il rateo archi coperti / tutti gli archi
 *      e penalizza se la coalizione passata forma un vertex cover NON minimale
 */
double characteristic_function_v1(graph *g, uint64_t *coalition, size_t coalition_size)
{
    if (coalition_size == 0)
        return 0.0;

    int covered = count_covered_edges(g, coalition, coalition_size);
    if (covered < 0)
        return 0.0;

    double fraction = (double)covered / (double)g->num_edges;
    double value = fraction * 100.0;

    // Penalty for valid but non-minimal solutions
    if (is_coalition_valid_cover(g, coalition, coalition_size))
    {
        if (!is_coalition_minimal(g, coalition, coalition_size))
        {
            value -= 10.0;
        }
    }

    return value;
}

/**
 * funzione caratteristica 2:
 *      il valore della funziona è il numero di archi coperti.
 *      se la coalizione forma un cover valido premio con +100
 *      e se è minimale premio con +50
 */
double characteristic_function_v2(graph *g, uint64_t *coalition, size_t coalition_size)
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

/**
 * funzione caratteristica 1:
 *      trovo tutti gli archi coperti dalla coalizione e penalizzo di
 *      |coalition| * 0.5 (più la coalition è grande, meno valore ottengo)
 *      se è un cover valido aggiungo +50 e se è minimale +30
 */
double characteristic_function_v3(graph *g, uint64_t *coalition, size_t coalition_size)
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

    // array di valori di shapley
    double *shapley_values = calloc(n, sizeof(double));

    // array che contiene una permutazione dei nodi
    uint64_t *permutation = malloc(n * sizeof(uint64_t));

    // array di flag dei visitati
    unsigned char *visited = malloc(n * sizeof(unsigned char));

    for (size_t i = 0; i < n; i++)
    {
        permutation[i] = i;
    }

    printf("Inizio calcolo Shapley OTTIMIZZATO (%d iterazioni)...\n", iterations);

    double size_penalty = 0.0;
    if (version == 3)
        size_penalty = 0.5;

    // MONTE-CARLO
    for (int iter = 0; iter < iterations; ++iter)
    {
        // genero permutazione dei nodi e inizializzo
        // i flag per i visitati
        shuffle_array(permutation, n);
        memset(visited, 0, n * sizeof(unsigned char));

        for (size_t i = 0; i < n; ++i)
        {
            uint64_t curr_node = permutation[i];
            visited[curr_node] = 1;

            double marginal_contribution = 0.0;

            uint64_t start = g->row_ptr[curr_node];
            uint64_t end = g->row_ptr[curr_node + 1];

            // itero sui vicini del nodo corrente
            for (uint64_t j = start; j < end; ++j)
            {
                uint64_t neighbor = g->col_ind[j];

                // trucco per contare gli archi solo una volta:
                // se il mio vicino è vero, vuoldire che l'ho già controllato
                // prima, quindi non devo aggiornare la contribution
                if (!visited[neighbor])
                {
                    marginal_contribution += 1.0;
                }
            }

            // penalizza se stiamo usando la funzione caratteristica 3
            marginal_contribution -= size_penalty;

            shapley_values[curr_node] += marginal_contribution;
        }

        if (iter % 100 == 0 && iter > 0)
            printf("Iterazione %d/%d completata\n", iter, iterations);
    }

    // calcola la media su tutte le iterazioni
    for (size_t i = 0; i < n; i++)
    {
        shapley_values[i] /= iterations;
    }

    free(permutation);
    free(visited);
    printf("Calcolo Shapley completato!\n");
    return shapley_values;
}

// helper per chiamare qsort in ordine descrescente
static int compare_node_values(const void *a, const void *b)
{
    typedef struct
    {
        uint64_t node_id;
        double shapley_value;
    } node_value_pair;

    const node_value_pair *na = (const node_value_pair *)a;
    const node_value_pair *nb = (const node_value_pair *)b;

    double diff = nb->shapley_value - na->shapley_value;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}

/**
 * Build a minimal security set from Shapley values using a greedy approach
 * followed by minimization.
 * 
 * Algorithm:
 * 1. Start with all nodes in the set (guaranteed to be a valid cover)
 * 2. Reverse delete: try removing nodes in order of increasing Shapley value
 * 3. Iterative minimization: remove nodes without private edges
 */
unsigned char *build_security_set_from_shapley(graph *g, double *shapley_values)
{
    size_t n = g->num_nodes;

    typedef struct
    {
        uint64_t node_id;
        double shapley_value;
    } node_value_pair;

    // Sort nodes by Shapley value (descending)
    node_value_pair *sorted_nodes = malloc(n * sizeof(node_value_pair));
    for (size_t i = 0; i < n; i++)
    {
        sorted_nodes[i].node_id = i;
        sorted_nodes[i].shapley_value = shapley_values[i];
    }
    qsort(sorted_nodes, n, sizeof(node_value_pair), compare_node_values);

    printf("\nCostruzione minimal security set da Shapley values...\n");

    // Start with full set
    unsigned char *security_set = calloc(n, sizeof(unsigned char));
    for (size_t i = 0; i < n; i++)
        security_set[i] = 1;

    // Phase 1: Reverse Delete (remove weak nodes)
    printf("Tentativo di minimizzazione (Reverse Delete Order)...\n");
    size_t removed_count = 0;

    for (size_t i = n; i > 0; i--)
    {
        size_t index = i - 1;
        uint64_t candidate_node = sorted_nodes[index].node_id;

        // Try removing the node
        security_set[candidate_node] = 0;

        // Check if all edges are still covered
        int still_covered = 1;
        uint64_t start = g->row_ptr[candidate_node];
        uint64_t end = g->row_ptr[candidate_node + 1];

        for (uint64_t j = start; j < end; ++j)
        {
            uint64_t neighbor = g->col_ind[j];
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
            // Cannot remove, restore
            security_set[candidate_node] = 1;
        }
    }

    printf("Nodi rimossi: %zu. Dimensione set finale: %zu\n", removed_count, n - removed_count);

    // Phase 2: Ensure minimality by removing nodes without private edges
    printf("Verifica minimalità finale (usando logica is_minimal)...\n");

    int changed = 1;
    int extra_removed = 0;

    while (changed)
    {
        changed = 0;
        unsigned char *has_private = calloc(n, sizeof(unsigned char));

        // Scan all edges to identify private edges
        for (uint64_t u = 0; u < n; ++u)
        {
            for (uint64_t k = g->row_ptr[u]; k < g->row_ptr[u + 1]; ++k)
            {
                uint64_t v = g->col_ind[k];
                if (u >= v)
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

        // Remove one node without private edges
        for (size_t i = 0; i < n; i++)
        {
            if (security_set[i] && !has_private[i])
            {
                security_set[i] = 0;
                extra_removed++;
                changed = 1;
                printf("  Rimosso nodo %" PRIu64 " (nessun arco privato)\n", (uint64_t)i);
                break;
            }
        }

        free(has_private);
    }

    if (extra_removed > 0)
    {
        printf("Rimossi %d nodi addizionali per garantire minimalità\n", extra_removed);
    }
    else
    {
        printf("Il set era già minimale dopo la fase greedy\n");
    }

    // Count final size
    size_t final_size = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (security_set[i])
            final_size++;
    }
    printf("Set minimale finale: %zu nodi\n", final_size);

    free(sorted_nodes);
    return security_set;
}