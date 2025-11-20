#include <stdio.h>
#include "include/algorithm.h"
#include "include/data_structures.h"



int main(void)
{
    game_system game;

    // graph *g = load_graph_from_file("example_graph.txt");
    // graph * g = generate_erdos_renyi(1000000, 0.01);
    
    graph *g = generate_random_regular(8000000, 4);

    printf("Grafo generato\n");
    if (g == NULL) return 1;

    init_game(&game, g);

    printf("Stato iniziale (Random):\n");

    int converged = 0;
    while (!converged)
    {
        printf("--- Iterazione %d ---\n", game.iteration + 1);

        int changed = run_best_response_iteration(&game);
        if (!changed)
        {
            converged = 1;
            printf("Equilibrio di Nash raggiunto\n");
        }

        if (game.iteration > MAX_IT) break;
    }

    printf("Network Security Set:");
    for (int i = 0; i < game.num_players; ++i)
    {
        if (game.players[i].current_strategy == 1) printf("%d ", game.players[i].id);
    }

    printf("\n");
    return 0;
}