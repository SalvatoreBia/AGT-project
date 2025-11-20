#include <stdlib.h>
#include <stdio.h>
#include "../include/algorithm.h"


double calculate_utility(game_system *game, int player_id, int strategy)
{
    // indipendentemente se i miei vicini hanno
    // messo o no le misure di sicurezza, nel caso
    // le stia mettendo pure io pago -1 fisso
    if (strategy == 1) return -COST_SECURITY;

    // in caso invece non volessi mettere la sicurezza,
    // devo controllare quanti miei vicini non l'hanno messa
    // siccome il mio costo dipenderà da quello
    double curr_payoff = 0.0;

    adj_node *ptr = game->g->nodes[player_id];
    while (ptr != NULL)
    {
        int neighbor_id = ptr->dest;

        int neighbor_strategy = game->players[neighbor_id].current_strategy;
        
        // siccome o un nodo mette la sicurezza o TUTTI i suoi vicini
        // devono averla messa, se mi accorgo che un mio vicino non ce l'ha
        // devo incentivare il nodo corrente a farlo, ergo penalizzo
        if (neighbor_strategy == 0)
        {
            curr_payoff -= PENALTY_UNSECURED;
        }

        ptr = ptr->next;
    }

    return curr_payoff;
}

// ogni giocatore controlla le strategie correnti di tutti gli altri
// cercando di massimizzare la propia utilità
//
// ritorna 1 se almeno un giocatore ha cambiato strategia, 0 altrimenti (equilibrio di Nash)
int run_best_response_iteration(game_system *game)
{
    int change_occurred = 0;

    for (int i = 0; i < game->num_players; ++i)
    {
        int curr_strategy = game->players[i].current_strategy;

        double utility_if_out = calculate_utility(game, i, 0);
        double utility_if_in  = calculate_utility(game, i, 1);

        int best_strategy = curr_strategy;

        if (utility_if_in > utility_if_out)      best_strategy = 1;
        else if (utility_if_out > utility_if_in) best_strategy = 0;

        if (best_strategy != curr_strategy)
        {
            game->players[i].current_strategy = best_strategy;
            change_occurred = 1;
        }
    }

    game->iteration++;
    return change_occurred;
}