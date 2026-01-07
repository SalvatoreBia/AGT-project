#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../include/strategic_game.h"
#include "../include/logging.h"

static double get_random_double()
{
    return (double)rand() / (double)RAND_MAX;
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
