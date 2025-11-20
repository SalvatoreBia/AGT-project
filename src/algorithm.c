#include <stdlib.h>
#include <stdio.h>
#include "../include/algorithm.h"


double calculate_utility(game_system *game, uint64_t player_id, uint64_t strategy)
{
    // indipendentemente se i miei vicini hanno
    // messo o no le misure di sicurezza, nel caso
    // le stia mettendo pure io pago -1 fisso
    if (strategy == 1) return -COST_SECURITY;

    // in caso invece non volessi mettere la sicurezza,
    // devo controllare quanti miei vicini non l'hanno messa
    // siccome il mio costo dipenderà da quello
    double curr_payoff = 0.0;

    uint64_t start = game->g->row_ptr[player_id];
    uint64_t end = game->g->row_ptr[player_id + 1];

    for (uint64_t i = start; i < end; ++i)
    {
        uint64_t neighbor_id = game->g->col_ind[i];
        uint64_t neighbor_strategy = game->strategies[neighbor_id];
        
        // siccome o un nodo mette la sicurezza o TUTTI i suoi vicini
        // devono averla messa, se mi accorgo che un mio vicino non ce l'ha
        // devo incentivare il nodo corrente a farlo, ergo penalizzo
        if (neighbor_strategy == 0)
        {
            curr_payoff -= PENALTY_UNSECURED;
        }
    }

    return curr_payoff;
}

// ogni giocatore controlla le strategie correnti di tutti gli altri
// cercando di massimizzare la propia utilità
//
// ritorna 1 se almeno un giocatore ha cambiato strategia, 0 altrimenti (equilibrio di Nash)
uint64_t run_best_response_iteration(game_system *game)
{
    uint64_t change_occurred = 0;

    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        uint64_t curr_strategy = game->strategies[i];

        double utility_if_out = calculate_utility(game, i, 0);
        double utility_if_in  = calculate_utility(game, i, 1);

        uint64_t best_strategy = curr_strategy;

        if (utility_if_in > utility_if_out)      best_strategy = 1;
        else if (utility_if_out > utility_if_in) best_strategy = 0;

        if (best_strategy != curr_strategy)
        {
            game->strategies[i] = best_strategy;
            change_occurred = 1;
        }
    }

    game->iteration++;
    return change_occurred;
}

int is_minimal(game_system *game)
{
    graph *g = game->g;
    uint64_t n = game->num_players;

    // array che indica se un vertice del cover ha almeno un "private edge"
    unsigned char *has_private = (unsigned char*) calloc(n, sizeof(unsigned char));
    if (!has_private) return 0; // fallimento allocazione -> consideriamo non minimale

    // Scorriamo tutti i vicini per costruire l'informazione sui private edges.
    // Per grafi non orientati memorizzati simmetricamente, processiamo solo u < v per
    // non contare due volte lo stesso arco.
    for (uint64_t u = 0; u < n; ++u)
    {
        uint64_t start = g->row_ptr[u];
        uint64_t end   = g->row_ptr[u + 1];

        for (uint64_t ei = start; ei < end; ++ei)
        {
            uint64_t v = g->col_ind[ei];

            // evita di considerare lo stesso arco due volte (assumendo rappresentazione simmetrica)
            if (u >= v) continue;

            unsigned char in_u = game->strategies[u]; // 0 o 1
            unsigned char in_v = game->strategies[v];

            // Se esiste esattamente un endpoint nel cover, quell'endpoint ha un private edge
            if (in_u && !in_v)
            {
                has_private[u] = 1;
            }
            else if (in_v && !in_u)
            {
                has_private[v] = 1;
            }
            // se entrambi in cover o entrambi fuori, questo arco non produce private edge
        }
    }

    // Ora controlliamo che ogni vertice che è nel cover abbia almeno un private edge
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
/*
uint64_t is_solution_minimal(game_system *game)
{
    // Try removing each node that's currently in the cover (strategy == 1)
    for (uint64_t i = 0; i < game->num_players; ++i)
    {
        if (game->strategies[i] == 1)
        {
            // Temporarily remove node i from the cover
            game->strategies[i] = 0;
            
            // Check if the remaining cover is still valid:
            // Every node with strategy == 0 must have all neighbors with strategy == 1
            uint64_t valid = 1;
            
            for (uint64_t node = 0; node < game->num_players; ++node)
            {
                if (game->strategies[node] == 0)
                {
                    uint64_t start = game->g->row_ptr[node];
                    uint64_t end = game->g->row_ptr[node + 1];
                    
                    // Check all neighbors of this uncovered node
                    for (uint64_t j = start; j < end; ++j)
                    {
                        uint64_t neighbor_id = game->g->col_ind[j];
                        
                        // If any neighbor is also 0, edge (node, neighbor) is uncovered
                        if (game->strategies[neighbor_id] == 0)
                        {
                            valid = 0;
                            break;
                        }
                    }
                    
                    if (!valid) break;
                }
            }
            
            // Restore node i to the cover
            game->strategies[i] = 1;
            
            // If the cover was still valid without node i, then i is redundant
            if (valid) return 0;
        }
    }
    
    return 1; // No node could be removed, so the cover is minimal
}
*/