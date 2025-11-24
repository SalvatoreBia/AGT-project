#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "include/algorithm.h"
#include "include/data_structures.h"

#define ALGO_BRD 1
#define ALGO_RM 2
#define ALGO_FP 3

#define CURRENT_ALGORITHM ALGO_RM

#define GRAPH_FILENAME "graph_dump.bin"

#ifndef MAX_IT
#define MAX_IT 1000
#endif

int main(void)
{
    game_system game;
    graph *g = NULL;
    srand((unsigned int)time(NULL));

    printf("Checking for graph '%s'...\n", GRAPH_FILENAME);

    // decommenta per caricare il dump
    // g = load_graph_from_file(GRAPH_FILENAME);

    if (g)
    {
        printf("Graph loaded successfully!\n");
    }
    else
    {
        printf("Generating new random regular graph...\n");
        g = generate_random_regular(4, 2);
        if (!g)
        {
            fprintf(stderr, "Error: Failed to generate graph.\n");
            return 1;
        }
        save_graph_to_file(g, GRAPH_FILENAME);
    }

    clock_t start_time = clock();
    init_game(&game, g);

#if CURRENT_ALGORITHM == ALGO_BRD
    printf("Algorithm: Best Response Dynamics (BRD)\n");
#elif CURRENT_ALGORITHM == ALGO_RM
    printf("Algorithm: Regret Matching (RM)\n");
    init_regret_system(&game);
#elif CURRENT_ALGORITHM == ALGO_FP
    printf("Algorithm: Fictitious Play (FP)\n");
    // init_fictitious_play(&game);
#endif

    uint64_t converged = 0;

    while (game.iteration < MAX_IT)
    {
        if (game.iteration % 100 == 0)
        {
            printf("--- It %lu ---\n", game.iteration + 1);
        }

#if CURRENT_ALGORITHM == ALGO_BRD
        if (!run_best_response_iteration(&game))
        {
            converged = 1;
            printf("Nash Equilibrium reached at it %lu\n", game.iteration);
            break;
        }
#elif CURRENT_ALGORITHM == ALGO_RM
        run_regret_matching_iteration(&game);
#elif CURRENT_ALGORITHM == ALGO_FP
        // run_fictitious_play_iteration(&game);
#endif
    }

    double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("\nSimulation finished in %.2fs\n", elapsed);

    // 4. ANALISI RISULTATI
    int minimal = is_minimal(&game);
    int valid = is_valid_cover(&game);

    long active_count = 0;
    for (uint64_t i = 0; i < game.num_players; ++i)
    {
        if (game.strategies[i] == 1)
            active_count++;
    }

    printf("Cover Size: %ld / %lu\n", active_count, game.num_players);
    printf("Valid Cover: %s\n", valid ? "YES" : "NO");
    printf("Minimal Local: %s\n", minimal ? "YES" : "NO");

#if CURRENT_ALGORITHM == ALGO_RM
    free_regret_system(&game);
#endif

    free_game(&game);
    free_graph(g);

    return 0;
}