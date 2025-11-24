#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "../include/algorithm.h"

static double get_random_double()
{
    return (double)rand() / (double)RAND_MAX;
}

double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy){
    // Se mi attivo (1), pago il costo fisso
    if (strategy == 1)
        return -COST_SECURITY;

    // Se mi disattivo (0), pago penalità per ogni vicino disattivato
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

////////////////////////////
// BEST RESPONSE DYNAMICS
////////////////////////////

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
        }
    }

    return change_occurred;
}

//////////////////////
// REGRET MATCHING
//////////////////////

void init_regret_system(game_system *game)
{
    game->rs.regrets = (double *)calloc(game->num_players * 2, sizeof(double));
    game->rs.probs = (double *)calloc(game->num_players * 2, sizeof(double));

    // Inizializza probabilità uniformi
    for (uint64_t i = 0; i < game->num_players * 2; ++i)
    {
        game->rs.probs[i] = 0.5;
    }
}

void free_regret_system(game_system *game)
{
    if (game->rs.regrets)
        free(game->rs.regrets);
    if (game->rs.probs)
        free(game->rs.probs);
}

uint64_t run_regret_matching_iteration(game_system *game)
{
    uint64_t nnodes = game->num_players;
    
    // 1. Update strategies based on probabilities from PREVIOUS iteration
    for (uint64_t i = 0; i < nnodes; ++i)
    {
        double prob_1 = game->rs.probs[2 * i + 1];
        if (get_random_double() < prob_1)
        {
            game->strategies[i] = 1;
        }
        else
        {
            game->strategies[i] = 0;
        }
    }

    uint64_t is_nash = 1;

    // 2. Calculate regrets and update probabilities for NEXT iteration
    for (uint64_t i = 0; i < nnodes; ++i)
    {
        double u0 = calculate_utility(game, i, 0);
        double u1 = calculate_utility(game, i, 1);
        double u_real = (game->strategies[i] == 1) ? u1 : u0;

        double r0 = u0 - u_real;
        double r1 = u1 - u_real;

        // Check if anyone has an incentive to deviate (Instantaneous Regret > 0)
        // Using a small epsilon for floating point comparisons
        if (r0 > 1e-9 || r1 > 1e-9) {
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

    // Return 1 if NOT converged (not Nash), 0 if converged (Nash)
    return !is_nash;
}

///////////////////////////////////////////
// FICTIOUS PLAY
///////////////////////////////////////////

void init_fictious_system(game_system *game){
    game->fs.probs = (double *)calloc(game->num_players * 2, sizeof(double));
    game->fs.order = (uint64_t *)malloc(game->num_players * sizeof(uint64_t));

    for (uint64_t i = 0; i < game->num_players * 2; ++i){
        game->fs.probs[i] = 0.5;
    }
    for (uint64_t i = 0; i < game->num_players; ++i) {
        game->fs.order[i] = i;
    }
    game->fs.believes = (char *)calloc(game->num_players, sizeof(char));
    game->fs.turn = 0;
}

void free_fictious_system(game_system *game){
    if (game->fs.probs) free(game->fs.probs);
    if (game->fs.believes) free(game->fs.believes);
    if (game->fs.order) free(game->fs.order);
}

uint64_t run_fictious_play_iteration(game_system *game){
    // Fisher-Yates shuffle
    for (uint64_t i = game->num_players - 1; i > 0; --i) {
        uint64_t j = (uint64_t)(get_random_double() * (i + 1));
        if (j > i) j = i;
        
        uint64_t temp = game->fs.order[i];
        game->fs.order[i] = game->fs.order[j];
        game->fs.order[j] = temp;
    }

    uint64_t change_occurred = 0;
    
    for (size_t k = 0; k < game->num_players; k++){
        uint64_t i = game->fs.order[k];
        uint64_t old_strategy = game->strategies[i];

        double expected_utility_0 = 0.0;
        uint64_t start = game->g->row_ptr[i];
        uint64_t end = game->g->row_ptr[i + 1];

        for (uint64_t m = start; m < end; ++m)
        {
            uint64_t neighbor_id = game->g->col_ind[m];
            double prob_neighbor_0 = game->fs.probs[2 * neighbor_id];
            expected_utility_0 += prob_neighbor_0 * (-PENALTY_UNSECURED);
        }

        double expected_utility_1 = -COST_SECURITY;

        // Best Response against empirical distribution
        if (expected_utility_0 >= expected_utility_1){
            // printf("utilities: 0 = %lf ; 1 = %lf ; chosen 0\n", expected_utility_0, expected_utility_1);
            game->strategies[i] = 0;
        } else {
            // printf("utilities: 0 = %lf ; 1 = %lf ; chosen 1\n", expected_utility_0, expected_utility_1);
            game->strategies[i] = 1;
        }

        if (game->strategies[i] != old_strategy) {
            change_occurred = 1;
        }
        
        // Sequential Update (Gauss-Seidel)
        // Updating probabilities immediately allows subsequent players to react to the
        // updated beliefs within the same iteration, breaking symmetry and preventing oscillation.
        game->fs.probs[2*i] = (game->fs.probs[2*i] * game->fs.turn + (game->strategies[i] == 0 ? 1.0 : 0.0)) / (game->fs.turn + 1);
        game->fs.probs[2*i+1] = (game->fs.probs[2*i+1] * game->fs.turn + (game->strategies[i] == 1 ? 1.0 : 0.0)) / (game->fs.turn + 1);
    }
    
    game->fs.turn++;
    return change_occurred;
}

///////////////////////////////////////////
// HELPER PER CONTROLLARE LE SOLUZIONI
///////////////////////////////////////////

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